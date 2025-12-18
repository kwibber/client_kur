#include "simple_window.h"
#include <SFML/Window/Event.hpp>
#include <thread>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <set>
#include <iostream>
#include <algorithm>  // Добавьте эту строку
#include "async_manager.h"

static std::set<std::string> rightPanelSelection;

// ================== Constructor ==================

SimpleWindow::SimpleWindow()
    : window(sf::VideoMode({1200, 800}), "OPC UA Client Monitor",
             sf::Style::Titlebar | sf::Style::Close)
{
    window.setVerticalSyncEnabled(true);

    serverBox.setSize({420.f, 36.f});
    serverBox.setPosition({20.f, 10.f});
    serverBox.setFillColor(panel);

    leftPanel.setSize({320.f, 600.f});
    leftPanel.setPosition({20.f, 60.f});
    leftPanel.setFillColor(panel);

    rightPanel.setSize({520.f, 600.f});
    rightPanel.setPosition({780.f, 60.f});
    rightPanel.setFillColor(panel);

    moveRightBtn.setSize({80.f, 40.f});
    moveRightBtn.setPosition({550.f, 300.f});
    moveRightBtn.setFillColor(accent);

    moveLeftBtn.setSize({80.f, 40.f});
    moveLeftBtn.setPosition({550.f, 360.f});
    moveLeftBtn.setFillColor(accent);

    clearAllBtn.setSize({80.f, 40.f});
    clearAllBtn.setPosition({550.f, 420.f});
    clearAllBtn.setFillColor(sf::Color(180, 70, 70));

    initializeAttributes();
}

SimpleWindow::~SimpleWindow() {
    if (asyncManager) asyncManager->stop();
    if (client) client->disconnect();
}

// ================== Init ==================

bool SimpleWindow::initialize()
{
    fontLoaded = font.openFromFile("DejaVuSans.ttf");
    return fontLoaded;
}

// ================== Инициализация атрибутов ==================

void SimpleWindow::initializeAttributes()
{
    // Атрибуты мультиметра
    multimeterAttributes.clear();
    multimeterAttributes.push_back(Attribute("voltage", "Напряжение", 0.0, false));
    multimeterAttributes.push_back(Attribute("current", "Ток", 0.0, false));
    multimeterAttributes.push_back(Attribute("resistance", "Сопротивление", 0.0, false));
    multimeterAttributes.push_back(Attribute("power", "Мощность", 0.0, false));
    
    // Атрибуты станка
    machineAttributes.clear();
    machineAttributes.push_back(Attribute("rpm", "Обороты", 0.0, false));
    machineAttributes.push_back(Attribute("power", "Мощность", 0.0, false));
    machineAttributes.push_back(Attribute("voltage", "Напряжение", 0.0, false));
    machineAttributes.push_back(Attribute("energy", "Энергопотребление", 0.0, false));
    
    // Атрибуты компьютера
    computerAttributes.clear();
    computerAttributes.push_back(Attribute("fan1", "Вентилятор 1", 0.0, false));
    computerAttributes.push_back(Attribute("fan2", "Вентилятор 2", 0.0, false));
    computerAttributes.push_back(Attribute("fan3", "Вентилятор 3", 0.0, false));
    computerAttributes.push_back(Attribute("cpuLoad", "Загрузка CPU", 0.0, false));
    computerAttributes.push_back(Attribute("gpuLoad", "Загрузка GPU", 0.0, false));
    computerAttributes.push_back(Attribute("ramUsage", "Использование RAM", 0.0, false));
}

// ================== Run ==================

void SimpleWindow::run()
{
    lastUpdate = std::chrono::steady_clock::now();
    while (window.isOpen() && running) {
        handleEvents();
        update();
        render();
    }
}

// ================== Events ==================

