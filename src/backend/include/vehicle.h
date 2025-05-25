/**
 * @file vehicle.h
 * @brief 车辆类的声明，管理停车场中的车辆信息
 */
#pragma once
#include <string>
#include <ctime>

/**
 * @class Vehicle
 * @brief 表示一个停车场中的车辆实体
 * 
 * 该类包含车辆的基本信息（车牌号、车型）和停车相关信息（入场时间、出场时间、费用）
 * 提供了完整的车辆信息管理功能，包括获取信息、更新状态和费用计算
 */
class Vehicle {
private:
    std::string licensePlate;  // 车牌号（例：苏A12345）
    std::string type;          // 车型（小型/大型）
    time_t entryTime;          // 入场时间（Unix时间戳）
    time_t exitTime;           // 离场时间（0表示未离场）
    double fee;                // 费用（单位：元）

public:
    /**
     * @brief 默认构造函数
     * 初始化一个空的车辆对象，所有时间字段设为0，费用设为0.0
     */
    Vehicle() : entryTime(0), exitTime(0), fee(0.0) {}
    
    /**
     * @brief 带参数的构造函数
     * @param plate 车牌号
     * @param vType 车辆类型（小型/大型）
     * 创建时自动记录当前时间为入场时间
     */
    Vehicle(const std::string& plate, const std::string& vType);
    
    /**
     * @brief 获取车牌号
     * @return 车辆的车牌号
     */
    std::string getLicensePlate() const;

    /**
     * @brief 获取车型
     * @return 车辆类型（小型/大型）
     */
    std::string getType() const;

    /**
     * @brief 获取入场时间
     * @return Unix时间戳格式的入场时间
     */
    time_t getEntryTime() const;

    /**
     * @brief 获取出场时间
     * @return Unix时间戳格式的出场时间，0表示尚未出场
     */
    time_t getExitTime() const;

    /**
     * @brief 获取停车费用
     * @return 当前计算的停车费用
     */
    double getFee() const;
    
    /**
     * @brief 登记车辆出场
     * 记录当前时间为出场时间，仅在车辆未出场时更新
     */
    void checkout();
    
    /**
     * @brief 计算停车费用
     * @param currentTime 计算费用时的时间点
     * @return 停车时长（小时）
     * 
     * 虚函数设计允许不同计费策略的扩展实现
     */
    virtual double calculateFee(time_t currentTime) const;
    
    /**
     * @brief 设置停车费用
     * @param newFee 新的费用金额
     */
    void setFee(double newFee);
    
    /**
     * @brief 设置入场时间
     * @param time Unix时间戳格式的入场时间
     * 主要用于数据加载时还原历史记录
     */
    void setEntryTime(time_t time);

    /**
     * @brief 设置出场时间
     * @param time Unix时间戳格式的出场时间
     * 主要用于数据加载时还原历史记录
     */
    void setExitTime(time_t time);
};