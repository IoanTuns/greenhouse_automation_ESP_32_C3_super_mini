#include "sensors.h"
#include "globals.h"
#include "rtc_module.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <Wire.h>
#include <string.h>
#include "config_hardware.h"
#include "config_system.h"
#include "app_settings.h"

static OneWire oneWire(PIN_TEMP_SENSORS);
static DallasTemperature ds18b20(&oneWire);
static DHT dht(PIN_DHT22, DHT22);
// BMP180 shares the RTC's I2C bus (GPIO7 SDA / GPIO6 SCL) — fixed address 0x77,
// no conflict with the DS3231 (0x68). Wire is already initialized by initRTC(),
// which runs before initSensors() in main.cpp's setup().
static Adafruit_BMP085 bmp;
static bool bmpPresent = false;

// Each DS18B20 has a unique factory ROM address — binding readings to these
// (instead of the array index from getDeviceCount()) keeps Soil/Water readings
// stable across reboots and rescans regardless of enumeration order. The
// mapping itself (addrSoil/addrWater, in app_settings) is user-editable from
// /config, so replacing a physical sensor doesn't require a reflash.
static DeviceAddress cachedAddrSoil;
static DeviceAddress cachedAddrWater;

static String addressToHex(const DeviceAddress& addr) {
    char buf[17];
    for (int i = 0; i < 8; i++) snprintf(buf + i * 2, 3, "%02X", addr[i]);
    return String(buf);
}

static bool hexToAddress(const String& hex, DeviceAddress& out) {
    if (hex.length() != 16) return false;
    for (int i = 0; i < 8; i++) {
        out[i] = (uint8_t)strtoul(hex.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
    }
    return true;
}

void applySensorMapping() {
    if (addrSoil.length() == 0) {
        // Unassigned — zero the cached address so getTempC() matches no real
        // device (every DS18B20 ROM starts with a nonzero family code) and
        // correctly reports "disconnected" instead of silently continuing to
        // read whatever sensor was previously mapped here.
        memset(cachedAddrSoil, 0, sizeof(DeviceAddress));
    } else if (!hexToAddress(addrSoil, cachedAddrSoil)) {
        Serial.printf("[CONFIG] Invalid Soil sensor address '%s', keeping previous mapping.\n", addrSoil.c_str());
    }
    if (addrWater.length() == 0) {
        memset(cachedAddrWater, 0, sizeof(DeviceAddress));
    } else if (!hexToAddress(addrWater, cachedAddrWater)) {
        Serial.printf("[CONFIG] Invalid Water sensor address '%s', keeping previous mapping.\n", addrWater.c_str());
    }
}

int getDS18B20Count() {
    return ds18b20.getDeviceCount();
}

String getDS18B20AddressAt(int index) {
    DeviceAddress addr;
    if (!ds18b20.getAddress(addr, index)) return "";
    return addressToHex(addr);
}

static unsigned long lastSensorRead = 0;
static const unsigned long SENSOR_INTERVAL = 5000;

void initSensors() {
    applySensorMapping();

    ds18b20.begin();
    int deviceCount = ds18b20.getDeviceCount();
    if (deviceCount == 0) {
        Serial.printf("[ERROR] DS18B20: No sensors on pin %d. Check wiring + 4.7k pull-up to 3.3V.\n", PIN_TEMP_SENSORS);
        uint8_t presence = oneWire.reset();
        Serial.printf("[DIAG] OneWire raw reset() presence pulse: %s\n",
                      presence ? "YES - a device IS answering electrically; ROM search/CRC is the problem"
                               : "NO - bus is electrically silent; wiring/pull-up/power issue, not a library bug");
    } else {
        statusDS18B20 = "OK (" + String(deviceCount) + " sensors)";
        Serial.printf("[OK] DS18B20: Found %d sensor(s) on pin %d.\n", deviceCount, PIN_TEMP_SENSORS);
        DeviceAddress addr;
        for (int i = 0; i < deviceCount; i++) {
            if (ds18b20.getAddress(addr, i)) {
                Serial.printf("[OK] DS18B20[%d] address: ", i);
                for (uint8_t b = 0; b < 8; b++) Serial.printf("%02X", addr[b]);
                if (memcmp(addr, cachedAddrSoil, sizeof(DeviceAddress)) == 0) Serial.println(" -> SOIL");
                else if (memcmp(addr, cachedAddrWater, sizeof(DeviceAddress)) == 0) Serial.println(" -> WATER");
                else Serial.println(" -> UNASSIGNED (map it on /config)");
            }
        }
    }

    dht.begin();
    statusDHT22 = "Initializing...";
    Serial.println("[OK] DHT22: Initialized. First reading will happen in loop().");

    // The RTC's last read just before this point can leave the ESP32-C3's I2C
    // bus in a state where the next transaction NACKs (see rtc_module.cpp).
    // Recover before BMP180's own I2C traffic starts, or begin() spuriously fails.
    recoverI2CBus();
    bmpPresent = bmp.begin();
    if (bmpPresent) {
        statusBMP180 = "OK";
        Serial.println("[OK] BMP180: Initialized on I2C bus (address 0x77).");
    } else {
        statusBMP180 = "Not detected";
        Serial.println("[ERROR] BMP180: Not responding at I2C address 0x77. Check wiring — it shares the RTC's SDA/SCL bus.");
        Serial.println("[DIAG] Running I2C bus scan to check what IS responding (RTC should show as 0x68 if the bus itself is healthy):");
        scanI2CBus();

        // bmp.begin() requires chip-ID register 0xD0 to read exactly 0x55 (BMP085/BMP180).
        // If the address ACKs but begin() still fails, this tells us what's actually there:
        // 0x58 = BMP280, 0x60 = BME280 — both commonly mislabeled/sold as "BMP180" modules.
        Wire.beginTransmission(0x77);
        Wire.write(0xD0);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)0x77, (uint8_t)1) == 1) {
            uint8_t chipId = Wire.read();
            const char* guess = (chipId == 0x55) ? "BMP085/BMP180 (should have worked!)" :
                                (chipId == 0x58) ? "BMP280 — wrong library, needs Adafruit_BMP280" :
                                (chipId == 0x60) ? "BME280 — wrong library, needs Adafruit_BME280" :
                                "unrecognized chip ID";
            Serial.printf("[DIAG] Chip ID at 0x77 reg 0xD0 = 0x%02X -> %s\n", chipId, guess);
        } else {
            Serial.println("[DIAG] Could not read chip-ID register 0xD0 from 0x77.");
        }
    }
}

