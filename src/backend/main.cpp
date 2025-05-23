/**
 * @file main.cpp
 * @brief 主程序入口，用于测试停车场管理系统的核心功能
 */
#include "parking_lot.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <iomanip>

/**
 * @brief 打印车辆详细信息到控制台
 * @param vehicle 要打印信息的车辆对象
 * 
 * 格式化输出车辆的所有信息，包括：
 * - 车牌号
 * - 车型
 * - 入场时间（格式化的日期时间）
 * - 出场时间（如果已出场）
 * - 费用
 */
void printVehicleInfo(const Vehicle& vehicle) {
    // 打印基本信息
    std::cout << "车牌号: " << vehicle.getLicensePlate() << std::endl;
    std::cout << "车型: " << vehicle.getType() << std::endl;
    
    // 格式化打印入场时间
    time_t entryTime = vehicle.getEntryTime();
    std::cout << "入场时间: " << std::put_time(std::localtime(&entryTime), "%Y-%m-%d %H:%M:%S") << std::endl;
    
    // 如果已出场，打印出场时间
    time_t exitTime = vehicle.getExitTime();
    if (exitTime != 0) {
        std::cout << "离场时间: " << std::put_time(std::localtime(&exitTime), "%Y-%m-%d %H:%M:%S") << std::endl;
    }
    
    // 打印费用信息
    std::cout << "费用: " << vehicle.getFee() << " 元" << std::endl;
    std::cout << "------------------------" << std::endl;
}

/**
 * @brief 程序入口点
 * 
 * 主程序流程：
 * 1. 创建API服务器实例
 * 2. 显示服务器信息和可用接口
 * 3. 启动服务器并监听请求
 * 4. 处理异常情况
 * 
 * @return 0表示正常退出，1表示发生错误
 */
int main() {
    try {
        // 创建一个容量为5的停车场，小型车费率10元/小时，大型车费率20元/小时
        ParkingLot parkingLot(5, 10.0, 20.0);
        
        std::cout << "初始状态：" << std::endl;
        std::cout << "总车位：" << parkingLot.getAvailableSpaces() + parkingLot.getOccupiedSpaces() << std::endl;
        std::cout << "可用车位：" << parkingLot.getAvailableSpaces() << std::endl;
        std::cout << "已用车位：" << parkingLot.getOccupiedSpaces() << std::endl;
        std::cout << "------------------------" << std::endl;

        // 测试1：添加车辆
        std::cout << "测试1：添加车辆" << std::endl;
        if (parkingLot.addVehicle("苏A12345", "小型")) {
            std::cout << "车辆苏A12345成功入场" << std::endl;
        }
        if (parkingLot.addVehicle("苏B12345", "大型")) {
            std::cout << "车辆苏B12345成功入场" << std::endl;
        }

        // 测试2：查询车辆
        std::cout << "\n测试2：查询车辆信息" << std::endl;
        Vehicle v1;
        if (parkingLot.queryVehicle("苏A12345", v1)) {
            printVehicleInfo(v1);
        }

        // 测试3：模拟经过一小时
        std::cout << "测试3：等待3秒钟（模拟经过一段时间）..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // 测试4：车辆出场
        std::cout << "\n测试4：车辆出场" << std::endl;
        if (parkingLot.removeVehicle("苏A12345")) {
            Vehicle v2;
            if (parkingLot.queryVehicle("苏A12345", v2)) {
                printVehicleInfo(v2);
            }
        }

        // 测试5：尝试添加已存在的车牌号
        std::cout << "\n测试5：尝试添加已存在的车牌号" << std::endl;
        if (!parkingLot.addVehicle("苏B12345", "小型")) {
            std::cout << "车辆已在停车场内，添加失败" << std::endl;
        }

        // 测试6：停车场容量限制
        std::cout << "\n测试6：测试停车场容量限制" << std::endl;
        for (int i = 0; i < 5; ++i) {
            std::string plate = "苏C" + std::to_string(i);
            if (parkingLot.addVehicle(plate, "小型")) {
                std::cout << "车辆" << plate << "成功入场" << std::endl;
            } else {
                std::cout << "停车场已满，车辆" << plate << "无法入场" << std::endl;
            }
        }

        // 打印最终状态
        std::cout << "\n最终状态：" << std::endl;
        std::cout << "总车位：" << parkingLot.getAvailableSpaces() + parkingLot.getOccupiedSpaces() << std::endl;
        std::cout << "可用车位：" << parkingLot.getAvailableSpaces() << std::endl;
        std::cout << "已用车位：" << parkingLot.getOccupiedSpaces() << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "错误：" << e.what() << std::endl;
        return 1;
    }

    return 0;
}