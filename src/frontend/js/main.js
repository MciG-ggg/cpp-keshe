// 集中管理所有需要操作的DOM元素
// 使用对象统一管理可以提高代码的可维护性和可读性
const elements = {
    // 显示可用停车位数量的元素
    availableSpaces: document.getElementById('available-spaces'),
    // 显示小型车费率的元素
    smallRate: document.getElementById('small-rate'),
    // 显示大型车费率的元素
    largeRate: document.getElementById('large-rate'),
    // 车辆入场表单
    entryForm: document.getElementById('entry-form'),
    // 车辆出场表单
    exitForm: document.getElementById('exit-form'),
    // 入场车牌号输入框
    plateNumber: document.getElementById('plate-number'),
    // 车辆类型选择框
    vehicleType: document.getElementById('vehicle-type'),
    // 出场车牌号输入框
    exitPlate: document.getElementById('exit-plate'),
    // 车辆信息显示区域
    vehicleInfo: document.getElementById('vehicle-info'),
    // 停车历史记录表格体
    historyBody: document.getElementById('history-body'),
    // 当前在场车辆表格体
    currentVehiclesBody: document.getElementById('current-vehicles-body')
};

/**
 * 更新停车场状态信息
 * 通过API获取最新的停车场状态，并更新页面显示
 */
async function updateParkingStatus() {
    try {
        // 调用API获取停车场状态
        const response = await ParkingAPI.getParkingStatus();
        if (response.success) {
            // 更新可用车位显示
            elements.availableSpaces.textContent = response.data.available;
        }
    } catch (error) {
        console.error('Failed to update parking status:', error);
    }
}

/**
 * 处理车辆入场逻辑
 * @param {Event} event - 表单提交事件
 */
async function handleVehicleEntry(event) {
    // 阻止表单默认提交行为
    event.preventDefault();
    // 获取输入的车牌号
    const plate = elements.plateNumber.value;
    // 转换车型格式以匹配后端期望的格式（英文转中文）
    const type = elements.vehicleType.value === 'small' ? '小型' : '大型';

    try {
        // 调用API处理车辆入场
        const response = await ParkingAPI.addVehicle(plate, type);
        if (response.success) {
            alert('车辆入场成功！');
            // 重置表单
            elements.entryForm.reset();
            // 更新停车场状态显示
            updateParkingStatus();
        } else {
            alert('车辆入场失败：' + response.message);
        }
    } catch (error) {
        alert('操作失败：' + error.message);
    }
}

/**
 * 处理车辆出场逻辑
 * @param {Event} event - 表单提交事件
 */
async function handleVehicleExit(event) {
    event.preventDefault();
    const plate = elements.exitPlate.value;

    try {
        // 调用API处理车辆出场
        const response = await ParkingAPI.removeVehicle(plate);
        if (response.success) {
            alert(`车辆出场成功！应收费用：${response.data.fee} 元`);
            // 重置出场表单
            elements.exitForm.reset();
            // 更新停车场状态和历史记录
            updateParkingStatus();
            updateHistory();
        } else {
            alert('车辆出场失败：' + response.message);
        }
    } catch (error) {
        alert('操作失败：' + error.message);
    }
}

/**
 * 格式化时间戳为本地时间字符串
 * @param {number} timestamp - Unix时间戳（秒）
 * @returns {string} 格式化后的时间字符串
 */
function formatTimestamp(timestamp) {
    if (!timestamp) return '未记录';
    const date = new Date(timestamp * 1000);
    return date.toLocaleString('zh-CN', CONFIG.DATE_FORMAT_OPTIONS);
}

/**
 * 更新停车历史记录
 * 获取并显示所有已经离场的车辆记录
 */
async function updateHistory() {
    try {
        const response = await ParkingAPI.getHistory();
        if (response.success) {
            // 生成历史记录HTML并更新到表格中
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
 * 更新当前在场车辆信息
 * 获取并显示所有正在停车场内的车辆信息
 */
async function updateCurrentVehicles() {
    try {
        const response = await ParkingAPI.getCurrentVehicles();
        if (response.success) {
            // 生成当前车辆列表HTML并更新到表格中
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
 * 计算当前车辆的停车费用
 * @param {Object} vehicle - 车辆信息对象
 * @returns {string} 格式化的费用字符串(保留2位小数)
 */
function calculateCurrentFee(vehicle) {
    const now = Math.floor(Date.now() / 1000); // 当前时间戳(秒)
    const duration = now - vehicle.entryTime; // 停车时长(秒)
    const hours = duration / 3600; // 转换为小时
    const fee = Math.round(hours * vehicle.hourlyRate * 100) / 100;
    return fee.toFixed(2);
}

// 绑定表单提交事件处理函数
elements.entryForm.addEventListener('submit', handleVehicleEntry);
elements.exitForm.addEventListener('submit', handleVehicleExit);

// 设置定时器，定期更新停车场状态和当前车辆信息
setInterval(() => {
    updateParkingStatus();
    updateCurrentVehicles();
}, CONFIG.REFRESH_INTERVAL);

// 页面加载时初始化数据
updateParkingStatus();
updateHistory();
updateCurrentVehicles();