void sensorsLoop() {
    if (millis() - lastSensorRead < SENSOR_INTERVAL) return;
    lastSensorRead = millis();

    // Re-scan if either mapped sensor is missing — either none were found at
    // all, or a specific assigned address (Soil/Water) doesn't respond even
    // though the bus has *some* device on it (e.g. only 1 of 2 sensors came
    // up at boot). Checking count==0 alone missed that second case entirely.
    // Throttled to once per 30 s.
    bool soilMissing = addrSoil.length() > 0 && !ds18b20.isConnected(cachedAddrSoil);
    bool waterMissing = addrWater.length() > 0 && !ds18b20.isConnected(cachedAddrWater);
    if (ds18b20.getDeviceCount() == 0 || soilMissing || waterMissing) {
        static uint32_t lastRescan = 0xFFFFFF00UL;
        if (millis() - lastRescan >= 30000UL) {
            lastRescan = millis();
            ds18b20.begin();
            int n = ds18b20.getDeviceCount();
            if (n > 0) {
                statusDS18B20 = "OK (" + String(n) + " sensors)";
                Serial.printf("[OK] DS18B20: Found %d sensor(s) on rescan (pin %d).\n", n, PIN_TEMP_SENSORS);
            } else {
                Serial.printf("[ERROR] DS18B20: Still no sensors on pin %d. Check wiring + 4.7k pull-up to 3.3V.\n", PIN_TEMP_SENSORS);
                uint8_t presence = oneWire.reset();
                Serial.printf("[DIAG] OneWire raw reset() presence pulse: %s\n",
                              presence ? "YES - a device IS answering electrically; ROM search/CRC is the problem"
                                       : "NO - bus is electrically silent; wiring/pull-up/power issue, not a library bug");
            }
        }
    }

    ds18b20.requestTemperatures();

    float rawSoil = ds18b20.getTempC(cachedAddrSoil);
    if (rawSoil == DEVICE_DISCONNECTED_C) {
        static uint32_t lastSoilErr = 0xFFFFFF00UL;
        if (millis() - lastSoilErr >= 10000UL) {
            lastSoilErr = millis();
            Serial.println("[ERROR] DS18B20: Soil sensor not responding. Check wiring, or re-map it on /config if it was replaced.");
        }
        tempSoil = NAN;
    } else {
        tempSoil = rawSoil;
    }

    float rawWater = ds18b20.getTempC(cachedAddrWater);
    if (rawWater == DEVICE_DISCONNECTED_C) {
        static uint32_t lastWaterErr = 0xFFFFFF00UL;
        if (millis() - lastWaterErr >= 10000UL) {
            lastWaterErr = millis();
            Serial.println("[ERROR] DS18B20: Water sensor not responding. Check wiring, or re-map it on /config if it was replaced.");
        }
        tempWater = NAN;
    } else {
        tempWater = rawWater;
    }

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
        humidityAir = h;
        tempAir = t;
        statusDHT22 = "OK";
    } else {
        statusDHT22 = "Read error";
    }

    if (!bmpPresent) {
        // Retry every 10s if it wasn't detected at boot — bmp.begin() has no
        // automatic retry of its own, so a transient failure (e.g. the same
        // I2C-bus-lock timing issue worked around elsewhere in this file)
        // would otherwise leave it "Not detected" forever until a reboot.
        static uint32_t lastBmpRetry = 0xFFFFFF00UL;
        if (millis() - lastBmpRetry >= 10000UL) {
            lastBmpRetry = millis();
            recoverI2CBus();
            bmpPresent = bmp.begin();
            if (bmpPresent) {
                statusBMP180 = "OK";
                Serial.println("[OK] BMP180: Detected on retry.");
            } else {
                Serial.println("[ERROR] BMP180: Still not detected on retry, will try again in 10 seconds.");
            }
        }
    }

    if (bmpPresent) {
        // irrigationLoop() reads the RTC just before sensorsLoop() runs each
        // main-loop iteration, which leaves the bus locked for the next
        // transaction on this chip (same issue worked around in rtc_module.cpp).
        // Recover here too, or these reads intermittently fail every cycle.
        recoverI2CBus();
        pressure = bmp.readPressure() / 100.0f; // Pa -> hPa
        altitude = bmp.readAltitude(seaLevelPressurePa); // meters, relative to the calibrated local reference
        tempBaro = bmp.readTemperature();       // BMP180's own sensor, separate from tempAir (DHT22)
    } else {
        pressure = NAN;
        altitude = NAN;
        tempBaro = NAN;
    }

    uint32_t pulseZ1, pulseZ2;
    noInterrupts();
    pulseZ1 = pulsesZ1;
    pulseZ2 = pulsesZ2;
    interrupts();

    if (rtcAvailable) {
        DateTime now = readRTC();
        Serial.printf("[%02d:%02d:%02d] Soil:%.1f°C | Water:%.1f°C | Air:%.1f°C %.0f%% | Baro:%.1f°C %.1fhPa | Z1:%.2fL Z2:%.2fL\n",
            now.hour(), now.minute(), now.second(),
            tempSoil, tempWater, tempAir, humidityAir, tempBaro, pressure,
            (float)pulseZ1 / pulsesPerLiter,
            (float)pulseZ2 / pulsesPerLiter);
    } else {
        Serial.printf("[--:--:--] Soil:%.1f°C | Water:%.1f°C | Air:%.1f°C %.0f%% | Baro:%.1f°C %.1fhPa\n", tempSoil, tempWater, tempAir, humidityAir, tempBaro, pressure);
    }
}

bool isBmp180Present() { return bmpPresent; }
