/**
 * @file api_server.h
 * @brief HTTP服务器和线程池的声明
 */
#pragma once
#include "parking_lot.h"
#include <memory>
#include <string>
#include <map>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
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

/**
 * @class ThreadPool
 * @brief 线程池实现，用于管理工作线程
 * 
 * 特性：
 * 1. 固定数量的工作线程
 * 2. 任务队列
 * 3. 优雅关闭机制
 * 4. 任务超时处理
 */
class ThreadPool {
public:
    /**
     * @brief 构造函数
     * @param threads 工作线程数量
     */
    explicit ThreadPool(size_t threads);
    
    /**
     * @brief 析构函数，确保所有线程正确关闭
     */
    ~ThreadPool();

    /**
     * @brief 提交任务到线程池
     * @param task 要执行的任务
     * @return 是否成功提交
     */
    template<class F>
    bool enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if(stop) return false;
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
        return true;
    }

private:
    std::vector<std::thread> workers;        // 工作线程集合
    std::queue<std::function<void()>> tasks; // 任务队列
    
    std::mutex queue_mutex;                  // 队列互斥锁
    std::condition_variable condition;       // 条件变量
    std::atomic<bool> stop;                  // 停止标志
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

    std::unique_ptr<ThreadPool> threadPool;  // 添加线程池
    const size_t MAX_THREADS = 32;           // 最大线程数
    const size_t MAX_QUEUE_SIZE = 100;       // 最大队列长度
    std::atomic<size_t> activeConnections;   // 当前活动连接数

public:
    ParkingApiServer(size_t capacity = 100, double smallRate = 5.0, double largeRate = 8.0);
    ~ParkingApiServer();

    void start(uint16_t port = 8080);
    void stop();
};