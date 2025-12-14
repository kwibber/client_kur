#include "opcua_client.h"
#include <iostream>
#include <cstring>
#include <type_traits>

// Реализация OPCUANode

OPCUANode::OPCUANode() : nodeId(UA_NODEID_NULL) {}

OPCUANode::OPCUANode(const UA_NodeId& id, const std::string& bName, const std::string& dName) 
    : browseName(bName), displayName(dName) {
    UA_NodeId_copy(&id, &nodeId);
}

OPCUANode::OPCUANode(const OPCUANode& other) 
    : browseName(other.browseName), displayName(other.displayName) {
    UA_NodeId_copy(&other.nodeId, &nodeId);
}

OPCUANode::OPCUANode(OPCUANode&& other) noexcept 
    : browseName(std::move(other.browseName)), 
      displayName(std::move(other.displayName)) {
    UA_NodeId_copy(&other.nodeId, &nodeId);
    UA_NodeId_clear(&other.nodeId);
}

OPCUANode::~OPCUANode() {
    UA_NodeId_clear(&nodeId);
}

OPCUANode& OPCUANode::operator=(const OPCUANode& other) {
    if (this != &other) {
        UA_NodeId_clear(&nodeId);
        UA_NodeId_copy(&other.nodeId, &nodeId);
        browseName = other.browseName;
        displayName = other.displayName;
    }
    return *this;
}

OPCUANode& OPCUANode::operator=(OPCUANode&& other) noexcept {
    if (this != &other) {
        UA_NodeId_clear(&nodeId);
        UA_NodeId_copy(&other.nodeId, &nodeId);
        UA_NodeId_clear(&other.nodeId);
        browseName = std::move(other.browseName);
        displayName = std::move(other.displayName);
    }
    return *this;
}

const UA_NodeId& OPCUANode::getId() const { return nodeId; }
std::string OPCUANode::getBrowseName() const { return browseName; }
std::string OPCUANode::getDisplayName() const { return displayName; }

bool OPCUANode::isValid() const { return !UA_NodeId_isNull(&nodeId); }

void OPCUANode::printInfo() const {
    if (isValid()) {
        std::cout << browseName << " (ID: ns=" << nodeId.namespaceIndex 
                  << "; i=" << nodeId.identifier.numeric << ")";
    }
}

// Реализация OPCUAClient

OPCUAClient::OPCUAClient(const std::string& endpoint) 
    : client(nullptr), endpoint(endpoint) {}

OPCUAClient::OPCUAClient(OPCUAClient&& other) noexcept 
    : client(other.client), endpoint(std::move(other.endpoint)) {
    other.client = nullptr;
}

OPCUAClient& OPCUAClient::operator=(OPCUAClient&& other) noexcept {
    if (this != &other) {
        cleanup();
        client = other.client;
        endpoint = std::move(other.endpoint);
        other.client = nullptr;
    }
    return *this;
}

OPCUAClient::~OPCUAClient() {
    disconnect();
    cleanup();
}

void OPCUAClient::cleanup() {
    if (client) {
        UA_Client_delete(client);
        client = nullptr;
    }
}

bool OPCUAClient::connect() {
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

void OPCUAClient::disconnect() {
    if (client) {
        UA_Client_disconnect(client);
    }
}

bool OPCUAClient::isConnected() const {
    if (!client) return false;
    
    UA_SecureChannelState channelState;
    UA_SessionState sessionState;
    UA_StatusCode connectStatus;
    
    UA_Client_getState(client, &channelState, &sessionState, &connectStatus);
    
    return (channelState == UA_SECURECHANNELSTATE_OPEN && 
            sessionState == UA_SESSIONSTATE_ACTIVATED);
}

OPCUANode OPCUAClient::findNodeByBrowseName(const OPCUANode& parentNode, const std::string& browseName) const {
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

std::vector<OPCUANode> OPCUAClient::findDeviceComponents(const OPCUANode& deviceNode) const {
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

bool OPCUAClient::readDisplayName(const OPCUANode& node, std::string& displayName) const {
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

std::vector<std::pair<bool, double>> OPCUAClient::readMultipleValues(const std::vector<OPCUANode>& nodes) const {
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
        for (size_t i = 0; i < nodes.size(); i++) {
            results.push_back({false, 0.0});
        }
    }
    
    UA_ReadRequest_clear(&rReq);
    UA_ReadResponse_clear(&rResp);
    
    return results;
}