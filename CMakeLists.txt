cmake_minimum_required(VERSION 3.10)

project(SearchEngine)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Определение версии Windows
add_definitions(-D_WIN32_WINNT=0x0601)

# Найти пакеты Boost и PostgreSQL
find_package(Boost REQUIRED COMPONENTS system filesystem regex locale)
find_package(PostgreSQL REQUIRED)

# Установка путей к заголовочным файлам и библиотекам Boost
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

# Установка путей к заголовочным файлам и библиотекам PostgreSQL
include_directories(${PostgreSQL_INCLUDE_DIRS})
link_directories(${PostgreSQL_LIBRARY_DIRS})

# Установка путей к исходным файлам и заголовочным файлам
set(SOURCES
    main.cpp
    config/config.h
    database/database.h
    database/database.cpp
    spider/spider.h
    spider/spider.cpp
)

# Создание исполняемого файла
add_executable(SearchEngine ${SOURCES} "config/config.cpp")

# Линковка с библиотеками Boost и PostgreSQL
target_link_libraries(SearchEngine 
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_REGEX_LIBRARY}
    ${Boost_LOCALE_LIBRARY}
    ${PostgreSQL_LIBRARIES}
    pqxx
)
