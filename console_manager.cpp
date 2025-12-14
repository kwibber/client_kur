#include "console_manager.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <ctime>

// Реализация ConsoleManager

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

void ConsoleManager::printWelcome() {
    std::cout << "Клиент OPC UA запускается..." << std::endl;
    std::cout << "Подключение к: opc.tcp://127.0.0.1:4840" << std::endl;
    std::cout << std::endl;
}

void ConsoleManager::printControls() {
    std::cout << "\nУправление:" << std::endl;
    std::cout << "  - 'q' - выход" << std::endl;
    std::cout << "  - 'r' - установить новые обороты маховика" << std::endl;
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

// Реализация OPCUAApplication

OPCUAApplication::OPCUAApplication(const std::string& endpoint)
    : client(endpoint), nodesFound(false), running(true) {
    objectsFolder = OPCUANode(UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), "Objects", "Objects Folder");
}

bool OPCUAApplication::initialize() {
    ConsoleManager::setupConsole();
    ConsoleManager::printWelcome();

    if (!client.connect()) {
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

    return true;
}

void OPCUAApplication::run() {
    std::cout << "\n\nНачало чтения значений..." << std::endl;
    ConsoleManager::printControls();
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    ConsoleManager::clearConsole();

    while (running) {
        handleInput();
        readAndDisplayValues();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    shutdown();
}

void OPCUAApplication::shutdown() {
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
        }
    }
}

void OPCUAApplication::handleRPMInput() {
    if (!machine.getFlywheelRPMNode().isValid()) {
        std::cerr << "\nУзел оборотов маховика не найден, невозможно установить значение." << std::endl;
        return;
    }

    std::cout << "\nВведите новые обороты маховика (об/мин): ";
    std::string input;
    std::getline(std::cin, input);

    try {
        double newRpm = std::stod(input);
        
        if (machine.setRPMValue(client, newRpm)) {
            std::cout << "Успешно установлены обороты: " << newRpm << " об/мин" << std::endl;
        } else {
            std::cerr << "Ошибка записи значения оборотов" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Неверный ввод: " << e.what() << std::endl;
    }
}

void OPCUAApplication::readAndDisplayValues() {
    ConsoleManager::clearConsole();
    
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::cout << "===========================================" << std::endl;
    std::cout << "Данные OPC UA - " << std::ctime(&now_time);
    std::cout << "===========================================" << std::endl;
    
    displayAllDevices();
    
    std::cout << "===========================================" << std::endl;
    ConsoleManager::printControls();
}

void OPCUAApplication::displayAllDevices() {
    bool hasMultimeter = multimeter.getDeviceNode().isValid();
    bool hasMachine = machine.getDeviceNode().isValid();
    bool hasComputer = computer.getDeviceNode().isValid();
    
    if (hasMultimeter) {
        std::cout << "\n[МУЛЬТИМЕТР]" << std::endl;
        auto multimeterValues = multimeter.readAllValues(client);
        std::vector<std::string> multimeterNames = {"Напряжение", "Ток", "Сопротивление", "Мощность"};
        std::vector<std::string> multimeterUnits = {"В", "А", "Ом", "Вт"};
        
        for (size_t i = 0; i < multimeterValues.size() && i < multimeterNames.size(); i++) {
            if (multimeterValues[i].first) {
                std::cout << "  " << multimeterNames[i] << ": " 
                          << std::fixed << std::setprecision(2) << multimeterValues[i].second 
                          << " " << multimeterUnits[i] << std::endl;
            }
        }
    }
    
    if (hasMachine) {
        std::cout << "\n[СТАНОК]" << std::endl;
        auto machineValues = machine.readAllValues(client);
        std::vector<std::string> machineNames = {"Обороты маховика", "Мощность", "Напряжение", "Потребление энергии"};
        std::vector<std::string> machineUnits = {"об/мин", "кВт", "В", "кВт·ч"};
        
        for (size_t i = 0; i < machineValues.size() && i < machineNames.size(); i++) {
            if (machineValues[i].first) {
                std::cout << "  " << machineNames[i] << ": " 
                          << std::fixed << std::setprecision(2) << machineValues[i].second 
                          << " " << machineUnits[i] << std::endl;
            }
        }
    }
    
    if (hasComputer) {
        std::cout << "\n[КОМПЬЮТЕР]" << std::endl;
        auto computerValues = computer.readAllValues(client);
        std::vector<std::string> computerNames = {"Вентилятор 1", "Вентилятор 2", "Вентилятор 3", 
                                                 "Загрузка ЦП", "Загрузка ГП", "Использование ОЗУ"};
        std::vector<std::string> computerUnits = {"об/мин", "об/мин", "об/мин", "%", "%", "%"};
        
        for (size_t i = 0; i < computerValues.size() && i < computerNames.size(); i++) {
            if (computerValues[i].first) {
                std::cout << "  " << computerNames[i] << ": " 
                          << std::fixed << std::setprecision(2) << computerValues[i].second 
                          << " " << computerUnits[i] << std::endl;
            }
        }
    }
}