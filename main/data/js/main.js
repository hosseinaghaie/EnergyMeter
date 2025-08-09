// متغیرهای جهانی
let ws;
let isConnected = false;
let powerChart;
let chartType = 'power';
let multiAxisChart;
let chartPeriod = '1m'; // دوره زمانی پیش‌فرض
let lastValues = {}; // برای ذخیره آخرین مقادیر دریافتی
let esp32Time = null; // زمان ESP32
let esp32TimeSource = 'Internal RTC'; // منبع زمان ESP32
let timeSyncStatus = 'SYNCING'; // وضعیت time sync

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
            // console.log('WebSocket data received:', data); // کامنت شده برای کاهش حجم console
            
            // Check if data is valid
            if (!data || typeof data !== 'object') {
                console.error('Invalid data received:', data);
                return;
            }
            
            updateDashboard(data);
            updateAllCharts(data);
            updateTrends(data);
            updateScaleRanges(data);
        } catch (e) {
            console.error('Error parsing WebSocket message:', e);
            console.error('Raw message:', event.data);
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
    // ذخیره زمان ESP32 و منبع زمان
    if (data.timestamp !== undefined) {
        esp32Time = data.timestamp;
    }
    if (data.timeSource !== undefined) {
        esp32TimeSource = data.timeSource;
    }
    
    // به‌روزرسانی مقادیر گیج‌ها و نمایشگرها با null checks
    if (data.voltage !== undefined && data.voltage !== null) {
        const voltageEl = document.getElementById('gaugeVoltageValue');
        if (voltageEl) voltageEl.textContent = data.voltage.toFixed(0);
    }
    
    // برای جریان، همیشه مقدار واقعی را نشان دهیم
    if (data.current !== undefined && data.current !== null) {
        const currentEl = document.getElementById('gaugeCurrentValue');
        if (currentEl) {
            currentEl.textContent = data.current.toFixed(2);
            currentEl.classList.remove('zero-load'); // حذف کلاس رنگ قرمز
        }
    }
    
    // برای توان، همیشه مقدار واقعی را نشان دهیم
    if (data.power !== undefined && data.power !== null) {
        const powerEl = document.getElementById('gaugePowerValue');
        if (powerEl) {
            powerEl.textContent = data.power.toFixed(0);
            powerEl.classList.remove('zero-load'); // حذف کلاس رنگ قرمز
        }
    }
    
    // سایر مقادیر
    if (data.energy !== undefined && data.energy !== null) {
        const energyEl = document.getElementById('total-energy');
        if (energyEl) energyEl.textContent = data.energy.toFixed(2);
    }
    
    // برای توان راکتیو و ظاهری، مقدار واقعی را نشان دهیم
    if (data.reactivePower !== undefined && data.reactivePower !== null) {
        const reactiveEl = document.getElementById('reactivePower');
        if (reactiveEl) reactiveEl.textContent = data.reactivePower.toFixed(0);
    }
    
    if (data.apparentPower !== undefined && data.apparentPower !== null) {
        const apparentEl = document.getElementById('apparentPower');
        if (apparentEl) apparentEl.textContent = data.apparentPower.toFixed(0);
    }
    
    if (data.frequency !== undefined && data.frequency !== null) {
        const freqEl = document.getElementById('frequency');
        if (freqEl) freqEl.textContent = data.frequency.toFixed(1);
    }
    
    // برای ضریب توان، مقدار واقعی را نشان دهیم
    if (data.pf !== undefined && data.pf !== null) {
        const pfEl = document.getElementById('pf');
        if (pfEl) pfEl.textContent = data.pf.toFixed(2);
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
    
    // استفاده از زمان ESP32 اگر موجود باشد، در غیر این صورت از زمان کلاینت
    let timestamp;
    if (data.timestamp !== undefined) {
        // ESP32 زمان را با Tehran offset ارسال می‌کند
        // باید آن را به UTC تبدیل کنیم تا JavaScript درست تفسیر کند
        const utcTime = data.timestamp - (3.5 * 60 * 60); // کم کردن offset تهران
        timestamp = new Date(utcTime * 1000); // تبدیل epoch به Date
    } else {
        timestamp = new Date(); // زمان کلاینت
    }
    
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

// تابع تبدیل تاریخ میلادی به شمسی (ساده و سریع)
function toJalali(gy, gm, gd) {
    // الگوریتم تبدیل ساده (برگرفته از jalaali-js)
    var g_d_m = [0,31,59,90,120,151,181,212,243,273,304,334];
    var jy = (gy <= 1600) ? 0 : 979;
    gy -= (gy <= 1600) ? 621 : 1600;
    var gy2 = (gm > 2) ? (gy + 1) : gy;
    var days = (365 * gy) + Math.floor((gy2 + 3) / 4) - Math.floor((gy2 + 99) / 100) + Math.floor((gy2 + 399) / 400) - 80 + gd + g_d_m[gm - 1];
    jy += 33 * Math.floor(days / 12053);
    days %= 12053;
    jy += 4 * Math.floor(days / 1461);
    days %= 1461;
    if (days > 365) {
        jy += Math.floor((days - 1) / 365);
        days = (days - 1) % 365;
    }
    var jm = (days < 186) ? 1 + Math.floor(days / 31) : 7 + Math.floor((days - 186) / 30);
    var jd = 1 + ((days < 186) ? (days % 31) : ((days - 186) % 30));
    return {jy, jm, jd};
}

function pad2(n) { return n < 10 ? '0' + n : n; }

function updatePersianDateTimeNavbar() {
    let now;
    
    // اگر زمان ESP32 موجود باشد، از آن استفاده کن
    if (esp32Time) {
        // ESP32 زمان را با offset تهران ارسال می‌کند، اما JavaScript آن را local time تفسیر می‌کند
        // باید آن را به UTC تبدیل کنیم
        const utcTime = esp32Time - (3.5 * 60 * 60); // کم کردن offset تهران
        now = new Date(utcTime * 1000); // تبدیل epoch به Date
        
        // Debug: نمایش زمان ESP32 (کامنت شده برای کاهش حجم console)
        /*
        console.log('=== TIME DEBUG ===');
        console.log('ESP32 Time (with Tehran offset):', esp32Time);
        console.log('UTC Time (without offset):', utcTime);
        console.log('Date Object:', now);
        console.log('Hours:', now.getHours(), 'Minutes:', now.getMinutes(), 'Seconds:', now.getSeconds());
        console.log('ISO String:', now.toISOString());
        console.log('Local String:', now.toString());
        console.log('==================');
        */
    } else {
        // در غیر این صورت از زمان کلاینت استفاده کن
        now = new Date();
        // تبدیل به وقت تهران
        const tehranOffset = 3.5 * 60; // دقیقه
        const utc = now.getTime() + (now.getTimezoneOffset() * 60000);
        now = new Date(utc + tehranOffset * 60000);
    }
    
    // تبدیل به شمسی
    const gYear = now.getFullYear();
    const gMonth = now.getMonth() + 1;
    const gDay = now.getDate();
    const {jy, jm, jd} = toJalali(gYear, gMonth, gDay);
    
    // نمایش ساعت و تاریخ
    const h = pad2(now.getHours());
    const m = pad2(now.getMinutes());
    const s = pad2(now.getSeconds());
    const persianDate = `${jy}/${pad2(jm)}/${pad2(jd)}`;
    const persianTime = `${h}:${m}:${s}`;
    
    const timeEl = document.getElementById('persian-navbar-time');
    const dateEl = document.getElementById('persian-navbar-date');
    
    if (timeEl) {
        timeEl.textContent = persianTime;
        // اضافه کردن نشانگر منبع زمان
        if (esp32TimeSource === 'NTP') {
            timeEl.title = 'زمان دقیق (NTP)';
        } else {
            timeEl.title = 'زمان داخلی (RTC)';
        }
    }
    if (dateEl) dateEl.textContent = persianDate;
}
setInterval(updatePersianDateTimeNavbar, 1000);

// تابع نمایش وضعیت time sync
function updateTimeSyncStatus() {
    fetch('/api/time-status')
        .then(response => response.json())
        .then(data => {
            timeSyncStatus = data.status;
            esp32TimeSource = data.timeSource;
            
            // بروزرسانی نمایش time در navbar
            const timeEl = document.getElementById('persian-navbar-time');
            if (timeEl) {
                if (data.status === 'SYNCING') {
                    timeEl.textContent = 'Syncing...';
                    timeEl.title = 'Time sync in progress';
                } else {
                    timeEl.title = data.message;
                }
            }
        })
        .catch(error => {
            console.error('Error fetching time status:', error);
        });
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
    
    // دکمه‌های کنترل لاگ کردن
    document.getElementById('toggle-logging-btn')?.addEventListener('click', toggleHistoryLogging);
    document.getElementById('start-stop-logging-btn')?.addEventListener('click', startStopLogging);
    
    // بارگذاری وضعیت لاگ کردن
    loadLoggingStatus();
    
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
    
    // بروزرسانی وضعیت time sync
    updateTimeSyncStatus();
    setInterval(updateTimeSyncStatus, 5000); // هر 5 ثانیه چک کن
    
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

// تابع برای فعال/غیرفعال کردن قابلیت لاگ کردن تاریخی
function toggleHistoryLogging() {
    const btn = document.getElementById('toggle-logging-btn');
    
    if (!btn) return;
    
    btn.disabled = true;
    
    fetch('/api/toggle-history-logging', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        }
    })
    .then(response => response.json())
    .then(data => {
        if (data.enabled) {
            btn.classList.remove('btn-success');
            btn.classList.add('btn-secondary');
            btn.innerHTML = '<i class="bx bx-data"></i>';
            btn.title = 'غیرفعال کردن لاگ کردن';
            console.log('قابلیت لاگ کردن تاریخی فعال شد');
        } else {
            btn.classList.remove('btn-secondary');
            btn.classList.add('btn-success');
            btn.innerHTML = '<i class="bx bx-data"></i>';
            btn.title = 'فعال کردن لاگ کردن';
            console.log('قابلیت لاگ کردن تاریخی غیرفعال شد');
        }
        
        // بروزرسانی وضعیت دکمه شروع/توقف
        updateStartStopButton(data.active);
    })
    .catch(error => {
        console.error('خطا در تغییر وضعیت لاگ کردن تاریخی:', error);
    })
    .finally(() => {
        btn.disabled = false;
    });
}

// تابع برای شروع/توقف لاگ کردن فعال
function startStopLogging() {
    const btn = document.getElementById('start-stop-logging-btn');
    
    if (!btn) return;
    
    btn.disabled = true;
    
    fetch('/api/start-stop-logging', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        }
    })
    .then(response => response.json())
    .then(data => {
        if (data.active) {
            btn.classList.remove('btn-warning');
            btn.classList.add('btn-danger');
            btn.innerHTML = '<i class="bx bx-stop"></i>';
            btn.title = 'توقف لاگ کردن';
            console.log('لاگ کردن شروع شد');
        } else {
            btn.classList.remove('btn-danger');
            btn.classList.add('btn-warning');
            btn.innerHTML = '<i class="bx bx-play"></i>';
            btn.title = 'شروع لاگ کردن';
            console.log('لاگ کردن متوقف شد');
        }
    })
    .catch(error => {
        console.error('خطا در شروع/توقف لاگ کردن:', error);
    })
    .finally(() => {
        btn.disabled = false;
    });
}

