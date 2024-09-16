#include <iostream>
#include <pqxx/pqxx>
#include "config/config.h"
#include "database/database.h"
#include "spider/spider.h"
#include "search_engine/search_engine.h"

int main() {
    try {
        std::string config_path = "C:/Users/alexr/Desktop/Search_Engine/config/config.ini";
        std::cout << "Reading config from: " << config_path << std::endl;
        Config config = read_config(config_path);

        std::cout << "Config read successfully." << std::endl;

        std::cout << "Starting search engine server..." << std::endl;
        SearchEngine search_engine(config);
        search_engine.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
