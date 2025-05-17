/**
 * @file api_server.cpp
 * @brief 停车场管理系统HTTP服务器的核心实现
 * 
 * 主要功能：
 * 1. HTTP服务器实现（请求解析、响应生成）
 * 2. RESTful API路由系统
 * 3. 静态文件服务
 * 4. 跨域资源共享(CORS)处理
 * 5. 多线程并发处理
 * 6. JSON数据处理
 * 
 * 技术要点：
 * - Socket网络编程
 * - HTTP协议实现
 * - 多线程并发
 * - 文件系统操作
 * - 异常处理机制
 */

#include "api_server.h"
#include <sys/socket.h>     // 提供Socket API
#include <netinet/in.h>     // 提供网络地址结构
#include <unistd.h>         // 提供Unix标准系统调用
#include <cstring>          // 字符串操作
#include <iostream>         // 标准输入输出
#include <thread>          // 线程支持
#include <sstream>         // 字符串流
#include <vector>          // 动态数组
#include <algorithm>       // 算法库
#include <iomanip>         // 输出格式控制
#include <fstream>         // 文件操作
#include <filesystem>      // 文件系统操作(C++17)

namespace fs = std::filesystem;

/**
 * @brief MIME类型映射表
 * 用于设置HTTP响应的Content-Type头
 * 
 * 支持的文件类型：
 * - HTML (.html)：网页文件
 * - CSS (.css)：样式表
 * - JavaScript (.js)：脚本文件
 * - JSON (.json)：数据交换
 * - 图片 (.png, .jpg, .jpeg)：图像文件
 * - 图标 (.ico)：网站图标
 */
std::map<std::string, std::string> MIME_TYPES = {
    {".html", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".ico", "image/x-icon"}
};

/**
 * @brief URL解码函数
 * @param encoded URL编码的字符串
 * @return 解码后的原始字符串
 * 
 * 实现细节：
 * 1. 处理%XX格式的编码字符：
 *    - %后跟两位16进制数
 *    - 将16进制转换为对应的ASCII字符
 * 2. 处理+号：转换为空格
 * 3. 保留其他字符不变
 * 
 * 错误处理：
 * - 如果%后没有足够的字符，保持原样
 * - 如果16进制转换失败，保持原样
 */
std::string urlDecode(const std::string& encoded) {
    std::string result;
    result.reserve(encoded.length()); // 预分配空间，避免频繁重新分配

    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%') {
            // 确保后面还有两个字符
            if (i + 2 < encoded.length()) {
                std::string hex = encoded.substr(i + 1, 2);
                int value;
                std::istringstream(hex) >> std::hex >> value;
                result += static_cast<char>(value);
                i += 2; // 跳过已处理的两个字符
            }
        } else if (encoded[i] == '+') {
            result += ' ';
        } else {
            result += encoded[i];
        }
    }
    return result;
}

/**
 * @brief ThreadPool构造函数实现
 */
ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for(size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [this] {
                        return stop || !tasks.empty();
                    });
                    
                    if(stop && tasks.empty()) {
                        return;
                    }
                    
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();  // 执行任务
            }
        });
    }
}

/**
 * @brief ThreadPool析构函数实现
 */
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(auto& worker: workers) {
        worker.join();
    }
}

/**
 * @brief ParkingApiServer构造函数实现
 */
ParkingApiServer::ParkingApiServer(size_t capacity, double smallRate, double largeRate)
    : parkingLot(std::make_unique<ParkingLot>(capacity, smallRate, largeRate))
    , serverSocket(-1)
    , running(false)
    , threadPool(std::make_unique<ThreadPool>(std::thread::hardware_concurrency()))  // 使用CPU核心数
    , activeConnections(0) {
    initializeRoutes();
}

/**
 * @brief 获取文件的MIME类型
 * @param path 文件路径
 * @return MIME类型字符串
 * 
 * 实现细节：
 * 1. 提取文件扩展名
 * 2. 转换为小写以确保匹配
 * 3. 在MIME_TYPES映射表中查找
 * 4. 如果未找到，返回默认类型
 */
std::string getMimeType(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    auto it = MIME_TYPES.find(ext);
    return it != MIME_TYPES.end() ? it->second : "application/octet-stream";
}

