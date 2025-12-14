cmake_minimum_required(VERSION 3.10)

project(Kursovaya)

# Устанавливаем стандарт C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Определяем, используем ли мы MinGW
if(MINGW)
    set(COMPILER_MINGW TRUE)
elseif(MSVC)
    set(COMPILER_MSVC TRUE)
endif()

find_package(open62541 CONFIG REQUIRED)

# Включение оптимизации скорости
if(MSVC)
    # Опции для MSVC
    add_compile_options(/O2 /Oi /Ot /fp:fast /MP)
    add_link_options(/LTCG)
else()
    # Опции для GCC/MinGW
    add_compile_options(-O3 -march=native -mtune=native -ffast-math -flto)
    add_link_options(-flto)
endif()

# Список всех исходных файлов
set(SOURCES
    main.cpp
    opcua_client.cpp
    device_managers.cpp
    console_manager.cpp
    async_manager.cpp
)

# Создание исполняемого файла
add_executable(Kursovaya ${SOURCES})

# Подключение библиотек
target_link_libraries(Kursovaya PRIVATE open62541::open62541)

# Для Windows (и MinGW и MSVC)
if(WIN32)
    target_link_libraries(Kursovaya PRIVATE ws2_32)
    
    # Для MinGW нужны дополнительные библиотеки
    if(MINGW)
        target_link_libraries(Kursovaya PRIVATE kernel32 user32)
    endif()
    
    # Флаги компиляции
    target_compile_definitions(Kursovaya PRIVATE 
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# Включение директории с заголовками
target_include_directories(Kursovaya PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)