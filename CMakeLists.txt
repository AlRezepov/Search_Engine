cmake_minimum_required(VERSION 3.20)

project(SearchEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Укажите путь к vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "C:/Users/alexr/Desktop/Search_Engine/vcpkg/scripts/buildsystems/vcpkg.cmake")

# Найти пакеты Boost, libpqxx и OpenSSL
find_package(Boost REQUIRED COMPONENTS system filesystem regex locale)
find_package(libpqxx CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# Установка путей к заголовочным файлам и библиотекам Boost
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

# Установка путей к исходным файлам и заголовочным файлам
set(SOURCES
    main.cpp
    config/config.h
    config/config.cpp
    database/database.h
    database/database.cpp
    spider/spider.h
    spider/spider.cpp
)

# Создание исполняемого файла
add_executable(SearchEngine ${SOURCES})

# Линковка с библиотеками Boost, libpqxx и OpenSSL
target_link_libraries(SearchEngine PRIVATE
    Boost::system
    Boost::filesystem
    Boost::regex
    Boost::locale
    libpqxx::pqxx
    OpenSSL::SSL
    OpenSSL::Crypto
)
