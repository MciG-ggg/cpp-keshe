#pragma once
#include "parking_lot.h"
#include <memory>
#include <string>
#include <map>
#include <functional>
#include <sstream>
#include <cstdint>
#include <vector>

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> params;
};

class HttpResponse {
public:
    int status;
    std::string body;
    std::map<std::string, std::string> headers;

    HttpResponse(int s = 200) : status(s) {
        headers["Content-Type"] = "application/json";
    }
};

class ParkingApiServer {
private:
    // 定义路由处理器类型
    using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;
    
    // 定义路由表项结构
    struct Route {
        std::string method;
        std::string path;
        RouteHandler handler;
        bool isPrefix;  // 是否是前缀匹配
    };

    std::vector<Route> routes;  // 路由表
    std::unique_ptr<ParkingLot> parkingLot;
    int serverSocket;
    bool running;

    // 初始化路由表
    void initializeRoutes();

    // API处理函数
    HttpResponse handleAddVehicle(const HttpRequest& req);
    HttpResponse handleRemoveVehicle(const HttpRequest& req);
    HttpResponse handleQueryVehicle(const HttpRequest& req);
    HttpResponse handleGetParkingStatus(const HttpRequest& req);
    HttpResponse handleSetRate(const HttpRequest& req);
    HttpResponse handleGetHistory(const HttpRequest& req);
    HttpResponse handleGetCurrentVehicles(const HttpRequest& req);

    // 静态文件处理
    HttpResponse handleStaticFile(const std::string& path);

    // 辅助函数
    HttpRequest parseRequest(int clientSocket);
    void sendResponse(int clientSocket, const HttpResponse& response);
    std::string createJsonResponse(bool success, const std::string& message, const std::string& data = "");

    // 路由匹配和分发
    HttpResponse routeRequest(const HttpRequest& request);

public:
    ParkingApiServer(size_t capacity = 100, double smallRate = 5.0, double largeRate = 8.0);
    ~ParkingApiServer();

    void start(uint16_t port = 8080);
    void stop();
};