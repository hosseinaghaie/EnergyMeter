document.addEventListener('DOMContentLoaded', function() {
    // مقداردهی اولیه فرم
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
            
            // تنظیمات پیشرفته
            fetch('/api/advanced-settings')
                .then(res => res.json())
                .then(advSettings => {
                    // پر کردن فیلدهای تنظیمات پیشرفته
                    if (document.getElementById('updateMethod')) {
                        // مپ کردن مقادیر عددی به مقادیر متنی برای select
                        let updateMethodValue = "0"; // مستقیم - پیش‌فرض
                        if (advSettings.updateMethod === 1) updateMethodValue = "1"; // میانگین‌گیری
                        else if (advSettings.updateMethod === 2) updateMethodValue = "2"; // حفظ حداکثر
                        
                        document.getElementById('updateMethod').value = updateMethodValue;
                        document.getElementById('thresholdPercent').value = advSettings.thresholdPercent || 20;
                        document.getElementById('voltageOffset').value = advSettings.voltageOffset || 0;
                        document.getElementById('currentOffset').value = advSettings.currentOffset || 0;
                        document.getElementById('powerOffset').value = advSettings.powerOffset || 0;
                    }
                })
                .catch(err => {
                    console.error('خطا در دریافت تنظیمات پیشرفته:', err);
                });
            
            // زمان
            document.getElementById('gmtOffset').value = cfg.time.gmtOffset;
            document.getElementById('gmtSign').value = cfg.time.gmtPositive ? '+' : '-';
            document.getElementById('ntpServer').value = cfg.time.ntpServer;
        });

    // رویداد تغییر روش به‌روزرسانی
    document.getElementById('updateMethod')?.addEventListener('change', function() {
        const thresholdContainer = document.getElementById('thresholdContainer');
        if (this.value === "2") { // حفظ حداکثر
            thresholdContainer.style.display = 'block';
        } else {
            thresholdContainer.style.display = 'none';
        }
    });

    // ذخیره تنظیمات شبکه
    document.getElementById('saveNetwork').addEventListener('click', function() {
        sendConfig();
    });
    // ذخیره تنظیمات کاربری
    document.getElementById('saveUser').addEventListener('click', function() {
        sendConfig();
    });
    
    // ذخیره تنظیمات سنسور
    document.getElementById('saveSensor')?.addEventListener('click', function() {
        sendConfig();
    });
    
    // ذخیره تنظیمات پیشرفته
    document.getElementById('saveAdvanced')?.addEventListener('click', function() {
        const methodValue = parseInt(document.getElementById('updateMethod').value);
        
        // ذخیره روش به‌روزرسانی
        fetch('/api/set-update-method', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: 'method=' + methodValue
        })
        .then(res => res.json())
        .then(data => {
            if (data.success) {
                console.log('روش به‌روزرسانی با موفقیت تغییر کرد');
                
                // ذخیره آستانه درصد کاهش
                if (methodValue === 2) { // فقط برای روش حفظ حداکثر
                    const percent = parseFloat(document.getElementById('thresholdPercent').value);
                    return fetch('/api/set-threshold', {
                        method: 'POST',
                        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                        body: 'percent=' + percent
                    });
                }
            } else {
                alert('خطا در تغییر روش به‌روزرسانی');
                throw new Error('خطا در تغییر روش به‌روزرسانی');
            }
        })
        .then(res => res ? res.json() : null)
        .then(data => {
            if (data && data.success) {
                console.log('آستانه درصد کاهش با موفقیت تغییر کرد');
            }
            
            // ذخیره افست‌های کالیبراسیون
            const voltageOffset = parseFloat(document.getElementById('voltageOffset').value) || 0;
            const currentOffset = parseFloat(document.getElementById('currentOffset').value) || 0;
            const powerOffset = parseFloat(document.getElementById('powerOffset').value) || 0;
            
            return fetch('/api/set-calibration', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'voltage=' + voltageOffset + '&current=' + currentOffset + '&power=' + powerOffset
            });
        })
        .then(res => res.json())
        .then(data => {
            alert('تنظیمات پیشرفته با موفقیت ذخیره شد!');
        })
        .catch(err => {
            console.error(err);
            alert('خطا در ذخیره تنظیمات پیشرفته!');
        });
    });

    function sendConfig() {
        let password = document.getElementById('newPassword').value;
        if (!password) {
            password = document.getElementById('username').value;
        }
        const cfg = {
            network: {
                apEnable: document.getElementById('apEnable').checked,
                apSSID: document.getElementById('apSSID').value,
                apPassword: document.getElementById('apPassword').value,
                apIP: document.getElementById('apIP').value,
                clientEnable: document.getElementById('clientEnable').checked,
                clientSSID: document.getElementById('clientSSID').value,
                clientPassword: document.getElementById('clientPassword').value,
                mdnsName: document.getElementById('mdnsName').value,
                ipMode: document.getElementById('ipMode').value,
                clientIP: document.getElementById('clientIP').value,
                clientGateway: document.getElementById('clientGateway').value,
                clientSubnet: document.getElementById('clientSubnet').value
            },
            user: {
                username: document.getElementById('username').value,
                password: password,
                authEnable: document.getElementById('authEnable').checked
            },
            sensor: {
                sampleRate: parseInt(document.getElementById('sampleRate').value) || 1000,
                avgSamples: parseInt(document.getElementById('avgSamples').value) || 10,
                minVoltage: parseFloat(document.getElementById('minVoltage').value) || 0,
                maxVoltage: parseFloat(document.getElementById('maxVoltage').value) || 300,
                minCurrent: parseFloat(document.getElementById('minCurrent').value) || 0,
                maxCurrent: parseFloat(document.getElementById('maxCurrent').value) || 100,
                minPower: parseFloat(document.getElementById('minPower').value) || 0,
                maxPower: parseFloat(document.getElementById('maxPower').value) || 23000,
                alertEnable: document.getElementById('alertEnable').checked
            },
            time: {
                gmtOffset: parseInt(document.getElementById('gmtOffset').value) || 210,
                gmtPositive: document.getElementById('gmtSign').value === '+',
                ntpServer: document.getElementById('ntpServer').value
            }
        };
        console.log(cfg); // برای دیباگ
        fetch('/api/config', {
            method: 'POST',
            headers: {'Content-Type': 'application/x-www-form-urlencoded'},
            body: 'body=' + encodeURIComponent(JSON.stringify(cfg))
        })
        .then(res => res.json())
        .then(data => {
            alert('تنظیمات با موفقیت ذخیره شد!');
        })
        .catch(err => {
            alert('خطا در ذخیره تنظیمات!');
        });
    }
}); 