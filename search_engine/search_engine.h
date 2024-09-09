#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <pqxx/pqxx>
#include "../config/config.h"
#include <memory>

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

class SearchEngine;

// Класс Session для обработки HTTP-сессий
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, SearchEngine& search_engine);

    void run();
    void send_response(const http::response<http::string_body>& res);
    void send_bad_response(http::status status, const std::string& message);
    void handle_error(http::status status, const std::string& message);

private:
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void handle_request();
    void do_write();
    void on_write(beast::error_code ec, std::size_t bytes_transferred);

    tcp::socket socket_;
    SearchEngine& search_engine_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_;
};

class SearchEngine {
public:
    SearchEngine(const Config& config);

    void start();
    void handle_get_request(const http::request<http::string_body>& req, std::shared_ptr<Session> session);
    void handle_post_request(const http::request<http::string_body>& req, std::shared_ptr<Session> session);

private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);

    net::io_context ioc_;
    tcp::acceptor acceptor_;
    ssl::context ctx_;
    pqxx::connection conn_;
    std::string host_;
    std::string port_;
};
