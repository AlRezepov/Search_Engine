#ifndef SPIDER_H
#define SPIDER_H

#include <string>
#include <set>
#include <queue>
#include <vector>
#include "../config/config.h"
#include "../database/database.h"

class Spider {
public:
    Spider(const Config& config, Database& db);

private:
    Config config_;
    Database& db_;
    std::set<std::string> visited_;
};

#endif // SPIDER_H
