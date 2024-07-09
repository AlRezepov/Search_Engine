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
    void start();

private:
    std::string fetch_page(const std::string& url);
    std::vector<std::string> extract_links(const std::string& content);
    void index_page(const std::string& url, const std::string& content);

    Config config_;
    Database& db_;
    std::set<std::string> visited_;
};

#endif // SPIDER_H