// تابع برای بروزرسانی وضعیت دکمه شروع/توقف
function updateStartStopButton(isActive) {
    const btn = document.getElementById('start-stop-logging-btn');
    
    if (!btn) return;
    
    if (isActive) {
        btn.classList.remove('btn-warning');
        btn.classList.add('btn-danger');
        btn.innerHTML = '<i class="bx bx-stop"></i>';
        btn.title = 'توقف لاگ کردن';
    } else {
        btn.classList.remove('btn-danger');
        btn.classList.add('btn-warning');
        btn.innerHTML = '<i class="bx bx-play"></i>';
        btn.title = 'شروع لاگ کردن';
    }
}

// تابع برای بارگذاری وضعیت لاگ کردن
function loadLoggingStatus() {
    fetch('/api/logging-status')
        .then(response => response.json())
        .then(data => {
            const toggleBtn = document.getElementById('toggle-logging-btn');
            const startStopBtn = document.getElementById('start-stop-logging-btn');
            
            if (toggleBtn) {
                if (data.historyLoggingEnabled) {
                    toggleBtn.classList.remove('btn-success');
                    toggleBtn.classList.add('btn-secondary');
                    toggleBtn.title = 'غیرفعال کردن لاگ کردن';
                } else {
                    toggleBtn.classList.remove('btn-secondary');
                    toggleBtn.classList.add('btn-success');
                    toggleBtn.title = 'فعال کردن لاگ کردن';
                }
            }
            
            if (startStopBtn) {
                updateStartStopButton(data.historyLoggingActive);
            }
        })
        .catch(error => {
            console.error('خطا در بارگذاری وضعیت لاگ کردن:', error);
        });
} 