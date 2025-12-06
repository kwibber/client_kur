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

    // Конструктор копирования
    OPCUANode(const OPCUANode& other) 
        : browseName(other.browseName), displayName(other.displayName) {
        UA_NodeId_copy(&other.nodeId, &nodeId);
    }

    // Оператор присваивания копированием
    OPCUANode& operator=(const OPCUANode& other) {
        if (this != &other) {
            UA_NodeId_clear(&nodeId);
            UA_NodeId_copy(&other.nodeId, &nodeId);
            browseName = other.browseName;
            displayName = other.displayName;
        }
        return *this;
    }

    // Конструктор перемещения
    OPCUANode(OPCUANode&& other) noexcept 
        : browseName(std::move(other.browseName)), 
          displayName(std::move(other.displayName)) {
        UA_NodeId_copy(&other.nodeId, &nodeId);
        UA_NodeId_clear(&other.nodeId);
    }

    // Оператор присваивания перемещением
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

    // Поиск всех компонентов устройства
    std::vector<OPCUANode> findDeviceComponents(const OPCUANode& deviceNode) const {
        std::vector<OPCUANode> components;
        if (!client || !deviceNode.isValid()) return components;

        UA_BrowseRequest bReq;
        UA_BrowseRequest_init(&bReq);
        
        bReq.requestedMaxReferencesPerNode = 0;
        bReq.nodesToBrowseSize = 1;
        bReq.nodesToBrowse = (UA_BrowseDescription*)UA_Array_new(1, &UA_TYPES[UA_TYPES_BROWSEDESCRIPTION]);
        
        bReq.nodesToBrowse[0].nodeId = deviceNode.getId();
        bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;
        bReq.nodesToBrowse[0].browseDirection = UA_BROWSEDIRECTION_FORWARD;
        bReq.nodesToBrowse[0].referenceTypeId = UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT);
        bReq.nodesToBrowse[0].includeSubtypes = true;
        bReq.nodesToBrowse[0].nodeClassMask = UA_NODECLASS_VARIABLE;
        
        UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);
        
        if (bResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
            for (size_t i = 0; i < bResp.resultsSize; i++) {
                UA_BrowseResult* res = &bResp.results[i];
                if (res->statusCode == UA_STATUSCODE_GOOD) {
                    for (size_t j = 0; j < res->referencesSize; j++) {
                        UA_ReferenceDescription* ref = &res->references[j];
                        
                        if (ref->browseName.name.length > 0) {
                            std::string browseName((char*)ref->browseName.name.data, ref->browseName.name.length);
                            std::string displayName = "";
                            if (ref->displayName.text.length > 0) {
                                displayName = std::string((char*)ref->displayName.text.data, 
                                                         ref->displayName.text.length);
                            }
                            components.push_back(OPCUANode(ref->nodeId.nodeId, browseName, displayName));
                        }
                    }
                }
            }
        }
        
        UA_BrowseRequest_clear(&bReq);
        UA_BrowseResponse_clear(&bResp);
        
        return components;
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

    // Чтение нескольких значений за один запрос
    std::vector<std::pair<bool, double>> readMultipleValues(const std::vector<OPCUANode>& nodes) const {
        std::vector<std::pair<bool, double>> results;
        if (!client || nodes.empty()) return results;

        UA_ReadRequest rReq;
        UA_ReadRequest_init(&rReq);
        rReq.nodesToReadSize = nodes.size();
        rReq.nodesToRead = (UA_ReadValueId*)UA_Array_new(nodes.size(), &UA_TYPES[UA_TYPES_READVALUEID]);
        
        for (size_t i = 0; i < nodes.size(); i++) {
            UA_ReadValueId_init(&rReq.nodesToRead[i]);
            rReq.nodesToRead[i].nodeId = nodes[i].getId();
            rReq.nodesToRead[i].attributeId = UA_ATTRIBUTEID_VALUE;
        }
        
        UA_ReadResponse rResp = UA_Client_Service_read(client, rReq);
        
        if (rResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
            for (size_t i = 0; i < rResp.resultsSize; i++) {
                if (rResp.results[i].hasValue && 
                    UA_Variant_hasScalarType(&rResp.results[i].value, &UA_TYPES[UA_TYPES_DOUBLE])) {
                    double value = *(double*)rResp.results[i].value.data;
                    results.push_back({true, value});
                } else {
                    results.push_back({false, 0.0});
                }
            }
        } else {
            // Если произошла ошибка, заполняем результаты как неудачные
            for (size_t i = 0; i < nodes.size(); i++) {
                results.push_back({false, 0.0});
            }
        }
        
        UA_ReadRequest_clear(&rReq);
        UA_ReadResponse_clear(&rResp);
        
        return results;
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
    OPCUANode resistanceNode;
    OPCUANode powerNode;
    std::vector<OPCUANode> allNodes;

public:
    MultimeterDevice() {}

    bool initialize(OPCUAClient& client, const OPCUANode& parentNode) {
        // Ищем устройство
        deviceNode = client.findNodeByBrowseName(parentNode, "Multimeter");
        if (!deviceNode.isValid()) {
            return false;
        }

        // Ищем все компоненты устройства
        auto components = client.findDeviceComponents(deviceNode);
        
        // Ищем конкретные переменные внутри устройства
        for (const auto& component : components) {
            if (component.getBrowseName() == "Voltage") {
                voltageNode = component;
            } else if (component.getBrowseName() == "Current") {
                currentNode = component;
            } else if (component.getBrowseName() == "Resistance") {
                resistanceNode = component;
            } else if (component.getBrowseName() == "Power") {
                powerNode = component;
            }
        }

        // Собираем все узлы для массового чтения
        if (voltageNode.isValid()) allNodes.push_back(voltageNode);
        if (currentNode.isValid()) allNodes.push_back(currentNode);
        if (resistanceNode.isValid()) allNodes.push_back(resistanceNode);
        if (powerNode.isValid()) allNodes.push_back(powerNode);

        return voltageNode.isValid() || currentNode.isValid() || 
               resistanceNode.isValid() || powerNode.isValid();
    }

    bool readValues(OPCUAClient& client, double& voltage, double& current, 
                    double& resistance, double& power) const {
        bool voltageOk = client.readValue(voltageNode, voltage);
        bool currentOk = client.readValue(currentNode, current);
        bool resistanceOk = client.readValue(resistanceNode, resistance);
        bool powerOk = client.readValue(powerNode, power);
        
        return voltageOk || currentOk || resistanceOk || powerOk;
    }

    std::vector<std::pair<bool, double>> readAllValues(OPCUAClient& client) const {
        return client.readMultipleValues(allNodes);
    }

    void printStatus() const {
        std::cout << "Мультиметр: ";
        if (voltageNode.isValid()) std::cout << "Напряжение доступно, ";
        if (currentNode.isValid()) std::cout << "Ток доступен, ";
        if (resistanceNode.isValid()) std::cout << "Сопротивление доступно, ";
        if (powerNode.isValid()) std::cout << "Мощность доступна";
        std::cout << std::endl;
    }

    const OPCUANode& getDeviceNode() const { return deviceNode; }
    const OPCUANode& getVoltageNode() const { return voltageNode; }
    const OPCUANode& getCurrentNode() const { return currentNode; }
    const OPCUANode& getResistanceNode() const { return resistanceNode; }
    const OPCUANode& getPowerNode() const { return powerNode; }
    const std::vector<OPCUANode>& getAllNodes() const { return allNodes; }
};

