#include "device_managers.h"
#include <iostream>

// Реализация MultimeterDevice

MultimeterDevice::MultimeterDevice() {}

bool MultimeterDevice::initialize(OPCUAClient& client, const OPCUANode& parentNode) {
    deviceNode = client.findNodeByBrowseName(parentNode, "Multimeter");
    if (!deviceNode.isValid()) {
        return false;
    }

    auto components = client.findDeviceComponents(deviceNode);
    
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

    if (voltageNode.isValid()) allNodes.push_back(voltageNode);
    if (currentNode.isValid()) allNodes.push_back(currentNode);
    if (resistanceNode.isValid()) allNodes.push_back(resistanceNode);
    if (powerNode.isValid()) allNodes.push_back(powerNode);

    return voltageNode.isValid() || currentNode.isValid() || 
           resistanceNode.isValid() || powerNode.isValid();
}

bool MultimeterDevice::readValues(OPCUAClient& client, double& voltage, double& current, 
                                  double& resistance, double& power) const {
    bool voltageOk = client.readValue(voltageNode, voltage);
    bool currentOk = client.readValue(currentNode, current);
    bool resistanceOk = client.readValue(resistanceNode, resistance);
    bool powerOk = client.readValue(powerNode, power);
    
    return voltageOk || currentOk || resistanceOk || powerOk;
}

std::vector<std::pair<bool, double>> MultimeterDevice::readAllValues(OPCUAClient& client) const {
    return client.readMultipleValues(allNodes);
}

void MultimeterDevice::printStatus() const {
    std::cout << "Мультиметр: ";
    if (voltageNode.isValid()) std::cout << "Напряжение доступно, ";
    if (currentNode.isValid()) std::cout << "Ток доступен, ";
    if (resistanceNode.isValid()) std::cout << "Сопротивление доступно, ";
    if (powerNode.isValid()) std::cout << "Мощность доступна";
    std::cout << std::endl;
}

const OPCUANode& MultimeterDevice::getDeviceNode() const { return deviceNode; }
const OPCUANode& MultimeterDevice::getVoltageNode() const { return voltageNode; }
const OPCUANode& MultimeterDevice::getCurrentNode() const { return currentNode; }
const OPCUANode& MultimeterDevice::getResistanceNode() const { return resistanceNode; }
const OPCUANode& MultimeterDevice::getPowerNode() const { return powerNode; }
const std::vector<OPCUANode>& MultimeterDevice::getAllNodes() const { return allNodes; }

// Реализация MachineDevice

MachineDevice::MachineDevice() {}

bool MachineDevice::initialize(OPCUAClient& client, const OPCUANode& parentNode) {
    deviceNode = client.findNodeByBrowseName(parentNode, "Machine");
    if (!deviceNode.isValid()) {
        return false;
    }

    auto components = client.findDeviceComponents(deviceNode);
    
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

    if (flywheelRPMNode.isValid()) allNodes.push_back(flywheelRPMNode);
    if (powerNode.isValid()) allNodes.push_back(powerNode);
    if (voltageNode.isValid()) allNodes.push_back(voltageNode);
    if (energyConsumptionNode.isValid()) allNodes.push_back(energyConsumptionNode);

    return flywheelRPMNode.isValid() || powerNode.isValid() || 
           voltageNode.isValid() || energyConsumptionNode.isValid();
}

bool MachineDevice::readValues(OPCUAClient& client, double& rpm, double& power, 
                               double& voltage, double& energy) const {
    bool rpmOk = client.readValue(flywheelRPMNode, rpm);
    bool powerOk = client.readValue(powerNode, power);
    bool voltageOk = client.readValue(voltageNode, voltage);
    bool energyOk = client.readValue(energyConsumptionNode, energy);
    
    return rpmOk || powerOk || voltageOk || energyOk;
}

std::vector<std::pair<bool, double>> MachineDevice::readAllValues(OPCUAClient& client) const {
    return client.readMultipleValues(allNodes);
}

bool MachineDevice::setRPMValue(OPCUAClient& client, double rpm) {
    return client.writeValue(flywheelRPMNode, rpm);
}

void MachineDevice::printStatus() const {
    std::cout << "Станок: ";
    if (flywheelRPMNode.isValid()) std::cout << "Обороты доступны, ";
    if (powerNode.isValid()) std::cout << "Мощность доступна, ";
    if (voltageNode.isValid()) std::cout << "Напряжение доступно, ";
    if (energyConsumptionNode.isValid()) std::cout << "Энергопотребление доступно";
    std::cout << std::endl;
}

const OPCUANode& MachineDevice::getDeviceNode() const { return deviceNode; }
const OPCUANode& MachineDevice::getFlywheelRPMNode() const { return flywheelRPMNode; }
const OPCUANode& MachineDevice::getPowerNode() const { return powerNode; }
const OPCUANode& MachineDevice::getVoltageNode() const { return voltageNode; }
const OPCUANode& MachineDevice::getEnergyConsumptionNode() const { return energyConsumptionNode; }

// Реализация ComputerDevice

ComputerDevice::ComputerDevice() {}

bool ComputerDevice::initialize(OPCUAClient& client, const OPCUANode& parentNode) {
    deviceNode = client.findNodeByBrowseName(parentNode, "Computer");
    if (!deviceNode.isValid()) {
        return false;
    }

    auto components = client.findDeviceComponents(deviceNode);
    
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

    if (fan1Node.isValid()) allNodes.push_back(fan1Node);
    if (fan2Node.isValid()) allNodes.push_back(fan2Node);
    if (fan3Node.isValid()) allNodes.push_back(fan3Node);
    if (cpuLoadNode.isValid()) allNodes.push_back(cpuLoadNode);
    if (gpuLoadNode.isValid()) allNodes.push_back(gpuLoadNode);
    if (ramUsageNode.isValid()) allNodes.push_back(ramUsageNode);

    return fan1Node.isValid() || fan2Node.isValid() || fan3Node.isValid() ||
           cpuLoadNode.isValid() || gpuLoadNode.isValid() || ramUsageNode.isValid();
}

bool ComputerDevice::readValues(OPCUAClient& client, double& fan1, double& fan2, double& fan3,
                                double& cpuLoad, double& gpuLoad, double& ramUsage) const {
    bool fan1Ok = client.readValue(fan1Node, fan1);
    bool fan2Ok = client.readValue(fan2Node, fan2);
    bool fan3Ok = client.readValue(fan3Node, fan3);
    bool cpuOk = client.readValue(cpuLoadNode, cpuLoad);
    bool gpuOk = client.readValue(gpuLoadNode, gpuLoad);
    bool ramOk = client.readValue(ramUsageNode, ramUsage);
    
    return fan1Ok || fan2Ok || fan3Ok || cpuOk || gpuOk || ramOk;
}

std::vector<std::pair<bool, double>> ComputerDevice::readAllValues(OPCUAClient& client) const {
    return client.readMultipleValues(allNodes);
}

void ComputerDevice::printStatus() const {
    std::cout << "Компьютер: ";
    if (fan1Node.isValid()) std::cout << "Вентилятор1 доступен, ";
    if (fan2Node.isValid()) std::cout << "Вентилятор2 доступен, ";
    if (fan3Node.isValid()) std::cout << "Вентилятор3 доступен, ";
    if (cpuLoadNode.isValid()) std::cout << "ЦП доступен, ";
    if (gpuLoadNode.isValid()) std::cout << "ГП доступен, ";
    if (ramUsageNode.isValid()) std::cout << "ОЗУ доступно";
    std::cout << std::endl;
}

const OPCUANode& ComputerDevice::getDeviceNode() const { return deviceNode; }