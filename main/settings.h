#ifndef SETTINGS_H
#define SETTINGS_H

// Core Arduino and ESP32 includes
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <PZEM004Tv30.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <Adafruit_NeoPixel.h>

// PZEM-004T RX and TX pins for ESP32-S3
#define PZEM_RX_PIN 44
#define PZEM_TX_PIN 43

// RGB LED pin definition
#define RGB_LED_PIN 48
#define NUM_RGB_LEDS 1

// LED Status Colors
#define LED_COLOR_OFF    0, 0, 0      // خاموش
#define LED_COLOR_RED    255, 0, 0    // قرمز - خطا
#define LED_COLOR_GREEN  0, 255, 0    // سبز - اتصال موفق به WiFi
#define LED_COLOR_BLUE   0, 0, 255    // آبی - حالت AP فعال
#define LED_COLOR_YELLOW 255, 255, 0  // زرد - در حال اتصال
#define LED_COLOR_CYAN   0, 255, 255  // فیروزه‌ای - فعالیت سنسور
#define LED_COLOR_PURPLE 255, 0, 255  // بنفش - بروزرسانی
#define LED_COLOR_WHITE  255, 255, 255 // سفید - بوت

/*
 * RGB LED Status Description:
 * --------------------------
 * WHITE (255,255,255): Device is booting/initializing
 * RED (255,0,0): Error state (file system error, network error, or no connections)
 * YELLOW (255,255,0): AP mode active but no devices connected, or connecting to WiFi
 * BLUE (0,0,255): At least one device connected to AP mode, not connected to WiFi
 * GREEN (0,255,0): Successfully connected to WiFi
 * PURPLE (255,0,255): Firmware update or file upload in progress
 * 
 * The LED status is updated:
 * 1. Every 10 seconds in the main loop
 * 2. On network events (connect/disconnect)
 * 3. On file system operations (upload/update)
 */

// Sensor measurement limits and resolution
#define MIN_VOLTAGE 80.0      // Minimum voltage (V) - datasheet: 80V
#define MAX_VOLTAGE 260.0     // Maximum voltage (V) - datasheet: 260V
#define MIN_FREQUENCY 45.0    // Minimum frequency (Hz) - datasheet: 45Hz
#define MAX_FREQUENCY 65.0    // Maximum frequency (Hz) - datasheet: 65Hz
#define MAX_CURRENT 100.0     // Maximum current (A) - datasheet: 100A
#define MAX_POWER 23000.0     // Maximum power (W) - datasheet: 23kW

// Sensor resolution
#define VOLTAGE_RESOLUTION 0.1    // Voltage
#define CURRENT_RESOLUTION 0.001  // Current
#define POWER_RESOLUTION 0.1      // Power
#define PF_RESOLUTION 0.01        // Power factor
#define FREQ_RESOLUTION 0.1       // Frequency
#define ENERGY_RESOLUTION 1       // Energy (Wh)

// Minimum values for measurement
#define MIN_CURRENT_FOR_CHANGE 0.2  // Minimum current to start measurement (per datasheet: 0.2A)
#define MIN_POWER_FOR_CHANGE 0.4     // Minimum power to start measurement (per datasheet: 0.4W)

// Change limits for 300ms
#define MAX_RATE_CHANGE_VOLTAGE 1.0  // Maximum voltage change
#define MAX_RATE_CHANGE_CURRENT 0.1  // Maximum current change
#define WINDOW_SIZE 20               // 6-second window
#define INRUSH_THRESHOLD 2.0         // Inrush current detection factor

// Network configuration structure
struct NetworkConfig {
    bool apEnable = false;
    String apSSID = "";
    String apPassword = "";
    String apIP = "";
    bool clientEnable = false;
    String clientSSID = "";
    String clientPassword = "";
    String mdnsName = "";
    String ipMode = "dhcp";
    String clientIP = "";
    String clientGateway = "";
    String clientSubnet = "";
};

// User configuration structure
struct UserConfig {
    String username = "admin";
    String password = "admin";
    bool authEnable = false;
};

// Sensor configuration structure
struct SensorConfig {
    int sampleRate = 1000;
    int avgSamples = 10;
    float minVoltage = 0;
    float maxVoltage = 300;
    float minCurrent = 0;
    float maxCurrent = 100;
    float minPower = 0;
    float maxPower = 23000;
    bool alertEnable = false;
};

// Time configuration structure
struct TimeConfig {
    int gmtOffset = 210;
    bool gmtPositive = true;
    String ntpServer = "pool.ntp.org";
};

// OTA configuration structure (reserved for future use)
struct OTAConfig {
    // Empty for now, can be extended if needed
};

// Advanced configuration structure (reserved for future use)
struct AdvancedConfig {
    bool serialLoggingEnabled = false; // فعال/غیرفعال کردن لاگ سریال مقادیر سنسور
    int serialLogInterval = 1000;      // فاصله زمانی بین لاگ‌های سریال (میلی‌ثانیه)
    
    // روش به‌روزرسانی مقادیر
    enum UpdateMethod {
        DIRECT = 0,       // مستقیم: همیشه مقدار فعلی را نمایش می‌دهد
        AVERAGE = 1,      // میانگین‌گیری: میانگین مقادیر اخیر را نمایش می‌دهد
        KEEP_MAX = 2      // حفظ حداکثر: کاهش مقادیر فقط با آستانه مشخص
    };
    
    int updateMethod = DIRECT;         // روش به‌روزرسانی مقادیر (پیش‌فرض: مستقیم)
    float thresholdPercent = 20.0;     // آستانه درصد کاهش برای روش حفظ حداکثر
    
    // افست‌های کالیبراسیون
    float voltageOffset = 0.0;         // افست ولتاژ
    float currentOffset = 0.0;         // افست جریان
    float powerOffset = 0.0;           // افست توان
};

// Main device configuration structure
struct DeviceConfig {
    NetworkConfig network;
    UserConfig user;
    SensorConfig sensor;
    TimeConfig time;
    OTAConfig ota;
    AdvancedConfig advanced;
};

// Sample structure with timestamp for sensor data
struct Sample {
    float voltage;
    float current;
    float power;
    float energy;
    float frequency;
    float pf;
    float apparentPower; // توان ظاهری (VA)
    float reactivePower; // توان راکتیو (VAR)
    time_t timestamp;
    Sample()
        : voltage(0), current(0), power(0), energy(0), frequency(0), pf(0),
          apparentPower(0), reactivePower(0), timestamp(0) {}
};

// Global variables for the project
DeviceConfig deviceConfig;
unsigned long lastSampleTime = 0;
unsigned long lastWebSocketUpdate = 0;

// RGB LED instance
Adafruit_NeoPixel rgbLed = Adafruit_NeoPixel(NUM_RGB_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// Web server and WebSocket instances
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// UDP instance for NTP (if needed)
extern WiFiUDP ntpUDP;

#endif // SETTINGS_H 