/**
 * @brief 读取静态文件内容
 * @param path 文件路径
 * @return 文件内容字符串，失败返回空串
 * 
 * 实现细节：
 * 1. 以二进制模式打开文件
 * 2. 使用迭代器读取全部内容
 * 3. 自动处理文件关闭
 * 
 * 安全考虑：
 * - 使用二进制模式避免文本转换问题
 * - 通过RAII确保文件正确关闭
 */
std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

/**
 * @brief 处理静态文件请求
 * @param path 请求路径
 * @return HTTP响应对象
 * 
 * 处理流程：
 * 1. 构造完整的文件路径
 * 2. 读取文件内容
 * 3. 设置适当的响应头
 * 4. 处理可能的错误情况
 * 
 * 特殊处理：
 * - 根路径("/")自动映射到index.html
 * - 对不存在的文件返回404错误
 * - 自动设置正确的Content-Type
 * - 添加CORS相关头部
 */
HttpResponse ParkingApiServer::handleStaticFile(const std::string& path) {
    // 构造完整文件路径，处理根路径特殊情况
    std::string fullPath = "src/frontend" + (path == "/" ? "/index.html" : path);
    std::string content = readFile(fullPath);
    
    if (content.empty()) {
        // 文件不存在或无法读取
        std::cerr << "Failed to read file: " << fullPath << std::endl;
        HttpResponse response(404);
        response.body = createJsonResponse(false, "File not found");
        return response;
    }

    // 构造成功响应
    HttpResponse response(200);
    response.headers["Content-Type"] = getMimeType(fullPath);
    // 设置CORS相关头部
    response.headers["Access-Control-Allow-Origin"] = "*";
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type";
    response.body = content;
    return response;
}

/**
 * @brief ParkingApiServer析构函数
 * 停止服务器并释放资源
 */
ParkingApiServer::~ParkingApiServer() {
    stop();
}

/**
 * @brief 启动服务器
 * @param port 监听端口号
 * @throws std::runtime_error 如果启动失败
 * 
 * 实现步骤：
 * 1. 创建服务器socket
 * 2. 设置socket选项（地址重用）
 * 3. 绑定地址和端口
 * 4. 开始监听连接
 * 5. 进入主循环接受连接
 * 
 * 错误处理：
 * - socket创建失败
 * - 设置选项失败
 * - 绑定失败
 * - 监听失败
 * - 接受连接失败
 * 
 * 并发处理：
 * - 每个客户端连接在独立线程中处理
 * - 使用detach模式避免资源泄漏
 */
