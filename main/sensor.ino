// PZEM sensor instance (using Serial2)
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// Array to store recent samples for averaging
Sample samples[WINDOW_SIZE];
int sampleIndex = 0;

// متغیر برای زمان آخرین لاگ سریال
unsigned long lastSerialLogTime = 0;

// متغیرهای برای حفظ حداکثر مقادیر
Sample lastReportedSample;

// تابع اعمال افست‌های کالیبراسیون
void applyCalibrationOffsets(Sample &sample) {
    // اعمال افست‌ها
    sample.voltage += deviceConfig.advanced.voltageOffset;
    sample.current += deviceConfig.advanced.currentOffset;
    sample.power += deviceConfig.advanced.powerOffset;
    
    // محاسبه مجدد توان ظاهری بر اساس مقادیر کالیبره شده
    if (sample.voltage > MIN_VOLTAGE && sample.current >= MIN_CURRENT_FOR_CHANGE) {
        sample.apparentPower = sample.voltage * sample.current;
    }
    
    // محاسبه مجدد توان راکتیو
    if (sample.apparentPower > 0 && sample.pf <= 1.0 && sample.pf > 0) {
        float phi = acos(sample.pf);
        sample.reactivePower = sample.apparentPower * sin(phi);
    }
}

// تابع پردازش مقادیر بر اساس روش انتخاب شده
Sample processValuesBasedOnMethod(const Sample &newSample) {
    Sample processedSample = newSample;
    
    // اعمال افست‌های کالیبراسیون
    applyCalibrationOffsets(processedSample);
    
    // همیشه مقادیر مستقیم را برمی‌گردانیم، بدون توجه به تنظیمات
    return processedSample;
}

// تابع لاگ کردن مقادیر سنسور در سریال (اگر فعال باشد)
void logSensorValues(const Sample &sample) {
    // اگر لاگ سریال فعال نیست یا زمان کافی نگذشته، خارج می‌شویم
    if (!deviceConfig.advanced.serialLoggingEnabled) return;
    
    unsigned long currentMillis = millis();
    if (currentMillis - lastSerialLogTime < deviceConfig.advanced.serialLogInterval) return;
    
    lastSerialLogTime = currentMillis;
    
    // چاپ مقادیر خام سنسور در سریال
    Serial.println("==== PZEM RAW VALUES ====");
    Serial.print("Voltage: "); Serial.print(sample.voltage); Serial.println(" V");
    Serial.print("Current: "); Serial.print(sample.current); Serial.println(" A");
    Serial.print("Power: "); Serial.print(sample.power); Serial.println(" W");
    Serial.print("Energy: "); Serial.print(sample.energy); Serial.println(" kWh");
    Serial.print("Frequency: "); Serial.print(sample.frequency); Serial.println(" Hz");
    Serial.print("Power Factor: "); Serial.println(sample.pf);
    Serial.print("Apparent Power: "); Serial.print(sample.apparentPower); Serial.println(" VA");
    Serial.print("Reactive Power: "); Serial.print(sample.reactivePower); Serial.println(" VAR");
    Serial.println("=========================");
    
    // ارسال به کلاینت‌های WebSocket
    String logMessage = "Voltage: " + String(sample.voltage) + "V, Current: " + String(sample.current) + "A, Power: " + 
                       String(sample.power) + "W, PF: " + String(sample.pf) + ", VA: " + String(sample.apparentPower) + 
                       ", VAR: " + String(sample.reactivePower);
    broadcastSerialLog(logMessage);
}

