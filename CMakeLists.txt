cmake_minimum_required(VERSION 3.20)
project(Kursovaya LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

set(UA_ENABLE_AMALGAMATION ON CACHE BOOL "" FORCE)
set(UA_ENABLE_SUBSCRIPTIONS ON CACHE BOOL "" FORCE)
set(UA_ENABLE_METHODCALLS ON CACHE BOOL "" FORCE)
set(UA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(UA_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    open62541
    GIT_REPOSITORY https://github.com/open62541/open62541.git
    GIT_TAG v1.3.8
)

set(UA_ENABLE_AMALGAMATION OFF CACHE BOOL "" FORCE)
set(UA_ENABLE_ENCRYPTION OFF CACHE BOOL "" FORCE)
set(UA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(UA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(open62541)

FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG master
)

FetchContent_MakeAvailable(SFML)

set(SOURCES
    main_gui.cpp
    simple_window.cpp
    opcua_client.cpp
    device_managers.cpp
    async_manager.cpp
)

add_executable(Kursovaya ${SOURCES})

add_custom_command(
    TARGET Kursovaya POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/res
            $<TARGET_FILE_DIR:Kursovaya>/res
)

target_include_directories(Kursovaya
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Kursovaya
    PRIVATE
        open62541
        SFML::Graphics
        SFML::Window
        SFML::System
)

if(WIN32)
    set_target_properties(Kursovaya PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