void ParkingApiServer::start(uint16_t port) {
    // 1. 创建服务器socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    // 2. 设置socket选项
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Failed to set socket options");
    }

    // 3. 准备地址结构并绑定
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网络接口
    serverAddr.sin_port = htons(port);        // 转换为网络字节序

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to bind socket");
    }

    // 4. 开始监听连接
    if (listen(serverSocket, 10) < 0) {  // 等待队列长度为10
        close(serverSocket);
        throw std::runtime_error("Failed to listen on socket");
    }

    running = true;
    std::cout << "Server started on port " << port << " with " 
              << std::thread::hardware_concurrency() << " worker threads." << std::endl;

    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        
        // 设置accept的超时，以便能够及时响应停止命令
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket, &readSet);
        
        struct timeval timeout;
        timeout.tv_sec = 1;  // 1秒超时
        timeout.tv_usec = 0;
        
        int ready = select(serverSocket + 1, &readSet, nullptr, nullptr, &timeout);
        if (ready < 0) {
            if (errno == EINTR) continue;  // 被信号中断，继续循环
            std::cerr << "Select error: " << strerror(errno) << std::endl;
            break;
        }
        
        if (ready == 0) continue;  // 超时，继续循环
        
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }

        // 使用RAII来管理连接计数
        class ConnectionGuard {
            std::atomic<size_t>& connections;
        public:
            ConnectionGuard(std::atomic<size_t>& conn) : connections(conn) {
                ++connections;
            }
            ~ConnectionGuard() {
                --connections;
            }
        };

        // 检查当前连接数是否超过限制
        if (activeConnections.load() >= MAX_QUEUE_SIZE) {
            std::cerr << "Too many connections, dropping new connection" << std::endl;
            HttpResponse response(503);  // Service Unavailable
            response.body = createJsonResponse(false, "Server is too busy, please try again later");
            sendResponse(clientSocket, response);
            close(clientSocket);
            continue;
        }

        // 提交任务到线程池
        threadPool->enqueue([this, clientSocket] {
            // 使用RAII确保连接计数正确管理
            ConnectionGuard guard(activeConnections);
            
            try {
                // 设置socket读取超时
                struct timeval tv;
                tv.tv_sec = 30;  // 增加超时时间到30秒，给复杂请求更多处理时间
                tv.tv_usec = 0;
                if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                    throw std::runtime_error("Failed to set socket timeout");
                }

                // 设置TCP keep-alive
                int keepAlive = 1;
                if (setsockopt(clientSocket, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive)) < 0) {
                    throw std::runtime_error("Failed to set keep-alive");
                }
                
                // 处理请求
                HttpRequest request = parseRequest(clientSocket);
                HttpResponse response = routeRequest(request);
                sendResponse(clientSocket, response);
                
            } catch (const std::exception& e) {
                std::cerr << "Error handling client: " << e.what() << std::endl;
                
                try {
                    // 发送错误响应，包含更多错误细节
                    HttpResponse errorResponse(500);
                    errorResponse.headers["X-Error-Detail"] = e.what();
                    errorResponse.body = createJsonResponse(false, "Internal server error occurred: " + 
                        std::string(e.what()));
                    sendResponse(clientSocket, errorResponse);
                } catch (...) {
                    // 如果发送错误响应也失败，只能记录日志
                    std::cerr << "Failed to send error response" << std::endl;
                }
            }
            
            shutdown(clientSocket, SHUT_RDWR);  // 正确关闭连接
            close(clientSocket);
        });
    }
}

/**
 * @brief 停止服务器
 * 关闭监听的套接字并停止运行
 */
void ParkingApiServer::stop() {
    running = false;
    if (serverSocket >= 0) {
        close(serverSocket);
        serverSocket = -1;
    }
}

/**
 * @brief 初始化路由表
 * 在这里配置所有的API路由规则
 */
void ParkingApiServer::initializeRoutes() {
    // 使用新的路由注册方法重构路由表初始化
    POST("/api/vehicle", 
         std::bind(&ParkingApiServer::handleAddVehicle, this, std::placeholders::_1));

    DELETE("/api/vehicle/", 
           std::bind(&ParkingApiServer::handleRemoveVehicle, this, std::placeholders::_1),
           true);  // 使用前缀匹配

    GET("/api/vehicle/", 
        std::bind(&ParkingApiServer::handleQueryVehicle, this, std::placeholders::_1),
        true);

    GET("/api/status", 
        std::bind(&ParkingApiServer::handleGetParkingStatus, this, std::placeholders::_1));

    PUT("/api/rate", 
        std::bind(&ParkingApiServer::handleSetRate, this, std::placeholders::_1));

    GET("/api/history", 
        std::bind(&ParkingApiServer::handleGetHistory, this, std::placeholders::_1));

    GET("/api/current-vehicles", 
        std::bind(&ParkingApiServer::handleGetCurrentVehicles, this, std::placeholders::_1));
}

/**
 * @brief 路由匹配和请求分发
 * 根据请求的HTTP方法和路径匹配路由表，调用相应的处理函数
 * 
 * @param request HTTP请求对象
 * @return HTTP响应对象
 */
