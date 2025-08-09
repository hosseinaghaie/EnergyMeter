/**
 * Time Management Module for Energy Meter
 * ======================================
 * 
 * This module provides centralized time management for the ESP32-based energy meter.
 * It handles NTP synchronization, timezone conversion, and timestamp generation.
 * 
 * Key Features:
 * - Automatic NTP sync with Tehran timezone (UTC+3:30)
 * - Fallback to internal RTC when WiFi is unavailable
 * - Periodic sync every hour
 * - Timezone-aware timestamp generation
 * - Easy integration with sensor data
 * 
 * Functions:
 * - initTimeManager(): Initialize time system during boot
 * - getCurrentTimestamp(): Get current epoch timestamp
 * - getFormattedTime(): Get human-readable time string
 * - updateTimeManager(): Periodic updates (call in loop)
 * - syncNTP(): Manual NTP synchronization
 * - forceNTPSync(): Force manual sync from API
 * 
 * Usage:
 * 1. Call initTimeManager() in setup()
 * 2. Call updateTimeManager() in loop()
 * 3. Use getCurrentTimestamp() for sensor data
 * 4. Use getFormattedTime() for display
 * 
 * Timezone Configuration:
 * - Tehran: UTC+3:30 (210 minutes)
 * - Configured in settings.h: deviceConfig.time.gmtOffset = 210
 * - NTP server: pool.ntp.org (configurable)
 * 
 * @author Energy Meter Project
 * @version 1.0
 * @date 2024
 */

#include <Arduino.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "settings.h"

// Global variables for time management
NTPClient* ntpClient = nullptr;
bool ntpInitialized = false;
unsigned long lastNTPUpdate = 0;
const unsigned long NTP_UPDATE_INTERVAL = 3600000; // 1 hour in milliseconds

// Flag to prevent double sync
bool syncAttempted = false;

/**
 * Test internet connectivity
 * 
 * @return true if internet is accessible, false otherwise
 */
bool testInternetConnection() {
    HTTPClient http;
    http.begin("http://www.google.com");
    int httpCode = http.GET();
    http.end();
    
    if (httpCode == 200) {
        Serial.println("✅ Internet connection test successful");
        return true;
    } else {
        Serial.print("❌ Internet connection test failed, HTTP code: ");
        Serial.println(httpCode);
        return false;
    }
}

/**
 * Initialize time management system
 * This function is called during ESP32 boot process
 * 
 * Smart WiFi Connection:
 * - Waits up to 10 seconds for WiFi to connect
 * - Retries NTP sync up to 3 times if initial sync fails
 * - Falls back to internal RTC if all attempts fail
 * 
 * Timezone Configuration:
 * - Tehran is UTC+3:30 (210 minutes ahead of GMT)
 * - GMT offset is set in settings.h: deviceConfig.time.gmtOffset = 210
 * - NTP provides UTC time, ESP32 internal RTC stores UTC time
 * - Local time (Tehran) = UTC + 3:30 when displayed
 * 
 * @return true if NTP sync was successful, false if using internal RTC only
 */
