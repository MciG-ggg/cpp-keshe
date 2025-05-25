/**
 * @file main.cpp
 * @brief 停车场管理系统的主程序入口
 * 
 * 该文件负责：
 * 1. 创建和启动API服务器
 * 2. 提供命令行界面功能
 * 3. 处理异常情况
 */
#include "include/api_server.h"
#include <iostream>
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
        std::cout << "Starting Parking Management API Server..." << std::endl;
        
        // 创建服务器实例
        // 参数：
        // - 容量：100个车位
        // - 小型车费率：5.0元/小时
        // - 大型车费率：8.0元/小时
        ParkingApiServer server(100, 5.0, 8.0);
        
        // 打印服务器信息和API接口说明
        std::cout << "Server is running on http://localhost:8080" << std::endl;
        std::cout << "Available endpoints:" << std::endl;
        std::cout << "POST   /api/vehicle       - Add a new vehicle" << std::endl;
        std::cout << "DELETE /api/vehicle/:plate - Remove a vehicle" << std::endl;
        std::cout << "GET    /api/vehicle/:plate - Query vehicle info" << std::endl;
        std::cout << "GET    /api/status        - Get parking lot status" << std::endl;
        std::cout << "PUT    /api/rate          - Update parking rates" << std::endl;
        std::cout << "GET    /api/history       - Get parking history" << std::endl;
        
        // 启动服务器并监听8080端口
        server.start(8080);
        return 0;  // 正常退出
        
    } catch (const std::exception& e) {
        // 捕获并打印所有异常
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;  // 错误退出
    }
}