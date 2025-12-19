#include "opcua_client.h"
#include "device_managers.h"
#include "console_manager.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif


namespace {
    OPCUAApplication* g_app = nullptr;
    std::atomic<bool> g_shutdownRequested{false};
}


class SignalHandler {
private:
    static std::atomic<bool> running;

public:
    static void initialize() {
        
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
#ifdef _WIN32
        
        SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#endif
    }

    static bool isRunning() { return running; }
    static void stop() { 
        running = false;
        g_shutdownRequested = true;
        
        
        if (g_app) {
            
            std::thread shutdownThread([]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (g_app) {
                    
                    g_app->shutdown();
                }
            });
            shutdownThread.detach();
        }
    }

private:
    static void signalHandler(int signal) {
        std::cout << "\nПолучен сигнал " << signal << ". Завершение работы..." << std::endl;
        stop();
    }

#ifdef _WIN32
    static BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
        switch (ctrlType) {
            case CTRL_C_EVENT:
                std::cout << "\nПолучен Ctrl+C. Завершение работы..." << std::endl;
                break;
            case CTRL_BREAK_EVENT:
                std::cout << "\nПолучен Ctrl+Break. Завершение работы..." << std::endl;
                break;
            case CTRL_CLOSE_EVENT:
                std::cout << "\nЗакрытие консоли. Завершение работы..." << std::endl;
                break;
            case CTRL_LOGOFF_EVENT:
                std::cout << "\nВыход пользователя из системы. Завершение работы..." << std::endl;
                break;
            case CTRL_SHUTDOWN_EVENT:
                std::cout << "\nЗавершение работы системы. Завершение работы..." << std::endl;
                break;
            default:
                break;
        }
        stop();
        return TRUE; 
    }
#endif
};

std::atomic<bool> SignalHandler::running(true);

int main() {
    SignalHandler::initialize();
    
    std::cout << "===========================================" << std::endl;
    std::cout << "Запуск OPC UA клиента" << std::endl;
    std::cout << "Версия: 1.0" << std::endl;
    std::cout << "===========================================\n" << std::endl;
    
    try {
        OPCUAApplication app;
        g_app = &app; 
        
        if (!app.initialize()) {
            std::cerr << "Ошибка инициализации приложения." << std::endl;
            std::cerr << "Возможные причины:" << std::endl;
            std::cerr << "1. Сервер OPC UA не запущен" << std::endl;
            std::cerr << "2. Неверный адрес сервера" << std::endl;
            std::cerr << "3. Проблемы с сетью" << std::endl;
            std::cerr << "Проверьте, что сервер запущен по адресу: opc.tcp://127.0.0.1:4840" << std::endl;
            return 1;
        }
        
        std::cout << "\nПриложение успешно инициализировано." << std::endl;
        std::cout << "Для выхода нажмите Ctrl+C или 'q' в программе." << std::endl;
        
        
        std::thread appThread([&app]() {
            app.run();
        });
        
        
        while (!g_shutdownRequested && SignalHandler::isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            
            if (!appThread.joinable()) {
                break;
            }
        }
        
        
        if (g_shutdownRequested && appThread.joinable()) {
            std::cout << "\nЗавершение приложения..." << std::endl;
            
            
            for (int i = 0; i < 10 && appThread.joinable(); i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        
        if (appThread.joinable()) {
            appThread.join();
        }
        
        std::cout << "\n===========================================" << std::endl;
        std::cout << "OPC UA клиент успешно завершил работу." << std::endl;
        std::cout << "===========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nКритическая ошибка: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\nНеизвестная критическая ошибка" << std::endl;
        return 1;
    }
    
    
    g_app = nullptr;
    
    return 0;
}