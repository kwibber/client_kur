#ifndef SIMPLE_WINDOW_H
#define SIMPLE_WINDOW_H

#include <SFML/Graphics.hpp>
#include <optional>
#include <string>
#include <memory>
#include <set>
#include <chrono>
#include <vector>
#include <map>
#include <algorithm>
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
    struct RightPanelAttribute {
        std::string name;
        std::string displayName;
        double value;
    };

    struct Attribute {
        std::string name;
        std::string displayName;
        double value;
        bool isSelected;
        
        Attribute(const std::string& n, const std::string& dn, double v, bool sel = false)
            : name(n), displayName(dn), value(v), isSelected(sel) {}
    };

    struct RightPanelDevice {
        std::string name;
        std::vector<Attribute> attributes;
    };

    enum DeviceType { 
        NONE = 0, 
        MULTIMETER = 1, 
        MACHINE = 2, 
        COMPUTER = 3 
    };

    void updateMultimeterData(const DeviceData::MultimeterData& data);
    void updateMachineData(const DeviceData::MachineData& data);
    void updateComputerData(const DeviceData::ComputerData& data);
    
    sf::RenderWindow window;
    sf::Font font;
    bool fontLoaded{false};
    bool running{true};

    std::shared_ptr<OPCUAClient> client;
    std::shared_ptr<AsyncDataManager> asyncManager;
    std::unique_ptr<MultimeterDevice> multimeter;
    std::unique_ptr<MachineDevice> machine;
    std::unique_ptr<ComputerDevice> computer;

    bool connected{false};
    bool devicesInitialized{false};

    DeviceType selectedDevice{NONE};
    std::vector<DeviceType> expandedDevices;
    std::vector<std::string> selectedAttributes;
    
    std::vector<Attribute> multimeterAttributes;
    std::vector<Attribute> machineAttributes;
    std::vector<Attribute> computerAttributes;
    
    std::map<std::string, std::vector<RightPanelAttribute>> rightPanelData;
    std::set<std::string> rightPanelSelection;

    std::chrono::steady_clock::time_point lastUpdate;

    sf::Color background{20, 20, 25};
    sf::Color panel{40, 40, 55};
    sf::Color text{220, 220, 220};
    sf::Color accent{70, 130, 180};
    sf::Color selectedColor{100, 180, 100};
    sf::Color disabled{120, 120, 120};

    sf::RectangleShape serverBox;
    sf::RectangleShape leftPanel;
    sf::RectangleShape rightPanel;
    
    sf::RectangleShape moveRightBtn;
    sf::RectangleShape moveLeftBtn;
    sf::RectangleShape clearAllBtn;
    sf::RectangleShape disconnectBtn;

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
    void handleEvents();
    void update();
    void render();

    void drawHeader();
    void drawLeftPanel();
    void drawRightPanel();
    void drawCenterButtons();

    void drawText(const std::string& str, float x, float y,
                  sf::Color color = sf::Color::White, unsigned size = 16);

    void drawButton(sf::RectangleShape& btn,
                    const std::string& label,
                    bool selected);

    bool isMouseOver(float x, float y, float width, float height);
    bool isMouseOver(const sf::RectangleShape& r);

    std::string currentTime() const;
    std::string currentDate() const;
    
    void initializeAttributes();
    
    void handleAttributeClick(const std::string& deviceName, int attributeIndex);
    
    void addAttributeToRightPanel(const std::string& deviceName, const Attribute& attribute);
    void removeAttributeFromRightPanel(const std::string& fullName);
    
    void updateAttributeValues();

    void connectToServer();
    void initializeDevices();
    void updateAttributes();
};

#endif