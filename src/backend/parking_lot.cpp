#include "parking_lot.h"
#include <fstream>
#include <ctime>
#include <cmath> // For std::round

ParkingLot::ParkingLot(size_t cap, double smallRate, double largeRate, const std::string& filePath)
    : capacity(cap)
    , currentCount(0)
    , hourlyRateSmall(smallRate)
    , hourlyRateLarge(largeRate)
    , dataFilePath(filePath)
{
    if (!loadData()) {
        // 如果加载数据失败，使用传入的初始值
        capacity = cap;
        currentCount = 0;
        hourlyRateSmall = smallRate;
        hourlyRateLarge = largeRate;
    }
}

bool ParkingLot::addVehicle(const std::string& plate, const std::string& type) {
    if (currentCount >= capacity || vehicles.find(plate) != vehicles.end()) {
        return false;
    }

    vehicles.emplace(plate, Vehicle(plate, type));
    currentCount++;
    saveData();
    return true;
}

bool ParkingLot::removeVehicle(const std::string& plate) {
    auto it = vehicles.find(plate);
    if (it == vehicles.end() || it->second.getExitTime() != 0) {
        return false;
    }

    Vehicle& vehicle = it->second;
    vehicle.checkout();

    // Calculate fee based on vehicle type and duration with rounding to 2 decimal places
    double hourlyRate = (vehicle.getType() == "小型") ? hourlyRateSmall : hourlyRateLarge;
    double hours = vehicle.calculateFee(std::time(nullptr));
    double fee = hours * hourlyRate;
    // Round to 2 decimal places
    fee = std::round(fee * 100) / 100.0;
    vehicle.setFee(fee);

    currentCount--;  // Decrement count when vehicle leaves
    saveData();
    return true;
}

bool ParkingLot::queryVehicle(const std::string& plate, Vehicle& outVehicle) const {
    auto it = vehicles.find(plate);
    if (it != vehicles.end()) {
        outVehicle = it->second;
        return true;
    }
    return false;
}

size_t ParkingLot::getAvailableSpaces() const {
    return capacity - currentCount;
}

size_t ParkingLot::getOccupiedSpaces() const {
    return currentCount;
}

bool ParkingLot::saveData() const {
    std::ofstream outFile(dataFilePath, std::ios::binary);
    if (!outFile) return false;
    
    // 写入车库元数据
    outFile.write(reinterpret_cast<const char*>(&capacity), sizeof(capacity));
    outFile.write(reinterpret_cast<const char*>(&currentCount), sizeof(currentCount));
    outFile.write(reinterpret_cast<const char*>(&hourlyRateSmall), sizeof(hourlyRateSmall));
    outFile.write(reinterpret_cast<const char*>(&hourlyRateLarge), sizeof(hourlyRateLarge));
    
    // 写入车辆数据数量
    size_t vehicleCount = vehicles.size();
    outFile.write(reinterpret_cast<const char*>(&vehicleCount), sizeof(vehicleCount));
    
    // 写入每个车辆的数据
    for (const auto& [plate, vehicle] : vehicles) {
        // 写入车牌号长度和内容
        size_t plateLength = plate.length();
        outFile.write(reinterpret_cast<const char*>(&plateLength), sizeof(plateLength));
        outFile.write(plate.c_str(), plateLength);
        
        // 写入车型
        std::string type = vehicle.getType();
        size_t typeLength = type.length();
        outFile.write(reinterpret_cast<const char*>(&typeLength), sizeof(typeLength));
        outFile.write(type.c_str(), typeLength);
        
        // 写入时间和费用信息
        time_t entryTime = vehicle.getEntryTime();
        time_t exitTime = vehicle.getExitTime();
        double fee = vehicle.getFee();
        
        outFile.write(reinterpret_cast<const char*>(&entryTime), sizeof(entryTime));
        outFile.write(reinterpret_cast<const char*>(&exitTime), sizeof(exitTime));
        outFile.write(reinterpret_cast<const char*>(&fee), sizeof(fee));
    }
    
    return true;
}

bool ParkingLot::loadData() {
    std::ifstream inFile(dataFilePath, std::ios::binary);
    if (!inFile) return false;
    
    // 读取车库元数据
    size_t savedCapacity;
    inFile.read(reinterpret_cast<char*>(&savedCapacity), sizeof(savedCapacity));
    
    // 验证容量是否合法
    if (savedCapacity > 0 && savedCapacity <= 1000) {  // 设置一个合理的上限
        capacity = savedCapacity;
    }

    inFile.read(reinterpret_cast<char*>(&currentCount), sizeof(currentCount));
    inFile.read(reinterpret_cast<char*>(&hourlyRateSmall), sizeof(hourlyRateSmall));
    inFile.read(reinterpret_cast<char*>(&hourlyRateLarge), sizeof(hourlyRateLarge));
    
    // 读取车辆数据数量
    size_t vehicleCount;
    inFile.read(reinterpret_cast<char*>(&vehicleCount), sizeof(vehicleCount));
    
    // 读取每个车辆的数据
    vehicles.clear();
    for (size_t i = 0; i < vehicleCount; ++i) {
        // 读取车牌号
        size_t plateLength;
        inFile.read(reinterpret_cast<char*>(&plateLength), sizeof(plateLength));
        std::string plate(plateLength, '\0');
        inFile.read(&plate[0], plateLength);
        
        // 读取车型
        size_t typeLength;
        inFile.read(reinterpret_cast<char*>(&typeLength), sizeof(typeLength));
        std::string type(typeLength, '\0');
        inFile.read(&type[0], typeLength);
        
        // 读取时间和费用信息
        time_t entryTime, exitTime;
        double fee;
        inFile.read(reinterpret_cast<char*>(&entryTime), sizeof(entryTime));
        inFile.read(reinterpret_cast<char*>(&exitTime), sizeof(exitTime));
        inFile.read(reinterpret_cast<char*>(&fee), sizeof(fee));
        
        // 创建车辆对象并设置完整信息
        Vehicle vehicle(plate, type);
        if (exitTime != 0) {
            // 如果是已出场的车辆，设置出场时间和费用
            vehicle.setEntryTime(entryTime);
            vehicle.setExitTime(exitTime);
            vehicle.setFee(fee);
        } else {
            // 如果是在场车辆，只设置入场时间
            vehicle.setEntryTime(entryTime);
        }
        
        vehicles.emplace(plate, vehicle);
    }
    
    return true;
}

void ParkingLot::setRate(double smallRate, double largeRate) {
    hourlyRateSmall = smallRate;
    hourlyRateLarge = largeRate;
    saveData();
}

std::vector<Vehicle> ParkingLot::getHistoryVehicles() const {
    std::vector<Vehicle> history;
    history.reserve(vehicles.size());
    
    for (const auto& [_, vehicle] : vehicles) {
        if (vehicle.getExitTime() != 0) {  // 只返回已离场的车辆
            history.push_back(vehicle);
        }
    }
    
    return history;
}

std::vector<Vehicle> ParkingLot::getCurrentVehicles() const {
    std::vector<Vehicle> current;
    current.reserve(currentCount);
    
    for (const auto& [_, vehicle] : vehicles) {
        if (vehicle.getExitTime() == 0) {  // 只返回在场的车辆
            current.push_back(vehicle);
        }
    }
    
    return current;
}