#include "rtc_module.h"
#include "globals.h"
#include <Wire.h>
#include "config_hardware.h"

static const unsigned long RTC_RETRY_INTERVAL = 10000;

void scanI2CBus() {
    Serial.println("[DEBUG] Scanare magistrală I2C...");
    bool foundAny = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t result = Wire.endTransmission();
        if (result == 0) {
            Serial.printf("[DEBUG] Dispozitiv I2C găsit la adresa 0x%02X\n", addr);
            foundAny = true;
        }
    }
    if (!foundAny) {
        Serial.println("[DEBUG] Nu a fost găsit niciun dispozitiv I2C.");
    }
}

bool initRTC() {
    if (!Wire.begin(PIN_SDA, PIN_SCL)) {
        statusRTC = "Eroare I2C";
        Serial.println("[EROARE] I2C: Inițializarea magistralei a eșuat!");
        return false;
    }
    Wire.setClock(1000);

    if (!rtc.begin()) {
        statusRTC = "Eroare RTC";
        Serial.println("[EROARE] RTC: Senzorul DS3231 nu a fost detectat la adresa 0x68.");
        scanI2CBus();
        return false;
    }

    statusRTC = "OK";
    Serial.println("[OK] RTC: Modul detectat corect.");
    if (rtc.lostPower()) {
        Serial.println("[AVERTISMENT] RTC: Ceasul a pierdut alimentarea, se resetează timpul...");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    return true;
}

void rtcRetryLoop() {
    if (rtcAvailable || millis() - lastRtcRetry < RTC_RETRY_INTERVAL) return;
    lastRtcRetry = millis();
    if (initRTC()) {
        rtcAvailable = true;
        Serial.println("[OK] RTC: Reîncercare reușită.");
    } else {
        Serial.println("[EROARE] RTC: Reîncercare eșuată, se va încerca din nou în 10 secunde.");
    }
}
