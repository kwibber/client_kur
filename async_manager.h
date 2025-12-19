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

struct DeviceData
{
    struct MultimeterData {
        bool valid{false};
        double voltage{};
        double current{};
        double resistance{};
        double power{};
        std::chrono::system_clock::time_point timestamp;
    } multimeter;

    struct MachineData {
        bool valid{false};
        double rpm{};
        double power{};
        double voltage{};
        double energy{};
        std::chrono::system_clock::time_point timestamp;
    } machine;

    struct ComputerData {
        bool valid{false};
        double fan1{};
        double fan2{};
        double fan3{};
        double cpuLoad{};
        double gpuLoad{};
        double ramUsage{};
        std::chrono::system_clock::time_point timestamp;
    } computer;

    bool allValid{false};
    std::chrono::system_clock::time_point lastUpdate;
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
    
    int updateIntervalMs;

    void workerFunction();

public:
    AsyncDataManager(OPCUAClient* client, 
                    MultimeterDevice* multimeter,
                    MachineDevice* machine,
                    ComputerDevice* computer,
                    int updateIntervalMs = 50);
    
    ~AsyncDataManager();
    
    void start();
    void stop();
    DeviceData getCurrentData();
    bool isRunning() const { return running; }
    void setUpdateInterval(int ms) { updateIntervalMs = ms; }
};

#endif