// Класс для управления устройством "Станок"
class MachineDevice {
private:
    OPCUANode deviceNode;
    OPCUANode flywheelRPMNode;
    OPCUANode powerNode;
    OPCUANode voltageNode;
    OPCUANode energyConsumptionNode;
    std::vector<OPCUANode> allNodes;

public:
    MachineDevice() {}

    bool initialize(OPCUAClient& client, const OPCUANode& parentNode) {
        // Ищем устройство
        deviceNode = client.findNodeByBrowseName(parentNode, "Machine");
        if (!deviceNode.isValid()) {
            return false;
        }

        // Ищем все компоненты устройства
        auto components = client.findDeviceComponents(deviceNode);
        
        // Ищем конкретные переменные внутри устройства
        for (const auto& component : components) {
            if (component.getBrowseName() == "FlywheelRPM") {
                flywheelRPMNode = component;
            } else if (component.getBrowseName() == "Power") {
                powerNode = component;
            } else if (component.getBrowseName() == "Voltage") {
                voltageNode = component;
            } else if (component.getBrowseName() == "EnergyConsumption") {
                energyConsumptionNode = component;
            }
        }

        // Собираем все узлы для массового чтения
        if (flywheelRPMNode.isValid()) allNodes.push_back(flywheelRPMNode);
        if (powerNode.isValid()) allNodes.push_back(powerNode);
        if (voltageNode.isValid()) allNodes.push_back(voltageNode);
        if (energyConsumptionNode.isValid()) allNodes.push_back(energyConsumptionNode);

        return flywheelRPMNode.isValid() || powerNode.isValid() || 
               voltageNode.isValid() || energyConsumptionNode.isValid();
    }

