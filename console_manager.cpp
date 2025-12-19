#include "console_manager.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>
#include <sstream>



void ConsoleManager::setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void ConsoleManager::clearConsole() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void ConsoleManager::moveCursorToTop() {
#ifdef _WIN32
    static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coord = {0, 0};
    SetConsoleCursorPosition(hConsole, coord);
#else
    std::cout << "\033[H";
#endif
}

void ConsoleManager::clearLine() {
#ifdef _WIN32
    static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    COORD coord = {0, csbi.dwCursorPosition.Y};
    DWORD charsWritten;
    FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X, coord, &charsWritten);
    SetConsoleCursorPosition(hConsole, coord);
#else
    std::cout << "\033[2K\r";
#endif
}

void ConsoleManager::hideCursor() {
#ifdef _WIN32
    static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = false;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
#else
    std::cout << "\033[?25l";
#endif
}

void ConsoleManager::showCursor() {
#ifdef _WIN32
    static HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = true;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
#else
    std::cout << "\033[?25h";
#endif
}

void ConsoleManager::printWelcome() {
    std::cout << "Клиент OPC UA запускается..." << std::endl;
    std::cout << "Подключение к: opc.tcp://127.0.0.1:4840" << std::endl;
    std::cout << std::endl;
}

void ConsoleManager::printControls() {
    std::cout << "\nУправление:" << std::endl;
    std::cout << "  - 'q' - выход" << std::endl;
    std::cout << "  - 'r' - установить новые обороты маховика" << std::endl;
    std::cout << "  - 'm' - переключить режим управления (авто/ручной)" << std::endl;
    std::cout << "  - 'p' - пауза/продолжить обновление данных" << std::endl;
    std::cout << std::endl;
}

char ConsoleManager::getKeyPress() {
    if (_kbhit()) {
        return _getch();
    }
    return 0;
}

bool ConsoleManager::isKeyPressed() {
    return _kbhit() != 0;
}



OPCUAApplication::OPCUAApplication(const std::string& endpoint)
    : client(endpoint), nodesFound(false), running(true), 
      connectionLost(false), displayIntervalMs(16),
      reconnectAttempts(0) {
    
    objectsFolder = OPCUANode(UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), "Objects", "Objects Folder");
}

OPCUAApplication::~OPCUAApplication() {
    shutdown();
}

bool OPCUAApplication::initialize() {
    ConsoleManager::setupConsole();
    ConsoleManager::printWelcome();

    if (!client.connect()) {
        std::cerr << "Не удалось подключиться к серверу" << std::endl;
        return false;
    }

    std::cout << "Подключено к серверу OPC UA" << std::endl;
    std::cout << "Поиск устройств..." << std::endl;

    bool multimeterFound = multimeter.initialize(client, objectsFolder);
    bool machineFound = machine.initialize(client, objectsFolder);
    bool computerFound = computer.initialize(client, objectsFolder);

    if (multimeterFound) {
        multimeter.printStatus();
    }

    if (machineFound) {
        machine.printStatus();
    }

    if (computerFound) {
        computer.printStatus();
    }

    nodesFound = multimeterFound || machineFound || computerFound;

    if (!nodesFound) {
        std::cerr << "\nОШИБКА: Не найдено ни одного устройства." << std::endl;
        std::cerr << "Убедитесь, что сервер запущен и создал переменные." << std::endl;
        return false;
    }

    
    asyncManager = std::make_unique<AsyncDataManager>(&client, &multimeter, &machine, &computer, 20);
    asyncManager->start();

    return true;
}

bool OPCUAApplication::reconnect() {
    if (reconnectAttempts >= maxReconnectAttempts) {
        std::cerr << "Достигнуто максимальное количество попыток переподключения" << std::endl;
        return false;
    }
    
    reconnectAttempts++;
    std::cout << "\nПопытка переподключения #" << reconnectAttempts << "..." << std::endl;
    
    
    if (asyncManager) {
        asyncManager->stop();
        asyncManager.reset();
    }
    
    
    client.disconnect();
    
   
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    
    if (client.connect()) {
        
        multimeter.initialize(client, objectsFolder);
        machine.initialize(client, objectsFolder);
        computer.initialize(client, objectsFolder);
        
        
        asyncManager = std::make_unique<AsyncDataManager>(&client, &multimeter, &machine, &computer, 20);
        asyncManager->start();
        
        reconnectAttempts = 0;
        connectionLost = false;
        std::cout << "Переподключение успешно!" << std::endl;
        return true;
    }
    
    return false;
}

