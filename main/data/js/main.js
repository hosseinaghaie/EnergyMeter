// متغیرهای جهانی
let ws;
let isConnected = false;
let powerChart;
let chartType = 'power';
let multiAxisChart;
let chartPeriod = '1m'; // دوره زمانی پیش‌فرض
let lastValues = {}; // برای ذخیره آخرین مقادیر دریافتی
// مقادیر دینامیک مقیاس
let minMaxValues = {
    voltage: { min: 180, max: 250 },
    current: { min: 0, max: 1 },
    power: { min: 0, max: 200 },
    pf: { min: 0, max: 1 }
};
let chartData = {
    power: [],
    voltage: [],
    current: [],
    energy: [],
    pf: [],
    frequency: [],
    labels: [],
    datasets: [
        {
            label: 'ولتاژ (V)',
            data: [],
            borderColor: '#00ffc3',
            backgroundColor: 'rgba(0,255,195,0.1)',
            yAxisID: 'y-voltage',
            tension: 0.3,
            pointRadius: 2,
            fill: false,
        },
        {
            label: 'جریان (A)',
            data: [],
            borderColor: '#2196f3',
            backgroundColor: 'rgba(33,150,243,0.1)',
            yAxisID: 'y-current',
            tension: 0.3,
            pointRadius: 2,
            fill: false,
        },
        {
            label: 'توان (W)',
            data: [],
            borderColor: '#ff9800',
            backgroundColor: 'rgba(255,152,0,0.1)',
            yAxisID: 'y-power',
            tension: 0.3,
            pointRadius: 2,
            fill: false,
        },
        {
            label: 'پاورفکتور',
            data: [],
            borderColor: '#e91e63',
            backgroundColor: 'rgba(233,30,99,0.1)',
            yAxisID: 'y-pf',
            tension: 0.3,
            pointRadius: 2,
            fill: false,
        }
    ]
};
const maxDataPoints = 60; // 5 دقیقه با نمونه‌برداری هر 5 ثانیه

// تنظیم تعداد نقاط داده براساس دوره زمانی
function setMaxPointsByPeriod(period) {
    switch(period) {
        case '1m': return 60; // 60 نقطه برای 1 دقیقه
        case '5m': return 300; // 300 نقطه برای 5 دقیقه
        case '15m': return 900; // 900 نقطه برای 15 دقیقه
        case '1h': return 3600; // 3600 نقطه برای 1 ساعت
        default: return 60;
    }
}

// تابع اتصال به WebSocket
function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    
    ws = new WebSocket(wsUrl);

    ws.onopen = function() {
        console.log('WebSocket Connected');
        isConnected = true;
        document.body.classList.add('connected');
        const statusEl = document.querySelector('.status-text');
        if (statusEl) statusEl.textContent = 'متصل';
    };

    ws.onclose = function() {
        console.log('WebSocket Disconnected');
        isConnected = false;
        document.body.classList.remove('connected');
        const statusEl = document.querySelector('.status-text');
        if (statusEl) statusEl.textContent = 'قطع اتصال';
        // تلاش مجدد برای اتصال بعد از 5 ثانیه
        setTimeout(connectWebSocket, 5000);
    };

    ws.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            updateDashboard(data);
            updateAllCharts(data);
            updateTrends(data);
            updateScaleRanges(data);
        } catch (e) {
            console.error('Error parsing WebSocket message:', e);
        }
    };

    ws.onerror = function(error) {
        console.error('WebSocket Error:', error);
        isConnected = false;
        document.body.classList.remove('connected');
        const statusEl = document.querySelector('.status-text');
        if (statusEl) statusEl.textContent = 'خطای اتصال';
    };
}

// تابع به‌روزرسانی اجباری مقادیر
function forceRefreshReadings() {
    const refreshBtn = document.getElementById('force-refresh-btn');
    
    if (refreshBtn) {
        refreshBtn.disabled = true;
        refreshBtn.classList.add('btn-loading');
        refreshBtn.innerHTML = '<i class="bx bx-loader bx-spin"></i>';
        
        fetch('/api/reset-readings', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                updateDashboard(data);
                updateAllCharts(data);
                updateTrends(data);
                updateScaleRanges(data);
                console.log('مقادیر با موفقیت به‌روزرسانی شدند');
            } else {
                console.error('خطا در به‌روزرسانی مقادیر');
            }
        })
        .catch(error => {
            console.error('خطا در به‌روزرسانی مقادیر:', error);
        })
        .finally(() => {
            // فعال کردن مجدد دکمه پس از 2 ثانیه
            setTimeout(() => {
                refreshBtn.disabled = false;
                refreshBtn.classList.remove('btn-loading');
                refreshBtn.innerHTML = '<i class="bx bx-refresh"></i>';
            }, 2000);
        });
    }
}

