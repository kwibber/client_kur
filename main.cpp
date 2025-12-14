#include "opcua_client.h"
#include "device_managers.h"
#include "console_manager.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

// Базовый класс для обработки сигналов
class SignalHandler {
private:
    static std::atomic<bool> running;

public:
    static void initialize() {
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
    }

    static bool isRunning() { return running; }
    static void stop() { running = false; }

private:
    static void signalHandler(int signal) {
        running = false;
    }
};

std::atomic<bool> SignalHandler::running(true);

int main() {
    SignalHandler::initialize();
    
    OPCUAApplication app;
    
    if (!app.initialize()) {
        std::cerr << "Ошибка инициализации приложения." << std::endl;
        return 1;
    }
    
    app.run();
    
    return 0;
}