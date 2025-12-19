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
constexpr unsigned ATTR_FONT_SIZE = 20;
constexpr float ATTR_LINE_HEIGHT = 38.f;
constexpr float LEFT_PANEL_START_Y = 130.f;
constexpr float DEVICE_ITEM_HEIGHT = 30.f;

const sf::Color DISCONNECT_ACTIVE   = sf::Color(160, 60, 60);
const sf::Color DISCONNECT_DISABLED = sf::Color(120, 120, 120);

constexpr float RP_X        = 750.f;
constexpr float RP_WIDTH    = 410.f;
constexpr float ROW_H    = 28.f;
constexpr float NAME_COL_W  = 230.f;
constexpr float VALUE_COL_W = 120.f;

static std::string formatValue(double v)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << v;
    return ss.str();
}

sf::String clampTextUtf8(const std::string& s, std::size_t maxChars)
{
    sf::String str = sf::String::fromUtf8(s.begin(), s.end());
    if (str.getSize() <= maxChars)
        return str;

    sf::String result;
    for (std::size_t i = 0; i < maxChars - 3; ++i)
        result += str[i];

    result += "...";
    return result;
}

struct RightPanelAttribute {
    std::string name;
    std::string displayName;
    double value;
};

// ================== Constructor ==================

SimpleWindow::SimpleWindow()
    : window(sf::VideoMode({1200, 800}), "OPC UA Client Monitor",
             sf::Style::Titlebar | sf::Style::Close)
{
    window.setVerticalSyncEnabled(true);

    serverBox.setSize({
    window.getSize().x - 330.f, 48.f});
    serverBox.setPosition({20.f, 10.f});
    serverBox.setFillColor(panel);

    // Кнопка "Отключение"
    disconnectBtn.setSize({170.f, 48.f});
    disconnectBtn.setPosition({serverBox.getPosition().x + serverBox.getSize().x + 20.f, serverBox.getPosition().y});
    disconnectBtn.setFillColor(DISCONNECT_DISABLED);


    leftPanel.setSize({450.f, window.getSize().y - 120.f});
    leftPanel.setPosition({20.f, 75.f});
    leftPanel.setFillColor(panel);

    rightPanel.setSize({450.f, window.getSize().y - 120.f});
    rightPanel.setPosition({730.f, 75.f});
    rightPanel.setFillColor(panel);

    moveRightBtn.setSize({180.f, 60.f});
    moveRightBtn.setPosition(sf::Vector2f(static_cast<float>(window.getSize().x) / 2.f - 90.f, window.getSize().y / 2.f - 110.f));
    moveRightBtn.setFillColor(accent);

    moveLeftBtn.setSize({180.f, 60.f});
    moveLeftBtn.setPosition(sf::Vector2f(static_cast<float>(window.getSize().x) / 2.f - 90.f, window.getSize().y / 2.f - 30.f));
    moveLeftBtn.setFillColor(accent);

    clearAllBtn.setSize({180.f, 60.f});
    clearAllBtn.setPosition(sf::Vector2f(static_cast<float>(window.getSize().x) / 2.f - 90.f, window.getSize().y / 2.f + 50.f));
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
    fontLoaded = font.openFromFile("res/fonts/DejaVuSans.ttf");
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

            // ===== Отключение от сервера =====
            if (connected && isMouseOver(disconnectBtn)) {
                if (asyncManager) {
                    asyncManager->stop();
                    asyncManager.reset();
                }

                if (client) {
                    client->disconnect();
                    client.reset();
                }

                connected = false;
                devicesInitialized = false;

                rightPanelData.clear();
                rightPanelSelection.clear();
                expandedDevices.clear();

                for (auto& a : multimeterAttributes) a.isSelected = false;
                for (auto& a : machineAttributes) a.isSelected = false;
                for (auto& a : computerAttributes) a.isSelected = false;

                multimeterData = {};
                machineData = {};
                computerData = {};

                return;
            }


            // ===== Центр кнопки =====
            if (isMouseOver(moveRightBtn)) {
                for (const auto& a : multimeterAttributes)
                    if (a.isSelected) addAttributeToRightPanel("Мультиметр", a);
                for (const auto& a : machineAttributes)
                    if (a.isSelected) addAttributeToRightPanel("Станок", a);
                for (const auto& a : computerAttributes)
                    if (a.isSelected) addAttributeToRightPanel("Компьютер", a);
                return;
            }

            if (isMouseOver(moveLeftBtn)) {
                for (const auto& fullName : rightPanelSelection) {
                    removeAttributeFromRightPanel(fullName);
                }
                rightPanelSelection.clear();
                return;
            }

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

            // ===== Левая панель =====
            if (mouse.x >= leftPanel.getPosition().x && mouse.x <= leftPanel.getPosition().x + leftPanel.getSize().x) {
                float y = LEFT_PANEL_START_Y;
                const float itemH = DEVICE_ITEM_HEIGHT;

                auto toggleDevice = [&](DeviceType d) {
                    auto it = std::find(expandedDevices.begin(), expandedDevices.end(), d);
                    if (it != expandedDevices.end())
                        expandedDevices.erase(it);
                    else
                        expandedDevices.push_back(d);
                };

                auto deviceHit = [&](float yy) {
                    return mouse.y >= yy && mouse.y <= yy + itemH;
                };

                // --- Мультиметр ---
                if (deviceHit(y)) toggleDevice(MULTIMETER);
                bool mExp = std::find(expandedDevices.begin(), expandedDevices.end(), MULTIMETER) != expandedDevices.end();
                y += itemH;

                if (mExp) {
                    for (auto& a : multimeterAttributes) {
                        if (mouse.y >= y && mouse.y <= y + ATTR_LINE_HEIGHT)
                            a.isSelected = !a.isSelected;
                        y += ATTR_LINE_HEIGHT;
                    }
                }

                // --- Станок ---
                if (deviceHit(y)) toggleDevice(MACHINE);
                bool maExp = std::find(expandedDevices.begin(), expandedDevices.end(), MACHINE) != expandedDevices.end();
                y += itemH;

                if (maExp) {
                    for (auto& a : machineAttributes) {
                        if (mouse.y >= y && mouse.y <= y + ATTR_LINE_HEIGHT)
                            a.isSelected = !a.isSelected;
                        y += ATTR_LINE_HEIGHT;
                    }
                }

                // --- Компьютер ---
                if (deviceHit(y)) toggleDevice(COMPUTER);
                bool cExp = std::find(expandedDevices.begin(), expandedDevices.end(), COMPUTER) != expandedDevices.end();
                y += itemH;

                if (cExp) {
                    for (auto& a : computerAttributes) {
                        if (mouse.y >= y && mouse.y <= y + ATTR_LINE_HEIGHT)
                            a.isSelected = !a.isSelected;
                        y += ATTR_LINE_HEIGHT;
                    }
                }
            }

            // ===== Правая панель =====
            float ry = 130.f;

            for (auto& [deviceName, attributes] : rightPanelData) {
                if (attributes.empty()) continue;

                ry += 38.f; // высота заголовка устройства

                for (auto& attr : attributes) {
                    if (mouse.x >= RP_X && mouse.x <= RP_X + RP_WIDTH &&
                        mouse.y >= ry && mouse.y <= ry + ROW_H)
                    {
                        std::string fullName = deviceName + ":" + attr.name;
                        if (rightPanelSelection.count(fullName))
                            rightPanelSelection.erase(fullName);
                        else
                            rightPanelSelection.insert(fullName);
                    }
                    ry += ROW_H; // точно как в drawRightPanel()
                }
                ry += 18.f; // отступ между устройствами
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
    std::string footer = "© Попов Вадим, Романюк Артём. OPC UA клиент. Москва, 2025.";
    float footerX = (window.getSize().x / 2.f) - 300.f;
    float footerY = window.getSize().y - 28.f;
    drawText(footer, footerX, footerY, disabled, 18);
    window.display();
}

// ================== Header ==================

void SimpleWindow::drawHeader()
{
    window.draw(serverBox);

    // ===== Кнопка "Отключение" =====
    disconnectBtn.setFillColor(
        connected ? DISCONNECT_ACTIVE : DISCONNECT_DISABLED
    );
    window.draw(disconnectBtn);

    drawText(
        "Отключение",
        disconnectBtn.getPosition().x + 14.f,
        disconnectBtn.getPosition().y + 12.f,
        connected ? sf::Color::White : disabled,
        22
    );

    // ===== Статус сервера =====
    if (connected)
        drawText("● opc.tcp://127.0.0.1:4840", 30.f, 18.f, sf::Color::White, 26);
    else
        drawText("✖ Сервер не подключён", 30.f, 18.f, disabled, 26);

    float padding = 15.f;
    float rightX = window.getSize().x - 120.f;

    drawText(currentTime(), rightX + 15.f, padding, sf::Color::Green, 19);
    drawText(currentDate(), rightX + 15.f, padding + 22.f, text, 14);
}

// ================== Left Panel ==================

void SimpleWindow::drawLeftPanel()
{
    window.draw(leftPanel);
    drawText("Доступные устройства", 60.f, 80.f, text, 30);

    if (!connected || !devicesInitialized) {
        drawText("Нет подключённых устройств", 60.f, 420.f, disabled, 22);
        return;
    }

    float y = LEFT_PANEL_START_Y;
    const float itemHeight = DEVICE_ITEM_HEIGHT;
    
    // Мультиметр
    bool isMultimeterExpanded = std::find(expandedDevices.begin(), expandedDevices.end(), MULTIMETER) != expandedDevices.end();
    drawText(isMultimeterExpanded ? "▼ Мультиметр" : "▶ Мультиметр", 40.f, y, text, 22);
    
    if (isMultimeterExpanded) {
        float attrY = y + itemHeight;
        for (size_t i = 0; i < multimeterAttributes.size(); ++i) {
            const auto& attr = multimeterAttributes[i];
            sf::Color color = attr.isSelected ? selectedColor : text;
            drawText("  • " + attr.displayName, 60.f, attrY, color, ATTR_FONT_SIZE);
            attrY += ATTR_LINE_HEIGHT;
        }
    }
    y += isMultimeterExpanded ? (multimeterAttributes.size() * ATTR_LINE_HEIGHT + itemHeight) : itemHeight;
    
    // Станок
    bool isMachineExpanded = std::find(expandedDevices.begin(), expandedDevices.end(), MACHINE) != expandedDevices.end();
    drawText(isMachineExpanded ? "▼ Станок" : "▶ Станок", 40.f, y, text, 22);
    
    if (isMachineExpanded) {
        float attrY = y + itemHeight;
        for (size_t i = 0; i < machineAttributes.size(); ++i) {
            const auto& attr = machineAttributes[i];
            sf::Color color = attr.isSelected ? selectedColor : text;
            drawText("  • " + attr.displayName, 60.f, attrY, color, ATTR_FONT_SIZE);
            attrY += ATTR_LINE_HEIGHT;
        }
    }
    y += isMachineExpanded ? (machineAttributes.size() * ATTR_LINE_HEIGHT + itemHeight) : itemHeight;
    
    // Компьютер
    bool isComputerExpanded = std::find(expandedDevices.begin(), expandedDevices.end(), COMPUTER) != expandedDevices.end();
    drawText(isComputerExpanded ? "▼ Компьютер" : "▶ Компьютер", 40.f, y, text, 22);
    
    if (isComputerExpanded) {
        float attrY = y + itemHeight;
        for (size_t i = 0; i < computerAttributes.size(); ++i) {
            const auto& attr = computerAttributes[i];
            sf::Color color = attr.isSelected ? selectedColor : text;
            drawText("  • " + attr.displayName, 60.f, attrY, color, ATTR_FONT_SIZE);
            attrY += ATTR_LINE_HEIGHT;
        }
    }
}

// ================== Right Panel ==================

void SimpleWindow::drawRightPanel()
{
    window.draw(rightPanel);
    drawText("Мониторинг параметров", RP_X + 10.f, 80.f, text, 30);

    if (rightPanelData.empty()) {
        drawText("Нет выбранных параметров", RP_X + 40.f, 420.f, disabled, 22);
        return;
    }

    float y = 130.f; // стартовая вертикальная позиция

    for (const auto& [deviceName, attributes] : rightPanelData) {
        if (attributes.empty()) continue;

        // ===== Заголовок устройства =====
        sf::RectangleShape header({RP_WIDTH, 34.f});
        header.setPosition(sf::Vector2f(RP_X + 10.f, y));
        header.setFillColor(sf::Color(55, 60, 70));
        window.draw(header);

        drawText(deviceName, RP_X + 20.f, y + 6.f, sf::Color::White, 18);
        y += 38.f;

        // ===== Атрибуты устройства =====
        for (const auto& attr : attributes) {
            std::string fullName = deviceName + ":" + attr.name;
            bool selected = rightPanelSelection.count(fullName);

            if (selected) {
                sf::RectangleShape bg({RP_WIDTH, ROW_H});
                bg.setPosition(sf::Vector2f(RP_X + 10.f, y));
                bg.setFillColor(sf::Color(70, 90, 120));
                window.draw(bg);
            }

            sf::Text t(font, clampTextUtf8(attr.displayName, 32), 15);
            t.setFillColor(selected ? sf::Color::White : text);
            t.setPosition(sf::Vector2f(RP_X + 20.f, y + 4.f));
            window.draw(t);

            // Значение атрибута
            drawText(
                formatValue(attr.value),
                RP_X + 20.f + NAME_COL_W,
                y + 4.f,
                selected ? sf::Color::White : accent,
                15
            );

            y += ROW_H; // шаг вниз на одну строку
        }

        y += 18.f; // отступ между устройствами
    }
}


// ================== Center Buttons ==================

void SimpleWindow::drawCenterButtons()
{
    window.draw(moveRightBtn);
    window.draw(moveLeftBtn);
    window.draw(clearAllBtn);
    
    drawText("Добавить >>", 
             moveRightBtn.getPosition().x + 10.f, 
             moveRightBtn.getPosition().y + 15.f, 
             sf::Color::White, 22);
    
    drawText("<< Удалить", 
             moveLeftBtn.getPosition().x + 17.f, 
             moveLeftBtn.getPosition().y + 15.f, 
             sf::Color::White, 22);
    
    drawText("Очистить", 
             clearAllBtn.getPosition().x + 33.f, 
             clearAllBtn.getPosition().y + 15.f, 
             sf::Color::White, 22);
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

    auto& attrs = rightPanelData[deviceName];
    for (const auto& a : attrs)
        if (a.name == attribute.name) return; // Уже есть

    RightPanelAttribute rpAttr;
    rpAttr.name = attribute.name;
    rpAttr.displayName = attribute.displayName;
    rpAttr.value = attribute.value;

    attrs.push_back(rpAttr);

    // Синхронизируем с левой панелью
    std::vector<Attribute>* leftAttrs = nullptr;
    if (deviceName == "Мультиметр") leftAttrs = &multimeterAttributes;
    else if (deviceName == "Станок") leftAttrs = &machineAttributes;
    else if (deviceName == "Компьютер") leftAttrs = &computerAttributes;

    if (leftAttrs) {
        for (auto& a : *leftAttrs)
            if (a.name == attribute.name)
                a.isSelected = true;
    }
}

void SimpleWindow::removeAttributeFromRightPanel(const std::string& fullName)
{
    size_t colonPos = fullName.find(':');
    if (colonPos == std::string::npos) return;
    
    std::string deviceName = fullName.substr(0, colonPos);
    std::string attrName = fullName.substr(colonPos + 1);

    if (rightPanelData.find(deviceName) != rightPanelData.end()) {
        auto& attributes = rightPanelData[deviceName];
        auto it = attributes.begin();
        while (it != attributes.end()) {
            if (it->name == attrName)
                it = attributes.erase(it);
            else
                ++it;
        }
        if (attributes.empty())
            rightPanelData.erase(deviceName);
    }

    // Сброс выбора в левой панели
    std::vector<Attribute>* leftAttrs = nullptr;
    if (deviceName == "Мультиметр") leftAttrs = &multimeterAttributes;
    else if (deviceName == "Станок") leftAttrs = &machineAttributes;
    else if (deviceName == "Компьютер") leftAttrs = &computerAttributes;

    if (leftAttrs) {
        for (auto& a : *leftAttrs)
            if (a.name == attrName)
                a.isSelected = false;
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