// تابع به‌روزرسانی مقیاس‌های نمودار
function updateScaleRanges(data) {
    let updated = false;
    
    if (data.voltage !== undefined) {
        if (data.voltage < minMaxValues.voltage.min) {
            // برای مقدار حداقل، مقیاس را کاهش می‌دهیم
            minMaxValues.voltage.min = Math.floor(data.voltage * 0.95);
            updated = true;
        }
        if (data.voltage > minMaxValues.voltage.max) {
            // برای مقدار حداکثر، فقط افزایش می‌دهیم، هرگز کاهش نمی‌دهیم
            minMaxValues.voltage.max = Math.ceil(data.voltage * 1.05);
            updated = true;
        }
    }
    
    if (data.current !== undefined) {
        if (data.current < minMaxValues.current.min) {
            // برای مقدار حداقل، مقیاس را کاهش می‌دهیم اما از صفر کمتر نمی‌شود
            minMaxValues.current.min = 0; // جریان از صفر شروع می‌شود
            updated = true;
        }
        if (data.current > minMaxValues.current.max) {
            // برای مقدار حداکثر، فقط افزایش می‌دهیم، هرگز کاهش نمی‌دهیم
            minMaxValues.current.max = Math.ceil(data.current * 1.5);
            updated = true;
        }
        // حذف کد کاهش مقیاس برای حفظ حداکثر مقدار
    }
    
    if (data.power !== undefined) {
        if (data.power < minMaxValues.power.min) {
            // برای مقدار حداقل، مقیاس را کاهش می‌دهیم اما از صفر کمتر نمی‌شود
            minMaxValues.power.min = 0; // توان از صفر شروع می‌شود
            updated = true;
        }
        if (data.power > minMaxValues.power.max) {
            // برای مقدار حداکثر، فقط افزایش می‌دهیم، هرگز کاهش نمی‌دهیم
            minMaxValues.power.max = Math.ceil(data.power * 1.5);
            updated = true;
        }
        // حذف کد کاهش مقیاس برای حفظ حداکثر مقدار
    }
    
    if (data.pf !== undefined) {
        if (data.pf < minMaxValues.pf.min) {
            minMaxValues.pf.min = Math.max(Math.floor(data.pf * 0.9), 0);
            updated = true;
        }
        if (data.pf > minMaxValues.pf.max) {
            minMaxValues.pf.max = Math.min(Math.ceil(data.pf * 1.1), 1);
            updated = true;
        }
    }
    
    if (updated && multiAxisChart) {
        // به‌روزرسانی مقیاس‌های نمودار
        multiAxisChart.options.scales['y-voltage'].min = minMaxValues.voltage.min;
        multiAxisChart.options.scales['y-voltage'].max = minMaxValues.voltage.max;
        
        multiAxisChart.options.scales['y-current'].min = minMaxValues.current.min;
        multiAxisChart.options.scales['y-current'].max = minMaxValues.current.max;
        
        multiAxisChart.options.scales['y-power'].min = minMaxValues.power.min;
        multiAxisChart.options.scales['y-power'].max = minMaxValues.power.max;
        
        multiAxisChart.options.scales['y-pf'].min = minMaxValues.pf.min;
        multiAxisChart.options.scales['y-pf'].max = minMaxValues.pf.max;
        
        multiAxisChart.update();
    }
}

