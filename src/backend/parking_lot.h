#pragma once
#include "vehicle.h"
#include <vector>
#include <map>
#include <string>

class ParkingLot {
private:
    std::map<std::string, Vehicle> vehicles;  // 车牌号 -> 车辆信息
    size_t capacity;                          // 车库容量
    size_t currentCount;                      // 当前车辆数
    double hourlyRateSmall;                   // 小型车费率
    double hourlyRateLarge;                   // 大型车费率
    
    // 文件持久化相关
    std::string dataFilePath;

public:
    // 初始化车库
    ParkingLot(size_t capacity = 100, 
              double smallRate = 5.0, 
              double largeRate = 8.0,
              const std::string& filePath = "parking_data.dat");
    
    // 入库操作（核心API）
    bool addVehicle(const std::string& plate, const std::string& type);
    
    // 出库操作（核心API）
    bool removeVehicle(const std::string& plate);
    
    // 查询车辆状态
    bool queryVehicle(const std::string& plate, Vehicle& outVehicle) const;
    
    // 获取实时车位统计
    size_t getAvailableSpaces() const;
    size_t getOccupiedSpaces() const;
    
    // 文件持久化接口
    bool saveData() const;
    bool loadData();
    
    // 扩展功能：动态调整费率
    void setRate(double smallRate, double largeRate);
    
    // 扩展功能：获取历史记录
    std::vector<Vehicle> getHistoryVehicles() const;
    
    // 获取当前在场车辆
    std::vector<Vehicle> getCurrentVehicles() const;

    // 获取费率
    double getSmallRate() const { return hourlyRateSmall; }
    double getLargeRate() const { return hourlyRateLarge; }
};