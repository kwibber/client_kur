cmake_minimum_required(VERSION 3.10)

project(Kursovaya)

find_package(open62541 CONFIG REQUIRED)

add_executable(Kursovaya client.cpp)

target_link_libraries(Kursovaya PRIVATE open62541::open62541)