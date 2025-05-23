cmake_minimum_required(VERSION 3.10)
project(parking_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(nlohmann_json 3.9.1 REQUIRED)

# 添加源文件
add_executable(parking_system
    src/backend/main.cpp
    src/backend/vehicle.cpp
    src/backend/parking_lot.cpp
)

# 链接依赖库
target_link_libraries(parking_system PRIVATE
    nlohmann_json::nlohmann_json
)