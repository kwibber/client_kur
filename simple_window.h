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
#include <algorithm>  // Добавьте эту строку

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

    // ===== Структура для правой панели =====
    struct RightPanelAttribute {
        std::string name;
        std::string displayName;
        double value;
    };
    // Структура для хранения атрибута
    struct Attribute {
        std::string name;
        std::string displayName;
        double value;
        bool isSelected;
        
        // Конструктор для удобства
        Attribute(const std::string& n, const std::string& dn, double v, bool sel = false)
            : name(n), displayName(dn), value(v), isSelected(sel) {}
    };

    // Структура для устройства в правой панели
    struct RightPanelDevice {
        std::string name;
        std::vector<Attribute> attributes;
    };

    // Тип для перечисления устройств
    enum DeviceType { 
        NONE = 0, 
        MULTIMETER = 1, 
        MACHINE = 2, 
        COMPUTER = 3 
    };

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
    DeviceType selectedDevice{NONE};
    std::vector<DeviceType> expandedDevices;  // Развернутые устройства в левой панели
    std::vector<std::string> selectedAttributes;  // Выбранные атрибуты (в формате "Устройство:Атрибут")
    
    // Данные атрибутов для отображения
    std::vector<Attribute> multimeterAttributes;
    std::vector<Attribute> machineAttributes;
    std::vector<Attribute> computerAttributes;
    
    // Данные для правой панели (сгруппированные по устройствам)
    std::map<std::string, std::vector<RightPanelAttribute>> rightPanelData;
    std::set<std::string> rightPanelSelection;


    // ===== Time =====
    std::chrono::steady_clock::time_point lastUpdate;

    // ===== Colors =====
    sf::Color background{20, 20, 25};
    sf::Color panel{40, 40, 55};
    sf::Color text{220, 220, 220};
    sf::Color accent{70, 130, 180};
    sf::Color selectedColor{100, 180, 100};
    sf::Color disabled{120, 120, 120};

    // ===== UI Elements =====
    sf::RectangleShape serverBox;
    sf::RectangleShape leftPanel;
    sf::RectangleShape rightPanel;
    
    sf::RectangleShape moveRightBtn;
    sf::RectangleShape moveLeftBtn;
    sf::RectangleShape clearAllBtn;

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

    // ===== Helpers =====
    std::string currentTime() const;
    std::string currentDate() const;
    
    // Инициализация списков атрибутов
    void initializeAttributes();
    
    // Обработка кликов на атрибуты
    void handleAttributeClick(const std::string& deviceName, int attributeIndex);
    
    // Добавление/удаление атрибута в правую панель
    void addAttributeToRightPanel(const std::string& deviceName, const Attribute& attribute);
    void removeAttributeFromRightPanel(const std::string& fullName);
    
    // Обновление значений атрибутов
    void updateAttributeValues();

    // ===== OPC =====
    void connectToServer();
    void initializeDevices();
    void updateAttributes();
};

#endif