// تابع به‌روزرسانی داشبورد
function updateDashboard(data) {
    // به‌روزرسانی مقادیر گیج‌ها و نمایشگرها
    if (data.voltage !== undefined) document.getElementById('gaugeVoltageValue').textContent = data.voltage.toFixed(0);
    
    // برای جریان، همیشه مقدار واقعی را نشان دهیم
    if (data.current !== undefined) {
        const currentEl = document.getElementById('gaugeCurrentValue');
        currentEl.textContent = data.current.toFixed(2);
        currentEl.classList.remove('zero-load'); // حذف کلاس رنگ قرمز
    }
    
    // برای توان، همیشه مقدار واقعی را نشان دهیم
    if (data.power !== undefined) {
        const powerEl = document.getElementById('gaugePowerValue');
        powerEl.textContent = data.power.toFixed(0);
        powerEl.classList.remove('zero-load'); // حذف کلاس رنگ قرمز
    }
    
    // سایر مقادیر
    if (data.energy !== undefined) document.getElementById('total-energy').textContent = data.energy.toFixed(2);
    
    // برای توان راکتیو و ظاهری، مقدار واقعی را نشان دهیم
    if (data.reactivePower !== undefined) {
        const reactiveEl = document.getElementById('reactivePower');
        reactiveEl.textContent = data.reactivePower.toFixed(0);
    }
    
    if (data.apparentPower !== undefined) {
        const apparentEl = document.getElementById('apparentPower');
        apparentEl.textContent = data.apparentPower.toFixed(0);
    }
    
    if (data.frequency !== undefined) document.getElementById('frequency').textContent = data.frequency.toFixed(1);
    
    // برای ضریب توان، مقدار واقعی را نشان دهیم
    if (data.pf !== undefined) {
        const pfEl = document.getElementById('pf');
        pfEl.textContent = data.pf.toFixed(2);
    }
}

// تابع به‌روزرسانی روند تغییرات
function updateTrends(data) {
    // برای هر پارامتر، مقدار جدید را با مقدار قبلی مقایسه کرده و نشانگر روند را به‌روز می‌کنیم
    if (!lastValues.voltage && data.voltage) {
        lastValues = { ...data };
        return;
    }
    
    // بررسی روند ولتاژ
    if (data.voltage !== undefined) {
        const trendElement = document.querySelector('.info-card:nth-child(1) .info-trend i');
        updateTrendIcon(trendElement, data.voltage, lastValues.voltage);
    }
    
    // بررسی روند جریان
    if (data.current !== undefined) {
        const trendElement = document.querySelector('.info-card:nth-child(2) .info-trend i');
        updateTrendIcon(trendElement, data.current, lastValues.current);
    }
    
    // بررسی روند توان
    if (data.power !== undefined) {
        const trendElement = document.querySelector('.info-card:nth-child(3) .info-trend i');
        updateTrendIcon(trendElement, data.power, lastValues.power);
    }
    
    // بررسی روند انرژی
    if (data.energy !== undefined) {
        const trendElement = document.querySelector('.info-card:nth-child(4) .info-trend i');
        updateTrendIcon(trendElement, data.energy, lastValues.energy);
    }
    
    // بررسی روند ضریب توان
    if (data.pf !== undefined) {
        const trendElement = document.querySelector('.info-card:nth-child(5) .info-trend i');
        updateTrendIcon(trendElement, data.pf, lastValues.pf);
    }
    
    // بررسی روند فرکانس
    if (data.frequency !== undefined) {
        const trendElement = document.querySelector('.info-card:nth-child(6) .info-trend i');
        updateTrendIcon(trendElement, data.frequency, lastValues.frequency);
    }
    
    // بررسی روند توان ظاهری
    if (data.apparentPower !== undefined) {
        const trendElement = document.querySelector('.info-card:nth-child(7) .info-trend i');
        updateTrendIcon(trendElement, data.apparentPower, lastValues.apparentPower);
    }
    
    // بررسی روند توان راکتیو
    if (data.reactivePower !== undefined) {
        const trendElement = document.querySelector('.info-card:nth-child(8) .info-trend i');
        updateTrendIcon(trendElement, data.reactivePower, lastValues.reactivePower);
    }
    
    // ذخیره مقادیر فعلی برای مقایسه بعدی
    lastValues = { ...data };
}

// تابع به‌روزرسانی آیکون روند
function updateTrendIcon(element, newValue, oldValue) {
    if (!element || newValue === undefined || oldValue === undefined) return;
    
    // آستانه تغییر قابل توجه (0.5 درصد)
    const threshold = oldValue * 0.005;
    
    if (Math.abs(newValue - oldValue) <= threshold) {
        // تغییر ناچیز - روند خنثی
        element.className = 'bx bx-trending-neutral';
    } else if (newValue > oldValue) {
        // روند افزایشی
        element.className = 'bx bx-trending-up';
    } else {
        // روند کاهشی
        element.className = 'bx bx-trending-down';
    }
}

