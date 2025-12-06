#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <conio.h>
#include <windows.h>

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

// Класс для работы с кодировкой (только для Windows)
class ConsoleEncoding {
public:
    static void setUTF8() {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
};

// Класс для представления узла OPC UA
class OPCUANode {
private:
    UA_NodeId nodeId;
    std::string browseName;
    std::string displayName;

public:
    OPCUANode() : nodeId(UA_NODEID_NULL) {}

    OPCUANode(const UA_NodeId& id, const std::string& bName = "", const std::string& dName = "") 
        : browseName(bName), displayName(dName) {
        UA_NodeId_copy(&id, &nodeId);
    }

    // Запрещаем копирование (правило пяти)
    OPCUANode(const OPCUANode&) = delete;
    OPCUANode& operator=(const OPCUANode&) = delete;

    OPCUANode(OPCUANode&& other) noexcept : browseName(std::move(other.browseName)), 
                                          displayName(std::move(other.displayName)) {
        UA_NodeId_copy(&other.nodeId, &nodeId);
        UA_NodeId_clear(&other.nodeId);
    }

    OPCUANode& operator=(OPCUANode&& other) noexcept {
        if (this != &other) {
            UA_NodeId_clear(&nodeId);
            UA_NodeId_copy(&other.nodeId, &nodeId);
            UA_NodeId_clear(&other.nodeId);
            browseName = std::move(other.browseName);
            displayName = std::move(other.displayName);
        }
        return *this;
    }

    ~OPCUANode() {
        UA_NodeId_clear(&nodeId);
    }

    const UA_NodeId& getId() const { return nodeId; }
    std::string getBrowseName() const { return browseName; }
    std::string getDisplayName() const { return displayName; }

    bool isValid() const { return !UA_NodeId_isNull(&nodeId); }

    void printInfo() const {
        if (isValid()) {
            std::cout << browseName << " (ID: ns=" << nodeId.namespaceIndex 
                      << "; i=" << nodeId.identifier.numeric << ")";
        }
    }
};

// Класс OPC UA клиента
class OPCUAClient {
private:
    UA_Client* client;
    std::string endpoint;

    // Вспомогательная функция для освобождения ресурсов
    void cleanup() {
        if (client) {
            UA_Client_delete(client);
            client = nullptr;
        }
    }

    // Вспомогательные функции для определения типа данных
    template<typename T>
    static const UA_DataType* getDataType();

public:
    OPCUAClient(const std::string& endpoint = "opc.tcp://127.0.0.1:4840") 
        : client(nullptr), endpoint(endpoint) {}

    // Запрещаем копирование
    OPCUAClient(const OPCUAClient&) = delete;
    OPCUAClient& operator=(const OPCUAClient&) = delete;

    OPCUAClient(OPCUAClient&& other) noexcept : client(other.client), endpoint(std::move(other.endpoint)) {
        other.client = nullptr;
    }

    OPCUAClient& operator=(OPCUAClient&& other) noexcept {
        if (this != &other) {
            cleanup();
            client = other.client;
            endpoint = std::move(other.endpoint);
            other.client = nullptr;
        }
        return *this;
    }

    ~OPCUAClient() {
        disconnect();
        cleanup();
    }

    bool connect() {
        if (client) return false;

        client = UA_Client_new();
        if (!client) return false;

        UA_ClientConfig* config = UA_Client_getConfig(client);
        UA_ClientConfig_setDefault(config);
        config->timeout = 5000;

        UA_StatusCode status = UA_Client_connect(client, endpoint.c_str());
        if (status != UA_STATUSCODE_GOOD) {
            std::cerr << "Failed to connect: " << UA_StatusCode_name(status) << std::endl;
            cleanup();
            return false;
        }

        return true;
    }

    void disconnect() {
        if (client) {
            UA_Client_disconnect(client);
        }
    }

    bool isConnected() const {
        if (!client) return false;
        
        // В новой версии open62541 UA_Client_getState имеет другую сигнатуру
        // Проверяем состояние через альтернативный метод
        UA_SecureChannelState channelState;
        UA_SessionState sessionState;
        UA_StatusCode connectStatus;
        
        UA_Client_getState(client, &channelState, &sessionState, &connectStatus);
        
        // Проверяем, что канал открыт и сессия активна
        return (channelState == UA_SECURECHANNELSTATE_OPEN && 
                sessionState == UA_SESSIONSTATE_ACTIVATED);
    }

