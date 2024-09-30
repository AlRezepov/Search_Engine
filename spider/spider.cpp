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
#include <execution>

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

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        // Здесь можно добавить условие ожидания завершения работы
    }

    stop_ = true;
    cv_.notify_all();

    for (auto& thread : thread_pool_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::cout << "Spider finished." << std::endl;
}

void Spider::worker_thread() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        active_threads_++;
    }

    while (true) {
        std::pair<std::string, int> task;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this]() { return !urls_.empty() || stop_; });

            if (stop_ && urls_.empty()) {
                active_threads_--;
                if (active_threads_ == 0) {
                    cv_.notify_all();
                }
                std::cout << "Worker thread exiting. Stop flag set and no more URLs." << std::endl;
                return;
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

        std::string content = fetch_page(ensure_scheme(url));
        if (content.empty()) {
            std::cerr << "Fetched content is empty for URL: " << url << std::endl;
            continue;
        }

        std::cout << "Processing content for URL: " << url << std::endl;
        // Закомментируем вывод большого объёма данных
        // std::cout << "Content snippet: " << content.substr(0, 1000) << "..." << std::endl;

        if (depth < config_.recursion_depth) {
            std::vector<std::string> links;
            try {
                // Передаем текущий URL как базовый
                links = extract_links(content, url);
            }
            catch (const std::exception& e) {
                std::cerr << "Exception while extracting links: " << e.what() << std::endl;
                continue;
            }

            for (const std::string& link : links) {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                {
                    std::lock_guard<std::mutex> visited_lock(visited_mutex_);
                    if (visited_.find(link) != visited_.end()) {
                        continue;
                    }
                    visited_.insert(link);
                }
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
        ssl::context ctx(ssl::context::tlsv12_client);  // Используем TLS
        ctx.set_default_verify_paths();

        auto const scheme_end = url.find("://");
        if (scheme_end == std::string::npos) {
            throw std::invalid_argument("Invalid URL: missing scheme");
        }

        auto const scheme = url.substr(0, scheme_end);
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
        auto const results = resolver.resolve(host, scheme);

        if (scheme == "https") {
            ssl::stream<tcp::socket> stream(ioc, ctx);
            if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
                boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), net::error::get_ssl_category() };
                throw boost::system::system_error{ ec };
            }

            // Устанавливаем соединение
            net::connect(stream.next_layer(), results.begin(), results.end());
            stream.handshake(ssl::stream_base::client);

            http::request<http::string_body> req{ http::verb::get, target, 11 };
            req.set(http::field::host, host);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            http::write(stream, req);

            boost::beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;

            http::read(stream, buffer, res);

            if (res.result() == http::status::moved_permanently || res.result() == http::status::found) {
                auto location = res[http::field::location];
                if (!location.empty()) {
                    return fetch_page(std::string(location));
                }
                else {
                    std::cerr << "Redirected without a new location" << std::endl;
                    return "";
                }
            }

            if (res.result() != http::status::ok) {
                std::cerr << "HTTP request failed: " << res.result_int() << " " << res.reason() << std::endl;
                return "";
            }

            std::string page_content = boost::beast::buffers_to_string(res.body().data());
            return page_content;
        }
        else if (scheme == "http") {
            tcp::socket socket(ioc);

            // Устанавливаем соединение
            net::connect(socket, results.begin(), results.end());

            http::request<http::string_body> req{ http::verb::get, target, 11 };
            req.set(http::field::host, host);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            http::write(socket, req);

            boost::beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;

            http::read(socket, buffer, res);

            if (res.result() == http::status::moved_permanently || res.result() == http::status::found) {
                auto location = res[http::field::location];
                if (!location.empty()) {
                    return fetch_page(std::string(location));
                }
                else {
                    std::cerr << "Redirected without a new location" << std::endl;
                    return "";
                }
            }

            if (res.result() != http::status::ok) {
                std::cerr << "HTTP request failed: " << res.result_int() << " " << res.reason() << std::endl;
                return "";
            }

            std::string page_content = boost::beast::buffers_to_string(res.body().data());
            return page_content;
        }
        else {
            std::cerr << "Unsupported URL scheme: " << scheme << std::endl;
            return "";
        }
    }
    catch (std::exception& e) {
        std::cerr << "Error fetching page: " << e.what() << std::endl;
        return "";
    }
}