bool OPCUAApplication::checkConnection() {
    bool connected = client.isConnected();
    if (!connected && !connectionLost) {
        connectionLost = true;
        std::cerr << "\nПотеряно соединение с сервером!" << std::endl;
    }
    return connected;
}

void OPCUAApplication::run() {
    std::cout << "\n\nНачало чтения значений..." << std::endl;
    ConsoleManager::printControls();
    ConsoleManager::hideCursor();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    ConsoleManager::clearConsole();

    auto lastDisplayTime = std::chrono::high_resolution_clock::now();
    auto lastConnectionCheck = std::chrono::high_resolution_clock::now();
    bool paused = false;
    
    while (running) {
        
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsedCheck = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastConnectionCheck).count();
        
        if (elapsedCheck >= 2000) {
            if (!checkConnection() && connectionLost) {
                if (!reconnect()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    continue;
                }
            }
            lastConnectionCheck = currentTime;
        }
        
        
        handleInput();
        
        
        if (ConsoleManager::isKeyPressed()) {
            char c = ConsoleManager::getKeyPress();
            if (c == 'p' || c == 'P') {
                paused = !paused;
                std::cout << "\n" << (paused ? "Пауза" : "Продолжение") << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
            }
        }
        
        
        if (!paused && !connectionLost) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastDisplayTime).count();
            
            if (elapsed >= displayIntervalMs) {
                readAndDisplayValues();
                lastDisplayTime = currentTime;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } else {
            if (connectionLost) {
                ConsoleManager::clearConsole();
                std::cout << "СОЕДИНЕНИЕ ПОТЕРЯНО" << std::endl;
                std::cout << "Попытка переподключения..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    shutdown();
}

void OPCUAApplication::shutdown() {
    if (asyncManager) {
        asyncManager->stop();
    }
    
    ConsoleManager::showCursor();
    std::cout << "\nОтключение от сервера..." << std::endl;
    client.disconnect();
    std::cout << "Клиент остановлен." << std::endl;
}

void OPCUAApplication::handleInput() {
    if (ConsoleManager::isKeyPressed()) {
        char c = ConsoleManager::getKeyPress();
        
        switch (c) {
            case 'q':
            case 'Q':
                std::cout << "\nВыход..." << std::endl;
                running = false;
                break;
                
            case 'r':
            case 'R':
                handleRPMInput();
                break;
                
            case 'm':
            case 'M':  
                handleControlModeInput();
                break;
        }
    }
}

void OPCUAApplication::handleRPMInput() {
    if (!machine.getTargetRPMNode().isValid()) {
        std::cerr << "\nУзел целевых оборотов не найден" << std::endl;
        return;
    }

    ConsoleManager::showCursor();
    std::cout << "\nВведите новые обороты маховика (0-3000 об/мин): ";
    std::string input;
    std::getline(std::cin, input);
    ConsoleManager::hideCursor();

    try {
        double newRpm = std::stod(input);
        
        
        if (newRpm < 0.0) newRpm = 0.0;
        if (newRpm > 3000.0) newRpm = 3000.0;
        
        if (machine.setTargetRPM(client, newRpm)) {
            std::cout << "Успешно установлены целевые обороты: " << newRpm << " об/мин" << std::endl;
        } else {
            std::cerr << "Ошибка записи значения оборотов" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Неверный ввод: " << e.what() << std::endl;
    }
}

void OPCUAApplication::handleControlModeInput() {
    if (!machine.getControlModeNode().isValid()) {
        std::cerr << "\nУзел режима управления не найден" << std::endl;
        return;
    }

    ConsoleManager::showCursor();
    std::cout << "\nВыберите режим управления:" << std::endl;
    std::cout << "  0 - Автоматический режим" << std::endl;
    std::cout << "  1 - Ручной режим" << std::endl;
    std::cout << "Введите значение (0 или 1): ";
    std::string input;
    std::getline(std::cin, input);
    ConsoleManager::hideCursor();

    try {
        int mode = std::stoi(input);
        
        if (mode == 0 || mode == 1) {
            if (machine.setControlMode(client, mode)) {
                std::cout << "Режим управления изменен на: " << (mode == 0 ? "АВТОМАТИЧЕСКИЙ" : "РУЧНОЙ") << std::endl;
            } else {
                std::cerr << "Ошибка изменения режима управления" << std::endl;
            }
        } else {
            std::cerr << "Недопустимое значение. Допустимы только 0 или 1." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Неверный ввод: " << e.what() << std::endl;
    }
}

void OPCUAApplication::readAndDisplayValues() {
    
    auto data = asyncManager->getCurrentData();
    
    
    static bool firstRun = true;
    if (firstRun) {
        ConsoleManager::clearConsole();
        firstRun = false;
    } else {
        ConsoleManager::moveCursorToTop();
    }
    
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    
    
    std::cout << "===========================================\n";
    std::cout << "Данные OPC UA - " << std::ctime(&now_time);
    std::cout << "Обновление: " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                  now - data.lastUpdate).count() 
              << " мс назад\n";
    std::cout << "Частота: " << (1000 / displayIntervalMs) << " FPS\n";
    
    
    std::cout << "Статус: " << (connectionLost ? "ОТКЛЮЧЕНО" : "ПОДКЛЮЧЕНО") << "\n";
    
    std::cout << "===========================================\n";
    
    
    displayAllDevicesAsync(data);
    
    
    std::cout << "\nУправление станциком:\n";
    std::cout << "  'r' - задать обороты (0-3000 об/мин)\n";
    std::cout << "  'm' - выбрать режим (0=авто, 1=ручной)\n";
    std::cout << "  'p' - пауза/продолжить\n";
    std::cout << "  'q' - выход\n";
    std::cout << "===========================================\n";
    
    std::cout.flush();
}

void OPCUAApplication::displayAllDevicesAsync(const DeviceData& data) {
    std::ostringstream buffer;
    
    if (data.multimeter.valid) {
        buffer << "\n[МУЛЬТИМЕТР] ";
        buffer << "(задержка: " 
               << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now() - data.multimeter.timestamp).count()
               << " мс)\n";
        
        buffer << "  Напряжение: " << std::fixed << std::setprecision(2) 
               << data.multimeter.voltage << " В\n";
        buffer << "  Ток: " << data.multimeter.current << " А\n";
        buffer << "  Сопротивление: " << data.multimeter.resistance << " Ом\n";
        buffer << "  Мощность: " << data.multimeter.power << " Вт\n";
    } else {
        buffer << "\n[МУЛЬТИМЕТР] Нет данных\n";
    }
    
    if (data.machine.valid) {
        buffer << "\n[СТАНОК] ";
        buffer << "(задержка: " 
               << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now() - data.machine.timestamp).count()
               << " мс)\n";
        
        buffer << "  Обороты маховика: " << data.machine.rpm << " об/мин\n";
        buffer << "  Мощность: " << data.machine.power << " кВт\n";
        buffer << "  Напряжение: " << data.machine.voltage << " В\n";
        buffer << "  Потребление энергии: " << data.machine.energy << " кВт·ч\n";
    } else {
        buffer << "\n[СТАНОК] Нет данных\n";
    }
    
    if (data.computer.valid) {
        buffer << "\n[КОМПЬЮТЕР] ";
        buffer << "(задержка: " 
               << std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now() - data.computer.timestamp).count()
               << " мс)\n";
        
        buffer << "  Вентилятор 1: " << data.computer.fan1 << " об/мин\n";
        buffer << "  Вентилятор 2: " << data.computer.fan2 << " об/мин\n";
        buffer << "  Вентилятор 3: " << data.computer.fan3 << " об/мин\n";
        buffer << "  Загрузка ЦП: " << data.computer.cpuLoad << " %\n";
        buffer << "  Загрузка ГП: " << data.computer.gpuLoad << " %\n";
        buffer << "  Использование ОЗУ: " << data.computer.ramUsage << " %\n";
    } else {
        buffer << "\n[КОМПЬЮТЕР] Нет данных\n";
    }
    
    std::cout << buffer.str();
}