function updateAllCharts(data) {
    if (!data || Object.keys(data).length === 0) return;
    const now = new Date();
    const timestamp = now;
    ['power','voltage','current','energy','pf','frequency'].forEach(key => {
        if (data[key] !== undefined) {
            chartData[key].push({x: timestamp, y: data[key]});
            // محدود کردن تعداد نقاط داده براساس دوره زمانی انتخاب شده
            const maxPoints = setMaxPointsByPeriod(chartPeriod);
            if (chartData[key].length > maxPoints) chartData[key].shift();
        }
    });
    updateChart();
}

function updateChart() {
    if (!multiAxisChart) return;
    
    // به‌روزرسانی داده‌های نمودار
    multiAxisChart.update();
}

// تابع فرمت‌بندی زمان کارکرد
function formatUptime(seconds) {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
}

// تابع راه‌اندازی نمودار مالتی‌اکسیز حرفه‌ای
function initMultiAxisChart() {
    const ctx = document.getElementById('multiAxisChart').getContext('2d');
    multiAxisChart = new Chart(ctx, {
        type: 'line',
        data: {
            datasets: [
                {
                    label: 'ولتاژ (V)',
                    data: chartData.voltage,
                    borderColor: '#00ffc3',
                    backgroundColor: 'rgba(0,255,195,0.1)',
                    yAxisID: 'y-voltage',
                    tension: 0.3,
                    pointRadius: 1,
                    borderWidth: 2,
                    fill: false,
                },
                {
                    label: 'جریان (A)',
                    data: chartData.current,
                    borderColor: '#2196f3',
                    backgroundColor: 'rgba(33,150,243,0.1)',
                    yAxisID: 'y-current',
                    tension: 0.3,
                    pointRadius: 1,
                    borderWidth: 2,
                    fill: false,
                },
                {
                    label: 'توان (W)',
                    data: chartData.power,
                    borderColor: '#ff9800',
                    backgroundColor: 'rgba(255,152,0,0.1)',
                    yAxisID: 'y-power',
                    tension: 0.3,
                    pointRadius: 1,
                    borderWidth: 2,
                    fill: false,
                },
                {
                    label: 'پاورفکتور',
                    data: chartData.pf,
                    borderColor: '#e91e63',
                    backgroundColor: 'rgba(233,30,99,0.1)',
                    yAxisID: 'y-pf',
                    tension: 0.3,
                    pointRadius: 1,
                    borderWidth: 2,
                    fill: false,
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            interaction: { mode: 'index', intersect: false },
            stacked: false,
            animation: { duration: 0 },
            plugins: {
                legend: { display: false },
                tooltip: {
                    backgroundColor: 'rgba(0,0,0,0.8)',
                    titleFont: { family: 'Vazirmatn, Tahoma', size: 13 },
                    bodyFont: { family: 'Vazirmatn, Tahoma', size: 12 },
                    padding: 12,
                    displayColors: true,
                    callbacks: {
                        title: function(context) {
                            const date = new Date(context[0].parsed.x);
                            return date.toLocaleTimeString('fa-IR');
                        }
                    }
                }
            },
            scales: {
                x: {
                    type: 'time',
                    time: {
                        tooltipFormat: 'HH:mm:ss',
                        displayFormats: {
                            second: 'HH:mm:ss',
                            minute: 'HH:mm:ss'
                        }
                    },
                    grid: { color: 'rgba(255,255,255,0.1)' },
                    ticks: { color: '#fff', autoSkip: true, maxTicksLimit: 8 }
                },
                'y-voltage': {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: { display: true, text: 'ولتاژ (V)', color: '#00ffc3' },
                    min: minMaxValues.voltage.min,
                    max: minMaxValues.voltage.max,
                    grid: { color: 'rgba(255,255,255,0.05)' },
                    ticks: { color: '#00ffc3' }
                },
                'y-current': {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: { display: true, text: 'جریان (A)', color: '#2196f3' },
                    min: minMaxValues.current.min,
                    max: minMaxValues.current.max,
                    grid: { drawOnChartArea: false },
                    ticks: { color: '#2196f3' }
                },
                'y-power': {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: { display: true, text: 'توان (W)', color: '#ff9800' },
                    min: minMaxValues.power.min,
                    max: minMaxValues.power.max,
                    grid: { drawOnChartArea: false },
                    ticks: { color: '#ff9800' }
                },
                'y-pf': {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: { display: true, text: 'پاورفکتور', color: '#e91e63' },
                    min: minMaxValues.pf.min,
                    max: minMaxValues.pf.max,
                    grid: { drawOnChartArea: false },
                    ticks: { color: '#e91e63' }
                }
            }
        }
    });
}

// پیاده‌سازی فعال/غیرفعال کردن سری‌های نمودار
function setupChartLegend() {
    const legendItems = document.querySelectorAll('.legend-item input');
    legendItems.forEach(item => {
        item.addEventListener('change', function() {
            const series = this.dataset.series;
            
            // پیدا کردن ایندکس سری مورد نظر
            let index = -1;
            if (series === 'voltage') index = 0;
            else if (series === 'current') index = 1;
            else if (series === 'power') index = 2;
            else if (series === 'pf') index = 3;
            
            if (index >= 0 && multiAxisChart) {
                // تغییر وضعیت نمایش سری
                const meta = multiAxisChart.getDatasetMeta(index);
                meta.hidden = !meta.hidden;
                multiAxisChart.update();
            }
        });
    });
}

// تنظیم دوره زمانی نمودار
function setupChartPeriod() {
    const periodButtons = document.querySelectorAll('.chart-period button');
    periodButtons.forEach(button => {
        button.addEventListener('click', function() {
            // حذف کلاس active از همه دکمه‌ها
            periodButtons.forEach(btn => btn.classList.remove('active'));
            // اضافه کردن کلاس active به دکمه انتخاب شده
            this.classList.add('active');
            
            // تنظیم دوره زمانی جدید
            chartPeriod = this.dataset.period;
            
            // پاک‌سازی داده‌های قدیمی و محدود کردن به تعداد مناسب
            const maxPoints = setMaxPointsByPeriod(chartPeriod);
            Object.keys(chartData).forEach(key => {
                if (Array.isArray(chartData[key])) {
                    if (chartData[key].length > maxPoints) {
                        chartData[key] = chartData[key].slice(-maxPoints);
                    }
                }
            });
            
            // به‌روزرسانی نمودار
            if (multiAxisChart) {
                multiAxisChart.options.scales.x.time.unit = chartPeriod === '1h' ? 'minute' : 'second';
                multiAxisChart.update();
            }
        });
    });
}

// تابع برای ریست کردن شمارنده انرژی
function resetEnergyCounter() {
    // نمایش دیالوگ تأیید
    if (confirm('آیا از ریست کردن شمارنده انرژی اطمینان دارید؟')) {
        // ارسال درخواست به API برای ریست کردن شمارنده
        fetch('/api/reset-energy', {
            method: 'POST',
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                // به‌روزرسانی نمایش انرژی
                document.getElementById('total-energy').textContent = '0.00';
                // نمایش پیام موفقیت
                alert('شمارنده انرژی با موفقیت ریست شد.');
            } else {
                // نمایش پیام خطا
                alert('خطا در ریست کردن شمارنده انرژی: ' + data.message);
            }
        })
        .catch(error => {
            console.error('Error resetting energy counter:', error);
            alert('خطا در ارتباط با سرور. لطفاً مجدداً تلاش کنید.');
        });
    }
}

// تابع دریافت مقادیر اولیه از سرور
function fetchInitialData() {
    fetch('/api/current')
        .then(response => response.json())
        .then(data => {
            updateDashboard(data);
            lastValues = { ...data };
        })
        .catch(error => {
            console.error('خطا در دریافت مقادیر اولیه:', error);
        });
}

// تابع ریست کردن مقیاس‌های نمودار
function resetChartScales() {
    if (!multiAxisChart) return;
    
    // خواندن مقادیر فعلی
    const currentSample = {
        voltage: parseFloat(document.getElementById('gaugeVoltageValue').textContent) || 220,
        current: parseFloat(document.getElementById('gaugeCurrentValue').textContent) || 0.1,
        power: parseFloat(document.getElementById('gaugePowerValue').textContent) || 10,
        pf: parseFloat(document.getElementById('pf').textContent) || 0.8
    };
    
    // تنظیم مقیاس‌ها براساس مقادیر فعلی
    minMaxValues.voltage.min = Math.floor(currentSample.voltage * 0.9);
    minMaxValues.voltage.max = Math.ceil(currentSample.voltage * 1.1);
    
    // برای جریان و توان، حداقل مقدار صفر است
    minMaxValues.current.min = 0;
    minMaxValues.current.max = Math.max(Math.ceil(currentSample.current * 2), 1);
    
    minMaxValues.power.min = 0;
    minMaxValues.power.max = Math.max(Math.ceil(currentSample.power * 2), 50);
    
    minMaxValues.pf.min = 0;
    minMaxValues.pf.max = 1;
    
    // به‌روزرسانی مقیاس‌های نمودار
    multiAxisChart.options.scales['y-voltage'].min = minMaxValues.voltage.min;
    multiAxisChart.options.scales['y-voltage'].max = minMaxValues.voltage.max;
    
    multiAxisChart.options.scales['y-current'].min = minMaxValues.current.min;
    multiAxisChart.options.scales['y-current'].max = minMaxValues.current.max;
    
    multiAxisChart.options.scales['y-power'].min = minMaxValues.power.min;
    multiAxisChart.options.scales['y-power'].max = minMaxValues.power.max;
    
    multiAxisChart.options.scales['y-pf'].min = minMaxValues.pf.min;
    multiAxisChart.options.scales['y-pf'].max = minMaxValues.pf.max;
    
    multiAxisChart.update();
    
    console.log('مقیاس‌های نمودار ریست شدند');
}

// تابع راه‌اندازی برنامه
function initApp() {
    // اتصال به WebSocket
    connectWebSocket();
    
    // راه‌اندازی نمودارها
    initMultiAxisChart();
    
    // دکمه به‌روزرسانی اجباری
    document.getElementById('force-refresh-btn')?.addEventListener('click', forceRefreshReadings);
    
    // دکمه ریست انرژی
    document.querySelector('.reset-energy-btn')?.addEventListener('click', resetEnergyCounter);
    
    // دکمه ریست مقیاس نمودار - روش جدید با مکان مشخص
    const chartPeriodDiv = document.querySelector('.chart-period');
    if (chartPeriodDiv) {
        // ایجاد دکمه ریست
        const resetButton = document.createElement('button');
        resetButton.id = 'reset-scales-btn';
        resetButton.className = 'btn btn-sm btn-outline-danger ms-2';
        resetButton.innerHTML = '<i class="bx bx-reset"></i> ریست مقیاس';
        resetButton.addEventListener('click', resetChartScales);
        
        // اضافه کردن به کنار دکمه‌های دوره زمانی
        chartPeriodDiv.appendChild(resetButton);
    } else {
        // اگر مکان مورد نظر پیدا نشد، سعی می‌کنیم در نوار ابزار بالای نمودار اضافه کنیم
        const chartContainer = document.querySelector('.chart-container') || document.querySelector('.chart-card');
        if (chartContainer) {
            // ایجاد دکمه ریست
            const resetButton = document.createElement('button');
            resetButton.id = 'reset-scales-btn';
            resetButton.className = 'btn btn-sm btn-outline-danger float-end m-2';
            resetButton.innerHTML = '<i class="bx bx-reset"></i> ریست مقیاس';
            resetButton.addEventListener('click', resetChartScales);
            
            // اضافه کردن به ابتدای المان نمودار
            chartContainer.insertBefore(resetButton, chartContainer.firstChild);
        }
    }
    
    // گرفتن داده‌های اولیه
    fetchInitialData();
    
    // راه‌اندازی کنترل‌های نمودار
    setupChartLegend();
    setupChartPeriod();
    
    // در مورد نسخه دمو، داده‌های تستی را پر می‌کنیم
    if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
        setTimeout(fillTestData, 1000);
    }
}

