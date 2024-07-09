#ifndef DATABASE_H
#define DATABASE_H

#include <pqxx/pqxx>
#include "../config/config.h"
#include <string>

class Database {
public:
    Database(const Config& config);

    void save_document(const std::string& url, const std::string& content);
    void save_word_frequency(int document_id, const std::string& word, int frequency);
    pqxx::connection& conn() { return conn_; }

private:
    pqxx::connection conn_;
};

#endif // DATABASE_H
