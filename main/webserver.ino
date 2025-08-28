#include <FS.h>
#include <LittleFS.h>

/**
 * RGB LED Usage in Web Server
 * ---------------------------
 * During firmware updates and file uploads, the LED is temporarily set to PURPLE.
 * The previous LED color is stored in 'previousLEDColor' and restored when the
 * operation completes or fails.
 * 
 * - Normal operations: LED shows network status (GREEN, BLUE, YELLOW, or RED)
 * - File upload: LED turns PURPLE during upload, then returns to previous state
 * - Firmware update: LED turns PURPLE during update, then restarts device
 */

// متغیر جهانی برای ذخیره وضعیت LED قبل از بروزرسانی
uint32_t previousLEDColor = 0;

// متغیر استاتیک برای آپلود فایل
static File uploadFile;

// Broadcasts the latest sample data to all connected WebSocket clients
void broadcastData(const Sample &sample) {
    if (ws.count() == 0) return;
    
    // مقداردهی JSON با مقادیر اندازه‌گیری شده
    StaticJsonDocument<512> doc;
    doc["voltage"] = sample.voltage;
    doc["current"] = sample.current;
    doc["power"] = sample.power;
    doc["energy"] = sample.energy;
    doc["frequency"] = sample.frequency;
    doc["pf"] = sample.pf;
    doc["apparentPower"] = sample.apparentPower;
    doc["reactivePower"] = sample.reactivePower;
    
    // ارسال timestamp دقیق از ESP32 (epoch time)
    doc["timestamp"] = sample.timestamp;
    
    // ارسال زمان فرمت شده برای نمایش
    doc["formattedTime"] = getFormattedTime();
    
    // ارسال وضعیت منبع زمان (NTP یا Internal RTC)
    doc["timeSource"] = isNTPActive() ? "NTP" : "Internal RTC";
    
    // حذف فیلد zeroLoad - همیشه مقادیر واقعی را نمایش دهیم
    doc["zeroLoad"] = false; // همیشه false
    
    // تبدیل JSON به رشته و ارسال به تمام کلاینت‌ها
    String json;
    serializeJson(doc, json);
    
    // لاگ برای دیباگ
    if (deviceConfig.advanced.serialLoggingEnabled) {
        Serial.print("WS Broadcasting: ");
        Serial.println(json);
    }
    
    ws.textAll(json);
}

// ارسال لاگ سریال به کلاینت‌های متصل به WebSocket
void broadcastSerialLog(const String &logMessage) {
    if (ws.count() == 0 || !deviceConfig.advanced.serialLoggingEnabled) return;
    
    StaticJsonDocument<512> doc;
    doc["serialLog"] = logMessage;
    String json;
    serializeJson(doc, json);
    ws.textAll(json);
}

// Handles WebSocket events: connect, disconnect, data, etc.
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            {  // Added curly braces to create a new scope
                Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
                // Reset the sample buffer on new connection to get fresh readings
                resetSampleBuffer();
                // Send current values immediately to the new client
                Sample currentSample = getCurrentValues();
                char buffer[512];
                StaticJsonDocument<512> doc;
                doc["voltage"] = currentSample.voltage;
                doc["current"] = currentSample.current;
                doc["power"] = currentSample.power;
                doc["energy"] = currentSample.energy;
                doc["frequency"] = currentSample.frequency;
                doc["pf"] = currentSample.pf;
                doc["apparentPower"] = currentSample.apparentPower;
                doc["reactivePower"] = currentSample.reactivePower;
                
                // ارسال timestamp دقیق از ESP32
                doc["timestamp"] = currentSample.timestamp;
                
                // ارسال زمان فرمت شده برای نمایش
                doc["formattedTime"] = getFormattedTime();
                
                // ارسال وضعیت منبع زمان
                doc["timeSource"] = isNTPActive() ? "NTP" : "Internal RTC";
                
                doc["zeroLoad"] = false;
                serializeJson(doc, buffer);
                client->text(buffer);
            }  // Close the curly brace
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            // Handle incoming WebSocket data if needed
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

