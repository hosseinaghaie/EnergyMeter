#include "settings.h"
///////////////////////////
WiFiUDP ntpUDP;

// Main setup function: initializes all modules and services
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("Starting Power Meter...");

  // Initialize LittleFS file system
  if (!LittleFS.begin(true)) {
    Serial.println("Error mounting LittleFS");
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
    return;
  }
  Serial.println("Network setup done");

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