// Reads values from the PZEM sensor and returns a Sample struct
Sample readPZEM() {
    Sample sample;
    
    // خواندن مقادیر از ماژول PZEM
    sample.voltage = pzem.voltage();
    sample.current = pzem.current();
    sample.power = pzem.power();
    sample.energy = pzem.energy();
    sample.frequency = pzem.frequency();
    sample.pf = pzem.pf();
    sample.timestamp = getCurrentTimestamp(); // Use TimeManager function
    
    // محاسبه توان ظاهری (VA = V * I) - طبق دیتاشیت
    if (sample.voltage > MIN_VOLTAGE && sample.current > 0) {
        sample.apparentPower = sample.voltage * sample.current;
    } else {
        sample.apparentPower = 0;
    }
    
    // محاسبه توان راکتیو (VAR)
    // با استفاده از مثلث توان: S² = P² + Q²، پس Q = √(S² - P²)
    // Q = S * sin(acos(PF))
    if (sample.apparentPower > 0 && sample.pf <= 1.0 && sample.pf > 0) {
        // روش اول: با استفاده از ضریب توان
        float phi = acos(sample.pf);
        sample.reactivePower = sample.apparentPower * sin(phi);
        
        // روش دوم: با استفاده از رابطه پیتاگورس (برای اطمینان)
        float reactivePower2 = 0;
        if (sample.apparentPower > sample.power) {
            reactivePower2 = sqrt(sample.apparentPower * sample.apparentPower - sample.power * sample.power);
            
            // استفاده از میانگین دو روش برای دقت بیشتر
            sample.reactivePower = (sample.reactivePower + reactivePower2) / 2.0;
        }
    } else {
        sample.reactivePower = 0;
    }
    
    return sample;
}

// Checks if a sample is within valid measurement limits
bool isValidSample(const Sample &sample) {
    // فقط بررسی ولتاژ برای اطمینان از اتصال
    if (isnan(sample.voltage)) return false;
    
    // حتی اگر جریان و توان صفر باشند، نمونه معتبر است
    if (sample.voltage < MIN_VOLTAGE || sample.voltage > MAX_VOLTAGE) return false;
    if (sample.frequency < MIN_FREQUENCY || sample.frequency > MAX_FREQUENCY) return false;
    
    return true;
}

// Checks for sudden changes in voltage or current
bool isSuddenChange(const Sample &newSample, const Sample &lastSample) {
    if (abs(newSample.voltage - lastSample.voltage) > MAX_RATE_CHANGE_VOLTAGE) return true;
    if (abs(newSample.current - lastSample.current) > MAX_RATE_CHANGE_CURRENT) return true;
    return false;
}

// Detects inrush current events
bool isInrushCurrent(const Sample &newSample, const Sample &lastSample) {
    if (lastSample.current < MIN_CURRENT_FOR_CHANGE && 
        newSample.current > MIN_CURRENT_FOR_CHANGE * INRUSH_THRESHOLD) {
        return true;
    }
    return false;
}

// Calculates the average of all valid samples in the window
Sample calculateAverage() {
    Sample avg;
    int validSamples = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        if (samples[i].timestamp > 0) {
            avg.voltage += samples[i].voltage;
            avg.current += samples[i].current;
            avg.power += samples[i].power;
            avg.energy += samples[i].energy;
            avg.frequency += samples[i].frequency;
            avg.pf += samples[i].pf;
            avg.apparentPower += samples[i].apparentPower;
            avg.reactivePower += samples[i].reactivePower;
            validSamples++;
        }
    }
    if (validSamples > 0) {
        avg.voltage /= validSamples;
        avg.current /= validSamples;
        avg.power /= validSamples;
        avg.energy /= validSamples;
        avg.frequency /= validSamples;
        avg.pf /= validSamples;
        avg.apparentPower /= validSamples;
        avg.reactivePower /= validSamples;
    }
    avg.timestamp = getCurrentTimestamp(); // Use TimeManager function
    return avg;
}

// Process new sample function - with simple storage
Sample processNewSample() {
    Sample newSample = readPZEM();
    
    // Log sensor values if enabled
    logSensorValues(newSample);
    
    // Use simple and optimized storage
    logSampleOptimized(newSample);
    
    // Always accept new sample, even if there are sudden changes
    // (e.g., when consumer is disconnected)
    samples[sampleIndex] = newSample;
    sampleIndex = (sampleIndex + 1) % WINDOW_SIZE;
    
    // Always return direct values without any additional processing
    return newSample;
}