HttpResponse ParkingApiServer::routeRequest(const HttpRequest& request) {
    // 处理跨域预检请求(CORS preflight)
    if (request.method == "OPTIONS") {
        HttpResponse response(204);  // 204 No Content
        // 设置CORS相关响应头
        response.headers["Access-Control-Allow-Origin"] = "*";
        response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        response.headers["Access-Control-Allow-Headers"] = "Content-Type";
        return response;
    }

    // 处理API请求
    if (request.path.find("/api/") == 0) {  // 检查是否是API请求(以/api/开头)
        // 遍历路由表寻找匹配的处理函数
        for (const auto& route : routes) {
            // 根据路由配置决定使用精确匹配还是前缀匹配
            bool matched = route.isPrefix ? 
                         request.path.find(route.path) == 0 :  // 前缀匹配
                         request.path == route.path;           // 精确匹配
            
            // 如果路径匹配且HTTP方法一致,调用对应的处理函数
            if (matched && request.method == route.method) {
                return route.handler(request);
            }
        }
        // 未找到匹配的路由,返回404错误
        HttpResponse response(404);
        response.body = createJsonResponse(false, "API endpoint not found");
        return response;
    }

    // 如果不是API请求,当作静态文件请求处理
    return handleStaticFile(request.path);
}

/**
 * @brief 从socket读取指定字节数的数据
 * @param clientSocket socket描述符 
 * @param buffer 目标缓冲区
 * @param len 要读取的字节数
 * @param timeout 超时时间(秒)
 * @return 实际读取的字节数,出错返回-1
 */
ssize_t readWithTimeout(int clientSocket, char* buffer, size_t len, int timeout) {
    fd_set readSet;
    struct timeval tv;
    size_t totalRead = 0;

    while (totalRead < len) {
        // 设置select超时
        FD_ZERO(&readSet);
        FD_SET(clientSocket, &readSet);
        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        int ret = select(clientSocket + 1, &readSet, nullptr, nullptr, &tv);
        if (ret < 0) {
            return -1;  // select错误
        }
        if (ret == 0) {
            errno = ETIMEDOUT;
            return -1;  // 超时
        }

        // 读取数据
        ssize_t bytesRead = recv(clientSocket, buffer + totalRead, len - totalRead, 0);
        if (bytesRead <= 0) {
            return bytesRead;  // 连接关闭或错误
        }
        totalRead += bytesRead;
    }
    return totalRead;
}

/**
 * @brief 解析HTTP请求
 * @param clientSocket 客户端socket
 * @return 解析后的HTTP请求对象
 */
HttpRequest ParkingApiServer::parseRequest(int clientSocket) {
    const int TIMEOUT_SECONDS = 5;  // 读取超时时间
    const size_t MAX_HEADER_SIZE = 8192;  // 最大头部大小8KB
    const size_t MAX_BODY_SIZE = 1048576;  // 最大body大小1MB
    
    HttpRequest request;
    std::vector<char> headerBuffer(MAX_HEADER_SIZE);
    size_t headerPos = 0;
    bool foundHeaderEnd = false;

    // 1. 读取HTTP头部
    while (headerPos < MAX_HEADER_SIZE - 1) {
        // 每次读一个字节找\r\n\r\n
        ssize_t bytesRead = readWithTimeout(clientSocket, 
                                          &headerBuffer[headerPos], 
                                          1, 
                                          TIMEOUT_SECONDS);
        if (bytesRead <= 0) {
            throw std::runtime_error("Failed to read HTTP header");
        }

        headerPos++;
        // 检查是否找到头部结束标记\r\n\r\n
        if (headerPos >= 4 && 
            headerBuffer[headerPos-4] == '\r' && 
            headerBuffer[headerPos-3] == '\n' && 
            headerBuffer[headerPos-2] == '\r' && 
            headerBuffer[headerPos-1] == '\n') {
            foundHeaderEnd = true;
            break;
        }
    }

    if (!foundHeaderEnd) {
        throw std::runtime_error("HTTP header too large or malformed");
    }

    // 2. 解析头部
    std::string headerStr(headerBuffer.data(), headerPos);
    std::istringstream headerStream(headerStr);
    std::string line;

    // 解析请求行
    if (std::getline(headerStream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream lineStream(line);
        lineStream >> request.method >> request.path;
    }

    // 解析头部字段
    size_t contentLength = 0;
    while (std::getline(headerStream, line)) {
        if (line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }

        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            value.erase(0, value.find_first_not_of(" "));
            request.params[key] = value;

            // 获取Content-Length
            if (key == "Content-Length") {
                try {
                    contentLength = std::stoull(value);
                    if (contentLength > MAX_BODY_SIZE) {
                        throw std::runtime_error("Request body too large");
                    }
                } catch (const std::exception& e) {
                    throw std::runtime_error("Invalid Content-Length");
                }
            }
        }
    }

    // 3. 读取请求体
    if (contentLength > 0) {
        std::vector<char> bodyBuffer(contentLength);
        ssize_t bodyBytesRead = readWithTimeout(clientSocket, 
                                              bodyBuffer.data(), 
                                              contentLength, 
                                              TIMEOUT_SECONDS);
        
        if (bodyBytesRead < 0) {
            throw std::runtime_error("Failed to read request body");
        }
        if (static_cast<size_t>(bodyBytesRead) != contentLength) {
            throw std::runtime_error("Incomplete request body");
        }
        request.body = std::string(bodyBuffer.data(), bodyBytesRead);
    }

    return request;
}

