#include "database.h"

Database::Database(const Config& config)
    : conn_("host=" + config.db_host +
        " port=" + std::to_string(config.db_port) +
        " dbname=" + config.db_name +
        " user=" + config.db_user +
        " password=" + config.db_password) {
}
