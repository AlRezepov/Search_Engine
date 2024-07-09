#include "spider.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/locale.hpp>
#include <regex>
#include <iostream>

namespace http = boost::beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

Spider::Spider(const Config& config, Database& db) : config_(config), db_(db) {}

void Spider::start() {
    std::queue<std::pair<std::string, int>> urls;
    urls.push({ config_.start_url, 0 });

    while (!urls.empty()) {
        auto [url, depth] = urls.front();
        urls.pop();

        if (depth > config_.recursion_depth || visited_.find(url) != visited_.end()) {
            continue;
        }

        visited_.insert(url);

        std::string content = fetch_page(url);
        if (!content.empty()) {
            index_page(url, content);

            std::vector<std::string> links = extract_links(content);
            for (const auto& link : links) {
                urls.push({ link, depth + 1 });
            }
        }
    }
}

std::string Spider::fetch_page(const std::string& url) {
    try {
        net::io_context ioc;
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(url, "80");
        tcp::socket socket(ioc);
        net::connect(socket, results.begin(), results.end());

        http::request<http::string_body> req{ http::verb::get, url, 11 };
        req.set(http::field::host, url);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        http::write(socket, req);

        boost::beast::flat_buffer buffer;
        http::response<http::dynamic_body> res;
        http::read(socket, buffer, res);

        return boost::beast::buffers_to_string(res.body().data());
    }
    catch (std::exception& e) {
        std::cerr << "Error fetching page: " << e.what() << std::endl;
        return "";
    }
}

std::vector<std::string> Spider::extract_links(const std::string& content) {
    std::vector<std::string> links;
    std::regex link_regex(R"(<a\s+href=["']([^"']+)["'])");
    std::smatch match;

    std::string::const_iterator search_start(content.cbegin());
    while (std::regex_search(search_start, content.cend(), match, link_regex)) {
        links.push_back(match[1]);
        search_start = match.suffix().first;
    }

    return links;
}

void Spider::index_page(const std::string& url, const std::string& content) {
    try {
        std::string text = boost::locale::to_lower(content);
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

        db_.save_document(url, content);

        {
            pqxx::work txn(db_.conn());
            pqxx::result r = txn.exec_params("SELECT id FROM search_engine.documents WHERE url = $1", url);
            if (r.empty()) {
                std::cerr << "No document found with URL: " << url << std::endl;
                return;
            }
            int document_id = r[0][0].as<int>();

            for (const auto& [word, freq] : word_freq) {
                db_.save_word_frequency(document_id, word, freq);
            }

            txn.commit();
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