/**
 * @brief 发送HTTP响应
 * 将HTTP响应对象序列化并发送到客户端
 * 
 * @param clientSocket 客户端套接字
 * @param response HTTP响应对象
 * 
 * 实现步骤：
 * 1. 构造响应行（HTTP版本、状态码和状态描述）
 * 2. 添加必需的响应头（Content-Type、Content-Length等）
 * 3. 添加CORS相关响应头
 * 4. 序列化响应体
 * 5. 发送完整的HTTP响应
 * 
 * 错误处理：
 * - 如果发送失败，记录错误但不抛出异常
 */
void ParkingApiServer::sendResponse(int clientSocket, const HttpResponse& response) {
    std::ostringstream responseStream;
    responseStream << "HTTP/1.1 " << response.status << " ";
    
    switch (response.status) {
        case 200: responseStream << "OK"; break;
        case 204: responseStream << "No Content"; break;
        case 400: responseStream << "Bad Request"; break;
        case 404: responseStream << "Not Found"; break;
        case 500: responseStream << "Internal Server Error"; break;
        default: responseStream << "Unknown"; break;
    }
    responseStream << "\r\n";

    // Add CORS headers for all responses
    responseStream << "Access-Control-Allow-Origin: *\r\n";
    responseStream << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    responseStream << "Access-Control-Allow-Headers: Content-Type\r\n";
    
    // Add other headers
    for (const auto& [key, value] : response.headers) {
        if (key != "Access-Control-Allow-Origin" && 
            key != "Access-Control-Allow-Methods" && 
            key != "Access-Control-Allow-Headers") {
            responseStream << key << ": " << value << "\r\n";
        }
    }

    responseStream << "Content-Length: " << response.body.length() << "\r\n";
    responseStream << "\r\n";
    responseStream << response.body;

    std::string responseStr = responseStream.str();
    size_t totalSent = 0;
    const char* buffer = responseStr.c_str();
    size_t length = responseStr.length();
    
    while (totalSent < length) {
        ssize_t sent = send(clientSocket, 
                          buffer + totalSent, 
                          length - totalSent, 
                          MSG_NOSIGNAL);  // 防止SIGPIPE信号
        
        if (sent < 0) {
            if (errno == EINTR) {
                // 被信号中断，继续尝试
                continue;
            }
            std::cerr << "Error sending response: " << strerror(errno) << std::endl;
            return;
        }
        
        totalSent += sent;
    }
}

/**
 * @brief 创建JSON格式的响应字符串
 * @param success 操作是否成功
 * @param message 响应消息
 * @param data 可选的数据字段
 * @return JSON格式的字符串
 */
std::string ParkingApiServer::createJsonResponse(bool success, const std::string& message, const std::string& data) {
    std::ostringstream json;
    json << "{\"success\":" << (success ? "true" : "false")
         << ",\"message\":\"" << message << "\"";
    
    if (!data.empty()) {
        json << ",\"data\":" << data;
    }
    
    json << "}";
    return json.str();
}

// Add at the top of the file, after includes
LogLevel Logger::currentLevel = LogLevel::INFO;
std::mutex Logger::logMutex;

const char* Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel) return;
    
    std::lock_guard<std::mutex> lock(logMutex);
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto* tm = std::localtime(&now_c);
    
    std::cerr << std::put_time(tm, "%Y-%m-%d %H:%M:%S") 
              << " [" << getLevelString(level) << "] "
              << message << std::endl;
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel = level;
}