bool initTimeManager() {
    Serial.println("🕐 TimeManager: Starting initialization...");
    
    // Set sync attempted flag to prevent double sync
    syncAttempted = true;
    
    // Wait for WiFi to connect (up to 10 seconds)
    Serial.println("⏳ TimeManager: Waiting for WiFi connection...");
    int wifiWaitTime = 0;
    while (WiFi.status() != WL_CONNECTED && wifiWaitTime < 10000) {
        delay(100);
        wifiWaitTime += 100;
        if (wifiWaitTime % 2000 == 0) {
            Serial.print("⏳ TimeManager: Still waiting for WiFi... (");
            Serial.print(wifiWaitTime / 1000);
            Serial.println("s)");
        }
    }
    
    // Check if WiFi is connected after waiting
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ TimeManager: WiFi connected, attempting NTP sync...");
        
        // Initialize NTP client with Tehran timezone settings
        // deviceConfig.time.gmtOffset = 210 (3:30 hours for Tehran)
        // deviceConfig.time.ntpServer = "pool.ntp.org"
        ntpClient = new NTPClient(ntpUDP, deviceConfig.time.ntpServer.c_str());
        ntpClient->begin();
        
        // Set timezone offset to 0 to get true GMT time from NTP
        // We'll apply Tehran offset later when needed
        ntpClient->setTimeOffset(0);
        
        // Attempt NTP sync with retry mechanism (up to 3 attempts)
        for (int attempt = 1; attempt <= 3; attempt++) {
            Serial.print("🔄 TimeManager: NTP sync attempt ");
            Serial.print(attempt);
            Serial.println("/3");
            
            if (syncNTP()) {
                ntpInitialized = true;
                Serial.println("✅ TimeManager: NTP initialized successfully");
                Serial.print("📅 Local time: ");
                Serial.println(getFormattedTime());
                return true;
            } else {
                Serial.print("❌ TimeManager: NTP sync attempt ");
                Serial.print(attempt);
                Serial.println(" failed");
                
                if (attempt < 3) {
                    Serial.println("⏳ TimeManager: Waiting 2 seconds before retry...");
                    delay(2000);
                }
            }
        }
        
        Serial.println("⚠️ TimeManager: All NTP sync attempts failed, using internal RTC only");
        // Set approximate time for internal RTC
        setApproximateTime();
        return false;
    } else {
        Serial.println("⚠️ TimeManager: WiFi not connected after 10 seconds, using internal RTC only");
        // Set approximate time for internal RTC
        setApproximateTime();
        Serial.print("📅 Local time: ");
        Serial.println(getFormattedTime());
        return false;
    }
}

/**
 * Set approximate time for internal RTC when NTP is not available
 * This provides a reasonable starting point for time tracking
 */
void setApproximateTime() {
    // Set approximate time (you can adjust this as needed)
    // This is just a fallback when NTP is not available
    struct timeval tv;
    tv.tv_sec = 1733692800; // Approximate time: 2025-01-08 00:00:00 UTC
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    Serial.println("📅 Set approximate time for internal RTC");
}

/**
 * Sync internal RTC with NTP server
 * This function updates ESP32's internal RTC with accurate time
 * 
 * Timezone Handling:
 * - NTP provides UTC time from server
 * - ESP32 internal RTC stores UTC time (not local time)
 * - Tehran timezone offset (+3:30) is applied when displaying time
 * - Internal RTC always stores UTC for consistency
 * 
 * @return true if sync was successful, false otherwise
 */
bool syncNTP() {
    if (!ntpClient) {
        Serial.println("❌ TimeManager: NTP client not initialized");
        return false;
    }
    
    Serial.println("🔄 TimeManager: Syncing with NTP server...");
    Serial.print("📡 NTP Server: ");
    Serial.println(deviceConfig.time.ntpServer);
    Serial.print("⏰ GMT Offset: ");
    Serial.println(deviceConfig.time.gmtOffset);
    
    // Test internet connection first
    if (!testInternetConnection()) {
        Serial.println("❌ TimeManager: No internet connection, cannot sync NTP");
        return false;
    }
    
    // Update NTP client with timeout
    unsigned long startTime = millis();
    const unsigned long TIMEOUT = 10000; // 10 second timeout
    
    while (!ntpClient->update() && (millis() - startTime < TIMEOUT)) {
        delay(100);
    }
    
    if (ntpClient->isTimeSet()) {
        // Get epoch time from NTP (true GMT/UTC time)
        unsigned long gmtEpochTime = ntpClient->getEpochTime();
        
        // Set ESP32's internal RTC with GMT time
        struct timeval tv;
        tv.tv_sec = gmtEpochTime;
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);
        
        Serial.println("✅ TimeManager: NTP sync successful");
        Serial.print("📅 GMT time: ");
        Serial.println(getGMTTime());
        Serial.println("✅ Internal RTC set with GMT time");
        lastNTPUpdate = millis();
        return true;
    } else {
        Serial.println("❌ TimeManager: NTP sync failed - server not responding");
        Serial.println("🔍 Debug: Check internet connection and NTP server availability");
        return false;
    }
}

