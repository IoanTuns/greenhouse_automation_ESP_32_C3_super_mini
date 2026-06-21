#include "sensors.h"
#include "globals.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include "config_hardware.h"
#include "config_sistem.h"

static OneWire oneWire(PIN_Senzori_Temp);
static DallasTemperature ds18b20(&oneWire);
static DHT dht(PIN_DHT22, DHT22);

static unsigned long lastSensorRead = 0;
static const unsigned long SENSOR_INTERVAL = 5000;

void initSensors() {
    ds18b20.begin();
    int deviceCount = ds18b20.getDeviceCount();
    if (deviceCount == 0) {
        Serial.println("[EROARE] DS18B20: Nu au fost detectați senzori pe magistrala One-Wire.");
    } else {
        statusDS18B20 = "OK (" + String(deviceCount) + " senzori)";
        Serial.printf("[OK] DS18B20: Am găsit %d senzori pe pinul %d.\n", deviceCount, PIN_Senzori_Temp);
    }

    dht.begin();
    statusDHT22 = "Initializare...";
    Serial.println("[OK] DHT22: Inițializat. Prima citire va fi efectuată în loop().");
}

void sensorsLoop() {
    if (millis() - lastSensorRead < SENSOR_INTERVAL) return;
    lastSensorRead = millis();

    ds18b20.requestTemperatures();
    tempSol = ds18b20.getTempCByIndex(0);

    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(h) && !isnan(t)) {
        umidAer = h;
        tempAer = t;
        statusDHT22 = "OK";
    } else {
        statusDHT22 = "Eroare citire";
    }

    uint32_t pulseZ1, pulseZ2;
    noInterrupts();
    pulseZ1 = impulsuriZ1;
    pulseZ2 = impulsuriZ2;
    interrupts();

    if (rtcAvailable) {
        DateTime now = rtc.now();
        Serial.printf("[%02d:%02d:%02d] Sol:%.1f°C | Aer:%.1f°C %.0f%% | Z1:%.2fL Z2:%.2fL\n",
            now.hour(), now.minute(), now.second(),
            tempSol, tempAer, umidAer,
            (float)pulseZ1 / IMPULSURI_PER_LITRU,
            (float)pulseZ2 / IMPULSURI_PER_LITRU);
    } else {
        Serial.printf("[--:--:--] Sol:%.1f°C | Aer:%.1f°C %.0f%%\n", tempSol, tempAer, umidAer);
    }
}
