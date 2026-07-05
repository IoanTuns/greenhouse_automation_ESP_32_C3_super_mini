#include "rtc_module.h"
#include "globals.h"
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include "config_hardware.h"
#include "config_system.h"

static const unsigned long RTC_RETRY_INTERVAL = 10000;
static const uint8_t DS3231_ADDR = 0x68;

static uint8_t bcd2bin(uint8_t v) { return v - 6 * (v >> 4); }
static uint8_t bin2bcd(uint8_t v) { return v + 6 * (v / 10); }

// Soft-RTC state: updated by initRTC() (NTP/compile time) and syncTimeManual()
static DateTime  s_softBase(2000, 1, 1, 0, 0, 0);
static uint32_t  s_softBaseMs = 0;
static bool      s_softValid  = false;

// Resets the ESP32-C3 I2C peripheral and drains the ~103-transaction dead zone
// that follows every Wire.begin(). Call whenever endTransmission() returns NACK
// (code 2) or requestFrom() returns fewer bytes than expected — both leave the
// bus locked with the DS3231 holding SDA LOW.
static void recoverI2CBus() {
    Wire.begin(PIN_SDA, PIN_SCL);
    Wire.setClock(100000);
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        Wire.endTransmission();
    }
}

static bool rtcWriteRegister(uint8_t reg, const uint8_t* data, size_t len) {
    for (int attempt = 1; attempt <= 3; attempt++) {
        Wire.beginTransmission(DS3231_ADDR);
        Wire.write(reg);
        for (size_t i = 0; i < len; i++) Wire.write(data[i]);
        uint8_t status = Wire.endTransmission(true);
        if (status == 0) return true;
        Serial.printf("[RTC] I2C write reg 0x%02X attempt %d failed: %u\n", reg, attempt, status);
        recoverI2CBus(); // NACK (code 2) = bus locked; recovery clears it before retry
    }
    return false;
}

static bool rtcReadRegisters(uint8_t reg, uint8_t* buf, size_t len) {
    for (int attempt = 1; attempt <= 3; attempt++) {
        if (!rtcWriteRegister(reg, nullptr, 0)) {
            // rtcWriteRegister already ran recoverI2CBus() on each of its retries
            Serial.printf("[RTC] I2C set-register 0x%02X failed on attempt %d\n", reg, attempt);
            continue;
        }

        uint8_t received = Wire.requestFrom((uint8_t)DS3231_ADDR, (uint8_t)len, (uint8_t)true);
        if (received == len) {
            for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
            return true;
        }

        Serial.printf("[RTC] requestFrom reg 0x%02X attempt %d: got %u/%u bytes\n",
                      reg, attempt, received, (unsigned)len);
        recoverI2CBus(); // requestFrom() left bus locked; recover before next attempt
    }
    return false;
}

void scanI2CBus() {
    Serial.println("[DEBUG] Scanning I2C bus...");
    bool foundAny = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[DEBUG] I2C device found at address 0x%02X\n", addr);
            foundAny = true;
        }
    }
    if (!foundAny) {
        Serial.println("[DEBUG] WARNING: No I2C devices detected on the bus at all!");
    }
}