// Initializes the PZEM sensor (Serial2)
void setupSensor() {
    Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
    delay(1000); // Wait for PZEM to initialize
    
    // اطمینان از استفاده از روش مستقیم به‌روزرسانی
    deviceConfig.advanced.updateMethod = 0; // DIRECT
    
    // پاک کردن فایل‌های مشکل‌دار
    cleanupCorruptedFiles();
}

// Returns the current averaged sensor values
Sample getCurrentValues() {
    return processNewSample();
}

// Reset the PZEM energy counter
bool resetPZEMEnergy() {
    try {
        // تلاش برای ریست انرژی
        bool result = pzem.resetEnergy();
        
        // اضافه کردن تأخیر بیشتر برای پایدار شدن ماژول
        delay(500);
        
        // خواندن چندین نمونه برای اطمینان از برقراری ارتباط درست
        for (int i = 0; i < 5; i++) {
            float v = pzem.voltage();
            float c = pzem.current();
            float e = pzem.energy();
            
            // اگر مقادیر خوانده شده معتبر هستند، ریست موفقیت‌آمیز بوده
            if (!isnan(v) && !isnan(c) && !isnan(e) && e < 0.01) {
                return true;
            }
            
            delay(100);
        }
        
        return result;
    } catch (...) {
        return false; // در صورت بروز خطا
    }
}

// پاک کردن بافر نمونه‌ها برای اجبار به خواندن مقادیر جدید
void resetSampleBuffer() {
    // پاک کردن تمام نمونه‌های ذخیره شده
    for (int i = 0; i < WINDOW_SIZE; i++) {
        samples[i] = Sample(); // مقداردهی اولیه با صفر
    }
    sampleIndex = 0;
    
    // ریست مقادیر حداکثر ذخیره شده
    lastReportedSample = Sample();
} 

// ===== LOGGING FUNCTIONS =====

// تابع دریافت نام فایل روزانه (YYYY-MM-DD)
String getCurrentDateString() {
    time_t now;
    time(&now);
    
    // اضافه کردن offset تهران برای نام فایل
    time_t localTime = now + (deviceConfig.time.gmtOffset * 60);
    struct tm* timeinfo = gmtime(&localTime);
    
    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", timeinfo);
    return String(dateStr); // حذف .csv از اینجا
}

// تابع ذخیره سمپل در فایل CSV
bool logSample(const Sample &sample) {
    // بررسی فعال بودن لاگ کردن تاریخی
    if (!deviceConfig.advanced.historyLoggingActive) {
        return false; // اگر لاگ کردن غیرفعال است، هیچ کاری نکن
    }
    
    // بررسی وجود پوشه data
    if (!LittleFS.exists("/data")) {
        if (!LittleFS.mkdir("/data")) {
            Serial.println("❌ Failed to create /data directory");
            return false;
        }
    }
    
    String fileName = "/data/" + getCurrentDateString() + ".csv";
    File file = LittleFS.open(fileName, "a"); // append mode
    
    if (!file) {
        Serial.println("❌ Failed to open file for logging: " + fileName);
        return false;
    }
    
    // اگر فایل خالی است، header را اضافه کنیم
    if (file.size() == 0) {
        file.println("timestamp,voltage,current,power,energy,frequency,pf,apparentPower,reactivePower");
    }
    
    // تبدیل سمپل به فرمت CSV
    String csvLine = String(sample.timestamp) + "," +
                     String(sample.voltage, 2) + "," +
                     String(sample.current, 3) + "," +
                     String(sample.power, 2) + "," +
                     String(sample.energy, 3) + "," +
                     String(sample.frequency, 1) + "," +
                     String(sample.pf, 3) + "," +
                     String(sample.apparentPower, 2) + "," +
                     String(sample.reactivePower, 2);
    
    file.println(csvLine);
    file.close();
    
    // لاگ برای دیباگ (هر 100 سمپل)
    static int logCounter = 0;
    logCounter++;
    if (logCounter % 100 == 0) {
        Serial.printf("📊 Logged sample #%d to %s\n", logCounter, fileName.c_str());
    }
    
    return true;
}

