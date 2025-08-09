// متغیرهای جهانی
let mainChart = null;
let hourlyChart = null;
let currentData = [];
let currentStats = {};
let selectedFile = null; // Changed from selectedFiles Set to single selectedFile
let availableFiles = [];

// رنگ‌های نمودار
const chartColors = {
    power: '#ff6b6b',
    energy: '#4ecdc4',
    voltage: '#45b7d1',
    current: '#96ceb4',
    pf: '#feca57'
};

// تابع تبدیل timestamp به زمان محلی
function formatLocalDateTime(timestamp) {
    const date = new Date(timestamp * 1000);
    return date.toLocaleString('fa-IR', {
        timeZone: 'Asia/Tehran',
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
    });
}

// تابع تبدیل timestamp به زمان محلی (فقط ساعت)
function formatLocalTime(timestamp) {
    const date = new Date(timestamp * 1000);
    return date.toLocaleTimeString('fa-IR', {
        timeZone: 'Asia/Tehran',
        hour: '2-digit',
        minute: '2-digit'
    });
}

// تابع نمایش loading
function showLoading() {
    document.getElementById('loadingSpinner').style.display = 'block';
    document.getElementById('analysisContent').style.display = 'none';
    document.getElementById('noDataMessage').style.display = 'none';
}

// تابع مخفی کردن loading
function hideLoading() {
    document.getElementById('loadingSpinner').style.display = 'none';
}

// تابع نمایش محتوا
function showContent() {
    document.getElementById('analysisContent').style.display = 'block';
    document.getElementById('noDataMessage').style.display = 'none';
}

// تابع مخفی کردن محتوا
function hideContent() {
    document.getElementById('analysisContent').style.display = 'none';
}

// تابع نمایش پیام عدم وجود داده
function showNoDataMessage() {
    document.getElementById('noDataMessage').style.display = 'block';
    document.getElementById('analysisContent').style.display = 'none';
}

// تابع بارگذاری فایل‌های موجود
async function loadAvailableFiles() {
    try {
        console.log('🔍 Fetching files from /list?dir=/data...');
        const response = await fetch('/list?dir=/data');
        
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const text = await response.text();
        console.log('📄 Raw response:', text);
        
        let data;
        try {
            data = JSON.parse(text);
        } catch (jsonError) {
            console.error('❌ JSON parse error:', jsonError);
            console.error('📄 Invalid JSON:', text);
            throw new Error('Invalid JSON response from server');
        }
        
        console.log('📁 Data received from backend:', data);
        
        if (!data.files || !Array.isArray(data.files)) {
            console.error('❌ Expected files array, got:', typeof data.files);
            throw new Error('Expected files array response from server');
        }
        
        // فیلتر کردن فقط فایل‌های CSV
        const csvFiles = data.files.filter(file => 
            file.type === 'file' && file.name && file.name.endsWith('.csv')
        );
        
        console.log('📊 CSV files found:', csvFiles);
        
        // تبدیل به فرمت مورد نیاز history
        const files = csvFiles.map(file => {
            const date = file.name.replace('.csv', '');
            return {
                date: date,
                filename: '/data/' + file.name,
                size: file.size
            };
        });
        
        availableFiles = files;
        displayFiles(files);
    } catch (error) {
        console.error('خطا در بارگذاری فایل‌ها:', error);
        document.getElementById('filesList').innerHTML = 
            `<div class="col-12 text-center text-light">
                <div class="alert alert-danger">
                    <i class='bx bx-error-circle'></i>
                    خطا در بارگذاری فایل‌ها: ${error.message}
                </div>
            </div>`;
    }
}

