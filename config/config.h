#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct Config {
    std::string db_host;
    int db_port;
    std::string db_name;
    std::string db_user;
    std::string db_password;
    std::string start_url;
    int recursion_depth;
    int server_port;
};

Config read_config(const std::string& filename);

#endif // CONFIG_H