void SimpleWindow::handleEvents()
{
    while (auto e = window.pollEvent()) {
        if (e->is<sf::Event::Closed>()) {
            window.close();
        }

        if (auto* m = e->getIf<sf::Event::MouseButtonPressed>()) {
            if (m->button != sf::Mouse::Button::Left)
                continue;

            auto mouse = sf::Mouse::getPosition(window);

            // ===== Подключение к серверу =====
            if (!connected && isMouseOver(serverBox)) {
                connectToServer();
                return;
            }

            // ===== Добавить выбранные атрибуты =====
            if (isMouseOver(moveRightBtn)) {
                for (const auto& a : multimeterAttributes)
                    if (a.isSelected) addAttributeToRightPanel("Мультиметр", a);
                for (const auto& a : machineAttributes)
                    if (a.isSelected) addAttributeToRightPanel("Станок", a);
                for (const auto& a : computerAttributes)
                    if (a.isSelected) addAttributeToRightPanel("Компьютер", a);
                return;
            }

            // ===== Удалить выбранные в правой панели =====
            if (isMouseOver(moveLeftBtn)) {
                for (const auto& fullName : rightPanelSelection) {
                    removeAttributeFromRightPanel(fullName);
                }
                rightPanelSelection.clear();
                return;
            }

            // ===== Очистить всё =====
            if (isMouseOver(clearAllBtn)) {
                rightPanelData.clear();
                rightPanelSelection.clear();
                for (auto& a : multimeterAttributes) a.isSelected = false;
                for (auto& a : machineAttributes) a.isSelected = false;
                for (auto& a : computerAttributes) a.isSelected = false;
                return;
            }

            if (!connected || !devicesInitialized)
                continue;

            // ===== Левая панель (устройства + атрибуты) =====
            float y = 100.f;
            const float itemH = 30.f;

            auto toggleDevice = [&](DeviceType d) {
                auto it = std::find(expandedDevices.begin(), expandedDevices.end(), d);
                if (it != expandedDevices.end())
                    expandedDevices.erase(it);
                else
                    expandedDevices.push_back(d);
            };

            auto deviceHit = [&](float yy) {
                return mouse.x >= 40 && mouse.x <= 380 &&
                       mouse.y >= yy && mouse.y <= yy + itemH;
            };

            // --- Мультиметр ---
            if (deviceHit(y)) toggleDevice(MULTIMETER);
            bool mExp = std::find(expandedDevices.begin(), expandedDevices.end(), MULTIMETER) != expandedDevices.end();
            y += itemH;

            if (mExp) {
                for (auto& a : multimeterAttributes) {
                    if (mouse.x >= 60 && mouse.x <= 360 &&
                        mouse.y >= y && mouse.y <= y + 25)
                    {
                        a.isSelected = !a.isSelected;
                    }
                    y += 30.f;
                }
            }

            // --- Станок ---
            if (deviceHit(y)) toggleDevice(MACHINE);
            bool maExp = std::find(expandedDevices.begin(), expandedDevices.end(), MACHINE) != expandedDevices.end();
            y += itemH;

            if (maExp) {
                for (auto& a : machineAttributes) {
                    if (mouse.x >= 60 && mouse.x <= 360 &&
                        mouse.y >= y && mouse.y <= y + 25)
                    {
                        a.isSelected = !a.isSelected;
                    }
                    y += 30.f;
                }
            }

            // --- Компьютер ---
            if (deviceHit(y)) toggleDevice(COMPUTER);
            bool cExp = std::find(expandedDevices.begin(), expandedDevices.end(), COMPUTER) != expandedDevices.end();
            y += itemH;

            if (cExp) {
                for (auto& a : computerAttributes) {
                    if (mouse.x >= 60 && mouse.x <= 360 &&
                        mouse.y >= y && mouse.y <= y + 25)
                    {
                        a.isSelected = !a.isSelected;
                    }
                    y += 30.f;
                }
            }

            // ===== Правая панель (выбор атрибутов) =====
            float ry = 140.f;

            for (auto& [deviceName, attributes] : rightPanelData) {
                if (attributes.empty()) continue;

                ry += 25.f; // заголовок

                for (auto& attr : attributes) {
                    if (mouse.x >= 820 && mouse.x <= 1380 &&
                        mouse.y >= ry && mouse.y <= ry + 25)
                    {
                        std::string fullName = deviceName + ":" + attr.name;
                        if (rightPanelSelection.count(fullName))
                            rightPanelSelection.erase(fullName);
                        else
                            rightPanelSelection.insert(fullName);
                    }
                    ry += 25.f;
                }
                ry += 20.f;
            }
        }
    }
}


// ================== Update ==================

void SimpleWindow::update()
{
    if (!connected || !asyncManager) return;

    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() < 100)
        return;

    last = now;
    updateAttributes();
    updateAttributeValues();
}
// ================== Render ==================

void SimpleWindow::render()
{
    window.clear(background);
    drawHeader();
    drawLeftPanel();
    drawRightPanel();
    drawCenterButtons();
    window.display();
}

// ================== Header ==================