    bool readValues(OPCUAClient& client, double& rpm, double& power, 
                    double& voltage, double& energy) const {
        bool rpmOk = client.readValue(flywheelRPMNode, rpm);
        bool powerOk = client.readValue(powerNode, power);
        bool voltageOk = client.readValue(voltageNode, voltage);
        bool energyOk = client.readValue(energyConsumptionNode, energy);
        
        return rpmOk || powerOk || voltageOk || energyOk;
    }

    std::vector<std::pair<bool, double>> readAllValues(OPCUAClient& client) const {
        return client.readMultipleValues(allNodes);
    }

    bool setRPMValue(OPCUAClient& client, double rpm) {
        return client.writeValue(flywheelRPMNode, rpm);
    }

    void printStatus() const {
        std::cout << "Станок: ";
        if (flywheelRPMNode.isValid()) std::cout << "Обороты доступны, ";
        if (powerNode.isValid()) std::cout << "Мощность доступна, ";
        if (voltageNode.isValid()) std::cout << "Напряжение доступно, ";
        if (energyConsumptionNode.isValid()) std::cout << "Энергопотребление доступно";
        std::cout << std::endl;
    }

    const OPCUANode& getDeviceNode() const { return deviceNode; }
    const OPCUANode& getFlywheelRPMNode() const { return flywheelRPMNode; }
    const OPCUANode& getPowerNode() const { return powerNode; }
    const OPCUANode& getVoltageNode() const { return voltageNode; }
    const OPCUANode& getEnergyConsumptionNode() const { return energyConsumptionNode; }
};

// Класс для управления устройством "Компьютер"
class ComputerDevice {
private:
    OPCUANode deviceNode;
    OPCUANode fan1Node;
    OPCUANode fan2Node;
    OPCUANode fan3Node;
    OPCUANode cpuLoadNode;
    OPCUANode gpuLoadNode;
    OPCUANode ramUsageNode;
    std::vector<OPCUANode> allNodes;

public:
    ComputerDevice() {}

    bool initialize(OPCUAClient& client, const OPCUANode& parentNode) {
        // Ищем устройство
        deviceNode = client.findNodeByBrowseName(parentNode, "Computer");
        if (!deviceNode.isValid()) {
            return false;
        }

        // Ищем все компоненты устройства
        auto components = client.findDeviceComponents(deviceNode);
        
        // Ищем конкретные переменные внутри устройства
        for (const auto& component : components) {
            if (component.getBrowseName() == "Fan1") {
                fan1Node = component;
            } else if (component.getBrowseName() == "Fan2") {
                fan2Node = component;
            } else if (component.getBrowseName() == "Fan3") {
                fan3Node = component;
            } else if (component.getBrowseName() == "CPULoad") {
                cpuLoadNode = component;
            } else if (component.getBrowseName() == "GPULoad") {
                gpuLoadNode = component;
            } else if (component.getBrowseName() == "RAMUsage") {
                ramUsageNode = component;
            }
        }

        // Собираем все узлы для массового чтения
        if (fan1Node.isValid()) allNodes.push_back(fan1Node);
        if (fan2Node.isValid()) allNodes.push_back(fan2Node);
        if (fan3Node.isValid()) allNodes.push_back(fan3Node);
        if (cpuLoadNode.isValid()) allNodes.push_back(cpuLoadNode);
        if (gpuLoadNode.isValid()) allNodes.push_back(gpuLoadNode);
        if (ramUsageNode.isValid()) allNodes.push_back(ramUsageNode);

        return fan1Node.isValid() || fan2Node.isValid() || fan3Node.isValid() ||
               cpuLoadNode.isValid() || gpuLoadNode.isValid() || ramUsageNode.isValid();
    }

