#ifndef SIMPLE_WINDOW_H
#define SIMPLE_WINDOW_H

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <memory>
#include <chrono>

#include "opcua_client.h"
#include "device_managers.h"
#include "async_manager.h"

class SimpleWindow {
public:
    SimpleWindow();
    ~SimpleWindow();

    bool initialize();
    void run();

private:

    void updateMultimeterData(const DeviceData::MultimeterData& data);
    void updateMachineData(const DeviceData::MachineData& data);
    void updateComputerData(const DeviceData::ComputerData& data);
    
    // ===== Window =====
    sf::RenderWindow window;
    sf::Font font;
    bool fontLoaded{false};
    bool running{true};

    // ===== OPC UA =====
    std::shared_ptr<OPCUAClient> client;
    std::shared_ptr<AsyncDataManager> asyncManager;

    std::unique_ptr<MultimeterDevice> multimeter;
    std::unique_ptr<MachineDevice> machine;
    std::unique_ptr<ComputerDevice> computer;

    bool connected{false};
    bool devicesInitialized{false};

    // ===== UI state =====
    enum DeviceType { NONE, MULTIMETER, MACHINE, COMPUTER };
    DeviceType selectedDevice{NONE};

    // ===== Time =====
    std::chrono::steady_clock::time_point lastUpdate;

    // ===== Colors =====
    sf::Color background{20, 20, 25};
    sf::Color panel{40, 40, 55};
    sf::Color text{220, 220, 220};
    sf::Color accent{70, 130, 180};
    sf::Color disabled{120, 120, 120};

    // ===== UI Elements =====
    sf::RectangleShape serverBox;
    sf::RectangleShape devicePanel;
    sf::RectangleShape attrPanel;

    sf::RectangleShape multimeterBtn;
    sf::RectangleShape machineBtn;
    sf::RectangleShape computerBtn;

    // ===== Data =====
    struct {
        double voltage{}, current{}, resistance{}, power{};
    } multimeterData;

    struct {
        double rpm{}, power{}, voltage{}, energy{};
    } machineData;

    struct {
        double fan1{}, fan2{}, fan3{};
        double cpuLoad{}, gpuLoad{}, ramUsage{};
    } computerData;

private:
    // ===== Core loop =====
    void handleEvents();
    void update();
    void render();

    // ===== Drawing =====
    void drawHeader();
    void drawDevicePanel();
    void drawAttributesPanel();

    void drawText(const std::string& str, float x, float y,
                  sf::Color color = sf::Color::White, unsigned size = 16);

    void drawButton(sf::RectangleShape& btn,
                    const std::string& label,
                    bool selected);

    bool isMouseOver(const sf::RectangleShape& r);

    // ===== Helpers =====
    std::string currentTime() const;
    std::string currentDate() const;

    // ===== OPC =====
    void connectToServer();
    void initializeDevices();
    void updateAttributes();
};

#endif
