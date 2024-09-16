cmake_minimum_required(VERSION 3.20)
project(SearchEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D_WIN32_WINNT=0x0601)

# Путь к vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "C:/Users/alexr/Desktop/Search_Engine/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Найти пакеты Boost, libpqxx и OpenSSL
find_package(Boost REQUIRED COMPONENTS system filesystem regex locale)
find_package(libpqxx REQUIRED)
find_package(OpenSSL REQUIRED)

# Установка путей к заголовочным файлам и библиотекам Boost
include_directories(${Boost_INCLUDE_DIRS})

# Источники для Spider
set(SPIDER_SOURCES
    main_spider.cpp
    config/config.cpp
    database/database.cpp
    spider/spider.cpp
)

# Источники для Search Engine
set(SEARCH_ENGINE_SOURCES
    main_search_engine.cpp
    config/config.cpp
    database/database.cpp
    search_engine/search_engine.cpp
)

# Создание исполняемого файла для Spider
add_executable(SpiderProgram ${SPIDER_SOURCES})

# Линковка с библиотеками для Spider
target_link_libraries(SpiderProgram PRIVATE
    Boost::system
    Boost::filesystem
    Boost::regex
    Boost::locale
    libpqxx::pqxx
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Создание исполняемого файла для Search Engine
add_executable(SearchEngineProgram ${SEARCH_ENGINE_SOURCES})

# Линковка с библиотеками для Search Engine
target_link_libraries(SearchEngineProgram PRIVATE
    Boost::system
    Boost::filesystem
    Boost::regex
    Boost::locale
    libpqxx::pqxx
    OpenSSL::SSL
    OpenSSL::Crypto
)