    bool readValues(OPCUAClient& client, double& fan1, double& fan2, double& fan3,
                    double& cpuLoad, double& gpuLoad, double& ramUsage) const {
        bool fan1Ok = client.readValue(fan1Node, fan1);
        bool fan2Ok = client.readValue(fan2Node, fan2);
        bool fan3Ok = client.readValue(fan3Node, fan3);
        bool cpuOk = client.readValue(cpuLoadNode, cpuLoad);
        bool gpuOk = client.readValue(gpuLoadNode, gpuLoad);
        bool ramOk = client.readValue(ramUsageNode, ramUsage);
        
        return fan1Ok || fan2Ok || fan3Ok || cpuOk || gpuOk || ramOk;
    }

    std::vector<std::pair<bool, double>> readAllValues(OPCUAClient& client) const {
        return client.readMultipleValues(allNodes);
    }

    void printStatus() const {
        std::cout << "Компьютер: ";
        if (fan1Node.isValid()) std::cout << "Вентилятор1 доступен, ";
        if (fan2Node.isValid()) std::cout << "Вентилятор2 доступен, ";
        if (fan3Node.isValid()) std::cout << "Вентилятор3 доступен, ";
        if (cpuLoadNode.isValid()) std::cout << "ЦП доступен, ";
        if (gpuLoadNode.isValid()) std::cout << "ГП доступен, ";
        if (ramUsageNode.isValid()) std::cout << "ОЗУ доступно";
        std::cout << std::endl;
    }

    const OPCUANode& getDeviceNode() const { return deviceNode; }
};

// Основной класс приложения
class OPCUAApplication {
private:
    OPCUAClient client;
    MultimeterDevice multimeter;
    MachineDevice machine;
    ComputerDevice computer;
    
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

        std::cout << "Подключено к серверу OPC UA" << std::endl;
        std::cout << "Поиск устройств..." << std::endl;

        // Инициализация всех устройств
        bool multimeterFound = multimeter.initialize(client, objectsFolder);
        bool machineFound = machine.initialize(client, objectsFolder);
        bool computerFound = computer.initialize(client, objectsFolder);

        if (multimeterFound) {
            multimeter.printStatus();
        }

        if (machineFound) {
            machine.printStatus();
        }

        if (computerFound) {
            computer.printStatus();
        }

        nodesFound = multimeterFound || machineFound || computerFound;

        if (!nodesFound) {
            std::cerr << "\nОШИБКА: Не найдено ни одного устройства." << std::endl;
            std::cerr << "Убедитесь, что сервер запущен и создал переменные." << std::endl;
            return false;
        }

        return true;
    }

    void run() {
        std::cout << "\n\nНачало чтения значений..." << std::endl;
        printControls();
        
        // Ждем немного, чтобы пользователь увидел приветствие
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        clearConsole();

        while (SignalHandler::isRunning()) {
            handleInput();
            readAndDisplayValues();
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Обновление каждые 500 мс
        }

        shutdown();
    }

