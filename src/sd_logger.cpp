#include "sd_logger.h"
#include "globals.h"
#include "rtc_module.h"
#include <SPI.h>
#include <SD.h>
#include "config_hardware.h"
#include "config_system.h"

static unsigned long lastSDLog = 0;
static const unsigned long SD_LOG_INTERVAL = 60000;

bool initSD() {
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("[ERROR] SD: Card could not be initialized!");
        statusSD = "Error";
        return false;
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[OK] SD: Card detected (%llu MB).\n", cardSize);
    statusSD = "OK";
    return true;
}

void saveSDReport(float volZ1, float volZ2) {
    File f = SD.open("/date_solar.csv", FILE_APPEND);
    if (!f) {
        Serial.println("[ERROR] SD: Could not open /date_solar.csv.");
        return;
    }
    if (f.size() == 0) {
        f.println("Date,Time,State,Soil_Temp_C,Air_Temp_C,Humidity_%,Volume_Z1_L,Volume_Z2_L");
    }
    String dateStr = "N/A", timeStr = "N/A";
    if (rtcAvailable) {
        DateTime now = readRTC();
        char bufDate[11], bufTime[9];
        snprintf(bufDate, sizeof(bufDate), "%04d-%02d-%02d", now.year(), now.month(), now.day());
        snprintf(bufTime, sizeof(bufTime), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        dateStr = bufDate;
        timeStr = bufTime;
    }
    f.printf("%s,%s,COMPLETED,%.2f,%.2f,%.1f,%.2f,%.2f\n",
             dateStr.c_str(), timeStr.c_str(), tempSoil, tempAir, humidityAir, volZ1, volZ2);
    f.close();
    Serial.printf("[SD] Session report: %s %s | Z1=%.2fL Z2=%.2fL | Soil=%.1f°C Air=%.1f°C\n",
                  dateStr.c_str(), timeStr.c_str(), volZ1, volZ2, tempSoil, tempAir);
}

void sdLoggerLoop() {
    if (statusSD != "OK" || millis() - lastSDLog < SD_LOG_INTERVAL) return;
    lastSDLog = millis();

    String dateStr = "N/A", timeStr = "N/A";
    if (rtcAvailable) {
        DateTime now = readRTC();
        char bufDate[11], bufTime[9];
        snprintf(bufDate, sizeof(bufDate), "%04d-%02d-%02d", now.year(), now.month(), now.day());
        snprintf(bufTime, sizeof(bufTime), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        dateStr = bufDate;
        timeStr = bufTime;
    }
    const char* stateStr = (currentState == WATERING_ZONE_1) ? "ZONE_1" :
                           (currentState == WATERING_ZONE_2) ? "ZONE_2" : "STOPPED";

    uint32_t pulseZ1, pulseZ2;
    noInterrupts();
    pulseZ1 = pulsesZ1;
    pulseZ2 = pulsesZ2;
    interrupts();

    File f = SD.open("/date_solar.csv", FILE_APPEND);
    if (f) {
        if (f.size() == 0) {
            f.println("Date,Time,State,Soil_Temp_C,Air_Temp_C,Humidity_%,Volume_Z1_L,Volume_Z2_L");
        }
        f.printf("%s,%s,%s,%.2f,%.2f,%.1f,%.2f,%.2f\n",
            dateStr.c_str(), timeStr.c_str(), stateStr,
            tempSoil, tempAir, humidityAir,
            (float)pulseZ1 / PULSES_PER_LITER,
            (float)pulseZ2 / PULSES_PER_LITER);
        f.close();
    } else {
        Serial.println("[ERROR] SD: Could not write to /date_solar.csv.");
    }
}