/**
 * @brief 处理添加车辆的请求
 */
HttpResponse ParkingApiServer::handleAddVehicle(const HttpRequest& req) {
    try {
        // 查找请求体中的plate和type字段
        size_t plateStart = req.body.find("\"plate\":\"");
        size_t typeStart = req.body.find("\"type\":\"");
        if (plateStart == std::string::npos || typeStart == std::string::npos) {
            HttpResponse response(400);
            response.body = createJsonResponse(false, "Missing plate or type in request");
            return response;
        }

        // 提取车牌号和车型
        plateStart += 9;  // 跳过"plate":\"
        size_t plateEnd = req.body.find("\"", plateStart);
        std::string plate = req.body.substr(plateStart, plateEnd - plateStart);

        typeStart += 8;  // 跳过"type":\"
        size_t typeEnd = req.body.find("\"", typeStart);
        std::string type = req.body.substr(typeStart, typeEnd - typeStart);

        // 添加车辆
        if (parkingLot->addVehicle(plate, type)) {
            HttpResponse response(200);
            response.body = createJsonResponse(true, "Vehicle added successfully");
            return response;
        } else {
            HttpResponse response(400);
            response.body = createJsonResponse(false, "Failed to add vehicle, parking lot might be full");
            return response;
        }
    } catch (const std::exception& e) {
        HttpResponse response(500);
        response.body = createJsonResponse(false, std::string("Internal error: ") + e.what());
        return response;
    }
}

/**
 * @brief 处理移除车辆的请求
 */
HttpResponse ParkingApiServer::handleRemoveVehicle(const HttpRequest& req) {
    try {
        // 从路径中提取车牌号
        size_t lastSlash = req.path.find_last_of('/');
        if (lastSlash == std::string::npos) {
            HttpResponse response(400);
            response.body = createJsonResponse(false, "Invalid request path");
            return response;
        }

        std::string plate = urlDecode(req.path.substr(lastSlash + 1));
        Vehicle vehicleInfo;
        
        // 先查询车辆信息
        if (!parkingLot->queryVehicle(plate, vehicleInfo)) {
            HttpResponse response(404);
            response.body = createJsonResponse(false, "Vehicle not found");
            return response;
        }

        // 移除车辆
        if (parkingLot->removeVehicle(plate)) {
            // 构造响应数据
            std::ostringstream data;
            data << "{\"plate\":\"" << plate << "\","
                 << "\"type\":\"" << vehicleInfo.getType() << "\","
                 << "\"fee\":" << vehicleInfo.getFee() << "}";

            HttpResponse response(200);
            response.body = createJsonResponse(true, "Vehicle removed successfully", data.str());
            return response;
        } else {
            HttpResponse response(400);
            response.body = createJsonResponse(false, "Failed to remove vehicle");
            return response;
        }
    } catch (const std::exception& e) {
        HttpResponse response(500);
        response.body = createJsonResponse(false, std::string("Internal error: ") + e.what());
        return response;
    }
}

/**
 * @brief 处理查询车辆信息的请求
 */
HttpResponse ParkingApiServer::handleQueryVehicle(const HttpRequest& req) {
    try {
        size_t lastSlash = req.path.find_last_of('/');
        if (lastSlash == std::string::npos) {
            HttpResponse response(400);
            response.body = createJsonResponse(false, "Invalid request path");
            return response;
        }

        std::string plate = urlDecode(req.path.substr(lastSlash + 1));
        Vehicle vehicle;
        
        if (parkingLot->queryVehicle(plate, vehicle)) {
            std::ostringstream data;
            data << "{\"plate\":\"" << plate << "\","
                 << "\"type\":\"" << vehicle.getType() << "\","
                 << "\"entryTime\":" << vehicle.getEntryTime() << ","
                 << "\"fee\":" << vehicle.getFee() << "}";

            HttpResponse response(200);
            response.body = createJsonResponse(true, "Vehicle found", data.str());
            return response;
        } else {
            HttpResponse response(404);
            response.body = createJsonResponse(false, "Vehicle not found");
            return response;
        }
    } catch (const std::exception& e) {
        HttpResponse response(500);
        response.body = createJsonResponse(false, std::string("Internal error: ") + e.what());
        return response;
    }
}