void SimpleWindow::drawHeader()
{
    window.draw(serverBox);

    if (connected)
        drawText("opc.tcp://127.0.0.1:4840", 30.f, 18.f);
    else
        drawText("Сервер не подключён", 30.f, 18.f, disabled);

    float rx = window.getSize().x - 220.f;
    drawText(currentTime(), rx, 10.f, sf::Color::Green, 18);
    drawText(currentDate(), rx, 32.f, text, 14);
}

// ================== Left Panel ==================

void SimpleWindow::drawLeftPanel()
{
    window.draw(leftPanel);
    drawText("Доступные устройства", 40.f, 80.f, text, 18);

    if (!connected || !devicesInitialized) {
        drawText("Нет подключённых устройств", 40.f, 130.f, disabled);
        return;
    }

    float y = 100.f;
    const float itemHeight = 30.f;
    
    // Мультиметр
    bool isMultimeterExpanded = std::find(expandedDevices.begin(), expandedDevices.end(), MULTIMETER) != expandedDevices.end();
    drawText(isMultimeterExpanded ? "▼ Мультиметр" : "▶ Мультиметр", 40.f, y, text, 16);
    
    if (isMultimeterExpanded) {
        float attrY = y + itemHeight;
        for (size_t i = 0; i < multimeterAttributes.size(); ++i) {
            const auto& attr = multimeterAttributes[i];
            sf::Color color = attr.isSelected ? selectedColor : text;
            drawText("  • " + attr.displayName, 60.f, attrY, color, 14);
            attrY += 30.f;
        }
    }
    y += isMultimeterExpanded ? (multimeterAttributes.size() * 30.f + itemHeight) : itemHeight;
    
    // Станок
    bool isMachineExpanded = std::find(expandedDevices.begin(), expandedDevices.end(), MACHINE) != expandedDevices.end();
    drawText(isMachineExpanded ? "▼ Станок" : "▶ Станок", 40.f, y, text, 16);
    
    if (isMachineExpanded) {
        float attrY = y + itemHeight;
        for (size_t i = 0; i < machineAttributes.size(); ++i) {
            const auto& attr = machineAttributes[i];
            sf::Color color = attr.isSelected ? selectedColor : text;
            drawText("  • " + attr.displayName, 60.f, attrY, color, 14);
            attrY += 30.f;
        }
    }
    y += isMachineExpanded ? (machineAttributes.size() * 30.f + itemHeight) : itemHeight;
    
    // Компьютер
    bool isComputerExpanded = std::find(expandedDevices.begin(), expandedDevices.end(), COMPUTER) != expandedDevices.end();
    drawText(isComputerExpanded ? "▼ Компьютер" : "▶ Компьютер", 40.f, y, text, 16);
    
    if (isComputerExpanded) {
        float attrY = y + itemHeight;
        for (size_t i = 0; i < computerAttributes.size(); ++i) {
            const auto& attr = computerAttributes[i];
            sf::Color color = attr.isSelected ? selectedColor : text;
            drawText("  • " + attr.displayName, 60.f, attrY, color, 14);
            attrY += 30.f;
        }
    }
}

// ================== Right Panel ==================

void SimpleWindow::drawRightPanel()
{
    window.draw(rightPanel);
    drawText("Мониторинг в реальном времени", 800.f, 80.f, text, 18);

    if (rightPanelData.empty()) {
        drawText("Нет выбранных атрибутов", 800.f, 140.f, disabled);
        return;
    }

    float y = 140.f;
    const float sectionSpacing = 20.f;
    
    for (const auto& [deviceName, attributes] : rightPanelData) {
        if (attributes.empty()) continue;
        
        // Заголовок устройства
        drawText("=========== " + deviceName + " ============", 
                 800.f, y, accent, 16);
        y += 25.f;
        
        // Атрибуты устройства
        for (const auto& attr : attributes) {
        std::string fullName = deviceName + ":" + attr.name;
        bool selected = rightPanelSelection.count(fullName) > 0;

        drawText(
            attr.displayName + ": " + std::to_string(attr.value),
            820.f,
            y,
            selected ? selectedColor : text,
            14
        );
        y += 25.f;
    }

        
        y += sectionSpacing;
    }
}

// ================== Center Buttons ==================