private:
    void printWelcome() const {
        std::cout << "Клиент OPC UA запускается..." << std::endl;
        std::cout << "Подключение к: opc.tcp://127.0.0.1:4840" << std::endl;
        std::cout << std::endl;
    }

    void printControls() {
        std::cout << "\nУправление:" << std::endl;
        std::cout << "  - 'q' - выход" << std::endl;
        std::cout << "  - 'r' - установить новые обороты маховика" << std::endl;
        std::cout << std::endl;
    }

    void clearConsole() {
#ifdef _WIN32
        system("cls");
#else
        system("clear");
#endif
    }

    void handleInput() {
        if (_kbhit()) {
            char c = _getch();
            
            switch (c) {
                case 'q':
                case 'Q':
                    std::cout << "\nВыход..." << std::endl;
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
        if (!machine.getFlywheelRPMNode().isValid()) {
            std::cerr << "\nУзел оборотов маховика не найден, невозможно установить значение." << std::endl;
            return;
        }

        std::cout << "\nВведите новые обороты маховика (об/мин): ";
        std::string input;
        std::getline(std::cin, input);

        try {
            double newRpm = std::stod(input);
            
            if (machine.setRPMValue(client, newRpm)) {
                std::cout << "Успешно установлены обороты: " << newRpm << " об/мин" << std::endl;
            } else {
                std::cerr << "Ошибка записи значения оборотов" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Неверный ввод: " << e.what() << std::endl;
        }
    }

    void readAndDisplayValues() {
        // Очищаем консоль для аккуратного вывода
        clearConsole();
        
        // Выводим заголовок с текущим временем
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);
        std::cout << "===========================================" << std::endl;
        std::cout << "Данные OPC UA - " << std::ctime(&now_time);
        std::cout << "===========================================" << std::endl;
        
        // Чтение и отображение данных всех устройств
        displayAllDevices();
        
        std::cout << "===========================================" << std::endl;
        printControls();
    }

    void displayAllDevices() {
        bool hasMultimeter = multimeter.getDeviceNode().isValid();
        bool hasMachine = machine.getDeviceNode().isValid();
        bool hasComputer = computer.getDeviceNode().isValid();
        
        // Мультиметр
        if (hasMultimeter) {
            std::cout << "\n[МУЛЬТИМЕТР]" << std::endl;
            auto multimeterValues = multimeter.readAllValues(client);
            std::vector<std::string> multimeterNames = {"Напряжение", "Ток", "Сопротивление", "Мощность"};
            std::vector<std::string> multimeterUnits = {"В", "А", "Ом", "Вт"};
            
            for (size_t i = 0; i < multimeterValues.size() && i < multimeterNames.size(); i++) {
                if (multimeterValues[i].first) {
                    std::cout << "  " << multimeterNames[i] << ": " 
                              << std::fixed << std::setprecision(2) << multimeterValues[i].second 
                              << " " << multimeterUnits[i] << std::endl;
                }
            }
        }
        
        // Станок
        if (hasMachine) {
            std::cout << "\n[СТАНОК]" << std::endl;
            auto machineValues = machine.readAllValues(client);
            std::vector<std::string> machineNames = {"Обороты маховика", "Мощность", "Напряжение", "Потребление энергии"};
            std::vector<std::string> machineUnits = {"об/мин", "кВт", "В", "кВт·ч"};
            
            for (size_t i = 0; i < machineValues.size() && i < machineNames.size(); i++) {
                if (machineValues[i].first) {
                    std::cout << "  " << machineNames[i] << ": " 
                              << std::fixed << std::setprecision(2) << machineValues[i].second 
                              << " " << machineUnits[i] << std::endl;
                }
            }
        }
        
        // Компьютер
        if (hasComputer) {
            std::cout << "\n[КОМПЬЮТЕР]" << std::endl;
            auto computerValues = computer.readAllValues(client);
            std::vector<std::string> computerNames = {"Вентилятор 1", "Вентилятор 2", "Вентилятор 3", 
                                                     "Загрузка ЦП", "Загрузка ГП", "Использование ОЗУ"};
            std::vector<std::string> computerUnits = {"об/мин", "об/мин", "об/мин", "%", "%", "%"};
            
            for (size_t i = 0; i < computerValues.size() && i < computerNames.size(); i++) {
                if (computerValues[i].first) {
                    std::cout << "  " << computerNames[i] << ": " 
                              << std::fixed << std::setprecision(2) << computerValues[i].second 
                              << " " << computerUnits[i] << std::endl;
                }
            }
        }
    }

    void shutdown() {
        std::cout << "\nОтключение от сервера..." << std::endl;
        client.disconnect();
        std::cout << "Клиент остановлен." << std::endl;
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