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
    sample.timestamp = getCurrentTime();
    
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
    avg.timestamp = getCurrentTime();
    return avg;
}

// Reads a new sample, validates it, checks for sudden changes, and updates the sample array
Sample processNewSample() {
    Sample newSample = readPZEM();
    
    // لاگ کردن مقادیر خام سنسور قبل از هر پردازشی
    logSensorValues(newSample);
    
    // همیشه نمونه جدید را قبول کنیم، حتی اگر تغییرات ناگهانی باشد
    // (به‌طور مثال وقتی مصرف‌کننده قطع می‌شود)
    samples[sampleIndex] = newSample;
    sampleIndex = (sampleIndex + 1) % WINDOW_SIZE;
    
    // همیشه مقادیر مستقیم را برگردانیم بدون هیچ پردازش اضافی
    return newSample;
}

// Initializes the PZEM sensor (Serial2)
void setupSensor() {
    Serial2.begin(9600, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);
    delay(1000); // Wait for PZEM to initialize
    
    // اطمینان از استفاده از روش مستقیم به‌روزرسانی
    deviceConfig.advanced.updateMethod = 0; // DIRECT
    Serial.println("Update method set to DIRECT");
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