void SimpleWindow::drawCenterButtons()
{
    window.draw(moveRightBtn);
    window.draw(moveLeftBtn);
    window.draw(clearAllBtn);
    
    drawText(">> Добавить", 
             moveRightBtn.getPosition().x + 10.f, 
             moveRightBtn.getPosition().y + 10.f, 
             sf::Color::White, 12);
    
    drawText("<< Удалить", 
             moveLeftBtn.getPosition().x + 10.f, 
             moveLeftBtn.getPosition().y + 10.f, 
             sf::Color::White, 12);
    
    drawText("Очистить", 
             clearAllBtn.getPosition().x + 10.f, 
             clearAllBtn.getPosition().y + 10.f, 
             sf::Color::White, 12);
}

// ================== Helpers ==================

void SimpleWindow::drawText(const std::string& str, float x, float y,
                            sf::Color color, unsigned size)
{
    if (!fontLoaded) return;

    sf::Text t(
        font,
        sf::String::fromUtf8(str.begin(), str.end()),
        size
    );
    t.setFillColor(color);
    t.setPosition({x, y});
    window.draw(t);
}

void SimpleWindow::drawButton(sf::RectangleShape& btn,
                              const std::string& label,
                              bool selected)
{
    btn.setFillColor(selected ? accent : panel);
    window.draw(btn);
    drawText(label, btn.getPosition().x + 10, btn.getPosition().y + 10);
}

bool SimpleWindow::isMouseOver(float x, float y, float width, float height)
{
    auto m = sf::Mouse::getPosition(window);
    return m.x >= x && m.x <= x + width &&
           m.y >= y && m.y <= y + height;
}

bool SimpleWindow::isMouseOver(const sf::RectangleShape& r)
{
    auto m = sf::Mouse::getPosition(window);
    auto p = r.getPosition();
    auto s = r.getSize();
    return m.x >= p.x && m.x <= p.x + s.x &&
           m.y >= p.y && m.y <= p.y + s.y;
}

// ================== Time ==================

std::string SimpleWindow::currentTime() const
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S");
    return ss.str();
}

std::string SimpleWindow::currentDate() const
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%d.%m.%Y");
    return ss.str();
}

// ================== Обработка атрибутов ==================

void SimpleWindow::addAttributeToRightPanel(const std::string& deviceName, const Attribute& attribute)
{
    std::string fullName = deviceName + ":" + attribute.name;
    
    // Проверяем, не добавлен ли уже этот атрибут
    if (rightPanelData.find(deviceName) != rightPanelData.end()) {
        auto& attrs = rightPanelData[deviceName];
        for (const auto& attr : attrs) {
            if (attr.name == attribute.name) {
                return; // Атрибут уже добавлен
            }
        }
    }
    
    // Добавляем атрибут
    rightPanelData[deviceName].push_back(attribute);
    selectedAttributes.push_back(fullName);
}

void SimpleWindow::removeAttributeFromRightPanel(const std::string& fullName)
{
    // Парсим имя устройства и атрибута
    size_t colonPos = fullName.find(':');
    if (colonPos == std::string::npos) return;
    
    std::string deviceName = fullName.substr(0, colonPos);
    std::string attrName = fullName.substr(colonPos + 1);
    
    // Удаляем атрибут из правой панели
    if (rightPanelData.find(deviceName) != rightPanelData.end()) {
        auto& attributes = rightPanelData[deviceName];
        // Используем ручное удаление вместо std::remove_if
        auto it = attributes.begin();
        while (it != attributes.end()) {
            if (it->name == attrName) {
                it = attributes.erase(it);
            } else {
                ++it;
            }
        }
        
        // Если у устройства не осталось атрибутов, удаляем его из правой панели
        if (attributes.empty()) {
            rightPanelData.erase(deviceName);
        }
    }
}