std::vector<std::string> Spider::extract_links(const std::string& content, const std::string& base_url) {
    std::vector<std::string> links;
    try {
        std::regex link_regex(R"(<a\s+(?:[^>]*?\s+)?href=["']([^"']*)["'])");
        std::smatch match;

        std::string::const_iterator search_start(content.cbegin());
        while (std::regex_search(search_start, content.cend(), match, link_regex)) {
            std::string link = match[1].str();
            if (link.empty() || link.find("javascript:") == 0) {
                // std::cerr << "Skipping invalid link: " << link << std::endl;
                search_start = match.suffix().first;
                continue;
            }
            std::string absolute_link = resolve_url(base_url, link);
            // std::cout << "Found link: " << link << " Resolved to: " << absolute_link << std::endl;
            links.push_back(absolute_link);
            search_start = match.suffix().first;
        }

        // std::cout << "Total links found: " << links.size() << std::endl;
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

        // Initialize locale
        boost::locale::generator gen;
        std::locale loc = gen("");
        std::locale::global(loc);
        std::cout.imbue(loc);

        // Convert string to UTF-8
        std::string utf8_content = boost::locale::conv::to_utf<char>(content, "UTF-8");
        std::string text = boost::locale::to_lower(utf8_content);

        // Remove HTML tags and special characters
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

        try {
            db_.save_document(url, content, txn);

            pqxx::result r = txn.exec_params("SELECT id FROM search_engine.documents WHERE url = $1", url);
            if (r.empty()) {
                std::cerr << "Document not found for URL: " << url << std::endl;
                txn.abort();
                return;
            }

            int document_id = r[0][0].as<int>();

            for (const auto& [word, freq] : word_freq) {
                db_.save_word_frequency(document_id, word, freq, txn);
            }

            txn.commit();
            // std::cout << "Transaction committed for URL: " << url << std::endl;
        }
        catch (const std::exception& e) {
            txn.abort();
            std::cerr << "Transaction error: " << e.what() << std::endl;
        }
    }
    catch (const pqxx::sql_error& e) {
        std::cerr << "SQL error: " << e.what() << std::endl;
        std::cerr << "Query was: " << e.query() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

std::string Spider::ensure_scheme(const std::string& url) {
    if (url.find("http://") == 0 || url.find("https://") == 0) {
        return url;
    }
    return "http://" + url;
}

// Реализация функции parse_url
Spider::UrlComponents Spider::parse_url(const std::string& url) {
    std::regex url_regex(R"(^((https?)://)?([^/]+)(/.*)?)");
    std::smatch url_match_result;

    if (std::regex_match(url, url_match_result, url_regex)) {
        UrlComponents result;
        result.scheme = url_match_result[2].str().empty() ? "http" : url_match_result[2].str();
        result.host = url_match_result[3].str();
        result.path = url_match_result[4].str();
        if (result.path.empty()) {
            result.path = "/";
        }
        return result;
    }
    else {
        throw std::invalid_argument("Invalid URL: " + url);
    }
}

// Реализация функции resolve_url
std::string Spider::resolve_url(const std::string& base_url, const std::string& link) {
    // Если ссылка уже является абсолютной, возвращаем её
    if (link.find("http://") == 0 || link.find("https://") == 0) {
        return link;
    }
    else if (link.find("//") == 0) {
        // Протокольно-относительный URL
        UrlComponents base = parse_url(base_url);
        return base.scheme + ":" + link;
    }
    else if (link.find("/") == 0) {
        // Абсолютный путь относительно хоста
        UrlComponents base = parse_url(base_url);
        return base.scheme + "://" + base.host + link;
    }
    else {
        // Относительный путь
        UrlComponents base = parse_url(base_url);
        // Удаляем имя файла из базового пути, если оно есть
        std::string base_path = base.path;
        size_t pos = base_path.rfind('/');
        if (pos != std::string::npos) {
            base_path = base_path.substr(0, pos + 1);
        }
        else {
            base_path = "/";
        }
        std::string combined_path = base_path + link;

        // Нормализуем путь, обрабатывая "../" и "./"
        std::vector<std::string> segments;
        std::istringstream path_stream(combined_path);
        std::string segment;
        while (std::getline(path_stream, segment, '/')) {
            if (segment == "..") {
                if (!segments.empty()) {
                    segments.pop_back();
                }
            }
            else if (segment != "." && !segment.empty()) {
                segments.push_back(segment);
            }
        }

        // Восстанавливаем нормализованный путь
        std::string normalized_path = "/";
        for (size_t i = 0; i < segments.size(); ++i) {
            normalized_path += segments[i];
            if (i != segments.size() - 1) {
                normalized_path += "/";
            }
        }
        if (combined_path.back() == '/') {
            normalized_path += "/";
        }

        return base.scheme + "://" + base.host + normalized_path;
    }
}