// تابع نمایش فایل‌ها
function displayFiles(files) {
    const filesList = document.getElementById('filesList');
    
    if (!files || files.length === 0) {
        filesList.innerHTML = '<div class="col-12 text-center text-light">هیچ فایلی یافت نشد</div>';
        return;
    }
    
    // فیلتر کردن فایل‌های معتبر
    const validFiles = files.filter(file => file && file.date && file.date !== 'undefined' && file.date !== 'null');
    
    if (validFiles.length === 0) {
        filesList.innerHTML = '<div class="col-12 text-center text-light">هیچ فایل معتبری یافت نشد</div>';
        return;
    }
    
    filesList.innerHTML = validFiles.map(file => `
        <div class="col-lg-4 col-md-6 mb-3">
            <div class="file-card text-white p-3 ${selectedFile === file.date ? 'selected' : ''}" 
                 onclick="selectFile('${file.date}')" 
                 style="cursor: pointer;">
                <div class="d-flex justify-content-between align-items-start">
                    <div>
                        <h6 class="mb-1">${file.date}</h6>
                        <small>فایل CSV</small>
                    </div>
                    <i class='bx bx-file' style="font-size: 1.5rem;"></i>
                </div>
                <div class="file-stats">
                    <div class="file-stat">
                        <div>${file.size ? (file.size / 1024).toFixed(1) + 'KB' : 'نامشخص'}</div>
                        <small>حجم</small>
                    </div>
                    <div class="file-stat">
                        <div>CSV</div>
                        <small>نوع</small>
                    </div>
                    <div class="file-stat">
                        <div>📊</div>
                        <small>داده</small>
                    </div>
                </div>
            </div>
        </div>
    `).join('');
}

function selectFile(date) {
    // Deselect all previous files
    const allCards = document.querySelectorAll('.file-card');
    allCards.forEach(card => card.classList.remove('selected'));
    
    // Select new file
    if (selectedFile === date) {
        selectedFile = null; // Deselect if clicking same file
    } else {
        selectedFile = date;
        const newCard = document.querySelector(`.file-card[onclick="selectFile('${date}')"]`);
        if (newCard) newCard.classList.add('selected');
    }
    
    // If a file is selected, load its data immediately
    if (selectedFile) {
        loadHistoryData([selectedFile]);
    } else {
        // Clear charts if no file selected
        clearCharts();
    }
}

function clearCharts() {
    if (mainChart) {
        mainChart.destroy();
        mainChart = null;
    }
    if (hourlyChart) {
        hourlyChart.destroy();
        hourlyChart = null;
    }
    currentData = [];
    currentStats = {};
    
    // Clear statistics cards
    document.getElementById('maxValue').textContent = '0';
    document.getElementById('avgValue').textContent = '0';
    document.getElementById('totalEnergy').textContent = '0';
    document.getElementById('peakHours').textContent = '0';
    
    // Clear detailed table
    document.getElementById('detailedTable').innerHTML = '<tr><td colspan="6" class="text-center">هیچ داده‌ای انتخاب نشده</td></tr>';
    
    // Show no data message
    showNoDataMessage();
}



// تابع بارگذاری داده‌های تاریخی
async function loadHistoryData(dates) {
    if (dates.length === 0) {
        showNoDataMessage();
        return;
    }
    
    showLoading();
    
    try {
        const date = dates[0]; // Only use first date since we're doing single file selection
        // محدود کردن تعداد داده‌ها برای بهبود عملکرد
        const response = await fetch(`/api/history?date=${date}&maxLines=5000`);
        const data = await response.json();
        
        if (data && data.length > 0) {
            console.log(`📊 Loaded ${data.length} data points for ${date}`);
            currentData = data;
            await loadHistoryStats(dates);
            updateCharts();
            updateStatisticsCards();
            updateDetailedTable();
            showContent();
        } else {
            showNoDataMessage();
        }
    } catch (error) {
        console.error('خطا در بارگذاری داده‌ها:', error);
        showNoDataMessage();
    }
    
    hideLoading();
}

// تابع بارگذاری آمار
async function loadHistoryStats(dates) {
    if (dates.length === 0) return;
    
    try {
        const date = dates[0]; // Only use first date
        const response = await fetch(`/api/history/stats?date=${date}`);
        const stats = await response.json();
        
        if (stats) {
            currentStats = stats;
        }
    } catch (error) {
        console.error('خطا در بارگذاری آمار:', error);
    }
}



