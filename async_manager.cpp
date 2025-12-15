#include "async_manager.h"
#include <iostream>

AsyncDataManager::AsyncDataManager(OPCUAClient* client, 
                                  MultimeterDevice* multimeter,
                                  MachineDevice* machine,
                                  ComputerDevice* computer,
                                  int updateIntervalMs)
    : client(client)
    , multimeter(multimeter)
    , machine(machine)
    , computer(computer)
    , updateIntervalMs(updateIntervalMs) {
    currentData.lastUpdate = std::chrono::system_clock::now();
}

AsyncDataManager::~AsyncDataManager() {
    stop();
}

void AsyncDataManager::start() {
    if (running) return;
    
    running = true;
    workerThread = std::thread(&AsyncDataManager::workerFunction, this);
    
    // Устанавливаем приоритет потока (для MinGW/MINGW)
#ifdef __MINGW32__
    // Для MinGW используем Windows API
    auto handle = workerThread.native_handle();
    SetThreadPriority(reinterpret_cast<HANDLE>(handle), THREAD_PRIORITY_ABOVE_NORMAL);
#elif defined(_WIN32)
    // Для MSVC
    auto handle = workerThread.native_handle();
    SetThreadPriority(handle, THREAD_PRIORITY_ABOVE_NORMAL);
#endif
}

void AsyncDataManager::stop() {
    if (!running) return;
    
    running = false;
    dataCV.notify_all();
    
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void AsyncDataManager::workerFunction() {
    int connectionErrors = 0;
    const int maxConnectionErrors = 3;
    int readErrors = 0;
    const int maxReadErrors = 5;
    
    while (running) {
        // Проверяем соединение перед чтением данных
        if (!client || !client->isConnected()) {
            connectionErrors++;
            if (connectionErrors >= maxConnectionErrors) {
                // Помечаем данные как невалидные
                {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    currentData.multimeter.valid = false;
                    currentData.machine.valid = false;
                    currentData.computer.valid = false;
                    currentData.allValid = false;
                }
                
                // Ждем и пытаемся продолжить
                std::this_thread::sleep_for(std::chrono::milliseconds(updateIntervalMs));
                continue;
            }
        } else {
            connectionErrors = 0; // Сброс счетчика при успешном соединении
        }
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        DeviceData newData;
        newData.lastUpdate = std::chrono::system_clock::now();
        bool hasValidData = false;
        
        try {
            // Чтение данных мультиметра
            if (multimeter->getDeviceNode().isValid()) {
                auto values = multimeter->readAllValues(*client);
                if (!values.empty()) {
                    newData.multimeter.valid = true;
                    hasValidData = true;
                    newData.multimeter.timestamp = std::chrono::system_clock::now();
                    
                    // Распределяем значения по порядку
                    if (values.size() > 0 && values[0].first) newData.multimeter.voltage = values[0].second;
                    if (values.size() > 1 && values[1].first) newData.multimeter.current = values[1].second;
                    if (values.size() > 2 && values[2].first) newData.multimeter.resistance = values[2].second;
                    if (values.size() > 3 && values[3].first) newData.multimeter.power = values[3].second;
                } else {
                    newData.multimeter.valid = false;
                }
            } else {
                newData.multimeter.valid = false;
            }
            
            // Чтение данных станка
            if (machine->getDeviceNode().isValid()) {
                auto values = machine->readAllValues(*client);
                if (!values.empty()) {
                    newData.machine.valid = true;
                    hasValidData = true;
                    newData.machine.timestamp = std::chrono::system_clock::now();
                    
                    if (values.size() > 0 && values[0].first) newData.machine.rpm = values[0].second;
                    if (values.size() > 1 && values[1].first) newData.machine.power = values[1].second;
                    if (values.size() > 2 && values[2].first) newData.machine.voltage = values[2].second;
                    if (values.size() > 3 && values[3].first) newData.machine.energy = values[3].second;
                } else {
                    newData.machine.valid = false;
                }
            } else {
                newData.machine.valid = false;
            }
            
            // Чтение данных компьютера
            if (computer->getDeviceNode().isValid()) {
                auto values = computer->readAllValues(*client);
                if (!values.empty()) {
                    newData.computer.valid = true;
                    hasValidData = true;
                    newData.computer.timestamp = std::chrono::system_clock::now();
                    
                    if (values.size() > 0 && values[0].first) newData.computer.fan1 = values[0].second;
                    if (values.size() > 1 && values[1].first) newData.computer.fan2 = values[1].second;
                    if (values.size() > 2 && values[2].first) newData.computer.fan3 = values[2].second;
                    if (values.size() > 3 && values[3].first) newData.computer.cpuLoad = values[3].second;
                    if (values.size() > 4 && values[4].first) newData.computer.gpuLoad = values[4].second;
                    if (values.size() > 5 && values[5].first) newData.computer.ramUsage = values[5].second;
                } else {
                    newData.computer.valid = false;
                }
            } else {
                newData.computer.valid = false;
            }
            
            newData.allValid = hasValidData;
            
            // Сброс счетчика ошибок чтения при успешном чтении
            if (hasValidData) {
                readErrors = 0;
            } else {
                readErrors++;
                if (readErrors >= maxReadErrors) {
                    std::cerr << "Многократные ошибки чтения данных. Проверьте соединение с сервером." << std::endl;
                    // Не увеличиваем паузу, продолжаем попытки
                }
            }
            
            // Быстрая блокировка для обновления данных
            {
                std::lock_guard<std::mutex> lock(dataMutex);
                currentData = newData;
            }
            
        } catch (const std::exception& e) {
            readErrors++;
            std::cerr << "Ошибка чтения данных: " << e.what() << std::endl;
            
            if (readErrors >= maxReadErrors) {
                std::cerr << "Критическая ошибка: невозможно прочитать данные после " << maxReadErrors << " попыток." << std::endl;
            }
            
            // Помечаем данные как невалидные в случае исключения
            {
                std::lock_guard<std::mutex> lock(dataMutex);
                currentData.multimeter.valid = false;
                currentData.machine.valid = false;
                currentData.computer.valid = false;
                currentData.allValid = false;
            }
        }
        
        // Вычисляем время выполнения и регулируем паузу
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        int sleepTime = updateIntervalMs - static_cast<int>(duration);
        
        // Динамическая регулировка паузы в зависимости от ошибок
        if (readErrors > 0) {
            sleepTime = std::max(sleepTime, 50); // Минимум 50мс при ошибках
        }
        
        if (connectionErrors > 0) {
            sleepTime = std::max(sleepTime, 100); // Минимум 100мс при потере соединения
        }
        
        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        } else {
            // Если чтение заняло больше времени, чем интервал, продолжаем без задержки
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Минимальная задержка
        }
    }
}

DeviceData AsyncDataManager::getCurrentData() {
    std::lock_guard<std::mutex> lock(dataMutex);
    return currentData;
}