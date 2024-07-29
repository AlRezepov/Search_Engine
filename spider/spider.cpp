#include "spider.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/locale.hpp>
#include <regex>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

Spider::Spider(const Config& config, Database& db)
    : config_(config), db_(db), ssl_ctx_(ssl::context::sslv23_client),
    work_guard_(net::make_work_guard(ioc_)) {
    std::cout << "Spider initialized." << std::endl;
}

Spider::~Spider() {
    stop_ = true;
    cv_.notify_all(); // Уведомляем все потоки

    for (auto& thread : thread_pool_) {
        if (thread.joinable()) {
            thread.join(); // Ждем завершения всех потоков
        }
    }
}

void Spider::start() {
    std::cout << "Spider starting..." << std::endl;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        urls_.emplace(config_.start_url, 0);
    }
    cv_.notify_one();

    for (size_t i = 0; i < config_.thread_count; ++i) {
        thread_pool_.emplace_back([this]() {
            try {
                this->worker_thread();
            }
            catch (const std::exception& e) {
                std::cerr << "Exception in worker thread: " << e.what() << std::endl;
            }
            });
    }

    for (auto& thread : thread_pool_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::cout << "Spider finished." << std::endl;
}

void Spider::worker_thread() {
    while (true) {
        std::pair<std::string, int> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this]() { return !urls_.empty() || stop_; });

            if (stop_ && urls_.empty()) {
                return; // Завершаем поток, если stop_ установлен и нет URL для обработки
            }

            if (!urls_.empty()) {
                task = urls_.front();
                urls_.pop();
            }
        }

        if (task.first.empty()) {
            continue;
        }

        auto [url, depth] = task;
        std::cout << "Crawling URL: " << url << " at depth: " << depth << std::endl;

        if (depth > config_.recursion_depth) {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(visited_mutex_);
            if (visited_.find(url) != visited_.end()) {
                continue;
            }
            visited_.insert(url);
        }

        std::string content = fetch_page(url);
        if (content.empty()) {
            std::cerr << "Fetched content is empty for URL: " << url << std::endl;
            continue;
        }

        std::cout << "Processing content for URL: " << url << std::endl;
        std::cout << "Content snippet: " << content.substr(0, 1000) << "..." << std::endl;

        std::vector<std::string> links;
        try {
            links = extract_links(content);
        }
        catch (const std::exception& e) {
            std::cerr << "Exception while extracting links: " << e.what() << std::endl;
            continue;
        }

        for (const auto& link : links) {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (visited_.find(link) == visited_.end() && depth + 1 <= config_.recursion_depth) {
                urls_.emplace(link, depth + 1);
                cv_.notify_one();
            }
        }

        try {
            index_page(url, content);
        }
        catch (const std::exception& e) {
            std::cerr << "Exception while indexing page: " << e.what() << std::endl;
        }
    }
}



std::string Spider::fetch_page(const std::string& url) {
    try {
        net::io_context ioc;
        ssl::context ctx(ssl::context::sslv23_client);
        ctx.set_default_verify_paths();

        auto const scheme_end = url.find("://");
        if (scheme_end == std::string::npos) {
            throw std::invalid_argument("Invalid URL: missing scheme");
        }

        auto const host_start = scheme_end + 3;
        auto const host_end = url.find('/', host_start);
        std::string host, target;
        if (host_end == std::string::npos) {
            host = url.substr(host_start);
            target = "/";
        }
        else {
            host = url.substr(host_start, host_end - host_start);
            target = url.substr(host_end);
        }

        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host, "https");

        ssl::stream<tcp::socket> stream(ioc, ctx);
        if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
            boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), net::error::get_ssl_category() };
            throw boost::system::system_error{ ec };
        }

        net::connect(stream.next_layer(), results.begin(), results.end());
        stream.handshake(ssl::stream_base::client);

        http::request<http::string_body> req{ http::verb::get, target, 11 };
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");

        http::write(stream, req);

        boost::beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;

        http::read(stream, buffer, res);

        if (res.result() != http::status::ok) {
            std::cerr << "HTTP request failed: " << res.result_int() << " " << res.reason() << std::endl;
            return "";
        }

        std::string page_content = boost::beast::buffers_to_string(res.body().data());
        std::cout << "Fetched page content: " << page_content.substr(0, 500) << "..." << std::endl; // Показать первые 500 символов для отладки
        return page_content;
    }
    catch (std::exception& e) {
        std::cerr << "Error fetching page: " << e.what() << std::endl;
        return "";
    }
}



std::vector<std::string> Spider::extract_links(const std::string& content) {
    std::vector<std::string> links;
    try {
        std::regex link_regex(R"(<a\s+(?:[^>]*?\s+)?href=["']([^"']*)["'])");
        std::smatch match;

        std::string::const_iterator search_start(content.cbegin());
        while (std::regex_search(search_start, content.cend(), match, link_regex)) {
            std::string link = match[1].str();
            if (link.empty() || link.find("javascript:") == 0) {
                std::cerr << "Skipping invalid link: " << link << std::endl;
                search_start = match.suffix().first;
                continue;
            }
            std::cout << "Found link: " << link << std::endl;
            links.push_back(link);
            search_start = match.suffix().first;
        }

        std::cout << "Total links found: " << links.size() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in extract_links: " << e.what() << std::endl;
    }

    return links;
}



void Spider::index_page(const std::string& url, const std::string& content) {
    try {
        if (content.empty()) {
            std::cerr << "Empty content for URL: " << url << std::endl;
            return;
        }

        // Инициализация локали
        boost::locale::generator gen;
        std::locale loc = gen("");
        std::locale::global(loc);
        std::cout.imbue(loc);

        // Преобразование строки в UTF-8
        std::string utf8_content = boost::locale::conv::to_utf<char>(content, "UTF-8");
        std::string text = boost::locale::to_lower(utf8_content);

        // Удаление HTML тегов и специальных символов
        text = std::regex_replace(text, std::regex("<[^>]*>"), " ");
        text = std::regex_replace(text, std::regex("[^\\w\\s]"), " ");

        std::map<std::string, int> word_freq;
        std::istringstream iss(text);
        std::string word;
        while (iss >> word) {
            if (word.length() >= 3 && word.length() <= 32) {
                word_freq[word]++;
            }
        }

        pqxx::work txn(db_.conn());
        db_.save_document(url, content, txn);
        std::cout << "Document saved for URL: " << url << std::endl;

        pqxx::result r = txn.exec_params("SELECT id FROM search_engine.documents WHERE url = $1", url);
        if (r.empty()) {
            std::cerr << "No document found with URL: " << url << std::endl;
            return;
        }

        int document_id;
        try {
            document_id = r[0][0].as<int>();
        }
        catch (const std::bad_cast& e) {
            std::cerr << "Bad cast error: " << e.what() << std::endl;
            return;
        }

        std::cout << "Document ID: " << document_id << std::endl;

        for (const auto& [word, freq] : word_freq) {
            db_.save_word_frequency(document_id, word, freq, txn);
        }

        txn.commit();
    }
    catch (const pqxx::sql_error& e) {
        std::cerr << "SQL error: " << e.what() << std::endl;
        std::cerr << "Query was: " << e.query() << std::endl;
    }
    catch (const std::bad_cast& e) {
        std::cerr << "Bad cast error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}





