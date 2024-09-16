#include <iostream>
#include <pqxx/pqxx>
#include "config/config.h"
#include "database/database.h"
#include "spider/spider.h"
#include "search_engine/search_engine.h"

void create_tables(Database& db) {
    try {
        // Устанавливаем локаль сообщений на английский
        pqxx::work txn0(db.conn());
        txn0.exec("SET lc_messages TO 'en_US.UTF-8'");
        txn0.commit();

        // Создание схемы
        {
            pqxx::work txn(db.conn());
            std::string create_schema_query = "CREATE SCHEMA IF NOT EXISTS search_engine;";
            std::cout << "Creating schema: " << create_schema_query << std::endl;
            txn.exec(create_schema_query);
            std::cout << "Schema created." << std::endl;
            txn.commit();
        }

        // Устанавливаем search_path на search_engine
        {
            pqxx::work txn_set_schema(db.conn());
            txn_set_schema.exec("SET search_path TO search_engine");
            txn_set_schema.commit();
        }

        // Создание таблиц в схеме search_engine
        {
            pqxx::work txn(db.conn());
            std::string query1 = "CREATE TABLE IF NOT EXISTS documents (id SERIAL PRIMARY KEY, url TEXT UNIQUE, content TEXT);";
            std::cout << "Executing query1: " << query1 << std::endl;
            txn.exec(query1);
            std::cout << "Table documents created." << std::endl;
            txn.commit();
        }

        {
            pqxx::work txn(db.conn());
            std::string query2 = "CREATE TABLE IF NOT EXISTS words (id SERIAL PRIMARY KEY, word TEXT UNIQUE);";
            std::cout << "Executing query2: " << query2 << std::endl;
            txn.exec(query2);
            std::cout << "Table words created." << std::endl;
            txn.commit();
        }

        {
            pqxx::work txn(db.conn());
            std::string query3 = "CREATE TABLE IF NOT EXISTS word_frequencies (document_id INT REFERENCES documents(id), word_id INT REFERENCES words(id), frequency INT, PRIMARY KEY (document_id, word_id));";
            std::cout << "Executing query3: " << query3 << std::endl;
            txn.exec(query3);
            std::cout << "Table word_frequencies created." << std::endl;
            txn.commit();
        }

        std::cout << "Tables created successfully." << std::endl;
    }
    catch (const pqxx::sql_error& e) {
        std::cerr << "SQL error: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    try {
        std::string config_path = "C:/Users/alexr/Desktop/Search_Engine/config/config.ini";
        std::cout << "Reading config from: " << config_path << std::endl;
        Config config = read_config(config_path);

        std::cout << "Config read successfully." << std::endl;

        Database db(config);
        std::cout << "Database initialized." << std::endl;

        // Устанавливаем search_path на схему 'search_engine'
        {
            pqxx::work txn_set_schema(db.conn());
            txn_set_schema.exec("SET search_path TO search_engine");
            txn_set_schema.commit();
        }

        create_tables(db);

        std::cout << "Starting spider..." << std::endl;
        Spider spider(config, db);

        spider.start();
        std::cout << "Spider finished." << std::endl;

        std::cout << "Starting search engine server..." << std::endl;
        SearchEngine search_engine(config);
        search_engine.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