/**
 * Get current timestamp (epoch time) from ESP32's internal RTC
 * This is the main function used by other parts of the system
 * 
 * Timezone Information:
 * - Internal RTC stores GMT/UTC time
 * - Returns epoch timestamp with Tehran offset for client display
 * - Client should use this time directly for local time display
 * 
 * @return Current epoch timestamp (seconds since 1970) with Tehran offset
 */
unsigned long getCurrentTimestamp() {
    unsigned long gmtTime;
    
    // If NTP is active and time hasn't expired, use NTP
    if (ntpInitialized && ntpClient && (millis() - lastNTPUpdate < NTP_UPDATE_INTERVAL)) {
        ntpClient->update();
        // Get GMT time from NTP (true GMT)
        gmtTime = ntpClient->getEpochTime();
    } else {
        // Otherwise use ESP32's internal RTC (which stores GMT time)
        time_t now;
        time(&now);
        gmtTime = now;
    }
    
    // Add Tehran timezone offset for client display
    return gmtTime + (deviceConfig.time.gmtOffset * 60);
}

/**
 * Get GMT time string for display purposes
 * Converts epoch timestamp to human-readable GMT format
 * 
 * @return Formatted GMT time string (YYYY-MM-DD HH:MM:SS)
 */
String getGMTTime() {
    time_t now;
    time(&now);
    struct tm* timeinfo = gmtime(&now);
    
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
}

/**
 * Get formatted time string for display purposes
 * Converts epoch timestamp to human-readable format with local timezone
 * 
 * @return Formatted time string (YYYY-MM-DD HH:MM:SS) in local time
 */
 String getFormattedTime() {
    // Get GMT time from internal RTC
    time_t now;
    time(&now);
    
    // Add Tehran timezone offset for local time display
    time_t localTime = now + (deviceConfig.time.gmtOffset * 60);
    
    // Convert to local time structure
    struct tm* timeinfo = gmtime(&localTime);
    
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
}

/**
 * Update function called in main loop
 * Handles periodic NTP sync and time management
 * Performs automatic NTP sync every hour if WiFi is connected
 * 
 * Smart Sync Features:
 * - Only syncs if WiFi is connected and NTP was previously initialized
 * - Retries failed syncs up to 2 times
 * - Logs sync status for debugging
 * - Continues with internal RTC if sync fails
 * - Prevents infinite sync loops
 * 
 * NOTE: Temporarily disabled to prevent web server issues
 */
void updateTimeManager() {
    // Temporarily disabled to prevent web server blocking
    // Will be re-enabled once the issue is resolved
    
    /*
    static unsigned long lastSyncAttempt = 0;
    static int failedAttempts = 0;
    const unsigned long SYNC_RETRY_INTERVAL = 60000; // 60 seconds between failed attempts
    
    // Only attempt sync if WiFi is connected and NTP was previously initialized
    if (WiFi.status() == WL_CONNECTED && ntpInitialized) {
        if (millis() - lastNTPUpdate >= NTP_UPDATE_INTERVAL) {
            Serial.println("🔄 TimeManager: Performing periodic NTP sync...");
            
            // Try to sync with retry mechanism (without long delays)
            bool syncSuccess = false;
            for (int attempt = 1; attempt <= 2; attempt++) {
                Serial.print("🔄 TimeManager: Periodic sync attempt ");
                Serial.print(attempt);
                Serial.println("/2");
                
                if (syncNTP()) {
                    syncSuccess = true;
                    Serial.println("✅ TimeManager: Periodic NTP sync successful");
                    failedAttempts = 0; // Reset failed attempts counter
                    break;
                } else {
                    Serial.print("❌ TimeManager: Periodic sync attempt ");
                    Serial.print(attempt);
                    Serial.println(" failed");
                    
                    // Short delay only
                    if (attempt < 2) {
                        delay(100); // Reduced delay
                    }
                }
            }
            
            if (!syncSuccess) {
                Serial.println("⚠️ TimeManager: Periodic sync failed, continuing with internal RTC");
            }
        }
    } else if (WiFi.status() == WL_CONNECTED && !ntpInitialized && !syncAttempted) {
        // Only try late sync if we haven't tried recently
        if (millis() - lastSyncAttempt >= SYNC_RETRY_INTERVAL) {
            Serial.println("🔄 TimeManager: WiFi connected but NTP not initialized, attempting sync...");
            lastSyncAttempt = millis();
            failedAttempts++;
            
            if (syncNTP()) {
                ntpInitialized = true;
                failedAttempts = 0; // Reset failed attempts counter
                Serial.println("✅ TimeManager: Late NTP initialization successful");
            } else {
                Serial.println("❌ TimeManager: Late NTP initialization failed");
                
                // If we've failed too many times, stop trying
                if (failedAttempts >= 3) {
                    Serial.println("⚠️ TimeManager: Too many failed attempts, stopping sync attempts");
                    syncAttempted = true; // Prevent further attempts
                }
            }
        }
    }
    */
}

