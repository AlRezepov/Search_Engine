#include "database.h"

Database::Database(const Config& config)
    : conn_("host=" + config.db_host +
        " port=" + std::to_string(config.db_port) +
        " dbname=" + config.db_name +
        " user=" + config.db_user +
        " password=" + config.db_password) {
}

void Database::save_document(const std::string& url, const std::string& content) {
    pqxx::work txn(conn_);
    txn.exec_params("INSERT INTO search_engine.documents (url, content) VALUES ($1, $2) ON CONFLICT (url) DO NOTHING", url, content);
    txn.commit();
}

void Database::save_word_frequency(int document_id, const std::string& word, int frequency) {
    pqxx::work txn(conn_);
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
    txn.commit();
}