// تابع بروزرسانی نمودار اصلی - نمودار 24 ساعته
function updateMainChart(data, type = 'power') {
    const ctx = document.getElementById('mainChart').getContext('2d');
    
    if (mainChart) {
        mainChart.destroy();
    }
    
    // ایجاد آرایه 24 ساعته (00:00 تا 23:59)
    const hourlyData = new Array(24).fill(null);
    const hourlyLabels = [];
    
    // ایجاد برچسب‌های ساعت
    for (let i = 0; i < 24; i++) {
        hourlyLabels.push(`${i.toString().padStart(2, '0')}:00`);
    }
    
    // گروه‌بندی داده‌ها بر اساس ساعت
    data.forEach(item => {
        const date = new Date(item.timestamp * 1000);
        const hour = date.getHours();
        
        if (hourlyData[hour] === null) {
            hourlyData[hour] = [];
        }
        hourlyData[hour].push(item[type]);
    });
    
    // محاسبه میانگین برای هر ساعت
    const processedData = hourlyData.map(hourData => {
        if (hourData === null || hourData.length === 0) {
            return null; // بدون داده
        }
        // محاسبه میانگین داده‌های آن ساعت
        const sum = hourData.reduce((a, b) => a + b, 0);
        return sum / hourData.length;
    });
    
    // ایجاد نقاط داده برای نمودار
    const chartData = [];
    const chartLabels = [];
    
    for (let i = 0; i < 24; i++) {
        chartLabels.push(hourlyLabels[i]);
        if (processedData[i] !== null) {
            chartData.push(processedData[i]);
        } else {
            chartData.push(null); // برای نمایش فاصله بدون داده
        }
    }
    
    mainChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: chartLabels,
            datasets: [{
                label: getTypeLabel(type),
                data: chartData,
                borderColor: chartColors[type],
                backgroundColor: chartColors[type] + '20',
                borderWidth: 2,
                fill: true,
                tension: 0.1,
                spanGaps: false, // نمایش فاصله‌های بدون داده
                pointRadius: 4,
                pointHoverRadius: 6
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    labels: { color: 'white' }
                },
                tooltip: {
                    mode: 'index',
                    intersect: false,
                    callbacks: {
                        title: function(context) {
                            const hour = context[0].dataIndex;
                            return `${hourlyLabels[hour]} - ${hourlyLabels[hour].split(':')[0]}:59`;
                        },
                        label: function(context) {
                            const value = context.parsed.y;
                            if (value === null) {
                                return 'بدون داده';
                            }
                            return `${getTypeLabel(type)}: ${value.toFixed(2)}`;
                        }
                    }
                }
            },
            scales: {
                x: {
                    ticks: { 
                        color: 'white',
                        maxTicksLimit: 12 // نمایش 12 برچسب ساعت
                    },
                    grid: { color: 'rgba(255,255,255,0.1)' },
                    title: {
                        display: true,
                        text: 'ساعت',
                        color: 'white'
                    }
                },
                y: {
                    ticks: { color: 'white' },
                    grid: { color: 'rgba(255,255,255,0.1)' },
                    title: {
                        display: true,
                        text: getTypeLabel(type),
                        color: 'white'
                    }
                }
            },
            interaction: {
                intersect: false,
                mode: 'index'
            }
        }
    });
}

