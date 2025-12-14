#ifndef CONSOLE_MANAGER_H
#define CONSOLE_MANAGER_H

#include "opcua_client.h"
#include "device_managers.h"
#include <conio.h>
#include <windows.h>
#include <string>
#include <atomic>

// Класс для работы с консолью
class ConsoleManager {
public:
    static void setupConsole();
    static void clearConsole();
    static void printWelcome();
    static void printControls();
    static char getKeyPress();
    static bool isKeyPressed();
};

// Основной класс приложения
class OPCUAApplication {
private:
    OPCUAClient client;
    MultimeterDevice multimeter;
    MachineDevice machine;
    ComputerDevice computer;
    
    OPCUANode objectsFolder;
    bool nodesFound;
    std::atomic<bool> running;

public:
    OPCUAApplication(const std::string& endpoint = "opc.tcp://127.0.0.1:4840");
    bool initialize();
    void run();
    void shutdown();

private:
    void handleInput();
    void handleRPMInput();
    void readAndDisplayValues();
    void displayAllDevices();
};

#endif // CONSOLE_MANAGER_H