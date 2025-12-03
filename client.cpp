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

std::atomic<bool> running(true);

void signalHandler(int signal) {
    running = false;
}

// Функция для поиска узлов по BrowseName
UA_NodeId findNodeByBrowseName(UA_Client *client, const char* browseName, UA_UInt16 namespaceIndex = 1) {
    // Начнем с ObjectsFolder
    UA_BrowseRequest bReq;
    UA_BrowseRequest_init(&bReq);
    
    bReq.requestedMaxReferencesPerNode = 0;
    bReq.nodesToBrowseSize = 1;
    bReq.nodesToBrowse = (UA_BrowseDescription*)UA_Array_new(1, &UA_TYPES[UA_TYPES_BROWSEDESCRIPTION]);
    
    bReq.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;
    
    UA_BrowseResponse bResp = UA_Client_Service_browse(client, bReq);
    
    UA_NodeId result = UA_NODEID_NULL;
    
    if (bResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
        for (size_t i = 0; i < bResp.resultsSize; i++) {
            UA_BrowseResult *res = &bResp.results[i];
            if (res->statusCode == UA_STATUSCODE_GOOD) {
                for (size_t j = 0; j < res->referencesSize; j++) {
                    UA_ReferenceDescription *ref = &res->references[j];
                    
                    // Проверяем BrowseName
                    if (ref->browseName.name.length > 0) {
                        std::string name((char*)ref->browseName.name.data, ref->browseName.name.length);
                        if (name == browseName) {
                            result = ref->nodeId.nodeId;
                            UA_BrowseRequest_clear(&bReq);
                            UA_BrowseResponse_clear(&bResp);
                            return result;
                        }
                    }
                }
            }
        }
    }
    
    UA_BrowseRequest_clear(&bReq);
    UA_BrowseResponse_clear(&bResp);
    return result;
}