// تابع بروزرسانی نمودار ساعتی - توزیع مصرف ساعتی
function updateHourlyChart(data) {
    const ctx = document.getElementById('hourlyChart').getContext('2d');
    
    if (hourlyChart) {
        hourlyChart.destroy();
    }
    
    // محاسبه توزیع ساعتی
    const hourlyData = new Array(24).fill(0);
    const hourlyCount = new Array(24).fill(0);
    const hourlyLabels = [];
    
    // ایجاد برچسب‌های ساعت
    for (let i = 0; i < 24; i++) {
        hourlyLabels.push(`${i.toString().padStart(2, '0')}:00`);
    }
    
    // گروه‌بندی داده‌ها بر اساس ساعت
    data.forEach(item => {
        const date = new Date(item.timestamp * 1000);
        const hour = date.getHours();
        hourlyData[hour] += item.power;
        hourlyCount[hour]++;
    });
    
    // محاسبه میانگین برای هر ساعت
    const processedData = [];
    for (let i = 0; i < 24; i++) {
        if (hourlyCount[i] > 0) {
            processedData.push(hourlyData[i] / hourlyCount[i]);
        } else {
            processedData.push(0); // بدون داده
        }
    }
    
    // تعیین رنگ بر اساس مقدار مصرف
    const backgroundColors = processedData.map(value => {
        if (value === 0) return 'rgba(128, 128, 128, 0.3)'; // خاکستری برای بدون داده
        if (value < 100) return 'rgba(76, 175, 80, 0.7)'; // سبز برای مصرف کم
        if (value < 500) return 'rgba(255, 193, 7, 0.7)'; // زرد برای مصرف متوسط
        return 'rgba(244, 67, 54, 0.7)'; // قرمز برای مصرف بالا
    });
    
    hourlyChart = new Chart(ctx, {
        type: 'bar',
        data: {
            labels: hourlyLabels,
            datasets: [{
                label: 'میانگین مصرف ساعتی (W)',
                data: processedData,
                backgroundColor: backgroundColors,
                borderColor: backgroundColors.map(color => color.replace('0.7', '1')),
                borderWidth: 1
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    labels: { color: 'white' }
                },
                tooltip: {
                    callbacks: {
                        title: function(context) {
                            const hour = context[0].dataIndex;
                            return `${hourlyLabels[hour]} - ${hourlyLabels[hour].split(':')[0]}:59`;
                        },
                        label: function(context) {
                            const value = context.parsed.y;
                            if (value === 0) {
                                return 'بدون داده';
                            }
                            return `میانگین مصرف: ${value.toFixed(1)}W`;
                        }
                    }
                }
            },
            scales: {
                x: {
                    ticks: { 
                        color: 'white',
                        maxTicksLimit: 12
                    },
                    grid: { color: 'rgba(255,255,255,0.1)' },
                    title: {
                        display: true,
                        text: 'ساعت',
                        color: 'white'
                    }
                },
                y: {
                    ticks: { color: 'white' },
                    grid: { color: 'rgba(255,255,255,0.1)' },
                    title: {
                        display: true,
                        text: 'توان (W)',
                        color: 'white'
                    }
                }
            }
        }
    });
}

// تابع بروزرسانی کارت‌های آمار
function updateStatisticsCards() {
    if (!currentStats || !currentStats.power) return;
    
    document.getElementById('peakValue').textContent = currentStats.power.max.toFixed(2) + 'W';
    document.getElementById('avgValue').textContent = currentStats.power.avg.toFixed(2) + 'W';
    document.getElementById('totalEnergy').textContent = currentStats.totalEnergy.toFixed(3) + 'kWh';
    
    // محاسبه ساعات پیک
    const peakHours = calculatePeakHours(currentData);
    document.getElementById('peakHours').textContent = peakHours.length;
}

// تابع بروزرسانی جدول تفصیلی
function updateDetailedTable() {
    const tbody = document.querySelector('#detailedTable tbody');
    const displayData = currentData.slice(-20); // آخرین 20 رکورد
    
    tbody.innerHTML = displayData.map(item => `
        <tr>
            <td>${formatLocalDateTime(item.timestamp)}</td>
            <td>${item.power.toFixed(2)}</td>
            <td>${item.voltage.toFixed(1)}</td>
            <td>${item.current.toFixed(3)}</td>
            <td>${item.energy.toFixed(3)}</td>
            <td>${item.pf.toFixed(3)}</td>
        </tr>
    `).join('');
}

// تابع محاسبه ساعات پیک
function calculatePeakHours(data) {
    const hourlyData = new Array(24).fill(0);
    const hourlyCount = new Array(24).fill(0);
    
    data.forEach(item => {
        const date = new Date(item.timestamp * 1000);
        const hour = date.getHours();
        hourlyData[hour] += item.power;
        hourlyCount[hour]++;
    });
    
    // محاسبه میانگین
    for (let i = 0; i < 24; i++) {
        if (hourlyCount[i] > 0) {
            hourlyData[i] /= hourlyCount[i];
        }
    }
    
    // یافتن ساعات پیک (بالاتر از میانگین)
    const avgPower = hourlyData.reduce((a, b) => a + b, 0) / 24;
    return hourlyData.map((power, hour) => ({ hour, power }))
                   .filter(item => item.power > avgPower)
                   .sort((a, b) => b.power - a.power);
}