// تابع خواندن داده‌های تاریخی از فایل
String getHistoryData(const String &date, int maxLines = 1000) {
    String fileName = "/data/" + date + ".csv";
    
    if (!LittleFS.exists(fileName)) {
        return "[]"; // فایل وجود ندارد
    }
    
    File file = LittleFS.open(fileName, "r");
    if (!file) {
        return "[]"; // خطا در باز کردن فایل
    }
    
    String result = "[";
    int lineCount = 0;
    bool firstLine = true;
    
    while (file.available() && lineCount < maxLines) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        // رد کردن header و خطوط خالی
        if (line.length() > 0 && !line.startsWith("timestamp")) {
            if (!firstLine) result += ",";
            
            // تبدیل CSV به JSON
            String jsonLine = csvToJson(line);
            result += jsonLine;
            firstLine = false;
            lineCount++;
        }
    }
    
    result += "]";
    file.close();
    return result;
}

// تابع تبدیل خط CSV به JSON
String csvToJson(const String &csvLine) {
    // تقسیم خط CSV به فیلدها
    String fields[9];
    int fieldIndex = 0;
    int startPos = 0;
    
    for (int i = 0; i < csvLine.length() && fieldIndex < 9; i++) {
        if (csvLine.charAt(i) == ',') {
            fields[fieldIndex] = csvLine.substring(startPos, i);
            fieldIndex++;
            startPos = i + 1;
        }
    }
    // آخرین فیلد
    if (fieldIndex < 9) {
        fields[fieldIndex] = csvLine.substring(startPos);
    }
    
    // بررسی معتبر بودن داده‌ها
    for (int i = 0; i < 9; i++) {
        if (fields[i].length() == 0) {
            fields[i] = "0"; // مقدار پیش‌فرض برای فیلدهای خالی
        }
    }
    
    // ساخت JSON با بررسی معتبر بودن مقادیر
    String json = "{";
    json += "\"timestamp\":" + fields[0] + ",";
    json += "\"voltage\":" + fields[1] + ",";
    json += "\"current\":" + fields[2] + ",";
    json += "\"power\":" + fields[3] + ",";
    json += "\"energy\":" + fields[4] + ",";
    json += "\"frequency\":" + fields[5] + ",";
    json += "\"pf\":" + fields[6] + ",";
    json += "\"apparentPower\":" + fields[7] + ",";
    json += "\"reactivePower\":" + fields[8];
    json += "}";
    
    return json;
}

// تابع محاسبه آمار از داده‌های تاریخی
String getHistoryStats(const String &date) {
    String fileName = "/data/" + date + ".csv";
    
    if (!LittleFS.exists(fileName)) {
        return "{\"error\":\"File not found\"}";
    }
    
    File file = LittleFS.open(fileName, "r");
    if (!file) {
        return "{\"error\":\"Cannot open file\"}";
    }
    
    // رد کردن header
    String header = file.readStringUntil('\n');
    
    // محاسبه آمار
    int count = 0;
    float sumVoltage = 0, sumCurrent = 0, sumPower = 0, sumEnergy = 0;
    float minVoltage = 999, maxVoltage = 0;
    float minCurrent = 999, maxCurrent = 0;
    float minPower = 999, maxPower = 0;
    
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0) {
            // پردازش خط CSV
            String fields[9];
            int fieldIndex = 0;
            int startPos = 0;
            
            for (int i = 0; i < line.length() && fieldIndex < 9; i++) {
                if (line.charAt(i) == ',') {
                    fields[fieldIndex] = line.substring(startPos, i);
                    fieldIndex++;
                    startPos = i + 1;
                }
            }
            if (fieldIndex < 9) {
                fields[fieldIndex] = line.substring(startPos);
            }
            
            // تبدیل به float و محاسبه آمار
            float voltage = fields[1].toFloat();
            float current = fields[2].toFloat();
            float power = fields[3].toFloat();
            float energy = fields[4].toFloat();
            
            sumVoltage += voltage;
            sumCurrent += current;
            sumPower += power;
            sumEnergy += energy;
            
            if (voltage < minVoltage) minVoltage = voltage;
            if (voltage > maxVoltage) maxVoltage = voltage;
            if (current < minCurrent) minCurrent = current;
            if (current > maxCurrent) maxCurrent = current;
            if (power < minPower) minPower = power;
            if (power > maxPower) maxPower = power;
            
            count++;
        }
    }
    
    file.close();
    
    if (count == 0) {
        return "{\"error\":\"No data found\"}";
    }
    
    // ساخت JSON آمار
    String stats = "{";
    stats += "\"count\":" + String(count) + ",";
    stats += "\"voltage\":{\"avg\":" + String(sumVoltage/count, 2) + 
             ",\"min\":" + String(minVoltage, 2) + 
             ",\"max\":" + String(maxVoltage, 2) + "},";
    stats += "\"current\":{\"avg\":" + String(sumCurrent/count, 3) + 
             ",\"min\":" + String(minCurrent, 3) + 
             ",\"max\":" + String(maxCurrent, 3) + "},";
    stats += "\"power\":{\"avg\":" + String(sumPower/count, 2) + 
             ",\"min\":" + String(minPower, 2) + 
             ",\"max\":" + String(maxPower, 2) + "},";
    stats += "\"totalEnergy\":" + String(sumEnergy, 3);
    stats += "}";
    
    return stats;
} 

