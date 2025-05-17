// 停车场管理系统的API接口封装
const ParkingAPI = {
    /**
     * API基础URL配置
     * 所有API请求都会基于这个地址
     */
    baseURL: CONFIG.API_BASE_URL,

    /**
     * 发送HTTP请求的通用方法
     * @param {string} endpoint - API端点
     * @param {Object} options - 请求配置项
     * @returns {Promise<Object>} 请求响应数据
     */
    async request(endpoint, options = {}) {
        try {
            const response = await fetch(`${this.baseURL}${endpoint}`, {
                headers: {
                    'Content-Type': 'application/json',
                },
                ...options
            });
            return await response.json();
        } catch (error) {
            console.error('API请求失败:', error);
            throw error;
        }
    },

    /**
     * 获取停车场当前状态
     * @returns {Promise<Object>} 包含可用车位等信息
     */
    getParkingStatus() {
        return this.request('/api/status');
    },

    /**
     * 添加新车辆（车辆入场）
     * @param {string} plate - 车牌号
     * @param {string} type - 车辆类型（小型/大型）
     * @returns {Promise<Object>} 入场结果
     */
    addVehicle(plate, type) {
        return this.request('/api/vehicle', {
            method: 'POST',
            body: JSON.stringify({ plate, type })
        });
    },

    /**
     * 车辆出场
     * @param {string} plate - 车牌号
     * @returns {Promise<Object>} 包含停车费用等出场信息
     */
    removeVehicle(plate) {
        return this.request(`/api/vehicle/${encodeURIComponent(plate)}`, {
            method: 'DELETE'
        });
    },

    /**
     * 获取停车历史记录
     * @returns {Promise<Object>} 包含所有已离场车辆的记录
     */
    getHistory() {
        return this.request('/api/history');
    },

    /**
     * 获取当前在场车辆信息
     * @returns {Promise<Object>} 包含所有在场车辆的信息
     */
    getCurrentVehicles() {
        return this.request('/api/current-vehicles');
    }
};