void SimpleWindow::updateAttributeValues()
{
    // Обновляем значения в структурах атрибутов
    for (auto& attr : multimeterAttributes) {
        if (attr.name == "voltage") attr.value = multimeterData.voltage;
        else if (attr.name == "current") attr.value = multimeterData.current;
        else if (attr.name == "resistance") attr.value = multimeterData.resistance;
        else if (attr.name == "power") attr.value = multimeterData.power;
    }
    
    for (auto& attr : machineAttributes) {
        if (attr.name == "rpm") attr.value = machineData.rpm;
        else if (attr.name == "power") attr.value = machineData.power;
        else if (attr.name == "voltage") attr.value = machineData.voltage;
        else if (attr.name == "energy") attr.value = machineData.energy;
    }
    
    for (auto& attr : computerAttributes) {
        if (attr.name == "fan1") attr.value = computerData.fan1;
        else if (attr.name == "fan2") attr.value = computerData.fan2;
        else if (attr.name == "fan3") attr.value = computerData.fan3;
        else if (attr.name == "cpuLoad") attr.value = computerData.cpuLoad;
        else if (attr.name == "gpuLoad") attr.value = computerData.gpuLoad;
        else if (attr.name == "ramUsage") attr.value = computerData.ramUsage;
    }
    
    // Обновляем значения в правой панели
    for (auto& [deviceName, attributes] : rightPanelData) {
        for (auto& attr : attributes) {
            if (deviceName == "Мультиметр") {
                if (attr.name == "voltage") attr.value = multimeterData.voltage;
                else if (attr.name == "current") attr.value = multimeterData.current;
                else if (attr.name == "resistance") attr.value = multimeterData.resistance;
                else if (attr.name == "power") attr.value = multimeterData.power;
            }
            else if (deviceName == "Станок") {
                if (attr.name == "rpm") attr.value = machineData.rpm;
                else if (attr.name == "power") attr.value = machineData.power;
                else if (attr.name == "voltage") attr.value = machineData.voltage;
                else if (attr.name == "energy") attr.value = machineData.energy;
            }
            else if (deviceName == "Компьютер") {
                if (attr.name == "fan1") attr.value = computerData.fan1;
                else if (attr.name == "fan2") attr.value = computerData.fan2;
                else if (attr.name == "fan3") attr.value = computerData.fan3;
                else if (attr.name == "cpuLoad") attr.value = computerData.cpuLoad;
                else if (attr.name == "gpuLoad") attr.value = computerData.gpuLoad;
                else if (attr.name == "ramUsage") attr.value = computerData.ramUsage;
            }
        }
    }
}

// ================== OPC ==================

void SimpleWindow::connectToServer()
{
    if (connected) return;

    std::thread([this]() {
        auto newClient = std::make_shared<OPCUAClient>("opc.tcp://127.0.0.1:4840");

        if (!newClient->connect()) {
            std::cerr << "Ошибка подключения к серверу OPC UA" << std::endl;
            return;
        }

        client = newClient;
        connected = true;

        initializeDevices();

        if (!multimeter || !machine || !computer) {
            std::cerr << "Устройства не инициализированы" << std::endl;
            return;
        }

        asyncManager = std::make_shared<AsyncDataManager>(
            client.get(),
            multimeter.get(),
            machine.get(),
            computer.get(),
            100
        );

        asyncManager->start();

    }).detach();
}

void SimpleWindow::initializeDevices()
{
    if (!client || !client->isConnected())
        return;

    OPCUANode objectsNode(
        UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
        "Objects",
        "Objects Folder"
    );

    multimeter = std::make_unique<MultimeterDevice>();
    machine    = std::make_unique<MachineDevice>();
    computer   = std::make_unique<ComputerDevice>();

    bool m  = multimeter->initialize(*client, objectsNode);
    bool ma = machine->initialize(*client, objectsNode);
    bool c  = computer->initialize(*client, objectsNode);

    devicesInitialized = m || ma || c;
}

void SimpleWindow::updateAttributes() {
    if (!connected || !asyncManager) {
        multimeterData = {};
        machineData = {};
        computerData = {};
        return;
    }

    DeviceData data = asyncManager->getCurrentData();

    if (data.multimeter.valid) {
        updateMultimeterData(data.multimeter);
    } else {
        multimeterData = {};
    }

    if (data.machine.valid) {
        updateMachineData(data.machine);
    } else {
        machineData = {};
    }

    if (data.computer.valid) {
        updateComputerData(data.computer);
    } else {
        computerData = {};
    }
}

void SimpleWindow::updateMultimeterData(const DeviceData::MultimeterData& data) {
    multimeterData.voltage    = data.voltage;
    multimeterData.current    = data.current;
    multimeterData.resistance = data.resistance;
    multimeterData.power      = data.power;
}

void SimpleWindow::updateMachineData(const DeviceData::MachineData& data) {
    machineData.rpm     = data.rpm;
    machineData.power   = data.power;
    machineData.voltage = data.voltage;
    machineData.energy  = data.energy;
}

void SimpleWindow::updateComputerData(const DeviceData::ComputerData& data) {
    computerData.fan1     = data.fan1;
    computerData.fan2     = data.fan2;
    computerData.fan3     = data.fan3;
    computerData.cpuLoad  = data.cpuLoad;
    computerData.gpuLoad  = data.gpuLoad;
    computerData.ramUsage = data.ramUsage;
}