// تابع حذف بازگشتی فولدر (بیرون از setupWebServer)
bool deleteRecursive(const String& path) {
    File dir = LittleFS.open(path);
    if (!dir.isDirectory()) return false;
    File file = dir.openNextFile();
    while (file) {
        String fpath = String(file.name());
        if (file.isDirectory()) {
            if (!deleteRecursive(fpath)) return false;
        } else {
            if (!LittleFS.remove(fpath)) return false;
        }
        file = dir.openNextFile();
    }
    dir.close();
    return LittleFS.rmdir(path);
}

// تابع resetPZEMEnergy در sensor.ino تعریف شده است

// Sets up the web server, API endpoints, and static file serving
void setupWebServer() {
    // Attach WebSocket event handler
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    // API endpoint: Get configuration as JSON
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = configToJson(deviceConfig);
        request->send(200, "application/json", json);
    });
    // API endpoint: Save configuration from JSON
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("body", true)) {
            request->send(400, "text/plain", "No body");
            return;
        }
        String body = request->getParam("body", true)->value();
        if (jsonToConfig(body, deviceConfig)) {
            saveConfig(deviceConfig);
            request->send(200, "application/json", "{\"ok\":true}");
            
            // بروزرسانی وضعیت LED بر اساس وضعیت شبکه جدید
            updateNetworkLEDStatus();
        } else {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
        }
    });
    // API endpoint: Get current sensor values as JSON
    server.on("/api/current", HTTP_GET, [](AsyncWebServerRequest *request) {
        Sample sample = getCurrentValues();
        StaticJsonDocument<256> doc;
        doc["voltage"] = sample.voltage;
        doc["current"] = sample.current;
        doc["power"] = sample.power;
        doc["energy"] = sample.energy;
        doc["frequency"] = sample.frequency;
        doc["pf"] = sample.pf;
        doc["apparentPower"] = sample.apparentPower;
        doc["reactivePower"] = sample.reactivePower;
        doc["timestamp"] = sample.timestamp;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });
    // API برای ریست کردن شمارنده انرژی
    server.on("/api/reset-energy", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool success = resetPZEMEnergy();
        String response = "{\"success\":" + String(success ? "true" : "false");
        if (!success) {
            response += ",\"message\":\"خطا در ارتباط با ماژول PZEM\"";
        }
        response += "}";
        request->send(200, "application/json", response);
    });
    // API برای ریست کردن بافر نمونه‌ها و خواندن مقادیر جدید
    server.on("/api/reset-readings", HTTP_POST, [](AsyncWebServerRequest *request) {
        resetSampleBuffer();
        // خواندن مستقیم از PZEM بدون استفاده از میانگین‌گیری
        Sample sample = readPZEM();
        StaticJsonDocument<256> doc;
        doc["voltage"] = sample.voltage;
        doc["current"] = sample.current;
        doc["power"] = sample.power;
        doc["energy"] = sample.energy;
        doc["frequency"] = sample.frequency;
        doc["pf"] = sample.pf;
        doc["apparentPower"] = sample.apparentPower;
        doc["reactivePower"] = sample.reactivePower;
        doc["success"] = true;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });
    // API برای فعال/غیرفعال کردن لاگ سریال
    server.on("/api/toggle-serial-log", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool currentState = deviceConfig.advanced.serialLoggingEnabled;
        
        // تغییر وضعیت لاگ سریال
        deviceConfig.advanced.serialLoggingEnabled = !currentState;
        
        // اگر لاگ فعال شد، یک پیام تأیید در سریال چاپ می‌کنیم
        if (deviceConfig.advanced.serialLoggingEnabled) {
            Serial.println("==== SERIAL LOGGING ENABLED ====");
            Serial.print("Log Interval: ");
            Serial.print(deviceConfig.advanced.serialLogInterval);
            Serial.println(" ms");
        } else {
            Serial.println("==== SERIAL LOGGING DISABLED ====");
        }
        
        StaticJsonDocument<64> doc;
        doc["enabled"] = deviceConfig.advanced.serialLoggingEnabled;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // API برای تنظیم فاصله زمانی لاگ سریال
    server.on("/api/set-log-interval", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("interval", true)) {
            request->send(400, "application/json", "{\"error\":\"Missing interval parameter\"}");
            return;
        }
        
        int interval = request->getParam("interval", true)->value().toInt();
        if (interval < 100 || interval > 10000) {
            request->send(400, "application/json", "{\"error\":\"Interval must be between 100 and 10000 ms\"}");
            return;
        }
        
        deviceConfig.advanced.serialLogInterval = interval;
        
        if (deviceConfig.advanced.serialLoggingEnabled) {
            Serial.print("==== LOG INTERVAL UPDATED: ");
            Serial.print(interval);
            Serial.println(" ms ====");
        }
        
        request->send(200, "application/json", "{\"success\":true}");
    });

    // API برای تنظیم روش به‌روزرسانی مقادیر
    server.on("/api/set-update-method", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("method", true)) {
            request->send(400, "application/json", "{\"error\":\"Missing method parameter\"}");
            return;
        }
        
        int method = request->getParam("method", true)->value().toInt();
        if (method < 0 || method > 2) { // DIRECT=0, AVERAGE=1, KEEP_MAX=2
            request->send(400, "application/json", "{\"error\":\"Method must be between 0 and 2\"}");
            return;
        }
        
        // اطمینان از صحت مقدار روش به‌روزرسانی
        Serial.print("Changing update method from ");
        Serial.print(deviceConfig.advanced.updateMethod);
        Serial.print(" to ");
        Serial.println(method);
        
        deviceConfig.advanced.updateMethod = method;
        
        StaticJsonDocument<64> doc;
        doc["success"] = true;
        doc["method"] = method;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // ===== HISTORY API ENDPOINTS =====
    
    // API برای دریافت داده‌های تاریخی بهینه شده
    server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest *request) {
        String date = request->getParam("date")->value();
        String maxLinesStr = request->getParam("maxLines")->value();
        int maxLines = maxLinesStr.length() > 0 ? maxLinesStr.toInt() : 1000;
        
        Serial.print("🔍 /api/history called for date: ");
        Serial.println(date);
        
        std::vector<Sample> samples = readOptimizedData(date);
        
        if (samples.empty()) {
            request->send(200, "application/json", "[]");
            return;
        }
        
        // محدود کردن تعداد نمونه‌ها
        if (samples.size() > maxLines) {
            samples.erase(samples.begin(), samples.end() - maxLines);
        }
        
        String json = "[";
        bool first = true;
        
        for (const auto& sample : samples) {
            if (!first) json += ",";
            json += "{";
            json += "\"timestamp\":" + String(sample.timestamp) + ",";
            json += "\"voltage\":" + String(sample.voltage, 2) + ",";
            json += "\"current\":" + String(sample.current, 3) + ",";
            json += "\"power\":" + String(sample.power, 2) + ",";
            json += "\"energy\":" + String(sample.energy, 3) + ",";
            json += "\"frequency\":" + String(sample.frequency, 1) + ",";
            json += "\"pf\":" + String(sample.pf, 3) + ",";
            json += "\"apparentPower\":" + String(sample.apparentPower, 2) + ",";
            json += "\"reactivePower\":" + String(sample.reactivePower, 2);
            json += "}";
            first = false;
        }
        
        json += "]";
        
        Serial.print("📊 Sending ");
        Serial.print(samples.size());
        Serial.println(" samples");
        
        request->send(200, "application/json", json);
    });
    
    // API برای دریافت آمار بهینه شده
    server.on("/api/history/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
        String date = request->getParam("date")->value();
        
        Serial.print("🔍 /api/history/stats called for date: ");
        Serial.println(date);
        
        String stats = getOptimizedStats(date);
        request->send(200, "application/json", stats);
    });
    
         // API برای دریافت لیست فایل‌های تاریخی موجود
    server.on("/api/history/files", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("🔍 /api/history/files called");
        
        String result = "[";
        bool firstFile = true;
        int fileCount = 0;
        
        // باز کردن فولدر data
        File dir = LittleFS.open("/data");
        if (!dir || !dir.isDirectory()) {
            Serial.println("❌ Cannot open /data directory");
            request->send(200, "application/json", "[]");
            return;
        }
        
        Serial.println("✅ /data directory opened");
        
        // لیست کردن تمام فایل‌ها
        File file = dir.openNextFile();
        while (file) {
            String fileName = String(file.name());
            Serial.print("📁 Found: ");
            Serial.println(fileName);
            
            // فقط فایل‌های CSV
            if (!file.isDirectory() && fileName.endsWith(".csv")) {
                // استخراج تاریخ از نام فایل
                String date = fileName;
                if (date.startsWith("/data/")) {
                    date = date.substring(6); // حذف /data/
                }
                if (date.endsWith(".csv")) {
                    date = date.substring(0, date.length() - 4); // حذف .csv
                }
                
                // اضافه کردن به JSON
                if (!firstFile) result += ",";
                result += "{\"date\":\"" + date + "\",\"filename\":\"" + fileName + "\",\"size\":" + String(file.size()) + "}";
                
                Serial.print("📊 Added: ");
                Serial.println(date);
                
                firstFile = false;
                fileCount++;
            }
            file = dir.openNextFile();
        }
        
        result += "]";
        dir.close();
        
        Serial.print("📊 Total CSV files: ");
        Serial.println(fileCount);
        Serial.print("📋 JSON: ");
        Serial.println(result);
        
        request->send(200, "application/json", result);
    });

    // API برای تنظیم روش بروزرسانی
    server.on("/api/set-update-method", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("method", true)) {
            request->send(400, "application/json", "{\"error\":\"Missing method parameter\"}");
            return;
        }
        
        int method = request->getParam("method", true)->value().toInt();
        if (method != 0 && method != 1) {
            request->send(400, "application/json", "{\"error\":\"Method must be 0 (AVERAGE) or 1 (DIRECT)\"}");
            return;
        }
        
        deviceConfig.advanced.updateMethod = method;
        
        // ریست بافر نمونه‌ها برای اعمال تغییرات سریع‌تر
        resetSampleBuffer();
        
        // ذخیره تنظیمات به صورت کامل
        bool saveResult = saveConfig(deviceConfig);
        
        if (saveResult) {
            Serial.println("Update method changed and config saved successfully");
            request->send(200, "application/json", "{\"success\":true,\"method\":" + String(method) + "}");
        } else {
            Serial.println("Failed to save config after changing update method");
            request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to save configuration\"}");
        }
    });

    // API برای تنظیم آستانه درصد کاهش
    server.on("/api/set-threshold", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("percent", true)) {
            request->send(400, "application/json", "{\"error\":\"Missing percent parameter\"}");
            return;
        }
        
        float percent = request->getParam("percent", true)->value().toFloat();
        if (percent < 1.0 || percent > 50.0) {
            request->send(400, "application/json", "{\"error\":\"Threshold must be between 1% and 50%\"}");
            return;
        }
        
        deviceConfig.advanced.thresholdPercent = percent;
        
        Serial.print("Threshold percent changed to: ");
        Serial.println(percent);
        
        saveConfig(deviceConfig);
        request->send(200, "application/json", "{\"success\":true}");
    });

    // API برای تنظیم افست‌های کالیبراسیون
    server.on("/api/set-calibration", HTTP_POST, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;
        
        if (request->hasParam("voltage", true)) {
            deviceConfig.advanced.voltageOffset = request->getParam("voltage", true)->value().toFloat();
            doc["voltage"] = deviceConfig.advanced.voltageOffset;
        }
        
        if (request->hasParam("current", true)) {
            deviceConfig.advanced.currentOffset = request->getParam("current", true)->value().toFloat();
            doc["current"] = deviceConfig.advanced.currentOffset;
        }
        
        if (request->hasParam("power", true)) {
            deviceConfig.advanced.powerOffset = request->getParam("power", true)->value().toFloat();
            doc["power"] = deviceConfig.advanced.powerOffset;
        }
        
        Serial.println("Calibration offsets updated");
        Serial.print("Voltage offset: "); Serial.println(deviceConfig.advanced.voltageOffset);
        Serial.print("Current offset: "); Serial.println(deviceConfig.advanced.currentOffset);
        Serial.print("Power offset: "); Serial.println(deviceConfig.advanced.powerOffset);
        
        saveConfig(deviceConfig);
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // API برای دریافت تنظیمات پیشرفته
    server.on("/api/advanced-settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;
        doc["serialLoggingEnabled"] = deviceConfig.advanced.serialLoggingEnabled;
        doc["serialLogInterval"] = deviceConfig.advanced.serialLogInterval;
        doc["updateMethod"] = deviceConfig.advanced.updateMethod;
        doc["thresholdPercent"] = deviceConfig.advanced.thresholdPercent;
        doc["voltageOffset"] = deviceConfig.advanced.voltageOffset;
        doc["currentOffset"] = deviceConfig.advanced.currentOffset;
        doc["powerOffset"] = deviceConfig.advanced.powerOffset;
        doc["historyLoggingEnabled"] = deviceConfig.advanced.historyLoggingEnabled;
        doc["historyLoggingActive"] = deviceConfig.advanced.historyLoggingActive;
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // API برای فعال/غیرفعال کردن قابلیت لاگ کردن تاریخی
    server.on("/api/toggle-history-logging", HTTP_POST, [](AsyncWebServerRequest *request) {
        deviceConfig.advanced.historyLoggingEnabled = !deviceConfig.advanced.historyLoggingEnabled;
        
        // اگر قابلیت غیرفعال شد، لاگ کردن فعال را نیز متوقف کن
        if (!deviceConfig.advanced.historyLoggingEnabled) {
            deviceConfig.advanced.historyLoggingActive = false;
        }
        
        saveConfig(deviceConfig);
        
        StaticJsonDocument<128> doc;
        doc["enabled"] = deviceConfig.advanced.historyLoggingEnabled;
        doc["active"] = deviceConfig.advanced.historyLoggingActive;
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // API برای شروع/توقف لاگ کردن فعال
    server.on("/api/start-stop-logging", HTTP_POST, [](AsyncWebServerRequest *request) {
        // فقط اگر قابلیت لاگ کردن فعال باشد، اجازه تغییر وضعیت بده
        if (!deviceConfig.advanced.historyLoggingEnabled) {
            request->send(400, "application/json", "{\"error\":\"History logging is disabled. Enable it first.\"}");
            return;
        }
        
        deviceConfig.advanced.historyLoggingActive = !deviceConfig.advanced.historyLoggingActive;
        
        // ذخیره تنظیمات
        saveConfig(deviceConfig);
        
        // پیام مناسب برای لاگ
        if (deviceConfig.advanced.historyLoggingActive) {
            Serial.println("✅ Historical logging STARTED");
        } else {
            Serial.println("⏹️ Historical logging STOPPED");
        }
        
        StaticJsonDocument<128> doc;
        doc["active"] = deviceConfig.advanced.historyLoggingActive;
        doc["message"] = deviceConfig.advanced.historyLoggingActive ? "Logging started" : "Logging stopped";
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // API برای دریافت وضعیت لاگ کردن
    server.on("/api/logging-status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<128> doc;
        doc["historyLoggingEnabled"] = deviceConfig.advanced.historyLoggingEnabled;
        doc["historyLoggingActive"] = deviceConfig.advanced.historyLoggingActive;
        doc["status"] = deviceConfig.advanced.historyLoggingActive ? "ACTIVE" : 
                       (deviceConfig.advanced.historyLoggingEnabled ? "READY" : "DISABLED");
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // API endpoint برای دریافت وضعیت time sync
    server.on("/api/time-status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;
        
        if (isNTPActive()) {
            doc["status"] = "NTP_ACTIVE";
            doc["message"] = "Time synchronized with NTP";
            doc["time"] = getFormattedTime();
        } else if (ntpInitialized) {
            doc["status"] = "NTP_FAILED";
            doc["message"] = "NTP sync failed, using internal RTC";
            doc["time"] = getFormattedTime();
        } else {
            doc["status"] = "SYNCING";
            doc["message"] = "Time sync in progress...";
            doc["time"] = "Syncing...";
        }
        
        doc["timeSource"] = isNTPActive() ? "NTP" : "Internal RTC";
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // API endpoint برای دریافت زمان فعلی ESP32
    server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<256> doc;
        doc["timestamp"] = getCurrentTimestamp();
        doc["formattedTime"] = getFormattedTime();
        doc["timeSource"] = isNTPActive() ? "NTP" : "Internal RTC";
        doc["timezone"] = "Tehran (UTC+3:30)";
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // API endpoint برای سینک دستی NTP
    server.on("/api/sync-time", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool success = forceNTPSync();
        StaticJsonDocument<128> doc;
        doc["success"] = success;
        doc["message"] = success ? "Time sync successful" : "Time sync failed";
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // Serve static files from LittleFS (default: index.html)
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    // --- File Manager APIs ---
    // لیست فایل‌ها و فولدرها
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
        String path = "/";
        if (request->hasParam("dir")) path = request->getParam("dir")->value();
        File root = LittleFS.open(path);
        if (!root || !root.isDirectory()) {
            request->send(404, "application/json", "{\"error\":\"Not found\"}");
            return;
        }
        String output = "[";
        File file = root.openNextFile();
        while (file) {
            if (output != "[") output += ',';
            String fname = String(file.name());
            if (fname.startsWith("/")) fname = fname.substring(1);
            output += "{\"name\":\"" + fname + "\",";
            output += "\"type\":\"" + String(file.isDirectory() ? "dir" : "file") + "\",";
            output += "\"size\":" + String(file.size());
            output += "}";
            file = root.openNextFile();
        }
        output += "]";
        request->send(200, "application/json", "{\"files\":" + output + "}");
    });

    // دانلود فایل
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("file")) {
            request->send(400, "text/plain", "No file param");
            return;
        }
        String path = request->getParam("file")->value();
        if (!LittleFS.exists(path)) {
            request->send(404, "text/plain", "File not found");
            return;
        }
        request->send(LittleFS, path, String(), true);
    });

    // حذف فایل/پوشه (حذف بازگشتی فولدر)
    server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("file")) {
            request->send(400, "application/json", "{\"error\":\"No file param\"}");
            return;
        }
        String path = request->getParam("file")->value();
        bool recursive = request->hasParam("recursive");
        if (!LittleFS.exists(path)) {
            request->send(404, "application/json", "{\"error\":\"File not found\"}");
            return;
        }
        File f = LittleFS.open(path);
        bool ok = false;
        if (f.isDirectory() && recursive) {
            ok = deleteRecursive(path);
        } else {
            ok = LittleFS.remove(path);
        }
        request->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}" );
    });

    // ایجاد پوشه جدید
    server.on("/mkdir", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("dir")) {
            request->send(400, "application/json", "{\"error\":\"No dir param\"}");
            return;
        }
        String path = request->getParam("dir")->value();
        bool ok = LittleFS.mkdir(path);
        request->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}" );
    });

    // مدیریت بروزرسانی فریم‌ور
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
        // وقتی بروزرسانی تمام شد، وضعیت LED را به حالت قبلی برگردان
        rgbLed.setPixelColor(0, previousLEDColor);
        rgbLed.show();
        
        boolean shouldReboot = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
        response->addHeader("Connection", "close");
        request->send(response);
        
        if (shouldReboot) {
            delay(500);
            ESP.restart();
        }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (index == 0) {
            // ذخیره رنگ فعلی LED قبل از بروزرسانی
            previousLEDColor = rgbLed.getPixelColor(0);
            
            // تنظیم LED به رنگ بنفش (بروزرسانی)
            setLEDColor(LED_COLOR_PURPLE);
            
            Serial.printf("Update start: %s\n", filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        }
        
        if (Update.write(data, len) != len) {
            Update.printError(Serial);
        }
        
        if (final) {
            if (Update.end(true)) {
                Serial.printf("Update success: %u bytes\n", index + len);
            } else {
                Update.printError(Serial);
                
                // برگرداندن LED به حالت قبلی در صورت خطا
                rgbLed.setPixelColor(0, previousLEDColor);
                rgbLed.show();
            }
        }
    });

    // آپلود فایل
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", "{\"ok\":true}");
        
        // برگرداندن LED به حالت قبلی بعد از آپلود
        rgbLed.setPixelColor(0, previousLEDColor);
        rgbLed.show();
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if (index == 0) {
            // ذخیره رنگ فعلی LED قبل از آپلود
            previousLEDColor = rgbLed.getPixelColor(0);
            
            // تنظیم LED به رنگ بنفش (آپلود)
            setLEDColor(LED_COLOR_PURPLE);
            
            String path = "/" + filename;
            if (request->hasParam("path", true)) {
                String dir = request->getParam("path", true)->value();
                if (!dir.endsWith("/")) dir += "/";
                path = dir + filename;
            }
            if (LittleFS.exists(path)) LittleFS.remove(path);
            uploadFile = LittleFS.open(path, "w");
        }
        
        if (uploadFile) uploadFile.write(data, len);
        
        if (final && uploadFile) {
            uploadFile.close();
            Serial.printf("Upload complete: %s, size: %u bytes\n", filename.c_str(), index + len);
        }
    });
    // Start the web server
    server.begin();
}

// Cleans up disconnected WebSocket clients
void updateWebSocket() {
    ws.cleanupClients();
} 