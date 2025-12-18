cmake_minimum_required(VERSION 3.20)
project(Kursovaya)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MINGW)
    set(MSYS2_PREFIX "C:/msys64/ucrt64")
    set(CMAKE_PREFIX_PATH ${MSYS2_PREFIX})
endif()

find_package(open62541 REQUIRED)
find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

set(SOURCES
    main_gui.cpp
    simple_window.cpp
    opcua_client.cpp
    device_managers.cpp
    async_manager.cpp
)

add_executable(Kursovaya ${SOURCES})

target_include_directories(Kursovaya PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Kursovaya
    PRIVATE
        open62541::open62541
        SFML::Graphics
        SFML::Window
        SFML::System
)

if(WIN32)
     set_target_properties(Kursovaya PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
