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

        Database db(config);
        std::cout << "Database initialized." << std::endl;

        // Создаём таблицы через метод класса Database
        db.create_tables();

        std::cout << "Starting spider..." << std::endl;
        Spider spider(config, db);

        spider.start();
        std::cout << "Spider finished." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in main: " << e.what() << std::endl;
    }

    return 0;
}
