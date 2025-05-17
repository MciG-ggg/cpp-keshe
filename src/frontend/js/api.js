/**
 * @file api.js
 * @brief 停车场管理系统的前端API接口封装
 * 
 * 该模块封装了所有与后端服务器的HTTP通信接口，
 * 提供了一个统一的、易用的API层，处理：
 * 1. HTTP请求的发送和响应处理
 * 2. 错误处理和异常捕获
 * 3. 数据格式转换
 */

// 停车场管理系统的API接口封装对象
const ParkingAPI = {
    /**
     * API基础URL配置
     * 所有API请求都会基于这个地址构建完整的URL
     * 从全局配置文件中获取，便于环境切换
     */
    baseURL: CONFIG.API_BASE_URL,

    /**
     * @brief 发送HTTP请求的通用方法
     * @param {string} endpoint - API端点路径（相对路径）
     * @param {Object} options - 请求配置项
     * @returns {Promise<Object>} 请求响应的Promise对象
     * 
     * 统一处理：
     * 1. URL构建
     * 2. 请求头设置
     * 3. 错误处理
     * 4. 响应解析
     */
    async request(endpoint, options = {}) {
        try {
            // 发送HTTP请求
            const response = await fetch(`${this.baseURL}${endpoint}`, {
                headers: {
                    'Content-Type': 'application/json',  // 设置JSON数据格式
                },
                ...options  // 合并其他配置项（方法、请求体等）
            });
            
            // 解析响应JSON数据
            return await response.json();
        } catch (error) {
            // 统一的错误处理
            console.error('API请求失败:', error);
            throw error;  // 向上层抛出错误，由业务代码处理
        }
    },

    /**
     * @brief 获取停车场当前状态
     * @returns {Promise<Object>} 包含可用车位等信息的Promise对象
     * 
     * 返回数据格式：
     * {
     *   success: boolean,
     *   data: {
     *     available: number,  // 可用车位数
     *     occupied: number    // 已占用车位数
     *   }
     * }
     */
    getParkingStatus() {
        return this.request('/api/status');
    },

    /**
     * @brief 添加新车辆（车辆入场）
     * @param {string} plate - 车牌号（例：苏A12345）
     * @param {string} type - 车辆类型（小型/大型）
     * @returns {Promise<Object>} 入场结果的Promise对象
     * 
     * 请求体格式：
     * {
     *   plate: string,
     *   type: string
     * }
     */
    addVehicle(plate, type) {
        return this.request('/api/vehicle', {
            method: 'POST',
            body: JSON.stringify({ plate, type })
        });
    },

    /**
     * @brief 处理车辆出场
     * @param {string} plate - 车牌号
     * @returns {Promise<Object>} 包含停车费用等出场信息的Promise对象
     * 
     * 响应数据格式：
     * {
     *   success: boolean,
     *   data: {
     *     plate: string,
     *     type: string,
     *     fee: number
     *   }
     * }
     */
    removeVehicle(plate) {
        return this.request(`/api/vehicle/${encodeURIComponent(plate)}`, {
            method: 'DELETE'
        });
    },

    /**
     * @brief 获取停车历史记录
     * @returns {Promise<Object>} 包含所有已离场车辆记录的Promise对象
     * 
     * 响应数据格式：
     * {
     *   success: boolean,
     *   data: Array<{
     *     plate: string,
     *     type: string,
     *     entryTime: number,
     *     exitTime: number,
     *     fee: number
     *   }>
     * }
     */
    getHistory() {
        return this.request('/api/history');
    },

    /**
     * @brief 获取当前在场车辆信息
     * @returns {Promise<Object>} 包含所有在场车辆信息的Promise对象
     * 
     * 响应数据格式：
     * {
     *   success: boolean,
     *   data: Array<{
     *     plate: string,
     *     type: string,
     *     entryTime: number,
     *     hourlyRate: number
     *   }>
     * }
     */
    getCurrentVehicles() {
        return this.request('/api/current-vehicles');
    }
};