// Citire RTC cu STOP+START (pentru evitarea bug-ului iicWriteReadNonStop pe ESP32-C3)
DateTime readRTC() {
    // Pre-recover the bus: ESP32-C3 Wire.requestFrom() leaves the DS1307 holding
    // SDA LOW after every read, which causes the *next* endTransmission() to NACK
    // (code 2).  Recovering here eliminates that "attempt 1 failed: 2" churn.
    recoverI2CBus();

    uint8_t buf[7] = {};
    bool ioOk = rtcReadRegisters(0x00, buf, sizeof(buf));

    if (!ioOk) {
        static uint32_t lastFailDiag = 0xFFFFFF00UL;
        if (millis() - lastFailDiag >= 30000UL) {
            lastFailDiag = millis();
            Serial.println("[RTC DIAG] I2C READ IS FAILING.");
            Serial.printf("[RTC DIAG]   1. SDA/SCL swapped - SDA:GPIO%d SCL:GPIO%d\n", PIN_SDA, PIN_SCL);
            Serial.println("[RTC DIAG]   2. Missing pull-ups - SDA+SCL need 4.7k to 3.3V");
            Serial.println("[RTC DIAG]   3. DS3231 pull-ups on 5V rail - connect to 3.3V instead");
            Serial.println("[RTC DIAG]   4. Loose breadboard/jumper connection");
            scanI2CBus();
        }
    } else {
        static uint32_t lastOkDiag = 0xFFFFFF00UL;
        if (millis() - lastOkDiag >= 30000UL) {
            lastOkDiag = millis();
            Serial.printf("[RTC DIAG] Read OK. Raw regs [0x00-0x06]: ");
            for (int i = 0; i < 7; i++) Serial.printf("%02X ", buf[i]);
            Serial.println();
            bool por = ((buf[0] & 0x7F) == 0 && buf[1] == 0 && buf[2] == 0 && buf[6] == 0);
            if (por) {
                Serial.println("[RTC DIAG] POR state - chip reset (no CR2032 battery + VCC dip).");
                if (buf[0] & 0x80)
                    Serial.println("[RTC DIAG] buf[0] bit7=1: DS1307 CH flag, oscillator HALTED.");
                uint8_t cs[2] = {};
                if (rtcReadRegisters(0x0E, cs, 2))
                    Serial.printf("[RTC DIAG] Ctrl(0x0E)=0x%02X EOSC=%d  Stat(0x0F)=0x%02X OSF=%d\n",
                        cs[0], (cs[0]>>7)&1, cs[1], (cs[1]>>7)&1);
            }
        }

        DateTime hw(
            bcd2bin(buf[6]) + 2000,
            bcd2bin(buf[5] & 0x1F),
            bcd2bin(buf[4]),
            bcd2bin(buf[2] & 0x3F),
            bcd2bin(buf[1]),
            bcd2bin(buf[0] & 0x7F)     // mask DS1307 CH bit before BCD decode
        );
        if (hw.year() >= 2020) {
            // Valid DS1307 time — update soft-RTC reference and return real value
            s_softBase   = hw;
            s_softBaseMs = millis();
            s_softValid  = true;
            return hw;
        }
        // else: DS1307 in POR state — fall through to soft-RTC below
    }

    if (s_softValid) {
        // Synthesize from last good reference + elapsed millis()
        uint32_t elapsed = millis() - s_softBaseMs;
        return s_softBase + TimeSpan((int32_t)(elapsed / 1000));
    }

    // No reference at all yet — force retry via rtcRetryLoop()
    rtcAvailable = false;
    return DateTime(2000, 1, 1, 0, 0, 0);
}

// Scriere curatÄƒ Ã®n RTC (evitÄƒ read-before-write)
static void adjustRTC(const DateTime& dt) {
    Serial.printf("[RTC] Adjusting clock to: %04d-%02d-%02d %02d:%02d:%02d\n", 
                  dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());

    uint8_t data[7] = {
        bin2bcd(dt.second()),
        bin2bcd(dt.minute()),
        bin2bcd(dt.hour()),
        1, // Day of Week, set to 1 as 0 is invalid
        bin2bcd(dt.day()),
        bin2bcd(dt.month()),
        bin2bcd(dt.year() - 2000)
    };
    bool okTime = rtcWriteRegister(0x00, data, sizeof(data));
    const uint8_t statusReset = 0x00;
    bool okStatus = rtcWriteRegister(0x0F, &statusReset, 1);

    if (okTime && okStatus) {
        Serial.println("[RTC] Time adjusted successfully.");
    } else {
        Serial.printf("[RTC ERROR] Failed to adjust time. time=%d status=%d\n", okTime, okStatus);
    }
}