    // Поиск узла по BrowseName в родительском узле
    OPCUANode findNodeByBrowseName(const OPCUANode& parentNode, const std::string& browseName) const {
        if (!client || !parentNode.isValid()) return OPCUANode();

        UA_BrowseRequest bReq;
        UA_BrowseRequest_init(&bReq);
        
        bReq.requestedMaxReferencesPerNode = 0;
        bReq.nodesToBrowseSize = 1;
        bReq.nodesToBrowse = (UA_BrowseDescription*)UA_Array_new(1, &UA_TYPES[UA_TYPES_BROWSEDESCRIPTION]);
        
        bReq.nodesToBrowse[0].nodeId = parentNode.getId();
        bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;
        bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
        bReq.nodesToBrowse[0].referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HIERARCHICALREFERENCES);
        bReq.nodesToBrowse[0].includeSubtypes = true;
        bReq.nodesToBrowse[0].nodeClassMask = UA_NODECLASS_VARIABLE | UA_NODECLASS_OBJECT;
        
        UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);
        
        OPCUANode result;
        
        if (bResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
            for (size_t i = 0; i < bResp.resultsSize; i++) {
                UA_BrowseResult* res = &bResp.results[i];
                if (res->statusCode == UA_STATUSCODE_GOOD) {
                    for (size_t j = 0; j < res->referencesSize; j++) {
                        UA_ReferenceDescription* ref = &res->references[j];
                        
                        if (ref->browseName.name.length > 0) {
                            std::string name((char*)ref->browseName.name.data, ref->browseName.name.length);
                            if (name == browseName) {
                                // Получаем DisplayName
                                std::string displayName = "";
                                if (ref->displayName.text.length > 0) {
                                    displayName = std::string((char*)ref->displayName.text.data, 
                                                             ref->displayName.text.length);
                                }
                                result = OPCUANode(ref->nodeId.nodeId, browseName, displayName);
                                break;
                            }
                        }
                    }
                }
                if (result.isValid()) break;
            }
        }
        
        UA_BrowseRequest_clear(&bReq);
        UA_BrowseResponse_clear(&bResp);
        
        return result;
    }

    // Чтение значения узла
    template<typename T>
    bool readValue(const OPCUANode& node, T& value) const {
        if (!client || !node.isValid()) return false;

        UA_ReadRequest rReq;
        UA_ReadRequest_init(&rReq);
        rReq.nodesToReadSize = 1;
        rReq.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
        UA_ReadValueId_init(&rReq.nodesToRead[0]);
        rReq.nodesToRead[0].nodeId = node.getId();
        rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_VALUE;
        
        UA_ReadResponse rResp = UA_Client_Service_read(client, rReq);
        
        bool success = false;
        
        const UA_DataType* dataType = getDataType<T>();
        if (dataType && 
            rResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
            rResp.resultsSize > 0 &&
            rResp.results[0].hasValue &&
            UA_Variant_hasScalarType(&rResp.results[0].value, dataType)) {
            
            value = *(T*)rResp.results[0].value.data;
            success = true;
        }
        
        UA_ReadRequest_clear(&rReq);
        UA_ReadResponse_clear(&rResp);
        
        return success;
    }

    // Чтение строкового значения (DisplayName)
    bool readDisplayName(const OPCUANode& node, std::string& displayName) const {
        if (!client || !node.isValid()) return false;

        UA_ReadRequest rReq;
        UA_ReadRequest_init(&rReq);
        rReq.nodesToReadSize = 1;
        rReq.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
        UA_ReadValueId_init(&rReq.nodesToRead[0]);
        rReq.nodesToRead[0].nodeId = node.getId();
        rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DISPLAYNAME;
        
        UA_ReadResponse rResp = UA_Client_Service_read(client, rReq);
        
        bool success = false;
        
        if (rResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
            rResp.resultsSize > 0 &&
            rResp.results[0].hasValue &&
            UA_Variant_hasScalarType(&rResp.results[0].value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
            
            UA_LocalizedText* lt = (UA_LocalizedText*)rResp.results[0].value.data;
            displayName = std::string((char*)lt->text.data, lt->text.length);
            success = true;
        }
        
        UA_ReadRequest_clear(&rReq);
        UA_ReadResponse_clear(&rResp);
        
        return success;
    }

    // Запись значения
    template<typename T>
    bool writeValue(const OPCUANode& node, const T& value) {
        if (!client || !node.isValid()) return false;

        UA_WriteRequest wReq;
        UA_WriteRequest_init(&wReq);
        
        wReq.nodesToWriteSize = 1;
        wReq.nodesToWrite = (UA_WriteValue*)UA_Array_new(1, &UA_TYPES[UA_TYPES_WRITEVALUE]);
        UA_WriteValue_init(&wReq.nodesToWrite[0]);
        
        wReq.nodesToWrite[0].nodeId = node.getId();
        wReq.nodesToWrite[0].attributeId = UA_ATTRIBUTEID_VALUE;
        wReq.nodesToWrite[0].value.hasValue = true;
        
        const UA_DataType* dataType = getDataType<T>();
        if (!dataType) return false;
        
        UA_Variant_setScalarCopy(&wReq.nodesToWrite[0].value.value, &value, dataType);
        
        UA_WriteResponse wResp = UA_Client_Service_write(client, wReq);
        
        bool success = (wResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
                       wResp.resultsSize > 0 &&
                       wResp.results[0] == UA_STATUSCODE_GOOD);
        
        UA_WriteRequest_clear(&wReq);
        UA_WriteResponse_clear(&wResp);
        
        return success;
    }
};

