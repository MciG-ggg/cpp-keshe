/**
 * @file parking_lot.h
 * @brief 停车场管理类 - 线程安全实现
 */
#pragma once
#include "vehicle.h"
#include <vector>
#include <map>
#include <string>
#include <mutex>           // 用于线程同步
#include <shared_mutex>    // 用于读写锁
#include <condition_variable>  // 用于条件等待

/**
 * @class ParkingLot
 * @brief 线程安全的停车场管理类
 * 
 * 并发安全保证：
 * 1. 使用读写锁(shared_mutex)保护共享数据
 * 2. 使用条件变量实现车位等待功能
 * 3. RAII风格的锁管理
 * 4. 死锁预防措施
 */
class ParkingLot {
private:
    std::map<std::string, Vehicle> vehicles;   // 车辆信息映射表
    size_t capacity;                           // 停车场容量
    size_t currentCount;                       // 当前占用车位数
    double hourlyRateSmall;                    // 小型车费率
    double hourlyRateLarge;                    // 大型车费率
    std::string dataFilePath;                  // 数据文件路径

    // 并发控制
    mutable std::shared_mutex mutex;           // 读写锁
    std::condition_variable_any spaceAvailable; // 等待车位
    mutable std::mutex fileMutex;              // 文件操作互斥锁

public:
    /**
     * @brief 构造函数
     * @param capacity 停车场容量
     * @param smallRate 小型车费率
     * @param largeRate 大型车费率
     * @param filePath 数据文件路径
     * 
     * 线程安全：构造函数天然线程安全，无需加锁
     */
    ParkingLot(size_t capacity = 100, 
               double smallRate = 5.0, 
               double largeRate = 8.0,
               const std::string& filePath = "parking_data.dat");

    // 禁止拷贝构造和赋值，避免并发安全问题
    ParkingLot(const ParkingLot&) = delete;
    ParkingLot& operator=(const ParkingLot&) = delete;

    /**
     * @brief 车辆入场（线程安全）
     * @param plate 车牌号
     * @param type 车型
     * @param timeout_ms 等待超时时间(毫秒)，0表示不等待
     * @return 是否成功添加
     * 
     * 并发控制：
     * 1. 使用写锁保护vehicles和currentCount
     * 2. 支持超时等待车位
     * 3. 条件变量避免忙等
     */
    bool addVehicle(const std::string& plate, const std::string& type, int timeout_ms = 0);

    /**
     * @brief 车辆出场（线程安全）
     * @param plate 车牌号
     * @return 是否成功移除
     * 
     * 并发控制：
     * 1. 使用写锁保护数据修改
     * 2. 通知等待车位的线程
     */
    bool removeVehicle(const std::string& plate);

    /**
     * @brief 查询车辆信息（线程安全）
     * @param plate 车牌号
     * @param[out] outVehicle 输出参数
     * @return 是否找到车辆
     * 
     * 并发控制：使用读锁，允许多个线程同时查询
     */
    bool queryVehicle(const std::string& plate, Vehicle& outVehicle) const;

    /**
     * @brief 获取可用车位数（线程安全）
     * @return 可用车位数
     * 
     * 并发控制：使用读锁，因只读取不修改数据
     */
    size_t getAvailableSpaces() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return capacity - currentCount;
    }

    /**
     * @brief 获取已占用车位数（线程安全）
     * @return 已占用车位数
     * 
     * 并发控制：使用读锁，因只读取不修改数据
     */
    size_t getOccupiedSpaces() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return currentCount;
    }

    /**
     * @brief 保存数据到文件（线程安全）
     * @return 是否成功保存
     * 
     * 并发控制：
     * 1. 使用读锁读取数据
     * 2. 使用互斥锁保护文件操作
     */
    bool saveData() const;

    /**
     * @brief 从文件加载数据（线程安全）
     * @return 是否成功加载
     * 
     * 并发控制：
     * 1. 使用写锁保护数据更新
     * 2. 使用互斥锁保护文件操作
     */
    bool loadData();

    /**
     * @brief 更新费率（线程安全）
     * @param smallRate 小型车费率
     * @param largeRate 大型车费率
     * 
     * 并发控制：使用写锁，因为修改共享数据
     */
    void setRate(double smallRate, double largeRate);

    /**
     * @brief 获取历史记录（线程安全）
     * @return 历史记录车辆列表
     * 
     * 并发控制：使用读锁，因为只读取不修改数据
     */
    std::vector<Vehicle> getHistoryVehicles() const;

    /**
     * @brief 获取当前车辆列表（线程安全）
     * @return 当前在场车辆列表
     * 
     * 并发控制：使用读锁，因为只读取不修改数据
     */
    std::vector<Vehicle> getCurrentVehicles() const;

    /**
     * @brief 获取小型车费率（线程安全）
     * @return 小型车每小时费率
     */
    double getSmallRate() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return hourlyRateSmall;
    }

    /**
     * @brief 获取大型车费率（线程安全）
     * @return 大型车每小时费率
     */
    double getLargeRate() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        return hourlyRateLarge;
    }
};