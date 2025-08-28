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

// Sensor resolution (2x accuracy for energy meter)
#define VOLTAGE_RESOLUTION 0.05    // Voltage (2x better than 0.1V)
#define CURRENT_RESOLUTION 0.0005  // Current (2x better than 0.001A)
#define POWER_RESOLUTION 0.05      // Power (2x better than 0.1W)
#define PF_RESOLUTION 0.005        // Power factor (2x better than 0.01)
#define FREQ_RESOLUTION 0.05       // Frequency (2x better than 0.1Hz)
#define ENERGY_RESOLUTION 0.5      // Energy (2x better than 1Wh)

// Minimum values for measurement
#define MIN_CURRENT_FOR_CHANGE 0.2  // Minimum current to start measurement (per datasheet: 0.2A)
#define MIN_POWER_FOR_CHANGE 0.4     // Minimum power to start measurement (per datasheet: 0.4W)


// Change thresholds for simple storage (2x sensor resolution)
#define VOLTAGE_CHANGE_THRESHOLD 0.1    // 2x 0.05V resolution
#define CURRENT_CHANGE_THRESHOLD 0.001  // 2x 0.0005A resolution  
#define POWER_CHANGE_THRESHOLD 0.1      // 2x 0.05W resolution
#define PF_CHANGE_THRESHOLD 0.01        // 2x 0.005 resolution

// Storage settings
#define MAX_STORAGE_DAYS 30             // Keep data for 30 days

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
    
    // تنظیمات لاگ کردن تاریخی
    bool historyLoggingEnabled = false; // فعال/غیرفعال کردن لاگ تاریخی (کنترل دستی)
    bool historyLoggingActive = false;  // وضعیت فعلی لاگ کردن (روشن/خاموش)
    
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

// Compression structure for RLE encoding (commented out - not used)
/*
struct CompressedData {
    uint8_t type;           // Data type (0=voltage, 1=current, 2=power, 3=pf)
    uint8_t count;          // Repeat count
    uint16_t value;         // Compressed value
    
    CompressedData() : type(0), count(0), value(0) {}
};
*/

// Smart buffer for temporary storage (commented out - replaced with simple comparison)
/*
struct SmartBuffer {
    Sample samples[SMART_BUFFER_SIZE];
    int index;
    int count;
    time_t lastStorageTime;
    Sample lastStoredSample;
    
    SmartBuffer() : index(0), count(0), lastStorageTime(0) {
        memset(samples, 0, sizeof(samples));
    }
    
    void addSample(const Sample& sample) {
        samples[index] = sample;
        index = (index + 1) % SMART_BUFFER_SIZE;
        if (count < SMART_BUFFER_SIZE) count++;
    }
    
    Sample getAverage() const {
        if (count == 0) return Sample();
        
        Sample avg;
        for (int i = 0; i < count; i++) {
            avg.voltage += samples[i].voltage;
            avg.current += samples[i].current;
            avg.power += samples[i].power;
            avg.pf += samples[i].pf;
        }
        
        avg.voltage /= count;
        avg.current /= count;
        avg.power /= count;
        avg.pf /= count;
        avg.timestamp = samples[(index - 1 + SMART_BUFFER_SIZE) % SMART_BUFFER_SIZE].timestamp;
        
        return avg;
    }
    
    bool hasSignificantChange(const Sample& newSample) const {
        if (count == 0) {
            Serial.println("DEBUG: hasSignificantChange - buffer empty, returning true");
            return true;
        }
        
        Sample avg = getAverage();
        
        float voltageDiff = abs(newSample.voltage - avg.voltage);
        float currentDiff = abs(newSample.current - avg.current);
        float powerDiff = abs(newSample.power - avg.power);
        float pfDiff = abs(newSample.pf - avg.pf);
        
        Serial.printf("DEBUG: hasSignificantChange - v_diff: %.3f (thresh: %.3f), i_diff: %.3f (thresh: %.3f), p_diff: %.3f (thresh: %.3f), pf_diff: %.3f (thresh: %.3f)\n",
                     voltageDiff, VOLTAGE_CHANGE_THRESHOLD,
                     currentDiff, CURRENT_CHANGE_THRESHOLD,
                     powerDiff, POWER_CHANGE_THRESHOLD,
                     pfDiff, PF_CHANGE_THRESHOLD);
        
        bool hasChange = (voltageDiff > VOLTAGE_CHANGE_THRESHOLD ||
                         currentDiff > CURRENT_CHANGE_THRESHOLD ||
                         powerDiff > POWER_CHANGE_THRESHOLD ||
                         pfDiff > PF_CHANGE_THRESHOLD);
        
        if (hasChange) {
            Serial.println("DEBUG: hasSignificantChange - CHANGE DETECTED!");
        } else {
            Serial.println("DEBUG: hasSignificantChange - No significant change");
        }
        
        return hasChange;
    }
};
*/

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