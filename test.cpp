#include <open62541/client.h>
#include <open62541/client_config_default.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

std::atomic<bool> running(true);

void signalHandler(int signal) {
    running = false;
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    std::cout << "Client starting..." << std::endl;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    UA_Client *client = UA_Client_new();
    UA_ClientConfig_setDefault(UA_Client_getConfig(client));

    // Подключаемся к серверу
    UA_StatusCode status = UA_Client_connect(client, "opc.tcp://localhost:4840");
    if (status != UA_STATUSCODE_GOOD) {
        std::cerr << "Failed to connect: " << UA_StatusCode_name(status) << std::endl;
        UA_Client_delete(client);
        return 1;
    }

    std::cout << "Connected to server" << std::endl;

    // Будем пробовать разные NodeId, начиная с наиболее вероятных
    std::vector<std::pair<int, int>> nodeIdsToTry = {
        {1, 1},  // ns=1, id=1 - самый вероятный
        {2, 1},
        {3, 1},
        {1, 2},
        {1, 3},
        {0, 1},
        {0, 2}
    };

    UA_NodeId foundNodeId = UA_NODEID_NULL;
    
    // Пробуем найти правильный узел
    for (auto &node : nodeIdsToTry) {
        UA_NodeId testNodeId = UA_NODEID_NUMERIC(node.first, node.second);
        
        // Создаем запрос на чтение
        UA_ReadRequest request;
        UA_ReadRequest_init(&request);
        
        request.nodesToReadSize = 1;
        request.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
        
        request.nodesToRead[0].nodeId = testNodeId;
        request.nodesToRead[0].attributeId = UA_ATTRIBUTEID_VALUE;
        
        // Выполняем запрос
        UA_ReadResponse response = UA_Client_Service_read(client, request);
        
        if (response.responseHeader.serviceResult == UA_STATUSCODE_GOOD && 
            response.resultsSize > 0 && 
            response.results[0].hasValue &&
            UA_Variant_hasScalarType(&response.results[0].value, &UA_TYPES[UA_TYPES_DOUBLE])) {
            
            double voltage = *(double*)response.results[0].value.data;
            std::cout << "Found Voltage at ns=" << node.first << ", id=" << node.second 
                      << ": " << voltage << " V" << std::endl;
            foundNodeId = testNodeId;
            
            UA_ReadRequest_clear(&request);
            UA_ReadResponse_clear(&response);
            break;
        }
        
        UA_ReadRequest_clear(&request);
        UA_ReadResponse_clear(&response);
    }

    if (UA_NodeId_isNull(&foundNodeId)) {
        std::cerr << "Could not find Voltage node. Trying brute force search..." << std::endl;
        
        // Полный перебор namespace 0-5, id 0-10
        for (int ns = 0; ns < 6; ns++) {
            for (int id = 0; id < 11; id++) {
                UA_NodeId testNodeId = UA_NODEID_NUMERIC(ns, id);
                
                UA_ReadRequest request;
                UA_ReadRequest_init(&request);
                request.nodesToReadSize = 1;
                request.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
                request.nodesToRead[0].nodeId = testNodeId;
                request.nodesToRead[0].attributeId = UA_ATTRIBUTEID_VALUE;
                
                UA_ReadResponse response = UA_Client_Service_read(client, request);
                
                if (response.responseHeader.serviceResult == UA_STATUSCODE_GOOD && 
                    response.resultsSize > 0 && 
                    response.results[0].hasValue &&
                    UA_Variant_hasScalarType(&response.results[0].value, &UA_TYPES[UA_TYPES_DOUBLE])) {
                    
                    double voltage = *(double*)response.results[0].value.data;
                    std::cout << "Found Voltage at ns=" << ns << ", id=" << id 
                              << ": " << voltage << " V" << std::endl;
                    foundNodeId = testNodeId;
                    
                    UA_ReadRequest_clear(&request);
                    UA_ReadResponse_clear(&response);
                    break;
                }
                
                UA_ReadRequest_clear(&request);
                UA_ReadResponse_clear(&response);
            }
            if (!UA_NodeId_isNull(&foundNodeId)) break;
        }
    }

    if (UA_NodeId_isNull(&foundNodeId)) {
        std::cerr << "Failed to find Voltage node after exhaustive search." << std::endl;
        std::cerr << "Make sure the server is running and check the server output for the correct NodeId." << std::endl;
        UA_Client_disconnect(client);
        UA_Client_delete(client);
        return 1;
    }

    // Основной цикл чтения
    std::cout << "Starting to read voltage values..." << std::endl;
    
    while (running) {
        UA_ReadRequest request;
        UA_ReadRequest_init(&request);
        
        request.nodesToReadSize = 1;
        request.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
        
        request.nodesToRead[0].nodeId = foundNodeId;
        request.nodesToRead[0].attributeId = UA_ATTRIBUTEID_VALUE;
        
        UA_ReadResponse response = UA_Client_Service_read(client, request);
        
        if (response.responseHeader.serviceResult == UA_STATUSCODE_GOOD && 
            response.resultsSize > 0 && 
            response.results[0].hasValue &&
            UA_Variant_hasScalarType(&response.results[0].value, &UA_TYPES[UA_TYPES_DOUBLE])) {
            
            double voltage = *(double*)response.results[0].value.data;
            std::cout << "Voltage: " << voltage << " V" << std::endl;
        } else {
            std::cerr << "Read failed: " << UA_StatusCode_name(response.responseHeader.serviceResult) << std::endl;
        }
        
        UA_ReadRequest_clear(&request);
        UA_ReadResponse_clear(&response);
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Disconnecting..." << std::endl;
    UA_Client_disconnect(client);
    UA_Client_delete(client);

    return 0;
}