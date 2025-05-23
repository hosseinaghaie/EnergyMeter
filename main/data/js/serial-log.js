// متغیرهای جهانی
let ws;
let isConnected = false;
let logContent = [];
const MAX_LOG_ENTRIES = 500; // حداکثر تعداد خطوط لاگ

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
        // پس از اتصال، وضعیت فعلی لاگ سریال را چک می‌کنیم
        checkSerialLoggingStatus();
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
            
            // اگر داده سریال دریافت شد، آن را به لاگ اضافه می‌کنیم
            if (data.serialLog) {
                addToSerialLog(data.serialLog);
            }
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

// اضافه کردن متن به لاگ سریال
function addToSerialLog(text) {
    // زمان فعلی را به لاگ اضافه می‌کنیم
    const timestamp = new Date().toLocaleTimeString('fa-IR');
    const logEntry = `[${timestamp}] ${text}`;
    
    // اضافه کردن به آرایه لاگ
    logContent.push(logEntry);
    
    // اگر تعداد خطوط بیشتر از حد مجاز شد، قدیمی‌ترین را حذف می‌کنیم
    if (logContent.length > MAX_LOG_ENTRIES) {
        logContent.shift();
    }
    
    // به‌روزرسانی نمایش لاگ
    updateLogDisplay();
}

// به‌روزرسانی نمایش لاگ
function updateLogDisplay() {
    const logElement = document.getElementById('serial-log-content');
    if (logElement) {
        logElement.textContent = logContent.join('\n');
        // اسکرول به پایین
        logElement.scrollTop = logElement.scrollHeight;
    }
}

// بررسی وضعیت فعلی لاگ سریال
function checkSerialLoggingStatus() {
    fetch('/api/config')
        .then(response => response.json())
        .then(config => {
            updateSerialLogButton(config.advanced?.serialLoggingEnabled || false);
            
            // به‌روزرسانی مقدار اسلایدر فاصله زمانی
            const intervalRange = document.getElementById('log-interval-range');
            const intervalValue = document.getElementById('log-interval-value');
            
            if (intervalRange && intervalValue && config.advanced) {
                intervalRange.value = config.advanced.serialLogInterval || 1000;
                intervalValue.textContent = `${intervalRange.value} میلی‌ثانیه`;
            }
        })
        .catch(error => {
            console.error('Error fetching config:', error);
        });
}

// به‌روزرسانی دکمه لاگ سریال
function updateSerialLogButton(isEnabled) {
    const btn = document.getElementById('toggle-serial-log-btn');
    const statusSpan = btn?.querySelector('.serial-log-status');
    
    if (!btn || !statusSpan) return;
    
    if (isEnabled) {
        btn.classList.remove('btn-secondary');
        btn.classList.add('btn-success');
        statusSpan.textContent = 'غیرفعال کردن لاگ سریال';
        addToSerialLog('لاگ سریال فعال است');
    } else {
        btn.classList.remove('btn-success');
        btn.classList.add('btn-secondary');
        statusSpan.textContent = 'فعال کردن لاگ سریال';
        addToSerialLog('لاگ سریال غیرفعال است');
    }
}

// تغییر وضعیت لاگ سریال
function toggleSerialLogging() {
    const btn = document.getElementById('toggle-serial-log-btn');
    
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
        updateSerialLogButton(data.enabled);
    })
    .catch(error => {
        console.error('خطا در تغییر وضعیت لاگ سریال:', error);
    })
    .finally(() => {
        btn.disabled = false;
    });
}

// تنظیم فاصله زمانی لاگ سریال
function setLogInterval(interval) {
    fetch('/api/set-log-interval', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/x-www-form-urlencoded'
        },
        body: `interval=${interval}`
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            addToSerialLog(`فاصله زمانی لاگ به ${interval} میلی‌ثانیه تغییر کرد`);
        }
    })
    .catch(error => {
        console.error('خطا در تنظیم فاصله زمانی لاگ:', error);
    });
}

// پاک کردن لاگ
function clearLog() {
    logContent = [];
    updateLogDisplay();
    addToSerialLog('لاگ پاک شد');
}

// راه‌اندازی رویدادها
function initEvents() {
    // دکمه فعال/غیرفعال کردن لاگ سریال
    document.getElementById('toggle-serial-log-btn')?.addEventListener('click', toggleSerialLogging);
    
    // دکمه پاک کردن لاگ
    document.getElementById('clear-log-btn')?.addEventListener('click', clearLog);
    
    // اسلایدر فاصله زمانی
    const intervalRange = document.getElementById('log-interval-range');
    const intervalValue = document.getElementById('log-interval-value');
    
    if (intervalRange && intervalValue) {
        intervalRange.addEventListener('input', function() {
            intervalValue.textContent = `${this.value} میلی‌ثانیه`;
        });
        
        intervalRange.addEventListener('change', function() {
            setLogInterval(this.value);
        });
    }
}

// راه‌اندازی برنامه
function initApp() {
    // اگر در صفحه لاگ سریال مستقل یا در تب لاگ سریال تنظیمات هستیم
    if (document.getElementById('serial-log-content')) {
        // اتصال به WebSocket
        connectWebSocket();
        
        // راه‌اندازی رویدادها
        initEvents();
        
        // اضافه کردن پیام شروع
        addToSerialLog('صفحه لاگ سریال راه‌اندازی شد');
    }
}

// شروع برنامه پس از بارگذاری صفحه
document.addEventListener('DOMContentLoaded', initApp); 