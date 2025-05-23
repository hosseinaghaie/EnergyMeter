// (No includes needed here)
// Only function implementations should be present in this file.

// Load config from LittleFS (config.json)
bool loadConfig(DeviceConfig &cfg) {
    File file = LittleFS.open("/config.json", "r");
    if (!file) return false;
    
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) return false;

    // Network
    cfg.network.apEnable = doc["network"]["apEnable"] | false;
    cfg.network.apSSID = doc["network"]["apSSID"] | "";
    cfg.network.apPassword = doc["network"]["apPassword"] | "";
    cfg.network.apIP = doc["network"]["apIP"] | "";
    cfg.network.clientEnable = doc["network"]["clientEnable"] | false;
    cfg.network.clientSSID = doc["network"]["clientSSID"] | "";
    cfg.network.clientPassword = doc["network"]["clientPassword"] | "";
    cfg.network.mdnsName = doc["network"]["mdnsName"] | "";
    cfg.network.ipMode = doc["network"]["ipMode"] | "dhcp";
    cfg.network.clientIP = doc["network"]["clientIP"] | "";
    cfg.network.clientGateway = doc["network"]["clientGateway"] | "";
    cfg.network.clientSubnet = doc["network"]["clientSubnet"] | "";

    // User
    cfg.user.username = doc["user"]["username"] | "admin";
    cfg.user.password = doc["user"]["password"] | "admin";
    cfg.user.authEnable = doc["user"]["authEnable"] | false;

    // Sensor
    cfg.sensor.sampleRate = doc["sensor"]["sampleRate"] | 1000;
    cfg.sensor.avgSamples = doc["sensor"]["avgSamples"] | 10;
    cfg.sensor.minVoltage = doc["sensor"]["minVoltage"] | 0;
    cfg.sensor.maxVoltage = doc["sensor"]["maxVoltage"] | 300;
    cfg.sensor.minCurrent = doc["sensor"]["minCurrent"] | 0;
    cfg.sensor.maxCurrent = doc["sensor"]["maxCurrent"] | 100;
    cfg.sensor.minPower = doc["sensor"]["minPower"] | 0;
    cfg.sensor.maxPower = doc["sensor"]["maxPower"] | 23000;
    cfg.sensor.alertEnable = doc["sensor"]["alertEnable"] | false;

    // Time
    cfg.time.gmtOffset = doc["time"]["gmtOffset"] | 210;
    cfg.time.gmtPositive = doc["time"]["gmtPositive"] | true;
    cfg.time.ntpServer = doc["time"]["ntpServer"] | "pool.ntp.org";

    // Advanced
    cfg.advanced.serialLoggingEnabled = doc["advanced"]["serialLoggingEnabled"] | false;
    cfg.advanced.serialLogInterval = doc["advanced"]["serialLogInterval"] | 1000;
    cfg.advanced.updateMethod = doc["advanced"]["updateMethod"] | 0;
    cfg.advanced.thresholdPercent = doc["advanced"]["thresholdPercent"] | 20.0;
    cfg.advanced.voltageOffset = doc["advanced"]["voltageOffset"] | 0.0;
    cfg.advanced.currentOffset = doc["advanced"]["currentOffset"] | 0.0;
    cfg.advanced.powerOffset = doc["advanced"]["powerOffset"] | 0.0;

    return true;
}