int main() {
    // Устанавливаем кодировку UTF-8 для Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "OPC UA Client starting..." << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - 'q' to quit" << std::endl;
    std::cout << "  - 'r' to set new RPM value" << std::endl;
    std::cout << std::endl;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    UA_Client *client = UA_Client_new();
    UA_ClientConfig *config = UA_Client_getConfig(client);
    UA_ClientConfig_setDefault(config);
    config->timeout = 5000; // 5 секунд таймаут

    // Подключаемся к серверу
    UA_StatusCode status = UA_Client_connect(client, "opc.tcp://127.0.0.1:4840");
    if (status != UA_STATUSCODE_GOOD) {
        std::cerr << "Failed to connect: " << UA_StatusCode_name(status) << std::endl;
        UA_Client_delete(client);
        return 1;
    }

    std::cout << "Connected to server" << std::endl;
    
    // Поиск узлов по их именам
    std::cout << "Looking for nodes..." << std::endl;
    
    UA_NodeId voltageId = findNodeByBrowseName(client, "Voltage");
    UA_NodeId currentId = findNodeByBrowseName(client, "Current");
    UA_NodeId rpmId = findNodeByBrowseName(client, "FlywheelRPM");
    
    if (UA_NodeId_isNull(&voltageId)) {
        // Попробуем альтернативные варианты
        std::cout << "Node 'Voltage' not found by BrowseName, trying direct NodeId..." << std::endl;
        
        // Попробуем разные возможные NodeId
        std::vector<std::pair<int, int>> possibleIds = {
            {1, 1}, {2, 1}, {3, 1}, {4, 1},
            {1, 1000}, {1, 1001}, {1, 1002}
        };
        
        for (auto& idPair : possibleIds) {
            UA_NodeId testId = UA_NODEID_NUMERIC(idPair.first, idPair.second);
            UA_ReadRequest rReq;
            UA_ReadRequest_init(&rReq);
            rReq.nodesToReadSize = 1;
            rReq.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
            UA_ReadValueId_init(&rReq.nodesToRead[0]);
            rReq.nodesToRead[0].nodeId = testId;
            rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DISPLAYNAME;
            
            UA_ReadResponse rResp = UA_Client_Service_read(client, rReq);
            
            if (rResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
                rResp.resultsSize > 0 &&
                rResp.results[0].hasValue &&
                UA_Variant_hasScalarType(&rResp.results[0].value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
                
                UA_LocalizedText *lt = (UA_LocalizedText*)rResp.results[0].value.data;
                std::string name((char*)lt->text.data, lt->text.length);
                if (name.find("Voltage") != std::string::npos || 
                    name.find("voltage") != std::string::npos ||
                    name.find("VOLTAGE") != std::string::npos) {
                    voltageId = testId;
                    std::cout << "Found Voltage node: ns=" << idPair.first << ", i=" << idPair.second 
                              << " (DisplayName: " << name << ")" << std::endl;
                    break;
                }
            }
            
            UA_ReadRequest_clear(&rReq);
            UA_ReadResponse_clear(&rResp);
        }
    } else {
        std::cout << "Found Voltage node" << std::endl;
    }
    
    if (UA_NodeId_isNull(&currentId)) {
        std::cout << "Node 'Current' not found by BrowseName, trying direct NodeId..." << std::endl;
        
        std::vector<std::pair<int, int>> possibleIds = {
            {1, 2}, {2, 2}, {3, 2}, {4, 2},
            {1, 1003}, {1, 1004}, {1, 1005}
        };
        
        for (auto& idPair : possibleIds) {
            UA_NodeId testId = UA_NODEID_NUMERIC(idPair.first, idPair.second);
            UA_ReadRequest rReq;
            UA_ReadRequest_init(&rReq);
            rReq.nodesToReadSize = 1;
            rReq.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
            UA_ReadValueId_init(&rReq.nodesToRead[0]);
            rReq.nodesToRead[0].nodeId = testId;
            rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DISPLAYNAME;
            
            UA_ReadResponse rResp = UA_Client_Service_read(client, rReq);
            
            if (rResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
                rResp.resultsSize > 0 &&
                rResp.results[0].hasValue &&
                UA_Variant_hasScalarType(&rResp.results[0].value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
                
                UA_LocalizedText *lt = (UA_LocalizedText*)rResp.results[0].value.data;
                std::string name((char*)lt->text.data, lt->text.length);
                if (name.find("Current") != std::string::npos || 
                    name.find("current") != std::string::npos ||
                    name.find("CURRENT") != std::string::npos) {
                    currentId = testId;
                    std::cout << "Found Current node: ns=" << idPair.first << ", i=" << idPair.second 
                              << " (DisplayName: " << name << ")" << std::endl;
                    break;
                }
            }
            
            UA_ReadRequest_clear(&rReq);
            UA_ReadResponse_clear(&rResp);
        }
    } else {
        std::cout << "Found Current node" << std::endl;
    }
    
    if (UA_NodeId_isNull(&rpmId)) {
        std::cout << "Node 'FlywheelRPM' not found by BrowseName, trying direct NodeId..." << std::endl;
        
        std::vector<std::pair<int, int>> possibleIds = {
            {1, 3}, {2, 3}, {3, 3}, {4, 3},
            {1, 1006}, {1, 1007}, {1, 1008}
        };
        
        for (auto& idPair : possibleIds) {
            UA_NodeId testId = UA_NODEID_NUMERIC(idPair.first, idPair.second);
            UA_ReadRequest rReq;
            UA_ReadRequest_init(&rReq);
            rReq.nodesToReadSize = 1;
            rReq.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
            UA_ReadValueId_init(&rReq.nodesToRead[0]);
            rReq.nodesToRead[0].nodeId = testId;
            rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DISPLAYNAME;
            
            UA_ReadResponse rResp = UA_Client_Service_read(client, rReq);
            
            if (rResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
                rResp.resultsSize > 0 &&
                rResp.results[0].hasValue &&
                UA_Variant_hasScalarType(&rResp.results[0].value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
                
                UA_LocalizedText *lt = (UA_LocalizedText*)rResp.results[0].value.data;
                std::string name((char*)lt->text.data, lt->text.length);
                if (name.find("Flywheel") != std::string::npos || 
                    name.find("flywheel") != std::string::npos ||
                    name.find("RPM") != std::string::npos ||
                    name.find("rpm") != std::string::npos) {
                    rpmId = testId;
                    std::cout << "Found Flywheel RPM node: ns=" << idPair.first << ", i=" << idPair.second 
                              << " (DisplayName: " << name << ")" << std::endl;
                    break;
                }
            }
            
            UA_ReadRequest_clear(&rReq);
            UA_ReadResponse_clear(&rResp);
        }
    } else {
        std::cout << "Found Flywheel RPM node" << std::endl;
    }
    
    // Проверим, что нашли хотя бы один узел
    if (UA_NodeId_isNull(&voltageId) && UA_NodeId_isNull(&currentId) && UA_NodeId_isNull(&rpmId)) {
        std::cerr << "Could not find any nodes. Make sure server is running and has created the variables." << std::endl;
        std::cerr << "Trying brute force search..." << std::endl;
        
        // Полный перебор по ограниченному диапазону
        for (int ns = 0; ns < 5; ns++) {
            for (int id = 1; id < 50; id++) {
                UA_NodeId testId = UA_NODEID_NUMERIC(ns, id);
                UA_ReadRequest rReq;
                UA_ReadRequest_init(&rReq);
                rReq.nodesToReadSize = 1;
                rReq.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
                UA_ReadValueId_init(&rReq.nodesToRead[0]);
                rReq.nodesToRead[0].nodeId = testId;
                rReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_VALUE;
                
                UA_ReadResponse rResp = UA_Client_Service_read(client, rReq);
                
                if (rResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
                    rResp.resultsSize > 0 &&
                    rResp.results[0].hasValue) {
                    
                    // Просто выведем, что нашли
                    std::cout << "Found node at ns=" << ns << ", i=" << id 
                              << " with some value" << std::endl;
                    
                    // Если это double, то возможно это наша переменная
                    if (UA_Variant_hasScalarType(&rResp.results[0].value, &UA_TYPES[UA_TYPES_DOUBLE])) {
                        double value = *(double*)rResp.results[0].value.data;
                        std::cout << "  Value: " << value << std::endl;
                        
                        // Проверим DisplayName для идентификации
                        UA_ReadRequest dnReq;
                        UA_ReadRequest_init(&dnReq);
                        dnReq.nodesToReadSize = 1;
                        dnReq.nodesToRead = (UA_ReadValueId*)UA_Array_new(1, &UA_TYPES[UA_TYPES_READVALUEID]);
                        UA_ReadValueId_init(&dnReq.nodesToRead[0]);
                        dnReq.nodesToRead[0].nodeId = testId;
                        dnReq.nodesToRead[0].attributeId = UA_ATTRIBUTEID_DISPLAYNAME;
                        
                        UA_ReadResponse dnResp = UA_Client_Service_read(client, dnReq);
                        
                        if (dnResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
                            dnResp.resultsSize > 0 &&
                            dnResp.results[0].hasValue &&
                            UA_Variant_hasScalarType(&dnResp.results[0].value, &UA_TYPES[UA_TYPES_LOCALIZEDTEXT])) {
                            
                            UA_LocalizedText *lt = (UA_LocalizedText*)dnResp.results[0].value.data;
                            std::string name((char*)lt->text.data, lt->text.length);
                            std::cout << "  DisplayName: " << name << std::endl;
                        }
                        
                        UA_ReadRequest_clear(&dnReq);
                        UA_ReadResponse_clear(&dnResp);
                    }
                }
                
                UA_ReadRequest_clear(&rReq);
                UA_ReadResponse_clear(&rResp);
            }
        }
        
        std::cerr << "Exiting..." << std::endl;
        UA_Client_disconnect(client);
        UA_Client_delete(client);
        return 1;
    }

    // Основной цикл
    std::cout << "\nStarting to read values... Press 'r' to set new RPM value, 'q' to quit\n" << std::endl;
    
    while (running) {
        // Проверяем нажатие клавиш
        if (_kbhit()) {
            char c = _getch();
            
            if (c == 'q' || c == 'Q') {
                std::cout << "\nQuitting..." << std::endl;
                running = false;
                break;
            }
            else if (c == 'r' || c == 'R') {
                if (!UA_NodeId_isNull(&rpmId)) {
                    // Запрос нового значения RPM
                    std::cout << "\nEnter new RPM value: ";
                    
                    std::string input;
                    std::getline(std::cin, input);
                    
                    try {
                        double newRpm = std::stod(input);
                        
                        // Записываем новое значение через UA_Client_Service_write
                        UA_WriteRequest wReq;
                        UA_WriteRequest_init(&wReq);
                        
                        wReq.nodesToWriteSize = 1;
                        wReq.nodesToWrite = (UA_WriteValue*)UA_Array_new(1, &UA_TYPES[UA_TYPES_WRITEVALUE]);
                        
                        // Инициализируем WriteValue
                        UA_WriteValue_init(&wReq.nodesToWrite[0]);
                        wReq.nodesToWrite[0].nodeId = rpmId;
                        wReq.nodesToWrite[0].attributeId = UA_ATTRIBUTEID_VALUE;
                        
                        // Устанавливаем значение
                        wReq.nodesToWrite[0].value.hasValue = true;
                        UA_Variant_setScalarCopy(&wReq.nodesToWrite[0].value.value, &newRpm, &UA_TYPES[UA_TYPES_DOUBLE]);
                        
                        UA_WriteResponse wResp = UA_Client_Service_write(client, wReq);
                        
                        if (wResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
                            wResp.resultsSize > 0 &&
                            wResp.results[0] == UA_STATUSCODE_GOOD) {
                            std::cout << "Successfully set RPM to: " << newRpm << std::endl;
                        } else {
                            std::cerr << "Failed to write RPM value" << std::endl;
                            if (wResp.resultsSize > 0) {
                                std::cerr << "Error: " << UA_StatusCode_name(wResp.results[0]) << std::endl;
                            }
                        }
                        
                        UA_WriteRequest_clear(&wReq);
                        UA_WriteResponse_clear(&wResp);
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Invalid input: " << e.what() << std::endl;
                    }
                } else {
                    std::cerr << "\nRPM node not found, cannot set value." << std::endl;
                }
            }
        }

        // Читаем значения всех доступных переменных
        std::cout << "\r";
        
        // Считаем, сколько узлов найдено
        int foundNodes = 0;
        if (!UA_NodeId_isNull(&voltageId)) foundNodes++;
        if (!UA_NodeId_isNull(&currentId)) foundNodes++;
        if (!UA_NodeId_isNull(&rpmId)) foundNodes++;
        
        if (foundNodes == 0) {
            std::cout << "No nodes found to read. Waiting..." << std::flush;
        } else {
            // Создаем запрос на чтение для всех найденных переменных
            UA_ReadRequest rReq;
            UA_ReadRequest_init(&rReq);
            
            rReq.nodesToReadSize = foundNodes;
            rReq.nodesToRead = (UA_ReadValueId*)UA_Array_new(foundNodes, &UA_TYPES[UA_TYPES_READVALUEID]);
            
            int idx = 0;
            
            if (!UA_NodeId_isNull(&voltageId)) {
                UA_ReadValueId_init(&rReq.nodesToRead[idx]);
                rReq.nodesToRead[idx].nodeId = voltageId;
                rReq.nodesToRead[idx].attributeId = UA_ATTRIBUTEID_VALUE;
                idx++;
            }
            
            if (!UA_NodeId_isNull(&currentId)) {
                UA_ReadValueId_init(&rReq.nodesToRead[idx]);
                rReq.nodesToRead[idx].nodeId = currentId;
                rReq.nodesToRead[idx].attributeId = UA_ATTRIBUTEID_VALUE;
                idx++;
            }
            
            if (!UA_NodeId_isNull(&rpmId)) {
                UA_ReadValueId_init(&rReq.nodesToRead[idx]);
                rReq.nodesToRead[idx].nodeId = rpmId;
                rReq.nodesToRead[idx].attributeId = UA_ATTRIBUTEID_VALUE;
                idx++;
            }
            
            UA_ReadResponse rResp = UA_Client_Service_read(client, rReq);
            
            if (rResp.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
                idx = 0;
                
                if (!UA_NodeId_isNull(&voltageId)) {
                    if (rResp.resultsSize > idx && rResp.results[idx].hasValue && 
                        UA_Variant_hasScalarType(&rResp.results[idx].value, &UA_TYPES[UA_TYPES_DOUBLE])) {
                        double voltage = *(double*)rResp.results[idx].value.data;
                        std::cout << "Voltage: " << std::fixed << std::setprecision(2) << voltage << " V, ";
                    } else {
                        std::cout << "Voltage: N/A, ";
                    }
                    idx++;
                }
                
                if (!UA_NodeId_isNull(&currentId)) {
                    if (rResp.resultsSize > idx && rResp.results[idx].hasValue && 
                        UA_Variant_hasScalarType(&rResp.results[idx].value, &UA_TYPES[UA_TYPES_DOUBLE])) {
                        double current = *(double*)rResp.results[idx].value.data;
                        std::cout << "Current: " << std::fixed << std::setprecision(2) << current << " A, ";
                    } else {
                        std::cout << "Current: N/A, ";
                    }
                    idx++;
                }
                
                if (!UA_NodeId_isNull(&rpmId)) {
                    if (rResp.resultsSize > idx && rResp.results[idx].hasValue && 
                        UA_Variant_hasScalarType(&rResp.results[idx].value, &UA_TYPES[UA_TYPES_DOUBLE])) {
                        double rpm = *(double*)rResp.results[idx].value.data;
                        std::cout << "Flywheel RPM: " << std::fixed << std::setprecision(2) << rpm << " RPM      ";
                    } else {
                        std::cout << "Flywheel RPM: N/A      ";
                    }
                    idx++;
                }
            } else {
                std::cout << "Read failed: " << UA_StatusCode_name(rResp.responseHeader.serviceResult) << "      ";
            }
            
            UA_ReadRequest_clear(&rReq);
            UA_ReadResponse_clear(&rResp);
        }
        
        std::cout << std::flush;
        
        // Небольшая задержка
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::cout << "\nDisconnecting..." << std::endl;
    UA_Client_disconnect(client);
    UA_Client_delete(client);

    std::cout << "Client stopped." << std::endl;
    return 0;
}