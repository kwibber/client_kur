#ifndef ASYNC_MANAGER_H
#define ASYNC_MANAGER_H

#include "opcua_client.h"
#include "device_managers.h"
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

struct DeviceData {
    struct MultimeterData {
        double voltage = 0.0;
        double current = 0.0;
        double resistance = 0.0;
        double power = 0.0;
        bool valid = false;
        std::chrono::system_clock::time_point timestamp;
    } multimeter;

    struct MachineData {
        double rpm = 0.0;
        double power = 0.0;
        double voltage = 0.0;
        double energy = 0.0;
        bool valid = false;
        std::chrono::system_clock::time_point timestamp;
    } machine;

    struct ComputerData {
        double fan1 = 0.0;
        double fan2 = 0.0;
        double fan3 = 0.0;
        double cpuLoad = 0.0;
        double gpuLoad = 0.0;
        double ramUsage = 0.0;
        bool valid = false;
        std::chrono::system_clock::time_point timestamp;
    } computer;

    std::chrono::system_clock::time_point lastUpdate;
    bool allValid = false;
};

class AsyncDataManager {
private:
    std::atomic<bool> running{false};
    std::thread workerThread;
    std::mutex dataMutex;
    std::condition_variable dataCV;
    
    DeviceData currentData;
    OPCUAClient* client;
    MultimeterDevice* multimeter;
    MachineDevice* machine;
    ComputerDevice* computer;
    
    int updateIntervalMs;  // Интервал обновления в миллисекундах

    void workerFunction();

public:
    AsyncDataManager(OPCUAClient* client, 
                    MultimeterDevice* multimeter,
                    MachineDevice* machine,
                    ComputerDevice* computer,
                    int updateIntervalMs = 50);  // По умолчанию 50 мс
    
    ~AsyncDataManager();
    
    void start();
    void stop();
    DeviceData getCurrentData();
    bool isRunning() const { return running; }
    void setUpdateInterval(int ms) { updateIntervalMs = ms; }
};

#endif // ASYNC_MANAGER_H