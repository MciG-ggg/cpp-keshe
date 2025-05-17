/**
 * @file main.js
 * @brief 停车场管理系统的前端主要业务逻辑
 * 
 * 该模块负责：
 * 1. DOM元素操作和事件处理
 * 2. 数据的展示和更新
 * 3. 用户交互响应
 * 4. 定时任务管理
 */

/**
 * @brief DOM元素引用集中管理对象
 * 
 * 将所有需要操作的DOM元素统一管理，好处：
 * 1. 提高代码可维护性
 * 2. 避免重复查询DOM
 * 3. 便于修改元素ID时同步更新
 */
const elements = {
    // 状态显示元素
    availableSpaces: document.getElementById('available-spaces'),  // 可用车位数量
    smallRate: document.getElementById('small-rate'),             // 小型车费率
    largeRate: document.getElementById('large-rate'),             // 大型车费率
    
    // 表单元素
    entryForm: document.getElementById('entry-form'),             // 入场表单
    exitForm: document.getElementById('exit-form'),               // 出场表单
    plateNumber: document.getElementById('plate-number'),         // 入场车牌输入框
    vehicleType: document.getElementById('vehicle-type'),         // 车型选择框
    exitPlate: document.getElementById('exit-plate'),             // 出场车牌输入框
    vehicleInfo: document.getElementById('vehicle-info'),         // 车辆信息显示区
    
    // 数据展示表格
    historyBody: document.getElementById('history-body'),         // 历史记录表格体
    currentVehiclesBody: document.getElementById('current-vehicles-body')  // 在场车辆表格体
};

/**
 * @brief 更新停车场状态信息
 * 
 * 通过API获取并显示：
 * 1. 当前可用车位数
 * 2. 已占用车位数
 * 
 * 发生错误时会在控制台输出错误信息，但不影响界面正常显示
 */
async function updateParkingStatus() {
    try {
        const response = await ParkingAPI.getParkingStatus();
        if (response.success) {
            elements.availableSpaces.textContent = response.data.available;
        }
    } catch (error) {
        console.error('Failed to update parking status:', error);
    }
}

/**
 * @brief 处理车辆入场逻辑
 * @param {Event} event - 表单提交事件对象
 * 
 * 处理流程：
 * 1. 阻止表单默认提交行为
 * 2. 收集表单数据
 * 3. 调用API登记入场
 * 4. 根据响应更新界面
 * 5. 处理可能的错误情况
 */
async function handleVehicleEntry(event) {
    event.preventDefault();
    const plate = elements.plateNumber.value;
    // 将界面的英文车型转换为后端期望的中文格式
    const type = elements.vehicleType.value === 'small' ? '小型' : '大型';

    try {
        const response = await ParkingAPI.addVehicle(plate, type);
        if (response.success) {
            alert('车辆入场成功！');
            elements.entryForm.reset();  // 重置表单
            updateParkingStatus();       // 更新停车场状态
        } else {
            alert('车辆入场失败：' + response.message);
        }
    } catch (error) {
        alert('操作失败：' + error.message);
    }
}

/**
 * @brief 处理车辆出场逻辑
 * @param {Event} event - 表单提交事件对象
 * 
 * 处理流程：
 * 1. 获取车牌号
 * 2. 调用API处理出场
 * 3. 显示费用信息
 * 4. 更新相关显示
 */
async function handleVehicleExit(event) {
    event.preventDefault();
    const plate = elements.exitPlate.value;

    try {
        const response = await ParkingAPI.removeVehicle(plate);
        if (response.success) {
            alert(`车辆出场成功！应收费用：${response.data.fee} 元`);
            elements.exitForm.reset();    // 重置表单
            updateParkingStatus();        // 更新停车场状态
            updateHistory();              // 更新历史记录
        } else {
            alert('车辆出场失败：' + response.message);
        }
    } catch (error) {
        alert('操作失败：' + error.message);
    }
}

/**
 * @brief 格式化时间戳为本地时间字符串
 * @param {number} timestamp - Unix时间戳（秒）
 * @returns {string} 格式化后的时间字符串
 * 
 * 使用全局配置的格式化选项，确保时间显示格式统一
 */
function formatTimestamp(timestamp) {
    if (!timestamp) return '未记录';
    const date = new Date(timestamp * 1000);  // 转换为毫秒
    return date.toLocaleString('zh-CN', CONFIG.DATE_FORMAT_OPTIONS);
}

/**
 * @brief 更新停车历史记录
 * 
 * 获取并显示所有已经离场的车辆记录
 * 包括：车牌号、车型、入场时间、出场时间、费用
 */
async function updateHistory() {
    try {
        const response = await ParkingAPI.getHistory();
        if (response.success) {
            // 生成历史记录HTML表格行
            elements.historyBody.innerHTML = response.data
                .map(vehicle => `
                    <tr>
                        <td>${vehicle.plate}</td>
                        <td>${vehicle.type}</td>
                        <td>${formatTimestamp(vehicle.entryTime)}</td>
                        <td>${formatTimestamp(vehicle.exitTime)}</td>
                        <td>${Number(vehicle.fee).toFixed(2)} 元</td>
                    </tr>
                `)
                .join('');
        }
    } catch (error) {
        console.error('Failed to update history:', error);
    }
}

/**
 * @brief 更新当前在场车辆信息
 * 
 * 获取并显示所有正在停车场内的车辆信息
 * 包括：车牌号、车型、入场时间、当前费用
 */
async function updateCurrentVehicles() {
    try {
        const response = await ParkingAPI.getCurrentVehicles();
        if (response.success) {
            // 生成当前车辆列表HTML表格行
            elements.currentVehiclesBody.innerHTML = response.data
                .map(vehicle => {
                    const entryTime = formatTimestamp(vehicle.entryTime);
                    const currentFee = calculateCurrentFee(vehicle);
                    return `
                        <tr>
                            <td>${vehicle.plate}</td>
                            <td>${vehicle.type}</td>
                            <td>${entryTime}</td>
                            <td>${currentFee} 元</td>
                        </tr>
                    `;
                })
                .join('');
        }
    } catch (error) {
        console.error('Failed to update current vehicles:', error);
    }
}

/**
 * @brief 计算当前车辆的停车费用
 * @param {Object} vehicle - 车辆信息对象
 * @returns {string} 格式化的费用字符串（保留2位小数）
 * 
 * 计算逻辑：
 * 1. 获取当前时间戳
 * 2. 计算停车时长（小时）
 * 3. 根据车型和费率计算费用
 * 4. 格式化结果
 */
function calculateCurrentFee(vehicle) {
    const now = Math.floor(Date.now() / 1000);  // 当前时间戳（秒）
    const duration = now - vehicle.entryTime;    // 停车时长（秒）
    const hours = duration / 3600;               // 转换为小时
    const fee = Math.round(hours * vehicle.hourlyRate * 100) / 100;  // 四舍五入到2位小数
    return fee.toFixed(2);
}

// 绑定表单提交事件处理函数
elements.entryForm.addEventListener('submit', handleVehicleEntry);
elements.exitForm.addEventListener('submit', handleVehicleExit);

// 设置定时器，定期更新数据显示
setInterval(() => {
    updateParkingStatus();      // 更新停车场状态
    updateCurrentVehicles();    // 更新在场车辆信息
}, CONFIG.REFRESH_INTERVAL);

// 页面加载时初始化数据
updateParkingStatus();    // 更新停车场状态
updateHistory();         // 更新历史记录
updateCurrentVehicles(); // 更新在场车辆信息