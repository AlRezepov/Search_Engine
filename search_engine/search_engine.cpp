#include "search_engine.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio/signal_set.hpp>
#include <pqxx/pqxx>
#include <iostream>
#include <string>
#include <cctype>
#include <algorithm>
#include <sstream>

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;


SearchEngine::SearchEngine(const Config& config)
    : ioc_(),
    acceptor_(ioc_, tcp::endpoint{ net::ip::make_address("0.0.0.0"), static_cast<unsigned short>(config.server_port) }),
    ctx_(ssl::context::tlsv12),
    conn_("host=" + config.db_host +
        " port=" + std::to_string(config.db_port) +
        " dbname=" + config.db_name +
        " user=" + config.db_user +
        " password=" + config.db_password),
    port_(std::to_string(config.server_port)),
    host_("0.0.0.0") {
    ctx_.set_default_verify_paths();
    ctx_.set_options(ssl::context::default_workarounds |
        ssl::context::no_sslv2 |
        ssl::context::no_sslv3);
}

void SearchEngine::start() {
    std::cout << "Starting server..." << std::endl;
    do_accept();
    std::cout << "Running I/O context..." << std::endl;
    ioc_.run();
}

void SearchEngine::do_accept() {
    std::cout << "Waiting for connections on port " << port_ << "..." << std::endl; // Отладочный вывод
    acceptor_.async_accept(
        net::make_strand(ioc_),
        beast::bind_front_handler(
            &SearchEngine::on_accept,
            this));
}

void SearchEngine::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        std::cerr << "Error during accept: " << ec.message() << std::endl;
        return;
    }
    std::cout << "New connection accepted." << std::endl; // Отладочный вывод
    auto session = std::make_shared<Session>(std::move(socket), *this);
    session->run();
    do_accept();
}

void SearchEngine::handle_get_request(const http::request<http::string_body>& req, std::shared_ptr<Session> session) {
    std::string html = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <title>Search Engine</title>
    </head>
    <body>
        <h1>Search</h1>
        <form action="/" method="post">
            <input type="text" name="query" />
            <button type="submit">Search</button>
        </form>
    </body>
    </html>
    )";

    http::response<http::string_body> res{ http::status::ok, req.version() };
    res.set(http::field::server, "SearchEngine");
    res.set(http::field::content_type, "text/html");
    res.content_length(html.size());
    res.body() = html;
    res.prepare_payload();

    session->send_response(res);
}

void SearchEngine::handle_post_request(const http::request<http::string_body>& req, std::shared_ptr<Session> session) {
    std::string query = req.body();

    // Преобразование в нижний регистр
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

    // Удаление "query=" в начале строки, если присутствует
    if (query.rfind("query=", 0) == 0) {
        query = query.substr(6);
    }

    // Замена всех '+' на пробелы
    std::replace(query.begin(), query.end(), '+', ' ');

    // Удаляем начальные и конечные пробелы
    boost::algorithm::trim(query);

    // Проверяем, что запрос не пустой
    if (query.empty()) {
        session->handle_error(http::status::bad_request, "Invalid or empty query.");
        return;
    }

    // Разбиваем строку на слова с использованием boost
    std::vector<std::string> words;
    boost::split(words, query, boost::is_any_of(" "), boost::token_compress_on);

    if (words.empty()) {
        session->handle_error(http::status::bad_request, "Query must contain at least one word.");
        return;
    }

    try {
        // Устанавливаем search_path на схему 'search_engine'
        pqxx::work txn_set_schema(conn_);
        txn_set_schema.exec("SET search_path TO search_engine");
        txn_set_schema.commit();

        pqxx::work txn(conn_);

        // Формируем SQL-запрос
        std::string sql = "SELECT d.url, SUM(wf.frequency) AS total_frequency "
            "FROM documents d "
            "JOIN word_frequencies wf ON d.id = wf.document_id "
            "JOIN words w ON wf.word_id = w.id "
            "WHERE LOWER(w.word) IN (";

        // Добавление слов в SQL-запрос
        for (std::size_t i = 0; i < words.size(); ++i) {
            if (i > 0) sql += ",";
            sql += txn.quote(words[i]);
        }
        sql += ") GROUP BY d.url ORDER BY total_frequency DESC;";

        std::cout << "Executing SQL query: " << sql << std::endl;

        // Выполнение SQL-запроса
        pqxx::result res = txn.exec(sql);
        txn.commit();

        std::cout << "Query executed. Number of rows returned: " << res.size() << std::endl;

        // Формирование HTML ответа
        std::string html = "<!DOCTYPE html><html><head><title>Search Results</title></head><body><h1>Search Results</h1>";

        if (res.empty()) {
            html += "<p>No results found.</p>";
        }
        else {
            html += "<table border='1'><thead><tr><th>URL</th><th>Total Frequency</th></tr></thead><tbody>";
            for (const auto& row : res) {
                std::string url = row["url"].c_str();
                std::string total_frequency = row["total_frequency"].c_str();
                html += "<tr><td><a href=\"" + url + "\">" + url + "</a></td><td>" + total_frequency + "</td></tr>";
            }
            html += "</tbody></table>";
        }

        html += "</body></html>";

        http::response<http::string_body> response{ http::status::ok, req.version() };
        response.set(http::field::server, "SearchEngine");
        response.set(http::field::content_type, "text/html");
        response.content_length(html.size());
        response.body() = html;
        response.prepare_payload();

        session->send_response(response);
    }
    catch (const std::exception& e) {
        std::cerr << "Error executing SQL query: " << e.what() << std::endl;
        session->handle_error(http::status::internal_server_error, "Database query failed");
    }
}

Session::Session(tcp::socket socket, SearchEngine& search_engine)
    : socket_(std::move(socket)), search_engine_(search_engine) {}

void Session::run() {
    do_read();
}

void Session::do_read() {
    auto self(shared_from_this());
    http::async_read(socket_, buffer_, req_,
        [self](beast::error_code ec, std::size_t) {
            if (!ec) {
                self->handle_request();
            }
        });
}

void Session::handle_request() {
    if (req_.method() == http::verb::get) {
        search_engine_.handle_get_request(req_, shared_from_this());
    }
    else if (req_.method() == http::verb::post) {
        search_engine_.handle_post_request(req_, shared_from_this());
    }
    else {
        send_bad_response(http::status::bad_request, "Invalid request-method");
    }
}

void Session::do_write() {
    auto self(shared_from_this());
    http::async_write(socket_, res_,
        [self](beast::error_code ec, std::size_t) {
            if (!ec) {
                beast::error_code ec;
                self->socket_.shutdown(tcp::socket::shutdown_send, ec);
            }
        });
}

void Session::on_write(beast::error_code ec, std::size_t) {
    if (ec) {
        std::cerr << "Error during write: " << ec.message() << std::endl;
    }
}

void Session::send_response(const http::response<http::string_body>& res) {
    res_ = res;
    do_write();
}

void Session::send_bad_response(http::status status, const std::string& message) {
    http::response<http::string_body> res{ status, req_.version() };
    res.set(http::field::server, "SearchEngine");
    res.set(http::field::content_type, "text/plain");
    res.body() = message;
    res.prepare_payload();
    send_response(res);
}

void Session::handle_error(http::status status, const std::string& message) {
    send_bad_response(status, message);
}
