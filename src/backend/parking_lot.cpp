/**
 * @file parking_lot.cpp
 * @brief ParkingLot类的线程安全实现
 */
#include "parking_lot.h"
#include <fstream>
#include <ctime>
#include <cmath>
#include <chrono>

ParkingLot::ParkingLot(size_t cap, double smallRate, double largeRate, const std::string& filePath)
    : capacity(cap)
    , currentCount(0)
    , hourlyRateSmall(smallRate)
    , hourlyRateLarge(largeRate)
    , dataFilePath(filePath) {
    if (!loadData()) {
        // 如果加载失败，使用默认值初始化
        capacity = cap;
        currentCount = 0;
        hourlyRateSmall = smallRate;
        hourlyRateLarge = largeRate;
    }
}

bool ParkingLot::addVehicle(const std::string& plate, const std::string& type, int timeout_ms) {
    std::unique_lock<std::shared_mutex> lock(mutex);

    // 1. 检查车辆是否已存在
    if (vehicles.find(plate) != vehicles.end()) {
        return false;
    }

    // 2. 如果停车场已满，根据timeout_ms决定是等待还是直接返回
    if (currentCount >= capacity) {
        if (timeout_ms == 0) {
            return false;  // 不等待，直接返回
        }
        
        // 等待车位，使用条件变量避免忙等
        auto waitResult = spaceAvailable.wait_for(lock, 
            std::chrono::milliseconds(timeout_ms),
            [this]() { return currentCount < capacity; });
        
        if (!waitResult) {
            return false;  // 等待超时
        }
    }

    // 3. 添加车辆信息
    vehicles.emplace(plate, Vehicle(plate, type));
    currentCount++;

    // 4. 保存更新后的数据
    lock.unlock();  // 先解锁，避免在文件IO时持有锁
    saveData();
    
    return true;
}

bool ParkingLot::removeVehicle(const std::string& plate) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    
    // 1. 查找车辆
    auto it = vehicles.find(plate);
    if (it == vehicles.end() || it->second.getExitTime() != 0) {
        return false;
    }

    // 2. 更新车辆状态
    Vehicle& vehicle = it->second;
    vehicle.checkout();  // 记录出场时间

    // 3. 计算费用
    double hourlyRate = (vehicle.getType() == "小型") ? hourlyRateSmall : hourlyRateLarge;
    double hours = vehicle.calculateFee(std::time(nullptr));
    double fee = std::round(hours * hourlyRate * 100) / 100.0;
    vehicle.setFee(fee);

    // 4. 更新车位计数并通知等待的线程
    currentCount--;
    spaceAvailable.notify_one();

    // 5. 保存更新后的数据
    lock.unlock();  // 先解锁，避免在文件IO时持有锁
    saveData();
    
    return true;
}

bool ParkingLot::queryVehicle(const std::string& plate, Vehicle& outVehicle) const {
    // 使用共享锁(读锁)，允许多个线程同时读取
    std::shared_lock<std::shared_mutex> lock(mutex);
    
    auto it = vehicles.find(plate);
    if (it != vehicles.end()) {
        outVehicle = it->second;
        return true;
    }
    return false;
}

