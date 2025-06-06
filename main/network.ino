#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "settings.h"
#include <time.h>

/**
 * Network-related LED Status Indicators
 * -------------------------------------
 * The RGB LED reflects the current network status:
 * 
 * - GREEN: Successfully connected to WiFi
 * - BLUE: At least one device connected to AP mode (but not to WiFi)
 * - YELLOW: AP mode active without connections, or connecting to WiFi
 * - RED: No connections (neither WiFi nor AP with clients)
 * 
 * LED status is updated on network events through these handlers:
 * - onWiFiGotIP: When device gets an IP address (GREEN)
 * - onWiFiDisconnected: When WiFi disconnects (YELLOW or RED)
 * - onWiFiAPStationConnected: When a device connects to AP (BLUE)
 * - onWiFiAPStationDisconnected: When a device disconnects from AP
 */

// --- Event Handlers ---
void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("✅ [EVENT] WiFi Got IP!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
    
    // نمایش وضعیت اتصال با LED سبز
    setLEDColor(LED_COLOR_GREEN);
}

void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("❌ [EVENT] WiFi Disconnected.");
    Serial.print("Reason: ");
    Serial.println(info.wifi_sta_disconnected.reason);
    
    // بروزرسانی وضعیت LED بر اساس وضعیت AP
    if (deviceConfig.network.apEnable && WiFi.softAPgetStationNum() > 0) {
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

void onWiFiAPStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("✅ [EVENT] Station connected to AP");
    
    // اگر به شبکه WiFi متصل نیستیم، LED را آبی کنیم
    if (WiFi.status() != WL_CONNECTED) {
        setLEDColor(LED_COLOR_BLUE);
    }
}

void onWiFiAPStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.println("❌ [EVENT] Station disconnected from AP");
    
    // بروزرسانی وضعیت LED
    updateNetworkLEDStatus();
}

void wifi_event_handler_init() {
    WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.onEvent(onWiFiAPStationConnected, ARDUINO_EVENT_WIFI_AP_STACONNECTED);
    WiFi.onEvent(onWiFiAPStationDisconnected, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
}

// --- WiFi Setup (Dual Mode: AP + STA) ---
void setupNetworkDual(const NetworkConfig &net) {
    wifi_event_handler_init();
    // AP همیشه فعال
    String apSSID = net.apSSID.length() > 0 ? net.apSSID : "PowerMeter-Setup";
    String apPassword = net.apPassword.length() > 0 ? net.apPassword : "12345678";
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());
    Serial.print("AP started, SSID: ");
    Serial.print(apSSID);
    Serial.print(", IP: ");
    Serial.println(WiFi.softAPIP());
    
    // LED را زرد کنیم (در حال اتصال)
    setLEDColor(LED_COLOR_YELLOW);
    
    // اگر کلاینت فعال بود
    if (net.clientEnable) {
        if (net.ipMode == "static" && net.clientIP.length() > 0) {
            IPAddress ip, gateway, subnet;
            ip.fromString(net.clientIP);
            gateway.fromString(net.clientGateway);
            subnet.fromString(net.clientSubnet);
            WiFi.config(ip, gateway, subnet);
            Serial.println("Static IP set for STA");
        }
        WiFi.setAutoReconnect(true);
        WiFi.begin(net.clientSSID.c_str(), net.clientPassword.c_str());
        Serial.print("Connecting to WiFi: ");
        Serial.println(net.clientSSID);
    }
}

// --- mDNS & NTP ---
void syncTimeWithNTP() {
    configTime(deviceConfig.time.gmtOffset * 60, 0, deviceConfig.time.ntpServer.c_str());
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) {
        Serial.println("NTP sync OK");
    } else {
        Serial.println("NTP sync failed");
    }
}

time_t getCurrentTime() {
    return time(nullptr);
}

bool setupNetwork(const DeviceConfig &config) {
    // پرینت کامل تنظیمات شبکه
    Serial.println("\n===== Network Config =====");
    Serial.print("AP Enable: "); Serial.println(config.network.apEnable);
    Serial.print("AP SSID: "); Serial.println(config.network.apSSID);
    Serial.print("AP Password: "); Serial.println(config.network.apPassword);
    Serial.print("AP IP: "); Serial.println(config.network.apIP);
    Serial.print("Client Enable: "); Serial.println(config.network.clientEnable);
    Serial.print("Client SSID: "); Serial.println(config.network.clientSSID);
    Serial.print("Client Password: "); Serial.println(config.network.clientPassword);
    Serial.print("Client IP Mode: "); Serial.println(config.network.ipMode);
    Serial.print("Client IP: "); Serial.println(config.network.clientIP);
    Serial.print("Client Gateway: "); Serial.println(config.network.clientGateway);
    Serial.print("Client Subnet: "); Serial.println(config.network.clientSubnet);
    Serial.print("mDNS Name: "); Serial.println(config.network.mdnsName);
    Serial.print("NTP Server: "); Serial.println(config.time.ntpServer);
    Serial.print("GMT Offset: "); Serial.println(config.time.gmtOffset);
    Serial.println("========================\n");
    setupNetworkDual(config.network);
    // mDNS
    if (config.network.mdnsName.length() > 0) {
        if (MDNS.begin(config.network.mdnsName.c_str())) {
            Serial.println("mDNS started: " + config.network.mdnsName + ".local");
            MDNS.addService("http", "tcp", 80);
        } else {
            Serial.println("Failed to start mDNS!");
        }
    }
    // NTP و ست کردن RTC داخلی
    if (WiFi.status() == WL_CONNECTED) {
        syncTimeWithNTP();
        // اتصال موفق - LED سبز
        setLEDColor(LED_COLOR_GREEN);
    } else {
        Serial.println("NTP not started: WiFi not connected.");
        // بروزرسانی وضعیت LED براساس وضعیت شبکه
        updateNetworkLEDStatus();
    }
    return true;
}

// تابع مدیریت AP دیگر نیاز نیست چون AP همیشه فعال است
void manageAP() {}

// The old getNTPTime() function is removed as it's been replaced by getCurrentTime() 