#ifndef SPIDER_H
#define SPIDER_H

#include <string>
#include <set>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include "../config/config.h"
#include "../database/database.h"

class Spider {
public:
    Spider(const Config& config, Database& db);
    ~Spider(); // Добавлен деструктор для корректного завершения потоков
    void start();

private:
    std::string fetch_page(const std::string& url);
    std::vector<std::string> extract_links(const std::string& content);
    void index_page(const std::string& url, const std::string& content);
    void worker_thread(); // Функция для работы потоков
    std::string ensure_scheme(const std::string& url);

    Config config_;
    Database& db_;
    std::set<std::string> visited_;
    std::queue<std::pair<std::string, int>> urls_;
    std::mutex queue_mutex_;
    std::mutex visited_mutex_;
    std::condition_variable cv_;
    bool stop_ = false;

    boost::asio::io_context ioc_;
    boost::asio::ssl::context ssl_ctx_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    std::vector<std::thread> thread_pool_; // Пул потоков
    std::atomic<int> active_threads_{ 0 };

};

#endif // SPIDER_H
