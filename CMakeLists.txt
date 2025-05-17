cmake_minimum_required(VERSION 3.10)
project(parking_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json 3.9.1 REQUIRED)

# 下载和构建 Crow
include(FetchContent)
FetchContent_Declare(
    Crow
    GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
    GIT_TAG v1.0
)
FetchContent_MakeAvailable(Crow)

# 添加源文件
add_executable(parking_api_server
    main.cpp
    vehicle.cpp
    parking_lot.cpp
    api_server.cpp
)

# 链接依赖库
target_link_libraries(parking_api_server PRIVATE
    Crow::Crow
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)