// تابع دریافت داده‌های تاریخی در بازه زمانی
String getHistoryDataRange(const String &fromDate, const String &toDate, int maxLines) {
    String result = "[";
    bool firstEntry = true;
    int lineCount = 0;
    
    // برای سادگی، فعلاً فقط از یک روز استفاده می‌کنیم
    // در آینده می‌توانیم بازه زمانی را پیاده‌سازی کنیم
    String date = fromDate;
    if (date.length() > 10) {
        date = date.substring(0, 10); // فقط YYYY-MM-DD
    }
    
    // بررسی وجود پوشه data
    if (!LittleFS.exists("/data")) {
        Serial.println("❌ /data directory not found");
        return "[]";
    }
    
    String fileName = "/data/" + date + ".csv";
    Serial.print("🔍 Looking for file: ");
    Serial.println(fileName);
    
    if (!LittleFS.exists(fileName)) {
        Serial.println("❌ File not found: " + fileName);
        return "[]";
    }
    
    File file = LittleFS.open(fileName, "r");
    if (!file) {
        Serial.println("❌ Cannot open file: " + fileName);
        return "[]";
    }
    
    Serial.print("✅ File opened, size: ");
    Serial.println(file.size());
    
    // رد کردن header
    String header = file.readStringUntil('\n');
    Serial.print("📋 Header: ");
    Serial.println(header);
    
    while (file.available() && lineCount < maxLines) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0) {
            if (!firstEntry) result += ",";
            result += csvToJson(line);
            firstEntry = false;
            lineCount++;
        }
    }
    
    file.close();
    result += "]";
    
    Serial.print("📊 Found ");
    Serial.print(lineCount);
    Serial.println(" data points");
    
    return result;
}

// تابع محاسبه آمار از داده‌های تاریخی در بازه زمانی
String getHistoryStatsRange(const String &fromDate, const String &toDate) {
    // برای سادگی، فعلاً فقط از یک روز استفاده می‌کنیم
    String date = fromDate;
    if (date.length() > 10) {
        date = date.substring(0, 10); // فقط YYYY-MM-DD
    }
    
    // استفاده از تابع موجود getHistoryStats
    return getHistoryStats(date);
} 