// Save config to LittleFS (config.json)
bool saveConfig(const DeviceConfig &cfg) {
    File file = LittleFS.open("/config.json", "w");
    if (!file) return false;

    StaticJsonDocument<2048> doc;

    // Network
    doc["network"]["apEnable"] = cfg.network.apEnable;
    doc["network"]["apSSID"] = cfg.network.apSSID;
    doc["network"]["apPassword"] = cfg.network.apPassword;
    doc["network"]["apIP"] = cfg.network.apIP;
    doc["network"]["clientEnable"] = cfg.network.clientEnable;
    doc["network"]["clientSSID"] = cfg.network.clientSSID;
    doc["network"]["clientPassword"] = cfg.network.clientPassword;
    doc["network"]["mdnsName"] = cfg.network.mdnsName;
    doc["network"]["ipMode"] = cfg.network.ipMode;
    doc["network"]["clientIP"] = cfg.network.clientIP;
    doc["network"]["clientGateway"] = cfg.network.clientGateway;
    doc["network"]["clientSubnet"] = cfg.network.clientSubnet;

    // User
    doc["user"]["username"] = cfg.user.username;
    doc["user"]["password"] = cfg.user.password;
    doc["user"]["authEnable"] = cfg.user.authEnable;

    // Sensor
    doc["sensor"]["sampleRate"] = cfg.sensor.sampleRate;
    doc["sensor"]["avgSamples"] = cfg.sensor.avgSamples;
    doc["sensor"]["minVoltage"] = cfg.sensor.minVoltage;
    doc["sensor"]["maxVoltage"] = cfg.sensor.maxVoltage;
    doc["sensor"]["minCurrent"] = cfg.sensor.minCurrent;
    doc["sensor"]["maxCurrent"] = cfg.sensor.maxCurrent;
    doc["sensor"]["minPower"] = cfg.sensor.minPower;
    doc["sensor"]["maxPower"] = cfg.sensor.maxPower;
    doc["sensor"]["alertEnable"] = cfg.sensor.alertEnable;

    // Time
    doc["time"]["gmtOffset"] = cfg.time.gmtOffset;
    doc["time"]["gmtPositive"] = cfg.time.gmtPositive;
    doc["time"]["ntpServer"] = cfg.time.ntpServer;

    // Advanced
    doc["advanced"]["serialLoggingEnabled"] = cfg.advanced.serialLoggingEnabled;
    doc["advanced"]["serialLogInterval"] = cfg.advanced.serialLogInterval;
    doc["advanced"]["updateMethod"] = cfg.advanced.updateMethod;
    doc["advanced"]["thresholdPercent"] = cfg.advanced.thresholdPercent;
    doc["advanced"]["voltageOffset"] = cfg.advanced.voltageOffset;
    doc["advanced"]["currentOffset"] = cfg.advanced.currentOffset;
    doc["advanced"]["powerOffset"] = cfg.advanced.powerOffset;

    serializeJson(doc, file);
    file.close();
    return true;
}

// Convert config to JSON string
String configToJson(const DeviceConfig &cfg) {
    StaticJsonDocument<2048> doc;
    
    // Network
    doc["network"]["apEnable"] = cfg.network.apEnable;
    doc["network"]["apSSID"] = cfg.network.apSSID;
    doc["network"]["apPassword"] = cfg.network.apPassword;
    doc["network"]["apIP"] = cfg.network.apIP;
    doc["network"]["clientEnable"] = cfg.network.clientEnable;
    doc["network"]["clientSSID"] = cfg.network.clientSSID;
    doc["network"]["clientPassword"] = cfg.network.clientPassword;
    doc["network"]["mdnsName"] = cfg.network.mdnsName;
    doc["network"]["ipMode"] = cfg.network.ipMode;
    doc["network"]["clientIP"] = cfg.network.clientIP;
    doc["network"]["clientGateway"] = cfg.network.clientGateway;
    doc["network"]["clientSubnet"] = cfg.network.clientSubnet;

    // User
    doc["user"]["username"] = cfg.user.username;
    doc["user"]["password"] = cfg.user.password;
    doc["user"]["authEnable"] = cfg.user.authEnable;

    // Sensor
    doc["sensor"]["sampleRate"] = cfg.sensor.sampleRate;
    doc["sensor"]["avgSamples"] = cfg.sensor.avgSamples;
    doc["sensor"]["minVoltage"] = cfg.sensor.minVoltage;
    doc["sensor"]["maxVoltage"] = cfg.sensor.maxVoltage;
    doc["sensor"]["minCurrent"] = cfg.sensor.minCurrent;
    doc["sensor"]["maxCurrent"] = cfg.sensor.maxCurrent;
    doc["sensor"]["minPower"] = cfg.sensor.minPower;
    doc["sensor"]["maxPower"] = cfg.sensor.maxPower;
    doc["sensor"]["alertEnable"] = cfg.sensor.alertEnable;

    // Time
    doc["time"]["gmtOffset"] = cfg.time.gmtOffset;
    doc["time"]["gmtPositive"] = cfg.time.gmtPositive;
    doc["time"]["ntpServer"] = cfg.time.ntpServer;

    // Advanced
    doc["advanced"]["serialLoggingEnabled"] = cfg.advanced.serialLoggingEnabled;
    doc["advanced"]["serialLogInterval"] = cfg.advanced.serialLogInterval;
    doc["advanced"]["updateMethod"] = cfg.advanced.updateMethod;
    doc["advanced"]["thresholdPercent"] = cfg.advanced.thresholdPercent;
    doc["advanced"]["voltageOffset"] = cfg.advanced.voltageOffset;
    doc["advanced"]["currentOffset"] = cfg.advanced.currentOffset;
    doc["advanced"]["powerOffset"] = cfg.advanced.powerOffset;

    String json;
    serializeJson(doc, json);
    return json;
}

