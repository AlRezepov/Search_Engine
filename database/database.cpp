#include "database.h"
#include <iostream>

Database::Database(const Config& config)
    : conn_("host=" + config.db_host +
        " port=" + std::to_string(config.db_port) +
        " dbname=" + config.db_name +
        " user=" + config.db_user +
        " password=" + config.db_password) {
}

void Database::save_document(const std::string& url, const std::string& content, pqxx::work& txn) {
    txn.exec_params("INSERT INTO search_engine.documents (url, content) VALUES ($1, $2) ON CONFLICT (url) DO NOTHING", url, content);
}

void Database::save_word_frequency(int document_id, const std::string& word, int frequency, pqxx::work& txn) {
    pqxx::result r = txn.exec_params("SELECT id FROM search_engine.words WHERE word = $1", word);
    int word_id;
    if (r.empty()) {
        r = txn.exec_params("INSERT INTO search_engine.words (word) VALUES ($1) RETURNING id", word);
        word_id = r[0][0].as<int>();
    }
    else {
        word_id = r[0][0].as<int>();
    }
    txn.exec_params("INSERT INTO search_engine.word_frequencies (document_id, word_id, frequency) VALUES ($1, $2, $3) ON CONFLICT (document_id, word_id) DO NOTHING", document_id, word_id, frequency);
}

pqxx::connection& Database::conn() {
    return conn_;
}

// Реализация метода create_tables
void Database::create_tables() {
    try {
        // Создание схемы
        {
            pqxx::work txn(conn_);
            std::string create_schema_query = "CREATE SCHEMA IF NOT EXISTS search_engine;";
            std::cout << "Creating schema: " << create_schema_query << std::endl;
            txn.exec(create_schema_query);
            std::cout << "Schema created." << std::endl;
            txn.commit();
        }

        // Устанавливаем search_path на search_engine
        {
            pqxx::work txn_set_schema(conn_);
            txn_set_schema.exec("SET search_path TO search_engine");
            txn_set_schema.commit();
        }

        // Создание таблиц в схеме search_engine
        {
            pqxx::work txn(conn_);
            std::string query1 = "CREATE TABLE IF NOT EXISTS documents (id SERIAL PRIMARY KEY, url TEXT UNIQUE, content TEXT);";
            std::cout << "Executing query1: " << query1 << std::endl;
            txn.exec(query1);
            std::cout << "Table documents created." << std::endl;
            txn.commit();
        }

        {
            pqxx::work txn(conn_);
            std::string query2 = "CREATE TABLE IF NOT EXISTS words (id SERIAL PRIMARY KEY, word TEXT UNIQUE);";
            std::cout << "Executing query2: " << query2 << std::endl;
            txn.exec(query2);
            std::cout << "Table words created." << std::endl;
            txn.commit();
        }

        {
            pqxx::work txn(conn_);
            std::string query3 = "CREATE TABLE IF NOT EXISTS word_frequencies (document_id INT REFERENCES documents(id), word_id INT REFERENCES words(id), frequency INT, PRIMARY KEY (document_id, word_id));";
            std::cout << "Executing query3: " << query3 << std::endl;
            txn.exec(query3);
            std::cout << "Table word_frequencies created." << std::endl;
            txn.commit();
        }

        std::cout << "Tables created successfully." << std::endl;
    }
    catch (const pqxx::sql_error& e) {
        std::cerr << "SQL error in create_tables: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in create_tables: " << e.what() << std::endl;
    }
}
