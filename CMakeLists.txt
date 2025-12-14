cmake_minimum_required(VERSION 3.10)

project(Kursovaya)

set(CMAKE_CXX_STANDARD 17)

find_package(open62541 CONFIG REQUIRED)

# Список всех исходных файлов
set(SOURCES
    main.cpp
    opcua_client.cpp
    device_managers.cpp
    console_manager.cpp
)

# Создание исполняемого файла
add_executable(Kursovaya ${SOURCES})

# Подключение библиотек
target_link_libraries(Kursovaya PRIVATE open62541::open62541)

# Для Windows добавляем необходимые библиотеки
if(WIN32)
    target_link_libraries(Kursovaya PRIVATE ws2_32)
    # Добавляем флаги компиляции для Windows
    target_compile_definitions(Kursovaya PRIVATE 
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )
    
    # Для MSVC настройки компиляции
    if(MSVC)
        target_compile_options(Kursovaya PRIVATE 
            /W4           # Уровень предупреждений
            /EHsc         # Модель исключений
        )
    endif()
endif()