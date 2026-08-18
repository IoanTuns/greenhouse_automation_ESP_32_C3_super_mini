#include "sd_logger.h"
#include "globals.h"
#include "rtc_module.h"
#include <SPI.h>
#include <SD.h>
#include "config_hardware.h"
#include "config_system.h"
#include "app_settings.h"

static unsigned long lastSDLog = 0;
static const unsigned long SD_LOG_INTERVAL = 60000;

static const char* CSV_PATH = "/date_solar.csv";
// Semicolon-separated with comma decimals — the convention Excel under a
// Romanian (or most European) locale expects natively on a plain double-click.
// Using a period as the decimal separator caused values like "24.60" to be
// silently misread as a date ("24 iun") whenever the digits after the point
// happened to look like a valid month — commas can't collide with dates
// (which use periods, DD.MM.YYYY), so this removes the ambiguity entirely.
static const char* CSV_HEADER = "Date;Time;State;Soil_Temp_C;Water_Temp_C;Air_Temp_C;Humidity_%;Baro_Temp_C;Pressure_hPa;Altitude_m;Volume_Z1_L;Volume_Z2_L";

// Formats a numeric value using ',' as the decimal separator, or "" (a blank
// cell) instead of the literal text "nan" when a sensor reading is invalid —
// blank cells behave correctly in spreadsheet formulas/averages, "nan" text doesn't.
static String fmtNum(float v, uint8_t decimals) {
    if (isnan(v)) return "";
    String s = String(v, (unsigned int)decimals); // explicit cast: uint8_t is otherwise
                                                    // ambiguous between String's float/uint overloads
    s.replace('.', ',');
    return s;
}

// If a previous firmware version left a CSV with a different set of columns,
// appending new rows under the current header would misalign old vs. new
// rows in a spreadsheet. Detect a header mismatch once at boot and archive
// the old file under a dated name instead, so /date_solar.csv always has a
// header that matches every row beneath it.
static void ensureCsvHeader() {
    if (!SD.exists(CSV_PATH)) return; // nothing to migrate; first write creates it fresh

    File f = SD.open(CSV_PATH, FILE_READ);
    if (!f) return;
    String firstLine = f.readStringUntil('\n');
    firstLine.trim();
    f.close();
    if (firstLine == CSV_HEADER) return; // already current schema

    String archiveName = "/date_solar_prev.csv";
    if (rtcAvailable) {
        DateTime now = readRTC();
        char buf[32];
        snprintf(buf, sizeof(buf), "/date_solar_%04d%02d%02d.csv", now.year(), now.month(), now.day());
        archiveName = buf;
    }
    if (SD.exists(archiveName.c_str())) SD.remove(archiveName.c_str());
    if (SD.rename(CSV_PATH, archiveName.c_str())) {
        Serial.printf("[SD] CSV schema changed since this file was created — archived old data as %s, starting a fresh %s.\n",
                      archiveName.c_str(), CSV_PATH);
    } else {
        Serial.println("[ERROR] SD: Could not archive outdated CSV — new rows may not align with the existing header.");
    }
}

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
    ensureCsvHeader();
    return true;
}

static unsigned long lastSDRetry = 0;
static const unsigned long SD_RETRY_INTERVAL = 10000;

void sdRetryLoop() {
    if (statusSD == "OK" || millis() - lastSDRetry < SD_RETRY_INTERVAL) return;
    lastSDRetry = millis();
    if (initSD()) {
        Serial.println("[OK] SD: Retry succeeded.");
    } else {
        Serial.println("[ERROR] SD: Retry failed, will try again in 10 seconds.");
    }
}

void saveSDReport(float volZ1, float volZ2) {
    File f = SD.open(CSV_PATH, FILE_APPEND);
    if (!f) {
        Serial.println("[ERROR] SD: Could not open /date_solar.csv.");
        return;
    }
    if (f.size() == 0) {
        f.println(CSV_HEADER);
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
    String row = dateStr + ";" + timeStr + ";COMPLETED;" +
                 fmtNum(tempSoil, 2) + ";" + fmtNum(tempWater, 2) + ";" + fmtNum(tempAir, 2) + ";" +
                 fmtNum(humidityAir, 1) + ";" + fmtNum(tempBaro, 2) + ";" + fmtNum(pressure, 1) + ";" +
                 fmtNum(altitude, 0) + ";" + fmtNum(volZ1, 2) + ";" + fmtNum(volZ2, 2);
    f.println(row);
    f.close();
    Serial.printf("[SD] Session report: %s %s | Z1=%.2fL Z2=%.2fL | Soil=%.1f°C Water=%.1f°C Air=%.1f°C Pressure=%.1fhPa\n",
                  dateStr.c_str(), timeStr.c_str(), volZ1, volZ2, tempSoil, tempWater, tempAir, pressure);
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

    File f = SD.open(CSV_PATH, FILE_APPEND);
    if (f) {
        if (f.size() == 0) {
            f.println(CSV_HEADER);
        }
        String row = dateStr + ";" + timeStr + ";" + String(stateStr) + ";" +
                     fmtNum(tempSoil, 2) + ";" + fmtNum(tempWater, 2) + ";" + fmtNum(tempAir, 2) + ";" +
                     fmtNum(humidityAir, 1) + ";" + fmtNum(tempBaro, 2) + ";" + fmtNum(pressure, 1) + ";" +
                     fmtNum(altitude, 0) + ";" +
                     fmtNum((float)pulseZ1 / pulsesPerLiter, 2) + ";" + fmtNum((float)pulseZ2 / pulsesPerLiter, 2);
        f.println(row);
        f.close();
    } else {
        Serial.println("[ERROR] SD: Could not write to /date_solar.csv.");
    }
}

// Only "date_solar*.csv" is exposed to the web UI's log-management page —
// covers the live file plus any archives ensureCsvHeader() created — never
// arbitrary files that might exist on the card for other purposes.
static bool isLogFile(const String& name) {
    return name.startsWith("date_solar") && name.endsWith(".csv");
}

static String baseName(const String& path) {
    int slash = path.lastIndexOf('/');
    return slash >= 0 ? path.substring(slash + 1) : path;
}

int getLogFileCount() {
    int count = 0;
    File root = SD.open("/");
    if (!root) return 0;
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        if (!f.isDirectory() && isLogFile(baseName(String(f.name())))) count++;
    }
    root.close();
    return count;
}

LogFileInfo getLogFileAt(int index) {
    LogFileInfo info{"", 0};
    File root = SD.open("/");
    if (!root) return info;
    int i = 0;
    for (File f = root.openNextFile(); f; f = root.openNextFile()) {
        String name = baseName(String(f.name()));
        if (f.isDirectory() || !isLogFile(name)) continue;
        if (i == index) {
            info.name = name;
            info.sizeBytes = f.size();
            break;
        }
        i++;
    }
    root.close();
    return info;
}

bool deleteLogFile(const String& name) {
    if (!isLogFile(name)) {
        Serial.printf("[SD] Refused to delete '%s' — not a recognized log file.\n", name.c_str());
        return false;
    }
    String path = "/" + name;
    bool ok = SD.remove(path.c_str());
    Serial.printf("[SD] Delete %s: %s\n", path.c_str(), ok ? "OK" : "FAILED");
    return ok;
}