void syncTimeManual(const DateTime& dt) {
    adjustRTC(dt);
    s_softBase   = dt;
    s_softBaseMs = millis();
    s_softValid  = true;
    Serial.printf("[RTC] Time set manually: %04d-%02d-%02d %02d:%02d:%02d\n",
        dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
}

bool initRTC() {
    Serial.printf("\n[RTC] Initializing I2C on SDA: GPIO%d, SCL: GPIO%d...\n", PIN_SDA, PIN_SCL);
#if defined(I2C_MODE_MASTER)
    Wire.begin(PIN_SDA, PIN_SCL, I2C_MODE_MASTER);
    Serial.println("[RTC] Wire.begin() using explicit I2C_MODE_MASTER");
#else
    Wire.begin(PIN_SDA, PIN_SCL);
    Serial.println("[RTC] Wire.begin() using fallback mode (I2C_MODE_MASTER undefined)");
#endif
    Wire.setClock(100000);
    delay(20); // Give the bus lines time to settle on C3 before the first transaction.

    // ESP32-C3 warm-up & diagnostic buclÄƒ
    Serial.print("[RTC] Running ESP32-C3 I2C warm-up quota...");
    uint32_t warmUpErrors = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission(true) != 0) {
            warmUpErrors++;
        }
    }
    Serial.printf(" Done. (Ignored %u dead endpoints).\n", warmUpErrors);

    // Pre-check fundamental: RÄƒspunde fizic adresa 0x68?
    if (!rtcWriteRegister(0x00, nullptr, 0)) {
        statusRTC = "RTC Error";
        rtcAvailable = false;
        Serial.println("[ERROR] RTC: No device responded at address 0x68. Check physical hardware, pullups or power rails.");
        scanI2CBus();
        return false;
    }
    Serial.println("[OK] RTC: Hardware detected at address 0x68.");

    // Best available time reference: NTP if WiFi is up, compile time otherwise
    DateTime initTime(F(__DATE__), F(__TIME__));
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[RTC] WiFi up — attempting NTP sync...");
        configTime(UTC_OFFSET_SEC, 0, NTP_SERVER);
        struct tm timeinfo = {};
        if (getLocalTime(&timeinfo, 8000)) {
            initTime = DateTime(
                (uint16_t)(timeinfo.tm_year + 1900),
                (uint8_t)(timeinfo.tm_mon + 1),
                (uint8_t)timeinfo.tm_mday,
                (uint8_t)timeinfo.tm_hour,
                (uint8_t)timeinfo.tm_min,
                (uint8_t)timeinfo.tm_sec
            );
            Serial.printf("[RTC] NTP OK: %04d-%02d-%02d %02d:%02d:%02d\n",
                initTime.year(), initTime.month(), initTime.day(),
                initTime.hour(), initTime.minute(), initTime.second());
        } else {
            Serial.println("[RTC] NTP failed — using compile time.");
        }
    }
    // Prime soft-RTC now so readRTC() returns correct time even if DS1307 is in POR
    s_softBase   = initTime;
    s_softBaseMs = millis();
    s_softValid  = true;

    // Bucla de initializare cu reincercari
    for (int attempt = 1; attempt <= 5; attempt++) {
        Serial.printf("[RTC] Init attempt %d/5...\n", attempt);

        // Activare oscilator: EOSC=0 (active LOW) Ã®n registrul 0x0E. SetaÈ›i 0x1C (default din fabricÄƒ)
        if (!rtcWriteRegister(0x0E, (const uint8_t[]){0x1C}, 1)) {
            Serial.println("[RTC] Transmit to Control Register (0x0E) failed. Retrying in 1s...");
            delay(1000);
            continue;
        }

        // Citire status curent (OSF - Oscillator Stop Flag din registrul 0x0F)
        uint8_t statusReg = 0;
        bool lostPower = true;
        if (rtcReadRegisters(0x0F, &statusReg, 1)) {
            lostPower = (statusReg & 0x80); // Bit 7 este OSF
            Serial.printf("[RTC] Status Reg (0x0F) = 0x%02X (OSF: %d)\n", statusReg, lostPower ? 1 : 0);
        } else {
            Serial.println("[RTC] Failed to read Status Register. Retrying...");
            delay(1000);
            continue;
        }

        // VerificÄƒm dacÄƒ timpul din registre este plauzibil (an > 2020)
        if (!lostPower) {
            DateTime t = readRTC();
            Serial.printf("[RTC] Verifying internal year: %d\n", t.year());
            if (t.year() < 2020) {
                Serial.println("[RTC] Year is older than 2020. Marking as uninitialized/lost power.");
                lostPower = true;
            }
        }

        // DacÄƒ a pierdut curentul sau e neiniÈ›ializat, rescriem timpul de la compilare
        if (lostPower) {
            Serial.println("[WARNING] RTC: Clock lost power or uninitialized. Syncing with compile time...");
            adjustRTC(initTime);
        }

        // Validare finalÄƒ prin citire directÄƒ
        DateTime now = readRTC();
        if (now.year() >= 2020) {
            statusRTC = "OK";
            rtcAvailable = true; // SincronizÄƒm variabila globalÄƒ direct aici!
            Serial.printf("[OK] RTC Initialized successfully: %04d-%02d-%02d %02d:%02d:%02d\n",
                        now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
            return true;
        }
        
        delay(1000);
    }

    statusRTC = "RTC Error";
    rtcAvailable = false;
    Serial.println("[ERROR] RTC: Initialization failed after 5 attempts. Operating without real time.");
    return false;
}

void rtcRetryLoop() {
    if (rtcAvailable || millis() - lastRtcRetry < RTC_RETRY_INTERVAL) return;
    
    Serial.println("[RTC] Attempting periodic background reconnect...");
    lastRtcRetry = millis();
    if (initRTC()) {
        Serial.println("[OK] RTC: Background retry succeeded.");
    } else {
        Serial.println("[ERROR] RTC: Background retry failed. Will try again in 10 seconds.");
    }
}