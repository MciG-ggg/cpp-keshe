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
 * @brief ParkingApiServer构造函数
 * @param capacity 停车场容量
 * @param smallRate 小型车每小时费率
 * @param largeRate 大型车每小时费率
 * 
 * 初始化过程：
 * 1. 创建停车场管理对象
 * 2. 初始化服务器socket为-1（未创建）
 * 3. 设置运行状态为false
 * 4. 初始化路由表
 * 
 * 注意：
 * - 使用智能指针管理ParkingLot对象
 * - 构造函数不会创建socket或启动服务器
 */
ParkingApiServer::ParkingApiServer(size_t capacity, double smallRate, double largeRate) 
    : parkingLot(std::make_unique<ParkingLot>(capacity, smallRate, largeRate))
    , serverSocket(-1)
    , running(false) {
    initializeRoutes();  // 初始化路由表
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

    // 5. 服务器主循环
    running = true;
    std::cout << "Server started on port " << port << std::endl;

    while (running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket < 0) {
            std::cerr << "Failed to accept connection" << std::endl;
            continue;
        }

        // 创建新线程处理客户端请求
        std::thread([this, clientSocket]() {
            try {
                // 解析请求并生成响应
                HttpRequest request = parseRequest(clientSocket);
                HttpResponse response = routeRequest(request);
                sendResponse(clientSocket, response);
            } catch (const std::exception& e) {
                // 处理请求过程中的任何异常
                HttpResponse errorResponse(500);
                errorResponse.body = createJsonResponse(false, e.what());
                sendResponse(clientSocket, errorResponse);
            }
            close(clientSocket);  // 确保连接被关闭
        }).detach();  // 分离线程，避免资源泄漏
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
    // 路由表结构: {HTTP方法, URL路径, 处理函数, 是否使用前缀匹配}
    routes = {
        // 处理车辆入场请求 POST /api/vehicle
        {"POST", "/api/vehicle", 
         std::bind(&ParkingApiServer::handleAddVehicle, this, std::placeholders::_1), 
         false},  // false表示精确匹配路径

        // 处理车辆出场请求 DELETE /api/vehicle/{车牌号}
        {"DELETE", "/api/vehicle/", 
         std::bind(&ParkingApiServer::handleRemoveVehicle, this, std::placeholders::_1), 
         true},   // true表示前缀匹配,因为后面还有动态参数(车牌号)

        // 查询车辆信息 GET /api/vehicle/{车牌号}
        {"GET", "/api/vehicle/", 
         std::bind(&ParkingApiServer::handleQueryVehicle, this, std::placeholders::_1), 
         true},   // 同样使用前缀匹配

        // 获取停车场状态 GET /api/status
        {"GET", "/api/status", 
         std::bind(&ParkingApiServer::handleGetParkingStatus, this, std::placeholders::_1), 
         false},

        // 更新停车费率 PUT /api/rate
        {"PUT", "/api/rate", 
         std::bind(&ParkingApiServer::handleSetRate, this, std::placeholders::_1), 
         false},

        // 获取历史记录 GET /api/history
        {"GET", "/api/history", 
         std::bind(&ParkingApiServer::handleGetHistory, this, std::placeholders::_1), 
         false},

        // 获取当前在场车辆 GET /api/current-vehicles
        {"GET", "/api/current-vehicles", 
         std::bind(&ParkingApiServer::handleGetCurrentVehicles, this, std::placeholders::_1), 
         false}
    };
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
    send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
}

/**
 * @brief 创建JSON格式的响应体
 * 
 * @param success 操作是否成功
 * @param message 响应消息
 * @param data 附加数据(可选)
 * @return JSON字符串
 * 
 * 实现细节：
 * 1. 使用字符串流构造JSON对象
 * 2. 添加成功标志、消息和附加数据
 * 3. 支持链式调用以便于快速构造响应
 */
std::string ParkingApiServer::createJsonResponse(bool success, const std::string& message, const std::string& data) {
    std::ostringstream json;
    json << "{";
    json << "\"success\":" << (success ? "true" : "false") << ",";
    json << "\"message\":\"" << message << "\"";
    if (!data.empty()) {
        json << ",\"data\":" << data;
    }
    json << "}";
    return json.str();
}

