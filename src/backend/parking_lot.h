/**
 * @file parking_lot.h
 * @brief 停车场管理类的声明，处理车辆进出和费用计算的核心业务逻辑
 */
#pragma once
#include "vehicle.h"
#include <vector>
#include <map>
#include <string>

/**
 * @class ParkingLot
 * @brief 停车场管理类
 * 
 * 该类负责：
 * 1. 管理车位资源（车位分配和释放）
 * 2. 处理车辆进出（入场登记和出场结算）
 * 3. 费率管理（不同类型车辆的收费标准）
 * 4. 数据持久化（停车记录的存储和加载）
 */
class ParkingLot {
private:
    std::map<std::string, Vehicle> vehicles;   // 车牌号到车辆信息的映射表
    size_t capacity;                           // 停车场总车位数
    size_t currentCount;                       // 当前占用的车位数
    double hourlyRateSmall;                    // 小型车每小时费率（元/小时）
    double hourlyRateLarge;                    // 大型车每小时费率（元/小时）
    
    std::string dataFilePath;                  // 数据文件路径，用于持久化存储

public:
    /**
     * @brief 构造函数
     * @param capacity 停车场容量（默认100个车位）
     * @param smallRate 小型车每小时费率（默认5.0元）
     * @param largeRate 大型车每小时费率（默认8.0元）
     * @param filePath 数据文件路径（默认为"parking_data.dat"）
     * 
     * 初始化停车场，并尝试从文件加载历史数据
     * 如果加载失败，则使用默认参数初始化
     */
    ParkingLot(size_t capacity = 100, 
              double smallRate = 5.0, 
              double largeRate = 8.0,
              const std::string& filePath = "parking_data.dat");
    
    /**
     * @brief 处理车辆入场
     * @param plate 车牌号
     * @param type 车辆类型（小型/大型）
     * @return 是否成功添加
     * 
     * 返回false的情况：
     * 1. 停车场已满
     * 2. 该车牌号的车辆已在场内
     */
    bool addVehicle(const std::string& plate, const std::string& type);
    
    /**
     * @brief 处理车辆出场
     * @param plate 车牌号
     * @return 是否成功移除
     * 
     * 返回false的情况：
     * 1. 找不到该车牌号的车辆
     * 2. 该车辆已经出场
     */
    bool removeVehicle(const std::string& plate);
    
    /**
     * @brief 查询车辆信息
     * @param plate 车牌号
     * @param[out] outVehicle 输出参数，用于存储查询结果
     * @return 是否找到该车辆
     */
    bool queryVehicle(const std::string& plate, Vehicle& outVehicle) const;
    
    /**
     * @brief 获取空余车位数
     * @return 当前可用的车位数量
     */
    size_t getAvailableSpaces() const;

    /**
     * @brief 获取已占用车位数
     * @return 当前已停车辆数量
     */
    size_t getOccupiedSpaces() const;
    
    /**
     * @brief 保存停车场数据到文件
     * @return 是否成功保存
     * 
     * 将当前所有数据（包括配置和车辆信息）保存到文件
     */
    bool saveData() const;

    /**
     * @brief 从文件加载停车场数据
     * @return 是否成功加载
     * 
     * 从文件恢复停车场状态，包括：
     * 1. 场地配置（容量、费率等）
     * 2. 车辆信息（在场车辆和历史记录）
     */
    bool loadData();
    
    /**
     * @brief 更新停车费率
     * @param smallRate 小型车新费率
     * @param largeRate 大型车新费率
     * 
     * 更新费率后会自动保存到文件
     */
    void setRate(double smallRate, double largeRate);
    
    /**
     * @brief 获取历史停车记录
     * @return 包含所有已离场车辆信息的vector
     */
    std::vector<Vehicle> getHistoryVehicles() const;
    
    /**
     * @brief 获取当前在场车辆列表
     * @return 包含所有在场车辆信息的vector
     */
    std::vector<Vehicle> getCurrentVehicles() const;

    /**
     * @brief 获取小型车费率
     * @return 小型车每小时费率
     */
    double getSmallRate() const { return hourlyRateSmall; }

    /**
     * @brief 获取大型车费率
     * @return 大型车每小时费率
     */
    double getLargeRate() const { return hourlyRateLarge; }
};