#include "settings.h"
///////////////////////////
WiFiUDP ntpUDP;

/**
 * RGB LED Control Functions
 * -------------------------
 * The RGB LED is used to indicate the device's status:
 * - WHITE: Boot/initialization
 * - RED: Error state
 * - YELLOW: AP active without connections or connecting to WiFi
 * - BLUE: Devices connected to AP
 * - GREEN: Successfully connected to WiFi
 * - PURPLE: Firmware update or file upload in progress
 */

// توابع مدیریت LED RGB
void setupLED() {
  // راه‌اندازی LED RGB
  rgbLed.begin();
  rgbLed.clear(); // خاموش کردن همه پیکسل‌ها
  rgbLed.setBrightness(50); // تنظیم روشنایی LED (0-255)
  setLEDColor(LED_COLOR_WHITE); // LED سفید به معنای شروع بوت
  rgbLed.show();
  delay(500);
}

// تنظیم رنگ LED
void setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

// نمایش وضعیت شبکه با LED
void updateNetworkLEDStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    // اتصال موفق به WiFi
    setLEDColor(LED_COLOR_GREEN);
  } else if (deviceConfig.network.apEnable && WiFi.softAPgetStationNum() > 0) {
    // حداقل یک دستگاه به AP متصل شده
    setLEDColor(LED_COLOR_BLUE);
  } else if (deviceConfig.network.apEnable) {
    // AP فعال است اما هیچ دستگاهی متصل نیست
    setLEDColor(LED_COLOR_YELLOW);
  } else {
    // هیچ اتصالی برقرار نیست
    setLEDColor(LED_COLOR_RED);
  }
}

// Main setup function: initializes all modules and services
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("Starting Power Meter...");

  // راه‌اندازی LED RGB
  setupLED();
  Serial.println("RGB LED initialized");

  // Initialize LittleFS file system
  if (!LittleFS.begin(true)) {
    Serial.println("Error mounting LittleFS");
    setLEDColor(LED_COLOR_RED); // نشان دادن خطا با LED قرمز
    return;
  }
  Serial.println("LittleFS mounted OK");

  // Enable Watchdog Timer
  EnableWDT();
  Serial.println("WDT enabled");

  // Load configuration from file or use defaults
  if (!loadConfig(deviceConfig)) {
    Serial.println("Config load failed, using defaults.");
    saveConfig(deviceConfig);
  }
  Serial.println("Config loaded");

  // Initialize network (WiFi, mDNS, NTP)
  if (!setupNetwork(deviceConfig)) {
    Serial.println("Network setup failed! Skipping sensor and web server setup.");
    setLEDColor(LED_COLOR_RED); // نشان دادن خطا با LED قرمز
    return;
  }
  Serial.println("Network setup done");
  
  // بروزرسانی وضعیت LED بر اساس وضعیت شبکه
  updateNetworkLEDStatus();

  // Initialize PZEM sensor
  setupSensor();
  Serial.println("Sensor setup done");

  // Initialize web server and API endpoints
  setupWebServer();
  Serial.println("Web server setup done");

  Serial.println("Setup completed!");
}

// Main loop: handles sampling, WebSocket updates, and NTP sync
void loop() {
  // Reset Watchdog Timer
  esp_task_wdt_reset();

  // Read new sample every sampleRate ms and broadcast to clients
  if (millis() - lastSampleTime >= deviceConfig.sensor.sampleRate) {
    Sample sample = getCurrentValues();
    broadcastData(sample);
    lastSampleTime = millis();
    
    // Force an additional read when current drops to near-zero
    static bool wasLoadConnected = false;
    bool isLoadConnected = (sample.current > 0.1);
    
    // If we detect a change in load state, do extra readings
    if (wasLoadConnected != isLoadConnected) {
      Serial.println(isLoadConnected ? "Load connected" : "Load disconnected");
      // Wait a moment and take another reading
      delay(100);
      Sample confirmSample = getCurrentValues();
      broadcastData(confirmSample);
      wasLoadConnected = isLoadConnected;
    }
  }

  // Clean up WebSocket clients every 2 seconds (reduced from 5)
  if (millis() - lastWebSocketUpdate >= 2000) {
    updateWebSocket();
    lastWebSocketUpdate = millis();
  }

  // Update NTP time every hour
  static unsigned long lastNTPUpdate = 0;
  if (millis() - lastNTPUpdate >= 3600000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Performing hourly NTP sync...");
      syncTimeWithNTP();
      Serial.print("Current system time: ");
      time_t now = getCurrentTime();
      Serial.println(now);
    } else {
      Serial.println("Skipping NTP sync - WiFi not connected");
    }
    lastNTPUpdate = millis();
  }
  
  // بررسی وضعیت شبکه هر 10 ثانیه و بروزرسانی LED
  static unsigned long lastLEDUpdate = 0;
  if (millis() - lastLEDUpdate >= 10000) {
    updateNetworkLEDStatus();
    lastLEDUpdate = millis();
  }
}

// Enable and configure the Watchdog Timer
void EnableWDT() {
  esp_task_wdt_config_t config = {
    .timeout_ms = 120 * 1000,  // Increased to 2 minutes
    .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&config);
  enableLoopWDT();

  esp_err_t status = esp_task_wdt_status(NULL);
  if (status == ESP_OK) {
    Serial.println("✅ Loop Task is Registered in WDT");
  } else {
    Serial.println("❌ Loop Task is NOT Registered in WDT");
  }
}
