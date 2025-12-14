#ifndef OPCUA_CLIENT_H
#define OPCUA_CLIENT_H

#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <string>
#include <vector>
#include <memory>

// Класс для представления узла OPC UA
class OPCUANode {
private:
    UA_NodeId nodeId;
    std::string browseName;
    std::string displayName;

public:
    OPCUANode();
    OPCUANode(const UA_NodeId& id, const std::string& bName = "", const std::string& dName = "");
    OPCUANode(const OPCUANode& other);
    OPCUANode(OPCUANode&& other) noexcept;
    ~OPCUANode();

    OPCUANode& operator=(const OPCUANode& other);
    OPCUANode& operator=(OPCUANode&& other) noexcept;

    const UA_NodeId& getId() const;
    std::string getBrowseName() const;
    std::string getDisplayName() const;
    bool isValid() const;
    void printInfo() const;
};

// Класс OPC UA клиента
class OPCUAClient {
private:
    UA_Client* client;
    std::string endpoint;

    void cleanup();
    
    // Вспомогательные функции для определения типа данных
    template<typename T>
    static const UA_DataType* getDataType() {
        if constexpr (std::is_same_v<T, double>) {
            return &UA_TYPES[UA_TYPES_DOUBLE];
        } else if constexpr (std::is_same_v<T, int>) {
            return &UA_TYPES[UA_TYPES_INT32];
        } else if constexpr (std::is_same_v<T, float>) {
            return &UA_TYPES[UA_TYPES_FLOAT];
        } else {
            static_assert(sizeof(T) == 0, "Unsupported type");
            return nullptr;
        }
    }

public:
    OPCUAClient(const std::string& endpoint = "opc.tcp://127.0.0.1:4840");
    ~OPCUAClient();

    // Запрещаем копирование
    OPCUAClient(const OPCUAClient&) = delete;
    OPCUAClient& operator=(const OPCUAClient&) = delete;

    // Разрешаем перемещение
    OPCUAClient(OPCUAClient&& other) noexcept;
    OPCUAClient& operator=(OPCUAClient&& other) noexcept;

    bool connect();
    void disconnect();
    bool isConnected() const;

    // Методы для работы с узлами
    OPCUANode findNodeByBrowseName(const OPCUANode& parentNode, const std::string& browseName) const;
    std::vector<OPCUANode> findDeviceComponents(const OPCUANode& deviceNode) const;
    
    // Методы чтения/записи значений
    template<typename T>
    bool readValue(const OPCUANode& node, T& value) const;
    
    bool readDisplayName(const OPCUANode& node, std::string& displayName) const;
    
    template<typename T>
    bool writeValue(const OPCUANode& node, const T& value);
    
    std::vector<std::pair<bool, double>> readMultipleValues(const std::vector<OPCUANode>& nodes) const;
};

// Определения шаблонных методов inline
template<typename T>
bool OPCUAClient::readValue(const OPCUANode& node, T& value) const {
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

template<typename T>
bool OPCUAClient::writeValue(const OPCUANode& node, const T& value) {
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

#endif // OPCUA_CLIENT_H