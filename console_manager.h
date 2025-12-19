#ifndef CONSOLE_MANAGER_H
#define CONSOLE_MANAGER_H

#include "opcua_client.h"
#include "device_managers.h"
#include "async_manager.h"
#include <conio.h>
#include <windows.h>
#include <string>
#include <atomic>
#include <memory>


class ConsoleManager {
public:
    static void setupConsole();
    static void clearConsole();
    static void moveCursorToTop();
    static void clearLine();
    static void hideCursor();
    static void showCursor();
    static void printWelcome();
    static void printControls();
    static char getKeyPress();
    static bool isKeyPressed();
};


class OPCUAApplication {
private:
    OPCUAClient client;
    MultimeterDevice multimeter;
    MachineDevice machine;
    ComputerDevice computer;
    
    OPCUANode objectsFolder;
    bool nodesFound;
    std::atomic<bool> running;
    std::atomic<bool> connectionLost;
    
    
    std::unique_ptr<AsyncDataManager> asyncManager;
    int displayIntervalMs;  
    
    
    int reconnectAttempts;
    const int maxReconnectAttempts = 10;

public:
    OPCUAApplication(const std::string& endpoint = "opc.tcp://127.0.0.1:4840");
    ~OPCUAApplication();
    
    bool initialize();
    bool reconnect();  
    void run();
    void shutdown();
    
    bool checkConnection();  

private:
    void handleInput();
    void handleRPMInput();
    void handleControlModeInput();  
    void readAndDisplayValues();
    void displayAllDevicesAsync(const DeviceData& data);
    void displayConnectionStatus();  
};

#endif 