/**
 * @brief 处理获取停车场状态的请求
 */
HttpResponse ParkingApiServer::handleGetParkingStatus(const HttpRequest&) {
    try {
        std::ostringstream data;
        data << "{\"available\":" << parkingLot->getAvailableSpaces() << ","
             << "\"occupied\":" << parkingLot->getOccupiedSpaces() << ","
             << "\"smallRate\":" << parkingLot->getSmallRate() << ","
             << "\"largeRate\":" << parkingLot->getLargeRate() << "}";

        HttpResponse response(200);
        response.body = createJsonResponse(true, "Status retrieved", data.str());
        return response;
    } catch (const std::exception& e) {
        HttpResponse response(500);
        response.body = createJsonResponse(false, std::string("Internal error: ") + e.what());
        return response;
    }
}

HttpResponse ParkingApiServer::handleSetRate(const HttpRequest& req) {
    try {
        size_t smallRateStart = req.body.find("\"smallRate\":");
        size_t largeRateStart = req.body.find("\"largeRate\":");
        if (smallRateStart == std::string::npos || largeRateStart == std::string::npos) {
            HttpResponse response(400);
            response.body = createJsonResponse(false, "Missing rate parameters");
            return response;
        }

        smallRateStart += 11;  // 跳过"smallRate":
        largeRateStart += 11;  // 跳过"largeRate":

        double smallRate = std::stod(req.body.substr(smallRateStart));
        double largeRate = std::stod(req.body.substr(largeRateStart));

        parkingLot->setRate(smallRate, largeRate);

        HttpResponse response(200);
        response.body = createJsonResponse(true, "Rates updated successfully");
        return response;
    } catch (const std::exception& e) {
        HttpResponse response(500);
        response.body = createJsonResponse(false, std::string("Internal error: ") + e.what());
        return response;
    }
}

HttpResponse ParkingApiServer::handleGetHistory(const HttpRequest&) {
    try {
        auto history = parkingLot->getHistoryVehicles();
        
        std::ostringstream data;
        data << "[";
        for (size_t i = 0; i < history.size(); ++i) {
            const auto& vehicle = history[i];
            if (i > 0) data << ",";
            data << "{\"plate\":\"" << vehicle.getLicensePlate() << "\","
                 << "\"type\":\"" << vehicle.getType() << "\","
                 << "\"entryTime\":" << vehicle.getEntryTime() << ","
                 << "\"exitTime\":" << vehicle.getExitTime() << ","
                 << "\"fee\":" << vehicle.getFee() << "}";
        }
        data << "]";

        HttpResponse response(200);
        response.body = createJsonResponse(true, "History retrieved", data.str());
        return response;
    } catch (const std::exception& e) {
        HttpResponse response(500);
        response.body = createJsonResponse(false, std::string("Internal error: ") + e.what());
        return response;
    }
}

HttpResponse ParkingApiServer::handleGetCurrentVehicles(const HttpRequest&) {
    try {
        auto vehicles = parkingLot->getCurrentVehicles();
        
        std::ostringstream data;
        data << "[";
        for (size_t i = 0; i < vehicles.size(); ++i) {
            const auto& vehicle = vehicles[i];
            if (i > 0) data << ",";
            data << "{\"plate\":\"" << vehicle.getLicensePlate() << "\","
                 << "\"type\":\"" << vehicle.getType() << "\","
                 << "\"entryTime\":" << vehicle.getEntryTime() << ","
                 << "\"hourlyRate\":" << (vehicle.getType() == "小型" ? 
                                        parkingLot->getSmallRate() : 
                                        parkingLot->getLargeRate()) << "}";
        }
        data << "]";

        HttpResponse response(200);
        response.body = createJsonResponse(true, "Current vehicles retrieved", data.str());
        return response;
    } catch (const std::exception& e) {
        HttpResponse response(500);
        response.body = createJsonResponse(false, std::string("Internal error: ") + e.what());
        return response;
    }
}