/**
 * @file parking_lot.cpp
 * @brief ParkingLot类的具体实现
 */
#include "parking_lot.h"
#include <fstream>
#include <ctime>
#include <cmath> // 用于std::round函数

ParkingLot::ParkingLot(size_t cap, double smallRate, double largeRate, const std::string& filePath)
    : capacity(cap)           // 初始化停车场容量
    , currentCount(0)         // 初始化当前车辆数为0
    , hourlyRateSmall(smallRate)  // 设置小型车费率
    , hourlyRateLarge(largeRate)  // 设置大型车费率
    , dataFilePath(filePath)      // 设置数据文件路径
{
    // 尝试从文件加载历史数据
    if (!loadData()) {
        // 如果加载失败，使用传入的初始值
        // 这确保了即使文件不存在或损坏，系统仍能正常工作
        capacity = cap;
        currentCount = 0;
        hourlyRateSmall = smallRate;
        hourlyRateLarge = largeRate;
    }
}

bool ParkingLot::addVehicle(const std::string& plate, const std::string& type) {
    // 检查停车场是否已满或车辆是否已存在
    if (currentCount >= capacity || vehicles.find(plate) != vehicles.end()) {
        return false;  // 无法添加车辆
    }

    // 使用emplace创建新的Vehicle对象
    // emplace比insert更高效，因为它直接在map中构造对象
    vehicles.emplace(plate, Vehicle(plate, type));
    currentCount++;  // 更新当前车辆数
    
    // 保存更新后的数据到文件
    saveData();
    return true;
}

bool ParkingLot::removeVehicle(const std::string& plate) {
    // 查找车辆
    auto it = vehicles.find(plate);
    if (it == vehicles.end() || it->second.getExitTime() != 0) {
        // 车辆不存在或已经出场
        return false;
    }

    Vehicle& vehicle = it->second;
    vehicle.checkout();  // 登记出场时间

    // 根据车型和停车时长计算费用
    double hourlyRate = (vehicle.getType() == "小型") ? hourlyRateSmall : hourlyRateLarge;
    double hours = vehicle.calculateFee(std::time(nullptr));
    double fee = hours * hourlyRate;
    
    // 将费用四舍五入到2位小数
    fee = std::round(fee * 100) / 100.0;
    vehicle.setFee(fee);

    currentCount--;  // 更新当前车辆数
    saveData();     // 保存更新后的数据
    return true;
}

bool ParkingLot::queryVehicle(const std::string& plate, Vehicle& outVehicle) const {
    // 查找并返回车辆信息
    auto it = vehicles.find(plate);
    if (it != vehicles.end()) {
        outVehicle = it->second;  // 复制车辆信息到输出参数
        return true;
    }
    return false;
}

size_t ParkingLot::getAvailableSpaces() const {
    // 返回空余车位数
    return capacity - currentCount;
}

size_t ParkingLot::getOccupiedSpaces() const {
    // 返回已占用车位数
    return currentCount;
}

bool ParkingLot::saveData() const {
    // 以二进制模式打开文件
    std::ofstream outFile(dataFilePath, std::ios::binary);
    if (!outFile) return false;  // 文件打开失败
    
    // 1. 写入停车场配置信息
    // 使用reinterpret_cast进行类型转换，确保正确写入二进制数据
    outFile.write(reinterpret_cast<const char*>(&capacity), sizeof(capacity));
    outFile.write(reinterpret_cast<const char*>(&currentCount), sizeof(currentCount));
    outFile.write(reinterpret_cast<const char*>(&hourlyRateSmall), sizeof(hourlyRateSmall));
    outFile.write(reinterpret_cast<const char*>(&hourlyRateLarge), sizeof(hourlyRateLarge));
    
    // 2. 写入车辆数量
    size_t vehicleCount = vehicles.size();
    outFile.write(reinterpret_cast<const char*>(&vehicleCount), sizeof(vehicleCount));
    
    // 3. 写入每个车辆的详细信息
    for (const auto& [plate, vehicle] : vehicles) {
        // 写入车牌号（先写长度，再写内容）
        size_t plateLength = plate.length();
        outFile.write(reinterpret_cast<const char*>(&plateLength), sizeof(plateLength));
        outFile.write(plate.c_str(), plateLength);
        
        // 写入车型信息
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
    
    return true;  // 保存成功
}

bool ParkingLot::loadData() {
    // 以二进制模式打开文件
    std::ifstream inFile(dataFilePath, std::ios::binary);
    if (!inFile) return false;  // 文件打开失败
    
    // 1. 读取停车场配置信息
    size_t savedCapacity;
    inFile.read(reinterpret_cast<char*>(&savedCapacity), sizeof(savedCapacity));
    
    // 验证容量合法性（设置上限防止异常数据）
    if (savedCapacity > 0 && savedCapacity <= 1000) {
        capacity = savedCapacity;
    }

    // 读取其他配置信息
    inFile.read(reinterpret_cast<char*>(&currentCount), sizeof(currentCount));
    inFile.read(reinterpret_cast<char*>(&hourlyRateSmall), sizeof(hourlyRateSmall));
    inFile.read(reinterpret_cast<char*>(&hourlyRateLarge), sizeof(hourlyRateLarge));
    
    // 2. 读取车辆数量
    size_t vehicleCount;
    inFile.read(reinterpret_cast<char*>(&vehicleCount), sizeof(vehicleCount));
    
    // 3. 读取每个车辆的信息
    vehicles.clear();  // 清空现有数据
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
        
        // 创建并设置车辆对象
        Vehicle vehicle(plate, type);
        if (exitTime != 0) {
            // 已出场车辆需要设置完整信息
            vehicle.setEntryTime(entryTime);
            vehicle.setExitTime(exitTime);
            vehicle.setFee(fee);
        } else {
            // 在场车辆只需设置入场时间
            vehicle.setEntryTime(entryTime);
        }
        
        // 将车辆信息添加到map中
        vehicles.emplace(plate, vehicle);
    }
    
    return true;  // 加载成功
}

void ParkingLot::setRate(double smallRate, double largeRate) {
    // 更新费率
    hourlyRateSmall = smallRate;
    hourlyRateLarge = largeRate;
    // 保存更新后的配置
    saveData();
}

std::vector<Vehicle> ParkingLot::getHistoryVehicles() const {
    // 创建结果vector，预留足够空间避免重复分配
    std::vector<Vehicle> history;
    history.reserve(vehicles.size());
    
    // 遍历所有车辆，筛选已出场的车辆
    for (const auto& [_, vehicle] : vehicles) {
        if (vehicle.getExitTime() != 0) {  // exitTime非0表示已出场
            history.push_back(vehicle);
        }
    }
    
    return history;
}

std::vector<Vehicle> ParkingLot::getCurrentVehicles() const {
    // 创建结果vector，预留空间为当前在场车辆数
    std::vector<Vehicle> current;
    current.reserve(currentCount);
    
    // 遍历所有车辆，筛选在场的车辆
    for (const auto& [_, vehicle] : vehicles) {
        if (vehicle.getExitTime() == 0) {  // exitTime为0表示在场
            current.push_back(vehicle);
        }
    }
    
    return current;
}