// Simple and optimized storage function
void logSampleOptimized(const Sample& sample) {
    if (!deviceConfig.advanced.historyLoggingActive) {
        return;
    }
    
    static Sample lastStoredSample;
    static bool isFirstSample = true;
    
    // Store first sample
    if (isFirstSample) {
        lastStoredSample = sample;
        isFirstSample = false;
        storeSampleToFile(sample);
        Serial.printf("✅ First sample stored: v=%.2f, i=%.3f, p=%.2f\n", 
                     sample.voltage, sample.current, sample.power);
        return;
    }
    
    // Check for significant changes
    bool hasSignificantChange = false;
    
    float voltageDiff = abs(sample.voltage - lastStoredSample.voltage);
    float currentDiff = abs(sample.current - lastStoredSample.current);
    float powerDiff = abs(sample.power - lastStoredSample.power);
    float pfDiff = abs(sample.pf - lastStoredSample.pf);
    
    // Check if significant change exists
    if (voltageDiff > VOLTAGE_CHANGE_THRESHOLD ||
        currentDiff > CURRENT_CHANGE_THRESHOLD ||
        powerDiff > POWER_CHANGE_THRESHOLD ||
        pfDiff > PF_CHANGE_THRESHOLD) {
        
        hasSignificantChange = true;
        Serial.printf("🔄 Significant change detected: v_diff=%.3f, i_diff=%.3f, p_diff=%.3f, pf_diff=%.3f\n",
                     voltageDiff, currentDiff, powerDiff, pfDiff);
    }
    
    // Store if significant change detected
    if (hasSignificantChange) {
        lastStoredSample = sample;
        storeSampleToFile(sample);
        Serial.printf("✅ Sample stored: v=%.2f, i=%.3f, p=%.2f, pf=%.3f\n", 
                     sample.voltage, sample.current, sample.power, sample.pf);
    }
}

// Store sample to file function (optimized - only essential values)
void storeSampleToFile(const Sample& sample) {
    // Check if /data directory exists
    if (!LittleFS.exists("/data")) {
        if (!LittleFS.mkdir("/data")) {
            Serial.println("❌ Failed to create /data directory");
            return;
        }
    }
    
    String dateStr = getCurrentDateString();
    if (dateStr.endsWith(".csv")) {
        dateStr = dateStr.substring(0, dateStr.length() - 4);
    }
    String filename = "/data/" + dateStr + ".csv";
    
    File file = LittleFS.open(filename, "a");
    if (!file) {
        Serial.printf("❌ Failed to open file: %s\n", filename.c_str());
        return;
    }
    
    // Add header if file is empty (only essential values)
    if (file.size() == 0) {
        file.println("timestamp,voltage,current,power,energy,frequency,pf");
    }
    
    // Optimized CSV format - only essential values (calculated values will be computed on display)
    String line = String(sample.timestamp) + "," +
                 String(sample.voltage, 2) + "," +
                 String(sample.current, 3) + "," +
                 String(sample.power, 2) + "," +
                 String(sample.energy, 3) + "," +
                 String(sample.frequency, 1) + "," +
                 String(sample.pf, 3);
    
    file.println(line);
    file.close();
}

// Read optimized data from file function
std::vector<Sample> readOptimizedData(const String& date) {
    std::vector<Sample> samples;
    String filename = "/data/" + date + ".csv";
    
    if (!LittleFS.exists(filename)) {
        Serial.printf("❌ File not found: %s\n", filename.c_str());
        return samples;
    }
    
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.printf("❌ Cannot open file: %s\n", filename.c_str());
        return samples;
    }
    
    bool isFirstLine = true;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0) {
            // Skip header
            if (isFirstLine && line.startsWith("timestamp")) {
                isFirstLine = false;
                continue;
            }
            isFirstLine = false;
            
            // Parse CSV line: timestamp,voltage,current,power,energy,frequency,pf,apparentPower,reactivePower
            int comma1 = line.indexOf(',');
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);
            int comma4 = line.indexOf(',', comma3 + 1);
            int comma5 = line.indexOf(',', comma4 + 1);
            int comma6 = line.indexOf(',', comma5 + 1);
            int comma7 = line.indexOf(',', comma6 + 1);
            int comma8 = line.indexOf(',', comma7 + 1);
            
            if (comma8 != -1) {
                Sample sample;
                sample.timestamp = line.substring(0, comma1).toInt();
                sample.voltage = line.substring(comma1 + 1, comma2).toFloat();
                sample.current = line.substring(comma2 + 1, comma3).toFloat();
                sample.power = line.substring(comma3 + 1, comma4).toFloat();
                sample.energy = line.substring(comma4 + 1, comma5).toFloat();
                sample.frequency = line.substring(comma5 + 1, comma6).toFloat();
                sample.pf = line.substring(comma6 + 1, comma7).toFloat();
                sample.apparentPower = line.substring(comma7 + 1, comma8).toFloat();
                sample.reactivePower = line.substring(comma8 + 1).toFloat();
                
                samples.push_back(sample);
            }
        }
    }
    
    file.close();
    Serial.printf("📊 Read %d samples from %s\n", samples.size(), filename.c_str());
    return samples;
}