// Специализации для getDataType (выносим из класса)
template<>
const UA_DataType* OPCUAClient::getDataType<double>() { return &UA_TYPES[UA_TYPES_DOUBLE]; }

template<>
const UA_DataType* OPCUAClient::getDataType<int>() { return &UA_TYPES[UA_TYPES_INT32]; }

template<>
const UA_DataType* OPCUAClient::getDataType<float>() { return &UA_TYPES[UA_TYPES_FLOAT]; }

// Класс для управления устройством "Мультиметр"
class MultimeterDevice {
private:
    OPCUANode deviceNode;
    OPCUANode voltageNode;
    OPCUANode currentNode;

public:
    MultimeterDevice() {}

    bool initialize(OPCUAClient& client, const OPCUANode& parentNode) {
        // Ищем устройство
        deviceNode = client.findNodeByBrowseName(parentNode, "Multimeter");
        if (!deviceNode.isValid()) {
            return false;
        }

        // Ищем переменные внутри устройства
        voltageNode = client.findNodeByBrowseName(deviceNode, "Voltage");
        currentNode = client.findNodeByBrowseName(deviceNode, "Current");

        return true;
    }

    bool readValues(OPCUAClient& client, double& voltage, double& current) const {
        bool voltageOk = client.readValue(voltageNode, voltage);
        bool currentOk = client.readValue(currentNode, current);
        
        return voltageOk || currentOk; // Возвращаем true, если хотя бы одно значение прочитано
    }

    void printStatus() const {
        std::cout << "Мультиметр: ";
        if (voltageNode.isValid()) std::cout << "Напряжение доступно, ";
        if (currentNode.isValid()) std::cout << "Ток доступен";
        std::cout << std::endl;
    }

    const OPCUANode& getVoltageNode() const { return voltageNode; }
    const OPCUANode& getCurrentNode() const { return currentNode; }
    const OPCUANode& getDeviceNode() const { return deviceNode; }
};

// Класс для управления RPM (обороты маховика)
class FlywheelRPM {
private:
    OPCUANode rpmNode;

public:
    bool initialize(OPCUAClient& client, const OPCUANode& parentNode) {
        rpmNode = client.findNodeByBrowseName(parentNode, "FlywheelRPM");
        return rpmNode.isValid();
    }

    bool readValue(OPCUAClient& client, double& rpm) const {
        return client.readValue(rpmNode, rpm);
    }

    bool setValue(OPCUAClient& client, double rpm) {
        return client.writeValue(rpmNode, rpm);
    }

    const OPCUANode& getNode() const { return rpmNode; }
};

// Основной класс приложения
class OPCUAApplication {
private:
    OPCUAClient client;
    MultimeterDevice multimeter;
    FlywheelRPM flywheelRPM;
    
    OPCUANode objectsFolder;
    
