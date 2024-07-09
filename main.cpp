#include <iostream>
#include <pqxx/pqxx>
#include "config/config.h"
#include "database/database.h"
#include "spider/spider.h"

void create_tables(Database& db) {
    try {
        pqxx::work txn(db.conn());

        // Создание схемы
        std::string create_schema_query = "CREATE SCHEMA IF NOT EXISTS search_engine;";
        txn.exec(create_schema_query);

        // Создание таблиц
        std::string query1 = R"(
            CREATE TABLE IF NOT EXISTS search_engine.documents (
                id SERIAL PRIMARY KEY,
                url TEXT UNIQUE,
                content TEXT
            );
        )";

        std::string query2 = R"(
            CREATE TABLE IF NOT EXISTS search_engine.words (
                id SERIAL PRIMARY KEY,
                word TEXT UNIQUE
            );
        )";

        std::string query3 = R"(
            CREATE TABLE IF NOT EXISTS search_engine.word_frequencies (
                document_id INT REFERENCES search_engine.documents(id),
                word_id INT REFERENCES search_engine.words(id),
                frequency INT,
                PRIMARY KEY (document_id, word_id)
            );
        )";

        std::cout << "Executing query1: " << query1 << std::endl;
        txn.exec(query1);

        std::cout << "Executing query2: " << query2 << std::endl;
        txn.exec(query2);

        std::cout << "Executing query3: " << query3 << std::endl;
        txn.exec(query3);

        txn.commit();
        std::cout << "Tables created successfully." << std::endl;
    }
    catch (const pqxx::sql_error& e) {
        std::cerr << "SQL error: " << e.what() << std::endl;
        std::cerr << "Query was: " << e.query() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    try {
        std::string config_path = "C:/Users/alexr/Desktop/Search_Engine/config/config.ini";
        Config config = read_config(config_path);

        Database db(config);
        create_tables(db);

        Spider spider(config, db);
        spider.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