// تابع بروزرسانی نمودارها
function updateCharts() {
    const analysisType = document.getElementById('analysisType').value;
    updateMainChart(currentData, analysisType);
    updateHourlyChart(currentData);
}

// تابع دریافت برچسب نوع تحلیل
function getTypeLabel(type) {
    const labels = {
        power: 'توان (W)',
        energy: 'انرژی (kWh)',
        voltage: 'ولتاژ (V)',
        current: 'جریان (A)',
        pf: 'ضریب توان'
    };
    return labels[type] || 'توان (W)';
}

// تابع تحلیل داده‌ها
async function performAnalysis() {
    if (!selectedFile) {
        alert('لطفاً ابتدا یک فایل انتخاب کنید');
        return;
    }
    
    await loadHistoryData([selectedFile]);
}

// تابع پیشنهاد سیستم خورشیدی
function showSolarRecommendations() {
    if (!currentStats || !currentData.length) {
        alert('ابتدا داده‌ها را تحلیل کنید');
        return;
    }
    
    const maxPower = currentStats.power.max;
    const avgPower = currentStats.power.avg;
    const totalEnergy = currentStats.totalEnergy;
    
    // محاسبات ساده برای سیستم خورشیدی
    const requiredSolarPower = maxPower * 1.2; // 20% اضافه
    const requiredBatteryCapacity = totalEnergy * 2; // 2 روز ذخیره
    const panelCount = Math.ceil(requiredSolarPower / 400); // هر پنل 400W
    const estimatedCost = panelCount * 2000000; // هر پنل 2 میلیون تومان
    
    const recommendations = `
        <div class="mb-3">
            <h6>توان مورد نیاز:</h6>
            <p>${requiredSolarPower.toFixed(0)}W (حداکثر مصرف: ${maxPower.toFixed(0)}W)</p>
        </div>
        <div class="mb-3">
            <h6>ظرفیت باتری:</h6>
            <p>${requiredBatteryCapacity.toFixed(1)}kWh</p>
        </div>
        <div class="mb-3">
            <h6>تعداد پنل:</h6>
            <p>${panelCount} عدد (400W هر پنل)</p>
        </div>
        <div class="mb-3">
            <h6>هزینه تقریبی:</h6>
            <p>${estimatedCost.toLocaleString()} تومان</p>
        </div>
    `;
    
    document.getElementById('solarRecommendations').innerHTML = recommendations;
}

// Event Listeners
document.addEventListener('DOMContentLoaded', function() {
    // بارگذاری فایل‌های موجود
    loadAvailableFiles();
    
    // دکمه تحلیل
    const loadAnalysisBtn = document.getElementById('loadAnalysis');
    if (loadAnalysisBtn) {
        loadAnalysisBtn.addEventListener('click', performAnalysis);
    }
    
    // دکمه پیشنهاد خورشیدی
    const solarRecommendationBtn = document.getElementById('solarRecommendation');
    if (solarRecommendationBtn) {
        solarRecommendationBtn.addEventListener('click', showSolarRecommendations);
    }
    
    // تغییر نوع تحلیل
    const analysisTypeSelect = document.getElementById('analysisType');
    if (analysisTypeSelect) {
        analysisTypeSelect.addEventListener('change', () => {
            if (currentData.length > 0) {
                updateCharts();
            }
        });
    }
    
    // دکمه‌های گزارش
    const exportPDFBtn = document.getElementById('exportPDF');
    if (exportPDFBtn) {
        exportPDFBtn.addEventListener('click', () => alert('گزارش PDF در حال توسعه...'));
    }
    
    const exportExcelBtn = document.getElementById('exportExcel');
    if (exportExcelBtn) {
        exportExcelBtn.addEventListener('click', () => alert('گزارش اکسل در حال توسعه...'));
    }
});