    bool nodesFound;

public:
    OPCUAApplication(const std::string& endpoint = "opc.tcp://127.0.0.1:4840")
        : client(endpoint), nodesFound(false) {
        
        objectsFolder = OPCUANode(UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), "Objects", "Objects Folder");
    }

    bool initialize() {
        ConsoleEncoding::setUTF8();
        SignalHandler::initialize();

        printWelcome();

        if (!client.connect()) {
            return false;
        }

        std::cout << "Connected to server" << std::endl;
        std::cout << "Looking for nodes..." << std::endl;

        // Инициализация устройств
        bool multimeterFound = multimeter.initialize(client, objectsFolder);
        bool rpmFound = flywheelRPM.initialize(client, objectsFolder);

        if (multimeterFound) {
            multimeter.printStatus();
        }

        if (rpmFound) {
            std::cout << "Flywheel RPM found" << std::endl;
        }

        nodesFound = multimeterFound || rpmFound;

        if (!nodesFound) {
            std::cerr << "\nERROR: Could not find any nodes." << std::endl;
            return false;
        }

        return true;
    }

    void run() {
        std::cout << "\n\nStarting to read values..." << std::endl;
        std::cout << "Controls: Press 'r' to set new RPM value, 'q' to quit\n" << std::endl;

        while (SignalHandler::isRunning()) {
            handleInput();
            readAndDisplayValues();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        shutdown();
    }

private:
    void printWelcome() const {
        std::cout << "OPC UA Client starting..." << std::endl;
        std::cout << "Controls:" << std::endl;
        std::cout << "  - 'q' to quit" << std::endl;
        std::cout << "  - 'r' to set new RPM value" << std::endl;
        std::cout << std::endl;
    }

    void handleInput() {
        if (_kbhit()) {
            char c = _getch();
            
            switch (c) {
                case 'q':
                case 'Q':
                    std::cout << "\nQuitting..." << std::endl;
                    SignalHandler::stop();
                    break;
                    
                case 'r':
                case 'R':
                    handleRPMInput();
                    break;
            }
        }
    }

    void handleRPMInput() {
        if (!flywheelRPM.getNode().isValid()) {
            std::cerr << "\nRPM node not found, cannot set value." << std::endl;
            return;
        }

        std::cout << "\nEnter new RPM value: ";
        std::string input;
        std::getline(std::cin, input);

        try {
            double newRpm = std::stod(input);
            
            if (flywheelRPM.setValue(client, newRpm)) {
                std::cout << "Successfully set RPM to: " << newRpm << std::endl;
            } else {
                std::cerr << "Failed to write RPM value" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Invalid input: " << e.what() << std::endl;
        }
    }

    void readAndDisplayValues() {
        std::cout << "\r";

        double voltage = 0.0, current = 0.0, rpm = 0.0;
        bool hasVoltage = false, hasCurrent = false, hasRPM = false;

        // Чтение значений мультиметра
        if (multimeter.getDeviceNode().isValid()) {
            hasVoltage = client.readValue(multimeter.getVoltageNode(), voltage);
            hasCurrent = client.readValue(multimeter.getCurrentNode(), current);
        }

        // Чтение RPM
        if (flywheelRPM.getNode().isValid()) {
            hasRPM = flywheelRPM.readValue(client, rpm);
        }

        // Вывод значений
        if (multimeter.getDeviceNode().isValid()) {
            std::cout << "Мультиметр: ";
        }

        if (hasVoltage) {
            std::cout << "Напряжение: " << std::fixed << std::setprecision(2) << voltage << " V, ";
        }

        if (hasCurrent) {
            std::cout << "Ток: " << std::fixed << std::setprecision(2) << current << " A, ";
        }

        if (hasRPM) {
            std::cout << "Обороты маховика: " << std::fixed << std::setprecision(2) << rpm << " RPM      ";
        } else if (flywheelRPM.getNode().isValid()) {
            std::cout << "Обороты маховика: N/A      ";
        }

        if (!hasVoltage && !hasCurrent && !hasRPM) {
            std::cout << "Waiting for data...      ";
        }

        std::cout << std::flush;
    }

    void shutdown() {
        std::cout << "\nDisconnecting..." << std::endl;
        client.disconnect();
        std::cout << "Client stopped." << std::endl;
    }
};

int main() {
    OPCUAApplication app;
    
    if (!app.initialize()) {
        return 1;
    }
    
    app.run();
    
    return 0;
}