/**
 * Check if NTP is currently active and providing time
 * 
 * @return true if NTP is active and time is recent, false otherwise
 */
bool isNTPActive() {
    if (!ntpInitialized || !ntpClient) {
        return false;
    }
    
    // Check if last NTP update was within the last hour
    if (millis() - lastNTPUpdate > NTP_UPDATE_INTERVAL) {
        return false;
    }
    
    // Check if WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    return true;
}

/**
 * Get status information for debugging and monitoring
 * Shows current time source and formatted time
 * 
 * @return Status string with current time source and time
 */
String getTimeManagerStatus() {
    if (isNTPActive()) {
        return "NTP Active - " + getFormattedTime();
    } else {
        return "Internal RTC - " + getFormattedTime();
    }
}

/**
 * Check for late NTP sync when WiFi connects after boot
 * This function can be called when WiFi connection is detected
 * Useful for cases where WiFi connects after the initial boot
 * 
 * @return true if late sync was successful, false otherwise
 */
bool checkForLateSync() {
    // Only attempt late sync if we haven't already tried and WiFi is connected
    if (WiFi.status() == WL_CONNECTED && !ntpInitialized && !syncAttempted) {
        Serial.println("🔄 TimeManager: WiFi connected after boot, attempting late NTP sync...");
        
        // Set sync attempted flag to prevent double sync
        syncAttempted = true;
        
        // Initialize NTP client if not already done
        if (!ntpClient) {
            ntpClient = new NTPClient(ntpUDP, deviceConfig.time.ntpServer.c_str());
            ntpClient->begin();
            ntpClient->setTimeOffset(deviceConfig.time.gmtOffset * 60);
        }
        
        // Attempt sync with retry
        for (int attempt = 1; attempt <= 2; attempt++) {
            Serial.print("🔄 TimeManager: Late sync attempt ");
            Serial.print(attempt);
            Serial.println("/2");
            
            if (syncNTP()) {
                ntpInitialized = true;
                Serial.println("✅ TimeManager: Late NTP sync successful");
                return true;
            } else {
                Serial.print("❌ TimeManager: Late sync attempt ");
                Serial.print(attempt);
                Serial.println(" failed");
                
                if (attempt < 2) {
                    delay(1000);
                }
            }
        }
        
        Serial.println("❌ TimeManager: Late NTP sync failed");
        return false;
    }
    return false;
}

/**
 * Force manual NTP sync (can be called from API)
 * Useful for manual time synchronization
 * 
 * @return true if sync was successful
 */
bool forceNTPSync() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("🔄 TimeManager: Manual NTP sync requested...");
        return syncNTP();
    } else {
        Serial.println("❌ TimeManager: Cannot sync - WiFi not connected");
        return false;
    }
}
