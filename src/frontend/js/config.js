/**
 * @file config.js
 * @brief 停车场管理系统全局配置文件
 * 
 * 该模块集中管理系统的全局配置参数，包括：
 * 1. API服务器配置
 * 2. 界面刷新配置
 * 3. 日期时间格式化配置
 */

const CONFIG = {
    /**
     * @brief API服务器的基础URL
     * 所有API请求都将基于此URL构建
     * 开发环境使用localhost，生产环境可修改为实际域名
     */
    API_BASE_URL: 'http://localhost:8080',

    /**
     * @brief 页面数据刷新间隔（毫秒）
     * 用于定期更新：
     * 1. 停车场状态信息
     * 2. 当前车辆列表
     * 3. 实时费用计算
     * 
     * 设置为5秒(5000ms)，在保证数据实时性的同时避免过于频繁的请求
     */
    REFRESH_INTERVAL: 5000,

    /**
     * @brief 日期时间格式化配置
     * 用于统一系统中所有时间的显示格式
     * 
     * 配置说明：
     * - year: 'numeric' 显示4位数年份
     * - month: '2-digit' 显示2位数月份
     * - day: '2-digit' 显示2位数日期
     * - hour: '2-digit' 显示2位数小时
     * - minute: '2-digit' 显示2位数分钟
     * - second: '2-digit' 显示2位数秒
     * - hour12: false 使用24小时制
     */
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