bool ParkingLot::saveData() const {
    // 1. 获取读锁，因为我们只需要读取数据来保存
    std::shared_lock<std::shared_mutex> dataLock(mutex);
    
    // 2. 获取文件锁，保护文件操作
    std::lock_guard<std::mutex> fileLock(fileMutex);
    
    std::ofstream outFile(dataFilePath, std::ios::binary);
    if (!outFile) return false;
    
    // 3. 写入停车场配置
    outFile.write(reinterpret_cast<const char*>(&capacity), sizeof(capacity));
    outFile.write(reinterpret_cast<const char*>(&currentCount), sizeof(currentCount));
    outFile.write(reinterpret_cast<const char*>(&hourlyRateSmall), sizeof(hourlyRateSmall));
    outFile.write(reinterpret_cast<const char*>(&hourlyRateLarge), sizeof(hourlyRateLarge));
    
    // 4. 写入车辆数据
    size_t vehicleCount = vehicles.size();
    outFile.write(reinterpret_cast<const char*>(&vehicleCount), sizeof(vehicleCount));
    
    for (const auto& [plate, vehicle] : vehicles) {
        size_t plateLength = plate.length();
        outFile.write(reinterpret_cast<const char*>(&plateLength), sizeof(plateLength));
        outFile.write(plate.c_str(), plateLength);
        
        std::string type = vehicle.getType();
        size_t typeLength = type.length();
        outFile.write(reinterpret_cast<const char*>(&typeLength), sizeof(typeLength));
        outFile.write(type.c_str(), typeLength);
        
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
    // 1. 获取写锁，因为我们需要更新所有数据
    std::unique_lock<std::shared_mutex> dataLock(mutex);
    
    // 2. 获取文件锁
    std::lock_guard<std::mutex> fileLock(fileMutex);
    
    std::ifstream inFile(dataFilePath, std::ios::binary);
    if (!inFile) return false;
    
    // 3. 读取停车场配置
    size_t savedCapacity;
    inFile.read(reinterpret_cast<char*>(&savedCapacity), sizeof(savedCapacity));
    
    if (savedCapacity > 0 && savedCapacity <= 1000) {
        capacity = savedCapacity;
    }

    inFile.read(reinterpret_cast<char*>(&currentCount), sizeof(currentCount));
    inFile.read(reinterpret_cast<char*>(&hourlyRateSmall), sizeof(hourlyRateSmall));
    inFile.read(reinterpret_cast<char*>(&hourlyRateLarge), sizeof(hourlyRateLarge));
    
    // 4. 读取车辆数据
    size_t vehicleCount;
    inFile.read(reinterpret_cast<char*>(&vehicleCount), sizeof(vehicleCount));
    
    vehicles.clear();
    for (size_t i = 0; i < vehicleCount; ++i) {
        size_t plateLength;
        inFile.read(reinterpret_cast<char*>(&plateLength), sizeof(plateLength));
        std::string plate(plateLength, '\0');
        inFile.read(&plate[0], plateLength);
        
        size_t typeLength;
        inFile.read(reinterpret_cast<char*>(&typeLength), sizeof(typeLength));
        std::string type(typeLength, '\0');
        inFile.read(&type[0], typeLength);
        
        time_t entryTime, exitTime;
        double fee;
        inFile.read(reinterpret_cast<char*>(&entryTime), sizeof(entryTime));
        inFile.read(reinterpret_cast<char*>(&exitTime), sizeof(exitTime));
        inFile.read(reinterpret_cast<char*>(&fee), sizeof(fee));
        
        Vehicle vehicle(plate, type);
        if (exitTime != 0) {
            vehicle.setEntryTime(entryTime);
            vehicle.setExitTime(exitTime);
            vehicle.setFee(fee);
        } else {
            vehicle.setEntryTime(entryTime);
        }
        
        vehicles.emplace(plate, vehicle);
    }
    
    return true;
}

void ParkingLot::setRate(double smallRate, double largeRate) {
    // 使用写锁保护费率更新
    std::unique_lock<std::shared_mutex> lock(mutex);
    
    hourlyRateSmall = smallRate;
    hourlyRateLarge = largeRate;
    
    // 保存更新后的配置
    lock.unlock();
    saveData();
}

std::vector<Vehicle> ParkingLot::getHistoryVehicles() const {
    // 使用读锁获取历史记录
    std::shared_lock<std::shared_mutex> lock(mutex);
    
    std::vector<Vehicle> history;
    history.reserve(vehicles.size());
    
    for (const auto& [_, vehicle] : vehicles) {
        if (vehicle.getExitTime() != 0) {
            history.push_back(vehicle);
        }
    }
    
    return history;
}

std::vector<Vehicle> ParkingLot::getCurrentVehicles() const {
    // 使用读锁获取当前车辆列表
    std::shared_lock<std::shared_mutex> lock(mutex);
    
    std::vector<Vehicle> current;
    current.reserve(currentCount);
    
    for (const auto& [_, vehicle] : vehicles) {
        if (vehicle.getExitTime() == 0) {
            current.push_back(vehicle);
        }
    }
    
    return current;
}