// Parse JSON and update config
bool jsonToConfig(const String &json, DeviceConfig &cfg) {
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;

    // Network
    cfg.network.apEnable = doc["network"]["apEnable"] | false;
    cfg.network.apSSID = doc["network"]["apSSID"] | "";
    cfg.network.apPassword = doc["network"]["apPassword"] | "";
    cfg.network.apIP = doc["network"]["apIP"] | "";
    cfg.network.clientEnable = doc["network"]["clientEnable"] | false;
    cfg.network.clientSSID = doc["network"]["clientSSID"] | "";
    cfg.network.clientPassword = doc["network"]["clientPassword"] | "";
    cfg.network.mdnsName = doc["network"]["mdnsName"] | "";
    cfg.network.ipMode = doc["network"]["ipMode"] | "dhcp";
    cfg.network.clientIP = doc["network"]["clientIP"] | "";
    cfg.network.clientGateway = doc["network"]["clientGateway"] | "";
    cfg.network.clientSubnet = doc["network"]["clientSubnet"] | "";

    // User
    cfg.user.username = doc["user"]["username"] | "admin";
    cfg.user.password = doc["user"]["password"] | "admin";
    cfg.user.authEnable = doc["user"]["authEnable"] | false;

    // Sensor
    cfg.sensor.sampleRate = doc["sensor"]["sampleRate"] | 1000;
    cfg.sensor.avgSamples = doc["sensor"]["avgSamples"] | 10;
    cfg.sensor.minVoltage = doc["sensor"]["minVoltage"] | 0;
    cfg.sensor.maxVoltage = doc["sensor"]["maxVoltage"] | 300;
    cfg.sensor.minCurrent = doc["sensor"]["minCurrent"] | 0;
    cfg.sensor.maxCurrent = doc["sensor"]["maxCurrent"] | 100;
    cfg.sensor.minPower = doc["sensor"]["minPower"] | 0;
    cfg.sensor.maxPower = doc["sensor"]["maxPower"] | 23000;
    cfg.sensor.alertEnable = doc["sensor"]["alertEnable"] | false;

    // Time
    cfg.time.gmtOffset = doc["time"]["gmtOffset"] | 210;
    cfg.time.gmtPositive = doc["time"]["gmtPositive"] | true;
    cfg.time.ntpServer = doc["time"]["ntpServer"] | "pool.ntp.org";

    // Advanced
    cfg.advanced.serialLoggingEnabled = doc["advanced"]["serialLoggingEnabled"] | false;
    cfg.advanced.serialLogInterval = doc["advanced"]["serialLogInterval"] | 1000;
    cfg.advanced.updateMethod = doc["advanced"]["updateMethod"] | 0;
    cfg.advanced.thresholdPercent = doc["advanced"]["thresholdPercent"] | 20.0;
    cfg.advanced.voltageOffset = doc["advanced"]["voltageOffset"] | 0.0;
    cfg.advanced.currentOffset = doc["advanced"]["currentOffset"] | 0.0;
    cfg.advanced.powerOffset = doc["advanced"]["powerOffset"] | 0.0;

    return true;
} 