/**
 * @brief 处理车辆入场请求
 * 解析请求体中的车牌号和车型, 并将车辆信息添加到停车场
 * 
 * @param req HTTP请求对象
 * @return HTTP响应对象
 * 
 * 处理流程：
 * 1. 解析请求体中的JSON数据
 * 2. 验证必需字段（车牌号和车型）
 * 3. 调用停车场管理对象的addVehicle方法
 * 4. 根据返回结果生成相应的HTTP响应
 * 
 * 边界情况处理：
 * - 车牌号或车型为空
 * - 停车场已满
 * - 车辆已在停车场内
 * 
 * 错误处理：
 * - 任何解析或处理错误返回400 Bad Request
 */
HttpResponse ParkingApiServer::handleAddVehicle(const HttpRequest& req) {
    try {
        std::cout << "Received body: " << req.body << std::endl;
        
        std::string body = req.body;
        // Remove any leading/trailing whitespace
        body.erase(0, body.find_first_not_of(" \n\r\t"));
        body.erase(body.find_last_not_of(" \n\r\t") + 1);
        
        // Find plate and type values
        size_t plateStart = body.find("\"plate\"");
        size_t typeStart = body.find("\"type\"");
        
        if (plateStart == std::string::npos || typeStart == std::string::npos) {
            std::cout << "Missing plate or type fields" << std::endl;
            HttpResponse response(400);
            response.body = createJsonResponse(false, "Missing required fields");
            return response;
        }

        // Extract plate value
        plateStart = body.find(':', plateStart) + 1;
        plateStart = body.find('\"', plateStart) + 1;
        size_t plateEnd = body.find('\"', plateStart);
        std::string plate = body.substr(plateStart, plateEnd - plateStart);

        // Extract type value
        typeStart = body.find(':', typeStart) + 1;
        typeStart = body.find('\"', typeStart) + 1;
        size_t typeEnd = body.find('\"', typeStart);
        std::string type = body.substr(typeStart, typeEnd - typeStart);

        std::cout << "Extracted plate: " << plate << ", type: " << type << std::endl;

        if (parkingLot->addVehicle(plate, type)) {
            HttpResponse response;
            response.body = createJsonResponse(true, "Vehicle added successfully");
            return response;
        } else {
            // 检查是否是因为车辆已存在而失败
            Vehicle v;
            if (parkingLot->queryVehicle(plate, v) && v.getExitTime() == 0) {
                HttpResponse response(400);
                response.body = createJsonResponse(false, "该车辆已在停车场内");
                return response;
            } else {
                HttpResponse response(400);
                response.body = createJsonResponse(false, "停车场已满");
                return response;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in handleAddVehicle: " << e.what() << std::endl;
        HttpResponse response(400);
        response.body = createJsonResponse(false, std::string("Error: ") + e.what());
        return response;
    }
}

/**
 * @brief 处理车辆出场请求
 * 根据车牌号移除停车场中的车辆信息，并返回车辆信息和费用
 * 
 * @param req HTTP请求对象
 * @return HTTP响应对象
 * 
 * 处理流程：
 * 1. 从请求路径中提取车牌号
 * 2. 调用停车场管理对象的removeVehicle方法
 * 3. 如果成功，返回车辆信息和费用
 * 4. 如果失败，返回404错误
 * 
 * 边界情况处理：
 * - 车牌号无效
 * - 车辆未找到
 * 
 * 错误处理：
 * - 任何处理错误返回400 Bad Request
 */
HttpResponse ParkingApiServer::handleRemoveVehicle(const HttpRequest& req) {
    try {
        size_t pos = req.path.find_last_of('/');
        if (pos == std::string::npos) {
            return HttpResponse(400);
        }

        std::string encodedPlate = req.path.substr(pos + 1);
        std::string plate = urlDecode(encodedPlate);
        std::cout << "Removing vehicle with plate: " << plate << std::endl;

        if (parkingLot->removeVehicle(plate)) {
            Vehicle v;
            parkingLot->queryVehicle(plate, v);
            
            std::ostringstream data;
            data << "{\"plate\":\"" << v.getLicensePlate() << "\",";
            data << "\"type\":\"" << v.getType() << "\",";
            data << "\"fee\":" << v.getFee() << "}";

            HttpResponse response;
            response.body = createJsonResponse(true, "Vehicle removed successfully", data.str());
            return response;
        } else {
            HttpResponse response(404);
            response.body = createJsonResponse(false, "Vehicle not found");
            return response;
        }
    } catch (const std::exception& e) {
        HttpResponse response(400);
        response.body = createJsonResponse(false, e.what());
        return response;
    }
}

/**
 * @brief 处理车辆查询请求
 * 根据车牌号返回车辆的入场时间、出场时间和费用
 * 
 * @param req HTTP请求对象
 * @return HTTP响应对象
 * 
 * 处理流程：
 * 1. 从请求路径中提取车牌号
 * 2. 调用停车场管理对象的queryVehicle方法
 * 3. 如果找到，返回车辆信息
 * 4. 如果未找到，返回404错误
 * 
 * 边界情况处理：
 * - 车牌号无效
 * 
 * 错误处理：
 * - 任何处理错误返回400 Bad Request
 */
HttpResponse ParkingApiServer::handleQueryVehicle(const HttpRequest& req) {
    try {
        size_t pos = req.path.find_last_of('/');
        if (pos == std::string::npos) {
            return HttpResponse(400);
        }

        std::string encodedPlate = req.path.substr(pos + 1);
        std::string plate = urlDecode(encodedPlate);
        std::cout << "Querying vehicle with plate: " << plate << std::endl;

        Vehicle v;
        if (parkingLot->queryVehicle(plate, v)) {
            std::ostringstream data;
            data << "{\"plate\":\"" << v.getLicensePlate() << "\",";
            data << "\"type\":\"" << v.getType() << "\",";
            data << "\"entryTime\":" << v.getEntryTime() << ",";
            data << "\"exitTime\":" << v.getExitTime() << ",";
            data << "\"fee\":" << v.getFee() << "}";

            HttpResponse response;
            response.body = createJsonResponse(true, "Vehicle found", data.str());
            return response;
        } else {
            HttpResponse response(404);
            response.body = createJsonResponse(false, "Vehicle not found");
            return response;
        }
    } catch (const std::exception& e) {
        HttpResponse response(400);
        response.body = createJsonResponse(false, e.what());
        return response;
    }
}

/**
 * @brief 处理停车场状态查询请求
 * 返回当前停车场的空闲车位和占用车位数量
 * 
 * @param req HTTP请求对象
 * @return HTTP响应对象
 * 
 * 处理流程：
 * 1. 获取停车场的空闲和占用车位数量
 * 2. 构造状态数据的JSON表示
 * 3. 返回成功的HTTP响应
 */
HttpResponse ParkingApiServer::handleGetParkingStatus(const HttpRequest&) {
    std::ostringstream data;
    data << "{\"available\":" << parkingLot->getAvailableSpaces() << ",";
    data << "\"occupied\":" << parkingLot->getOccupiedSpaces() << "}";

    HttpResponse response;
    response.body = createJsonResponse(true, "Status retrieved", data.str());
    return response;
}

/**
 * @brief 处理费率设置请求
 * 更新小型车和大型车的每小时费率
 * 
 * @param req HTTP请求对象
 * @return HTTP响应对象
 * 
 * 处理流程：
 * 1. 解析请求体中的JSON数据
 * 2. 验证费率字段（小型车和大型车费率）
 * 3. 调用停车场管理对象的setRate方法
 * 4. 返回成功的HTTP响应
 * 
 * 边界情况处理：
 * - 费率值无效（非数字或负数）
 * 
 * 错误处理：
 * - 任何解析或处理错误返回400 Bad Request
 */
HttpResponse ParkingApiServer::handleSetRate(const HttpRequest& req) {
    try {
        std::cout << "Received rate update body: " << req.body << std::endl;
        
        std::string body = req.body;
        // Remove any leading/trailing whitespace
        body.erase(0, body.find_first_not_of(" \n\r\t"));
        body.erase(body.find_last_not_of(" \n\r\t") + 1);

        // Extract smallRate
        size_t smallRatePos = body.find("\"smallRate\"");
        if (smallRatePos == std::string::npos) {
            throw std::runtime_error("Missing smallRate field");
        }
        smallRatePos = body.find(':', smallRatePos) + 1;
        while (smallRatePos < body.length() && std::isspace(body[smallRatePos])) {
            ++smallRatePos;
        }
        size_t smallRateEnd = body.find_first_not_of("0123456789.", smallRatePos);
        std::string smallRateStr = body.substr(smallRatePos, smallRateEnd - smallRatePos);

        // Extract largeRate
        size_t largeRatePos = body.find("\"largeRate\"");
        if (largeRatePos == std::string::npos) {
            throw std::runtime_error("Missing largeRate field");
        }
        largeRatePos = body.find(':', largeRatePos) + 1;
        while (largeRatePos < body.length() && std::isspace(body[largeRatePos])) {
            ++largeRatePos;
        }
        size_t largeRateEnd = body.find_first_not_of("0123456789.", largeRatePos);
        std::string largeRateStr = body.substr(largeRatePos, largeRateEnd - largeRatePos);

        std::cout << "Extracted rates - small: " << smallRateStr << ", large: " << largeRateStr << std::endl;

        double smallRate = std::stod(smallRateStr);
        double largeRate = std::stod(largeRateStr);

        if (smallRate <= 0 || largeRate <= 0) {
            throw std::runtime_error("Rates must be positive numbers");
        }

        parkingLot->setRate(smallRate, largeRate);

        HttpResponse response;
        response.body = createJsonResponse(true, "Rates updated successfully");
        return response;
    } catch (const std::exception& e) {
        std::cerr << "Error updating rates: " << e.what() << std::endl;
        HttpResponse response(400);
        response.body = createJsonResponse(false, std::string("Error updating rates: ") + e.what());
        return response;
    }
}

/**
 * @brief 处理历史记录查询请求
 * 返回停车场的历史车辆记录，包括车牌号、车型、入场时间、出场时间和费用
 * 
 * @param req HTTP请求对象
 * @return HTTP响应对象
 * 
 * 处理流程：
 * 1. 获取停车场的历史车辆记录
 * 2. 构造记录数据的JSON数组表示
 * 3. 返回成功的HTTP响应
 */
HttpResponse ParkingApiServer::handleGetHistory(const HttpRequest&) {
    auto history = parkingLot->getHistoryVehicles();
    std::ostringstream data;
    data << "[";
    for (size_t i = 0; i < history.size(); ++i) {
        const auto& v = history[i];
        data << "{\"plate\":\"" << v.getLicensePlate() << "\",";
        data << "\"type\":\"" << v.getType() << "\",";
        data << "\"entryTime\":" << v.getEntryTime() << ",";
        data << "\"exitTime\":" << v.getExitTime() << ",";
        data << "\"fee\":" << v.getFee() << "}";
        if (i < history.size() - 1) {
            data << ",";
        }
    }
    data << "]";

    HttpResponse response;
    response.body = createJsonResponse(true, "History retrieved", data.str());
    return response;
}

/**
 * @brief 处理当前在场车辆查询请求
 * 返回当前在停车场内的所有车辆信息，包括车牌号、车型、入场时间和每小时费率
 * 
 * @param req HTTP请求对象
 * @return HTTP响应对象
 * 
 * 处理流程：
 * 1. 获取当前在场车辆的列表
 * 2. 构造车辆信息的JSON数组表示
 * 3. 返回成功的HTTP响应
 * 
 * 边界情况处理：
 * - 当前没有车辆时返回空数组
 */
HttpResponse ParkingApiServer::handleGetCurrentVehicles(const HttpRequest&) {
    auto currentVehicles = parkingLot->getCurrentVehicles();
    std::ostringstream data;
    data << "[";
    for (size_t i = 0; i < currentVehicles.size(); ++i) {
        const auto& v = currentVehicles[i];
        double hourlyRate = (v.getType() == "小型") ? parkingLot->getSmallRate() : parkingLot->getLargeRate();
        data << "{\"plate\":\"" << v.getLicensePlate() << "\",";
        data << "\"type\":\"" << v.getType() << "\",";
        data << "\"entryTime\":" << v.getEntryTime() << ",";
        data << "\"hourlyRate\":" << hourlyRate << "}";
        if (i < currentVehicles.size() - 1) {
            data << ",";
        }
    }
    data << "]";

    HttpResponse response;
    response.body = createJsonResponse(true, "Current vehicles retrieved", data.str());
    return response;
}