#include "parking_lot.h"
#include "api_server.h"
#include <iostream>
#include <iomanip>

void printVehicleInfo(const Vehicle& vehicle) {
    std::cout << "车牌号: " << vehicle.getLicensePlate() << std::endl;
    std::cout << "车型: " << vehicle.getType() << std::endl;
    
    time_t entryTime = vehicle.getEntryTime();
    std::cout << "入场时间: " << std::put_time(std::localtime(&entryTime), "%Y-%m-%d %H:%M:%S") << std::endl;
    
    time_t exitTime = vehicle.getExitTime();
    if (exitTime != 0) {
        std::cout << "离场时间: " << std::put_time(std::localtime(&exitTime), "%Y-%m-%d %H:%M:%S") << std::endl;
    }
    
    std::cout << "费用: " << vehicle.getFee() << " 元" << std::endl;
    std::cout << "------------------------" << std::endl;
}

int main() {
    try {
        std::cout << "Starting Parking Management API Server..." << std::endl;
        
        // 创建并启动API服务器
        ParkingApiServer server(100, 5.0, 8.0);
        
        std::cout << "Server is running on http://localhost:8080" << std::endl;
        std::cout << "Available endpoints:" << std::endl;
        std::cout << "POST   /api/vehicle       - Add a new vehicle" << std::endl;
        std::cout << "DELETE /api/vehicle/:plate - Remove a vehicle" << std::endl;
        std::cout << "GET    /api/vehicle/:plate - Query vehicle info" << std::endl;
        std::cout << "GET    /api/status        - Get parking lot status" << std::endl;
        std::cout << "PUT    /api/rate          - Update parking rates" << std::endl;
        std::cout << "GET    /api/history       - Get parking history" << std::endl;
        
        server.start(8080);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
}