#ifndef DATABASE_H
#define DATABASE_H

#include <pqxx/pqxx>
#include "../config/config.h"

class Database {
public:
    Database(const Config& config);

    void save_document(const std::string& url, const std::string& content, pqxx::work& txn);
    void save_word_frequency(int document_id, const std::string& word, int frequency, pqxx::work& txn);

    pqxx::connection& conn() { return conn_; }

private:
    pqxx::connection conn_;
};

#endif // DATABASE_H
