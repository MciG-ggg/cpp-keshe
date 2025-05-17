/**
 * 停车场管理系统全局配置
 */
const CONFIG = {
    // API服务器的基础URL
    API_BASE_URL: 'http://localhost:8080',

    // 页面数据刷新间隔（毫秒）
    // 每5秒更新一次停车场状态和当前车辆信息
    REFRESH_INTERVAL: 5000,

    // 日期时间格式化配置
    DATE_FORMAT_OPTIONS: {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false
    }
};