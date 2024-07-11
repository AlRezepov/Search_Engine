#ifndef DATABASE_H
#define DATABASE_H

#include <pqxx/pqxx>
#include "../config/config.h"
#include <string>

class Database {
public:
    Database(const Config& config);

    pqxx::connection& conn() { return conn_; }

private:
    pqxx::connection conn_;
};

#endif // DATABASE_H
