#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/locale.hpp>
#include <pqxx/pqxx>
#include <thread>
#include <queue>
#include <mutex>
#include <set>
#include <regex>
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

using namespace std;
namespace http = boost::beast::http;
namespace asio = boost::asio;

struct Config {
    string db_host;
    int db_port;
    string db_name;
    string db_user;
    string db_password;
    string start_url;
    int recursion_depth;
    int server_port;
};

Config read_config(const string& filename) {
    Config config;
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(filename, pt);

    config.db_host = pt.get<string>("database.host");
    config.db_port = pt.get<int>("database.port");
    config.db_name = pt.get<string>("database.dbname");
    config.db_user = pt.get<string>("database.user");
    config.db_password = pt.get<string>("database.password");
    config.start_url = pt.get<string>("spider.start_url");
    config.recursion_depth = pt.get<int>("spider.recursion_depth");
    config.server_port = pt.get<int>("search_server.port");

    return config;
}

class Spider {
public:
    Spider(const Config& config)
        : config_(config), db_conn_str_("dbname=" + config.db_name + " user=" + config.db_user + " password=" + config.db_password) {
        db_conn_ = make_shared<pqxx::connection>(db_conn_str_);
    }

    void start() {
        queue<pair<string, int>> urls;
        urls.push({ config_.start_url, 0 });
        set<string> visited;
        visited.insert(config_.start_url);

        while (!urls.empty()) {
            auto [url, depth] = urls.front();
            urls.pop();

            if (depth > config_.recursion_depth) continue;

            // Fetch and parse the page
            string content = fetch_page(url);
            vector<string> links = extract_links(content);
            index_page(url, content);

            // Add new links to the queue
            for (const auto& link : links) {
                if (visited.find(link) == visited.end()) {
                    urls.push({ link, depth + 1 });
                    visited.insert(link);
                }
            }
        }
    }

private:
    Config config_;
    string db_conn_str_;
    shared_ptr<pqxx::connection> db_conn_;

    string fetch_page(const string& url) {
        // Implement HTTP client to fetch page content using Boost Beast
        // ...
        return "<html>...</html>"; // placeholder
    }

    vector<string> extract_links(const string& content) {
        vector<string> links;
        regex link_regex("<a href=\"(.*?)\"");
        smatch match;
        string::const_iterator search_start(content.cbegin());

        while (regex_search(search_start, content.cend(), match, link_regex)) {
            links.push_back(match[1]);
            search_start = match.suffix().first;
        }

        return links;
    }

    void index_page(const string& url, const string& content) {
        string text = clean_html(content);
        map<string, int> word_freq = analyze_text(text);

        pqxx::work txn(*db_conn_);
        // Insert document and word frequency into the database
        // ...
        txn.commit();
    }

    string clean_html(const string& html) {
        // Remove HTML tags, punctuation, and convert to lowercase
        string text = boost::locale::to_lower(html);
        text = regex_replace(text, regex("<[^>]*>"), " ");
        text = regex_replace(text, regex("[^\\w\\s]"), " ");
        return text;
    }

    map<string, int> analyze_text(const string& text) {
        map<string, int> word_freq;
        stringstream ss(text);
        string word;

        while (ss >> word) {
            if (word.length() >= 3 && word.length() <= 32) {
                word_freq[word]++;
            }
        }

        return word_freq;
    }
};

int main() {
    Config config = read_config("../config/config.ini");

    Spider spider(config);
    spider.start();

    return 0;
}
