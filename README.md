# 商场停车场管理系统

## 项目简介

这是一个基于C++后端和Web前端的商场停车场管理系统，支持车辆入场、出场、费用计算、历史记录查询等功能。系统采用前后端分离架构，后端使用C++实现RESTful API，前端使用HTML/CSS/JavaScript实现用户界面。

## 项目架构

### 后端架构 (C++)

```
src/backend/
├── api_server.cpp/h    - HTTP服务器和API实现
├── parking_lot.cpp/h   - 停车场业务逻辑
├── vehicle.cpp/h       - 车辆信息管理
└── main.cpp           - 程序入口
```

### 前端架构 (Web)

```
src/frontend/
├── index.html         - 主页面
├── css/
│   └── style.css     - 样式表
└── js/
    ├── api.js        - API调用封装
    ├── config.js     - 配置文件
    └── main.js       - 主要业务逻辑
```

## 编译与运行

### 环境要求

- Linux操作系统
- G++ 编译器 (支持C++17)
- Make工具

### 编译步骤

1. 克隆项目：
```bash
git clone <repository-url>
cd cpp-keshe
```

2. 编译项目：
```bash
make clean  # 清理之前的编译文件
make        # 编译项目
```

3. 运行服务器：
```bash
./parking_api_server
```

4. 访问前端界面：
   打开浏览器，访问 http://localhost:8080

### API测试

可以使用提供的测试脚本进行API测试：
```bash
chmod +x test_api.sh  # 添加执行权限
./test_api.sh        # 运行测试脚本
```

## 关键技术点

### 1. C++后端技术

1. 智能指针
```cpp
std::unique_ptr<ParkingLot> parkingLot;  // 智能指针管理资源
```

2. 多线程处理
```cpp
std::thread([this, clientSocket]() {
    // 处理客户端请求
}).detach();
```

3. RAII资源管理
```cpp
class ParkingApiServer {
    ~ParkingApiServer() {
        stop();  // 析构函数自动清理资源
    }
};
```

4. 模板和STL
```cpp
std::map<std::string, std::string> params;
std::vector<Vehicle> vehicles;
```

5. 文件系统操作（C++17）
```cpp
namespace fs = std::filesystem;
fs::path(path).extension();
```

### 2. 网络编程

1. Socket编程
```cpp
serverSocket = socket(AF_INET, SOCK_STREAM, 0);
bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
listen(serverSocket, 10);
```

2. HTTP协议处理
```cpp
HttpRequest parseRequest(int clientSocket);
void sendResponse(int clientSocket, const HttpResponse& response);
```

3. RESTful API设计
- GET /api/status - 获取停车场状态
- POST /api/vehicle - 添加车辆
- DELETE /api/vehicle/{plate} - 移除车辆
- GET /api/history - 获取历史记录

### 3. 前端技术

1. 异步编程
```javascript
async function updateParkingStatus() {
    const response = await ParkingAPI.getParkingStatus();
}
```

2. DOM操作
```javascript
document.getElementById('available-spaces').textContent = count;
```

3. 事件处理
```javascript
elements.entryForm.addEventListener('submit', handleVehicleEntry);
```

4. 响应式设计
```css
@media (max-width: 768px) {
    .control-panel {
        grid-template-columns: 1fr;
    }
}
```

## 数据持久化

系统使用文件系统进行数据持久化，所有车辆和费率信息保存在 `parking_data.dat` 文件中。每次程序启动时会自动加载数据，关闭时自动保存。

## 安全性考虑

1. 输入验证
2. URL编码处理
3. 跨域资源共享(CORS)支持
4. 异常处理机制

## 扩展性设计

1. 模块化架构
2. 配置文件抽离
3. API版本控制
4. 插件式设计

## 调试技巧

1. 使用console.log()在前端调试
2. 使用std::cout在后端调试
3. 使用test_api.sh脚本测试API
4. 使用浏览器开发者工具调试前端