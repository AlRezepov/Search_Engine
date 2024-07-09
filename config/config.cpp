#include "config.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

Config read_config(const std::string& filename) {
    Config config;
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(filename, pt);

    config.db_host = pt.get<std::string>("database.host");
    config.db_port = pt.get<int>("database.port");
    config.db_name = pt.get<std::string>("database.dbname");
    config.db_user = pt.get<std::string>("database.user");
    config.db_password = pt.get<std::string>("database.password");
    config.start_url = pt.get<std::string>("spider.start_url");
    config.recursion_depth = pt.get<int>("spider.recursion_depth");
    config.server_port = pt.get<int>("search_server.port");

    return config;
}
