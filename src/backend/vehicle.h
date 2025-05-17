#pragma once
#include <string>
#include <ctime>

class Vehicle {
private:
    std::string licensePlate;  // 车牌号
    std::string type;          // 车型（小型/大型）
    time_t entryTime;          // 入场时间
    time_t exitTime;           // 离场时间（0表示未离场）
    double fee;                // 费用

public:
    // 默认构造函数
    Vehicle() : entryTime(0), exitTime(0), fee(0.0) {}
    
    // 带参数的构造函数
    Vehicle(const std::string& plate, const std::string& vType);
    
    // 获取信息
    std::string getLicensePlate() const;
    std::string getType() const;
    time_t getEntryTime() const;
    time_t getExitTime() const;
    double getFee() const;
    
    // 更新离场信息
    void checkout();
    
    // 计算当前费用（虚函数预留扩展）
    virtual double calculateFee(time_t currentTime) const;
    
    // 设置费用
    void setFee(double newFee);
    
    // Add setter methods
    void setEntryTime(time_t time);
    void setExitTime(time_t time);
};