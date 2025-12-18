#include "simple_window.h"
#include <SFML/Window/Event.hpp>
#include <thread>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <iostream>
#include "async_manager.h"

// ================== Constructor ==================

SimpleWindow::SimpleWindow()
    : window(sf::VideoMode({1200, 800}), "OPC UA Client Monitor",
             sf::Style::Titlebar | sf::Style::Close)
{
    window.setVerticalSyncEnabled(true);

    // UI layout
    serverBox.setSize({420.f, 36.f});
    serverBox.setPosition({20.f, 10.f});
    serverBox.setFillColor(panel);

    devicePanel.setSize({240.f, 600.f});
    devicePanel.setPosition({20.f, 60.f});
    devicePanel.setFillColor(panel);

    attrPanel.setSize({880.f, 600.f});
    attrPanel.setPosition({300.f, 60.f});
    attrPanel.setFillColor(panel);

    multimeterBtn.setSize({200.f, 40.f});
    multimeterBtn.setPosition({40.f, 120.f});

    machineBtn = multimeterBtn;
    machineBtn.setPosition({40.f, 170.f});

    computerBtn = multimeterBtn;
    computerBtn.setPosition({40.f, 220.f});
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
        if (e->is<sf::Event::Closed>())
            window.close();

        if (auto* m = e->getIf<sf::Event::MouseButtonPressed>()) {
            if (m->button == sf::Mouse::Button::Left) {
                if (!connected && isMouseOver(serverBox))
                    connectToServer();

                if (isMouseOver(multimeterBtn)) selectedDevice = MULTIMETER;
                if (isMouseOver(machineBtn))    selectedDevice = MACHINE;
                if (isMouseOver(computerBtn))  selectedDevice = COMPUTER;
            }
        }
    }
}

// ================== Update ==================

void SimpleWindow::update()
{
    if (!connected || !asyncManager)
        return;

    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() < 100)
        return;

    last = now;

    updateAttributes();  // ← единственная точка обновления данных
}

// ================== Render ==================

void SimpleWindow::render()
{
    window.clear(background);
    drawHeader();
    drawDevicePanel();
    drawAttributesPanel();
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

// ================== Device panel ==================

void SimpleWindow::drawDevicePanel()
{
    window.draw(devicePanel);
    drawText("Устройства", 40.f, 80.f, text, 18);

    // ⛔ Нет подключения — нет устройств
    if (!connected || !devicesInitialized) {
        drawText("Нет подключённых устройств",
                 40.f, 130.f,
                 disabled);
        return;
    }

    // ✅ Сервер подключён — показываем устройства
    drawButton(multimeterBtn,
               "Мультиметр",
               selectedDevice == MULTIMETER);

    drawButton(machineBtn,
               "Станок",
               selectedDevice == MACHINE);

    drawButton(computerBtn,
               "Компьютер",
               selectedDevice == COMPUTER);
}

// ================== Attributes ==================

void SimpleWindow::drawAttributesPanel()
{
    window.draw(attrPanel);
    drawText("Атрибуты устройства", 320.f, 80.f, text, 18);

    // Нет подключения или не выбрано устройство
    if (!connected || !devicesInitialized || selectedDevice == NONE) {
        drawText("Выберите устройство для просмотра атрибутов",
                 340.f, 140.f,
                 disabled);
        return;
    }

    float y = 140.f;
    const float dy = 30.f;

    // Лямбда для аккуратной строки "название — значение"
    auto row = [&](const std::string& name, const std::string& value) {
        drawText(name, 320.f, y, text);
        drawText(value, 540.f, y, accent);
        y += dy;
    };

    // ===== МУЛЬТИМЕТР =====
    if (selectedDevice == MULTIMETER) {
        drawText("Мультиметр", 320.f, y, accent, 16);
        y += dy;

        row("Напряжение:",
            std::to_string(multimeterData.voltage) + " В");

        row("Сила тока:",
            std::to_string(multimeterData.current) + " А");

        row("Сопротивление:",
            std::to_string(multimeterData.resistance) + " Ом");

        row("Мощность:",
            std::to_string(multimeterData.power) + " Вт");
    }

    // ===== СТАНОК =====
    if (selectedDevice == MACHINE) {
        drawText("Станок", 320.f, y, accent, 16);
        y += dy;

        row("Обороты:",
            std::to_string(machineData.rpm) + " об/мин");

        row("Мощность:",
            std::to_string(machineData.power) + " кВт");

        row("Напряжение:",
            std::to_string(machineData.voltage) + " В");

        row("Энергопотребление:",
            std::to_string(machineData.energy) + " кВт·ч");
    }

    // ===== КОМПЬЮТЕР =====
    if (selectedDevice == COMPUTER) {
        drawText("Компьютер", 320.f, y, accent, 16);
        y += dy;

        row("Вентилятор 1:",
            std::to_string(computerData.fan1) + " об/мин");

        row("Вентилятор 2:",
            std::to_string(computerData.fan2) + " об/мин");

        row("Вентилятор 3:",
            std::to_string(computerData.fan3) + " об/мин");

        row("Загрузка CPU:",
            std::to_string(computerData.cpuLoad) + " %");

        row("Загрузка GPU:",
            std::to_string(computerData.gpuLoad) + " %");

        row("Использование RAM:",
            std::to_string(computerData.ramUsage) + " %");
    }
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

// ================== OPC ==================

void SimpleWindow::connectToServer()
{
    // Защита от повторного запуска
    if (connected) return;

    std::thread([this]() {
        auto newClient = std::make_shared<OPCUAClient>("opc.tcp://127.0.0.1:4840");

        if (!newClient->connect()) {
            std::cerr << "Ошибка подключения к серверу OPC UA" << std::endl;
            return;
        }

        // ⬇⬇⬇ ВАЖНО: возвращаемся в главный поток ЛОГИЧЕСКИ
        // (физически мы всё ещё в потоке, но UI-трогаем аккуратно)

        client = newClient;
        connected = true;

        // 🔹 Инициализация устройств
        initializeDevices();

        // 🔹 Проверка: устройства реально созданы
        if (!multimeter || !machine || !computer) {
            std::cerr << "Устройства не инициализированы" << std::endl;
            return;
        }

        // 🔹 Запуск асинхронного чтения
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

    // 📌 Корневой узел Objects (ns=0;i=85)
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
    // Если не подключены — сбрасываем всё и выходим
    if (!connected || !asyncManager) {
        multimeterData = {};
        machineData = {};
        computerData = {};
        return;
    }

    // Забираем актуальные данные из асинхронного менеджера
    DeviceData data = asyncManager->getCurrentData();

    // ===== МУЛЬТИМЕТР =====
    if (data.multimeter.valid) {
        updateMultimeterData(data.multimeter);
    } else {
        multimeterData = {}; // сброс, если данные невалидны
    }

    // ===== СТАНОК =====
    if (data.machine.valid) {
        updateMachineData(data.machine);
    } else {
        machineData = {};
    }

    // ===== КОМПЬЮТЕР =====
    if (data.computer.valid) {
        updateComputerData(data.computer);
    } else {
        computerData = {};
    }
}

void updateMultimeterData(const DeviceData::MultimeterData&);
void updateMachineData(const DeviceData::MachineData&);
void updateComputerData(const DeviceData::ComputerData&);

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

