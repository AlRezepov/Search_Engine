#include "spider.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/locale.hpp>
#include <regex>
#include <iostream>
#include <string>

namespace http = boost::beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

Spider::Spider(const Config& config, Database& db) : config_(config), db_(db) {
    std::cout << "Spider initialized." << std::endl;
}
