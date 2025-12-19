#ifndef DEVICE_MANAGERS_H
#define DEVICE_MANAGERS_H

#include "opcua_client.h"
#include <vector>
#include <string>
#include <utility>


class MultimeterDevice {
private:
    OPCUANode deviceNode;
    OPCUANode voltageNode;
    OPCUANode currentNode;
    OPCUANode resistanceNode;
    OPCUANode powerNode;
    std::vector<OPCUANode> allNodes;

public:
    MultimeterDevice();
    bool initialize(OPCUAClient& client, const OPCUANode& parentNode);
    bool readValues(OPCUAClient& client, double& voltage, double& current, 
                    double& resistance, double& power) const;
    std::vector<std::pair<bool, double>> readAllValues(OPCUAClient& client) const;
    void printStatus() const;
    
    const OPCUANode& getDeviceNode() const;
    const OPCUANode& getVoltageNode() const;
    const OPCUANode& getCurrentNode() const;
    const OPCUANode& getResistanceNode() const;
    const OPCUANode& getPowerNode() const;
    const std::vector<OPCUANode>& getAllNodes() const;
};


class MachineDevice {
private:
    OPCUANode deviceNode;
    OPCUANode flywheelRPMNode;
    OPCUANode powerNode;
    OPCUANode voltageNode;
    OPCUANode energyConsumptionNode;
    OPCUANode targetRPMNode;          
    OPCUANode rpmControlModeNode;     
    std::vector<OPCUANode> allNodes;

public:
    MachineDevice();
    bool initialize(OPCUAClient& client, const OPCUANode& parentNode);
    bool readValues(OPCUAClient& client, double& rpm, double& power, 
                    double& voltage, double& energy) const;
    std::vector<std::pair<bool, double>> readAllValues(OPCUAClient& client) const;
    bool setTargetRPM(OPCUAClient& client, double rpm);  
    bool setControlMode(OPCUAClient& client, int mode);  
    void printStatus() const;
    
    const OPCUANode& getDeviceNode() const;
    const OPCUANode& getFlywheelRPMNode() const;
    const OPCUANode& getPowerNode() const;
    const OPCUANode& getVoltageNode() const;
    const OPCUANode& getEnergyConsumptionNode() const;
    const OPCUANode& getTargetRPMNode() const;      
    const OPCUANode& getControlModeNode() const;    
    const std::vector<OPCUANode>& getAllNodes() const;
};


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
    ComputerDevice();
    bool initialize(OPCUAClient& client, const OPCUANode& parentNode);
    bool readValues(OPCUAClient& client, double& fan1, double& fan2, double& fan3,
                    double& cpuLoad, double& gpuLoad, double& ramUsage) const;
    std::vector<std::pair<bool, double>> readAllValues(OPCUAClient& client) const;
    void printStatus() const;
    
    const OPCUANode& getDeviceNode() const;
    const std::vector<OPCUANode>& getAllNodes() const;
};

#endif 