function fillTestData() {
    // داده‌های آزمایشی برای تست در محیط توسعه
    const testData = {
        voltage: 220 + Math.random() * 10 - 5,
        current: 0.05 + Math.random() * 0.1,
        power: 10 + Math.random() * 15,
        energy: 36.34 + Math.random() * 0.01,
        frequency: 50 + Math.random() * 0.2 - 0.1,
        pf: 0.82 + Math.random() * 0.05 - 0.025,
        reactivePower: Math.floor(Math.random() * 5),
        apparentPower: Math.floor(15 + Math.random() * 10)
    };
    updateDashboard(testData);
    updateAllCharts(testData);
    updateTrends(testData);
    updateScaleRanges(testData);
}

// بارگذاری و راه‌اندازی برنامه
document.addEventListener('DOMContentLoaded', initApp);

// مقداردهی اولیه فرم تنظیمات از API
if (window.location.pathname.endsWith('config.html')) {
    document.addEventListener('DOMContentLoaded', function() {
        fetch('/api/config')
            .then(res => res.json())
            .then(cfg => {
                // شبکه
                document.getElementById('apEnable').checked = cfg.network.apEnable;
                document.getElementById('apSSID').value = cfg.network.apSSID;
                document.getElementById('apPassword').value = cfg.network.apPassword;
                document.getElementById('apIP').value = cfg.network.apIP;
                document.getElementById('clientEnable').checked = cfg.network.clientEnable;
                document.getElementById('clientSSID').value = cfg.network.clientSSID;
                document.getElementById('clientPassword').value = cfg.network.clientPassword;
                document.getElementById('mdnsName').value = cfg.network.mdnsName;
                document.getElementById('ipMode').value = cfg.network.ipMode;
                document.getElementById('clientIP').value = cfg.network.clientIP;
                document.getElementById('clientGateway').value = cfg.network.clientGateway;
                document.getElementById('clientSubnet').value = cfg.network.clientSubnet;
                // کاربری
                document.getElementById('username').value = cfg.user.username;
                document.getElementById('authEnable').checked = cfg.user.authEnable;
                // سنسور
                document.getElementById('sampleRate').value = cfg.sensor.sampleRate;
                document.getElementById('avgSamples').value = cfg.sensor.avgSamples;
                document.getElementById('minVoltage').value = cfg.sensor.minVoltage;
                document.getElementById('maxVoltage').value = cfg.sensor.maxVoltage;
                document.getElementById('minCurrent').value = cfg.sensor.minCurrent;
                document.getElementById('maxCurrent').value = cfg.sensor.maxCurrent;
                document.getElementById('minPower').value = cfg.sensor.minPower;
                document.getElementById('maxPower').value = cfg.sensor.maxPower;
                document.getElementById('alertEnable').checked = cfg.sensor.alertEnable;
                
                // تنظیمات جدید به‌روزرسانی مقادیر
                if (document.getElementById('updateMethod')) {
                    document.getElementById('updateMethod').value = cfg.sensor.updateMethod || 'average';
                    document.getElementById('valueThreshold').value = cfg.sensor.valueThreshold || 5;
                    // اجرای رویداد تغییر برای نمایش/مخفی‌سازی تنظیمات مربوطه
                    document.getElementById('updateMethod').dispatchEvent(new Event('change'));
                }
                
                // زمان
                document.getElementById('gmtOffset').value = cfg.time.gmtOffset;
                document.getElementById('gmtSign').value = cfg.time.gmtPositive ? '+' : '-';
                document.getElementById('ntpServer').value = cfg.time.ntpServer;
            });
    });
}

// تابع برای فعال/غیرفعال کردن لاگ سریال
function toggleSerialLogging() {
    const btn = document.getElementById('toggle-serial-log-btn');
    const statusSpan = btn.querySelector('.serial-log-status');
    
    if (!btn) return;
    
    btn.disabled = true;
    
    fetch('/api/toggle-serial-log', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        }
    })
    .then(response => response.json())
    .then(data => {
        if (data.enabled) {
            btn.classList.remove('btn-secondary');
            btn.classList.add('btn-warning');
            statusSpan.textContent = 'غیرفعال کردن لاگ سریال';
            console.log('لاگ سریال فعال شد');
        } else {
            btn.classList.remove('btn-warning');
            btn.classList.add('btn-secondary');
            statusSpan.textContent = 'فعال کردن لاگ سریال';
            console.log('لاگ سریال غیرفعال شد');
        }
    })
    .catch(error => {
        console.error('خطا در تغییر وضعیت لاگ سریال:', error);
    })
    .finally(() => {
        btn.disabled = false;
    });
} 