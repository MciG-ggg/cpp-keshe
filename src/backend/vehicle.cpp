/**
 * @file vehicle.cpp
 * @brief Vehicle类的实现文件
 */
#include "vehicle.h"

Vehicle::Vehicle(const std::string& plate, const std::string& vType)
    : licensePlate(plate)     // 初始化车牌号
    , type(vType)            // 初始化车型
    , entryTime(std::time(nullptr))  // 获取当前系统时间作为入场时间
    , exitTime(0)            // 初始化出场时间为0（表示未出场）
    , fee(0.0)              // 初始化费用为0
{
    // 使用初始化列表完成所有成员初始化
    // 这样做比在构造函数体内赋值更高效
}

std::string Vehicle::getLicensePlate() const {
    // 返回车牌号（使用const引用避免不必要的复制）
    return licensePlate;
}

std::string Vehicle::getType() const {
    // 返回车型（使用const引用避免不必要的复制）
    return type;
}

time_t Vehicle::getEntryTime() const {
    // 返回入场时间戳
    return entryTime;
}

time_t Vehicle::getExitTime() const {
    // 返回出场时间戳（0表示未出场）
    return exitTime;
}

double Vehicle::getFee() const {
    // 返回当前计算的停车费用
    return fee;
}

void Vehicle::checkout() {
    // 只有在车辆未出场时（exitTime为0）才更新出场时间
    if (exitTime == 0) {
        // 记录当前系统时间为出场时间
        exitTime = std::time(nullptr);
    }
}

double Vehicle::calculateFee(time_t currentTime) const {
    // 计算停车时长（小时）
    // 如果车辆已出场，使用exitTime计算
    // 如果车辆在场，使用传入的currentTime计算
    time_t endTime = exitTime > 0 ? exitTime : currentTime;
    
    // 使用difftime计算时间差（秒），然后转换为小时
    // difftime返回两个time_t之间的秒数差
    double hours = std::difftime(endTime, entryTime) / 3600.0;
    
    return hours;  // 返回停车时长（小时）
}

void Vehicle::setFee(double newFee) {
    // 更新停车费用
    // 通常在车辆出场时，根据时长和费率计算后调用
    fee = newFee;
}

void Vehicle::setEntryTime(time_t time) {
    // 设置入场时间
    // 主要用于从文件加载历史数据时还原状态
    entryTime = time;
}

void Vehicle::setExitTime(time_t time) {
    // 设置出场时间
    // 主要用于从文件加载历史数据时还原状态
    exitTime = time;
}