// تابع محاسبه آمار از داده‌های بهینه شده
String getOptimizedStats(const String& date) {
    std::vector<Sample> samples = readOptimizedData(date);
    
    if (samples.empty()) {
        return "{\"error\":\"No data found\"}";
    }
    
    // محاسبه آمار
    float minVoltage = samples[0].voltage, maxVoltage = samples[0].voltage, avgVoltage = 0;
    float minCurrent = samples[0].current, maxCurrent = samples[0].current, avgCurrent = 0;
    float minPower = samples[0].power, maxPower = samples[0].power, avgPower = 0;
    float minPf = samples[0].pf, maxPf = samples[0].pf, avgPf = 0;
    float totalEnergy = 0;
    
    for (const auto& sample : samples) {
        // ولتاژ
        if (sample.voltage < minVoltage) minVoltage = sample.voltage;
        if (sample.voltage > maxVoltage) maxVoltage = sample.voltage;
        avgVoltage += sample.voltage;
        
        // جریان
        if (sample.current < minCurrent) minCurrent = sample.current;
        if (sample.current > maxCurrent) maxCurrent = sample.current;
        avgCurrent += sample.current;
        
        // توان
        if (sample.power < minPower) minPower = sample.power;
        if (sample.power > maxPower) maxPower = sample.power;
        avgPower += sample.power;
        
        // ضریب توان
        if (sample.pf < minPf) minPf = sample.pf;
        if (sample.pf > maxPf) maxPf = sample.pf;
        avgPf += sample.pf;
        
        // انرژی (محاسبه تقریبی)
        totalEnergy += sample.power * (1.0 / 3600.0); // تبدیل به kWh
    }
    
    int count = samples.size();
    avgVoltage /= count;
    avgCurrent /= count;
    avgPower /= count;
    avgPf /= count;
    
    // ساخت JSON
    String json = "{";
    json += "\"voltage\":{\"min\":" + String(minVoltage, 2) + ",\"max\":" + String(maxVoltage, 2) + ",\"avg\":" + String(avgVoltage, 2) + "},";
    json += "\"current\":{\"min\":" + String(minCurrent, 3) + ",\"max\":" + String(maxCurrent, 3) + ",\"avg\":" + String(avgCurrent, 3) + "},";
    json += "\"power\":{\"min\":" + String(minPower, 2) + ",\"max\":" + String(maxPower, 2) + ",\"avg\":" + String(avgPower, 2) + "},";
    json += "\"pf\":{\"min\":" + String(minPf, 3) + ",\"max\":" + String(maxPf, 3) + ",\"avg\":" + String(avgPf, 3) + "},";
    json += "\"totalEnergy\":" + String(totalEnergy, 3) + ",";
    json += "\"sampleCount\":" + String(count) + "";
    json += "}";
    
    return json;
} 

// تابع پاک کردن فایل‌های مشکل‌دار
void cleanupCorruptedFiles() {
    if (!LittleFS.exists("/data")) {
        return;
    }
    
    File dir = LittleFS.open("/data");
    if (!dir || !dir.isDirectory()) {
        return;
    }
    
    File file = dir.openNextFile();
    while (file) {
        String fileName = String(file.name());
        
        // بررسی فایل‌های مشکل‌دار (با پسوند .csv.csv)
        if (fileName.endsWith(".csv.csv")) {
            Serial.printf("🗑️ Removing corrupted file: %s\n", fileName.c_str());
            LittleFS.remove(fileName);
        }
        
        file = dir.openNextFile();
    }
    
    dir.close();
} 