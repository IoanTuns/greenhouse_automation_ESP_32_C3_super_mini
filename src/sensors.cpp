#include "sensors.h"
#include "globals.h"
#include "rtc_module.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include "config_hardware.h"
#include "config_system.h"

static OneWire oneWire(PIN_TEMP_SENSORS);
static DallasTemperature ds18b20(&oneWire);
static DHT dht(PIN_DHT22, DHT22);

static unsigned long lastSensorRead = 0;
static const unsigned long SENSOR_INTERVAL = 5000;

void initSensors() {
    ds18b20.begin();
    int deviceCount = ds18b20.getDeviceCount();
    if (deviceCount == 0) {
        Serial.println("[ERROR] DS18B20: No sensors detected on the One-Wire bus.");
    } else {
        statusDS18B20 = "OK (" + String(deviceCount) + " sensors)";
        Serial.printf("[OK] DS18B20: Found %d sensors on pin %d.\n", deviceCount, PIN_TEMP_SENSORS);
    }

    dht.begin();
    statusDHT22 = "Initializing...";
    Serial.println("[OK] DHT22: Initialized. First reading will happen in loop().");
}

void sensorsLoop() {
    if (millis() - lastSensorRead < SENSOR_INTERVAL) return;
    lastSensorRead = millis();

    ds18b20.requestTemperatures();
    tempSoil = ds18b20.getTempCByIndex(0);

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
        humidityAir = h;
        tempAir = t;
        statusDHT22 = "OK";
    } else {
        statusDHT22 = "Read error";
    }

    uint32_t pulseZ1, pulseZ2;
    noInterrupts();
    pulseZ1 = pulsesZ1;
    pulseZ2 = pulsesZ2;
    interrupts();

    if (rtcAvailable) {
        DateTime now = readRTC();
        Serial.printf("[%02d:%02d:%02d] Soil:%.1f°C | Air:%.1f°C %.0f%% | Z1:%.2fL Z2:%.2fL\n",
            now.hour(), now.minute(), now.second(),
            tempSoil, tempAir, humidityAir,
            (float)pulseZ1 / PULSES_PER_LITER,
            (float)pulseZ2 / PULSES_PER_LITER);
    } else {
        Serial.printf("[--:--:--] Soil:%.1f°C | Air:%.1f°C %.0f%%\n", tempSoil, tempAir, humidityAir);
    }
}
