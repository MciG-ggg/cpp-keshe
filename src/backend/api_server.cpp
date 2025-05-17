/**
 * @file api_server.cpp
 * @brief HTTP服务器实现，处理RESTful API请求和静态文件服务
 * 
 * 这个文件实现了一个基于Socket的HTTP服务器，支持：
 * 1. RESTful API请求处理
 * 2. 静态文件服务
 * 3. 跨域资源共享(CORS)
 * 4. 多线程并发处理
 */
#include "api_server.h"
#include <sys/socket.h>     // Socket API
#include <netinet/in.h>     // 网络地址结构
#include <unistd.h>         // Unix标准函数
#include <cstring>
#include <iostream>
#include <thread>           // 多线程支持
#include <sstream>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <filesystem>       // C++17文件系统

namespace fs = std::filesystem;

/**
 * @brief MIME类型映射表
 * 用于设置HTTP响应的Content-Type头
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
 * 将URL编码的字符串转换为原始字符串
 * 处理%XX格式的编码字符和+号(表示空格)
 * 
 * @param encoded URL编码的字符串
 * @return 解码后的字符串
 */
std::string urlDecode(const std::string& encoded) {
    std::string result;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%') {
            if (i + 2 < encoded.length()) {
                std::string hex = encoded.substr(i + 1, 2);
                int value;
                std::istringstream(hex) >> std::hex >> value;
                result += static_cast<char>(value);
                i += 2;
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
 * 初始化服务器实例和路由表
 * 
 * @param capacity 停车场容量
 * @param smallRate 小型车每小时费率
 * @param largeRate 大型车每小时费率
 */
ParkingApiServer::ParkingApiServer(size_t capacity, double smallRate, double largeRate) 
    : parkingLot(std::make_unique<ParkingLot>(capacity, smallRate, largeRate))
    , serverSocket(-1)
    , running(false) {
    initializeRoutes();  // 初始化路由表
}

/**
 * @brief 获取文件的MIME类型
 * 根据文件扩展名确定Content-Type
 * 
 * @param path 文件路径
 * @return MIME类型字符串
 */
std::string getMimeType(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    auto it = MIME_TYPES.find(ext);
    return it != MIME_TYPES.end() ? it->second : "application/octet-stream";
}

/**
 * @brief 读取静态文件内容
 * 将指定路径的文件内容读取到字符串中
 * 
 * @param path 文件路径
 * @return 文件内容字符串
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
 * 根据请求路径返回相应的静态文件内容
 * 
 * @param path 请求路径
 * @return HTTP响应对象
 */
HttpResponse ParkingApiServer::handleStaticFile(const std::string& path) {
    std::string fullPath = "src/frontend" + (path == "/" ? "/index.html" : path);
    std::string content = readFile(fullPath);
    
    if (content.empty()) {
        std::cerr << "Failed to read file: " << fullPath << std::endl;
        HttpResponse response(404);
        response.body = createJsonResponse(false, "File not found");
        return response;
    }

    HttpResponse response(200);
    response.headers["Content-Type"] = getMimeType(fullPath);
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
 * 开始监听指定端口并接受客户端连接
 * 
 * @param port 监听的端口号
 */
void ParkingApiServer::start(uint16_t port) {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    // 允许端口重用
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Failed to set socket options");
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网络接口
    serverAddr.sin_port = htons(port);        // 设置端口号

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to bind socket");
    }

    if (listen(serverSocket, 10) < 0) {
        close(serverSocket);
        throw std::runtime_error("Failed to listen on socket");
    }

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

        // 在新线程中处理请求
        std::thread([this, clientSocket]() {
            try {
                HttpRequest request = parseRequest(clientSocket);
                HttpResponse response = routeRequest(request);

                sendResponse(clientSocket, response);
            } catch (const std::exception& e) {
                HttpResponse errorResponse(500);
                errorResponse.body = createJsonResponse(false, e.what());
                sendResponse(clientSocket, errorResponse);
            }
            close(clientSocket);
        }).detach();
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
 * @brief 解析HTTP请求
 * 从客户端套接字读取数据并解析成HTTP请求对象
 * 
 * @param clientSocket 客户端套接字
 * @return 解析得到的HTTP请求对象
 */
HttpRequest ParkingApiServer::parseRequest(int clientSocket) {
    std::vector<char> buffer(4096);
    ssize_t bytesRead = recv(clientSocket, buffer.data(), buffer.size() - 1, 0);
    if (bytesRead <= 0) {
        throw std::runtime_error("Failed to read request");
    }
    buffer[bytesRead] = '\0';

    HttpRequest request;
    std::string requestStr(buffer.data());
    std::istringstream requestStream(requestStr);
    std::string line;
    
    // Parse request line
    if (std::getline(requestStream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::istringstream lineStream(line);
        lineStream >> request.method >> request.path;
        
        // Extract HTTP version if present
        std::string version;
        if (lineStream >> version) {
            request.params["http_version"] = version;
        }
    }

    // Parse headers
    size_t contentLength = 0;
    while (std::getline(requestStream, line)) {
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
            // Trim leading spaces from value
            value.erase(0, value.find_first_not_of(" "));
            request.params[key] = value;
            
            if (key == "Content-Length") {
                contentLength = std::stoul(value);
            }
        }
    }

    // Read body
    if (contentLength > 0) {
        std::string body;
        std::string remaining;
        std::getline(requestStream, remaining, '\0');
        body = remaining.substr(0, contentLength);
        request.body = body;
    }

    return request;
}

/**
 * @brief 发送HTTP响应
 * 将HTTP响应对象序列化并发送到客户端
 * 
 * @param clientSocket 客户端套接字
 * @param response HTTP响应对象
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