#include "app_settings.h"
#include "config_system.h"
#include <Preferences.h>

uint8_t startHourZ1 = DEFAULT_START_HOUR_Z1;
uint8_t startMinuteZ1 = DEFAULT_START_MINUTE_Z1;
uint8_t startHourZ2 = DEFAULT_START_HOUR_Z2;
uint8_t startMinuteZ2 = DEFAULT_START_MINUTE_Z2;
bool enabledZ1 = DEFAULT_ZONE1_ENABLED;
bool enabledZ2 = DEFAULT_ZONE2_ENABLED;
float targetVolumeZ1 = DEFAULT_TARGET_VOLUME_Z1;
float targetVolumeZ2 = DEFAULT_TARGET_VOLUME_Z2;
float maxZoneMinutes = DEFAULT_MAX_ZONE_MINUTES;
float pulsesPerLiter = DEFAULT_PULSES_PER_LITER;
float seaLevelPressurePa = DEFAULT_SEA_LEVEL_PRESSURE_PA;
String addrSoil = DEFAULT_ADDR_SOIL;
String addrWater = DEFAULT_ADDR_WATER;
String lastWateredZ1 = "Never";
float lastVolumeZ1 = 0;
String lastWateredZ2 = "Never";
float lastVolumeZ2 = 0;

static Preferences prefs;

void loadAppSettings() {
    prefs.begin("ghouse", true);
    startHourZ1 = prefs.getUChar("startHZ1", DEFAULT_START_HOUR_Z1);
    startMinuteZ1 = prefs.getUChar("startMZ1", DEFAULT_START_MINUTE_Z1);
    startHourZ2 = prefs.getUChar("startHZ2", DEFAULT_START_HOUR_Z2);
    startMinuteZ2 = prefs.getUChar("startMZ2", DEFAULT_START_MINUTE_Z2);
    enabledZ1 = prefs.getBool("enZ1", DEFAULT_ZONE1_ENABLED);
    enabledZ2 = prefs.getBool("enZ2", DEFAULT_ZONE2_ENABLED);
    targetVolumeZ1 = prefs.getFloat("volZ1", DEFAULT_TARGET_VOLUME_Z1);
    targetVolumeZ2 = prefs.getFloat("volZ2", DEFAULT_TARGET_VOLUME_Z2);
    maxZoneMinutes = prefs.getFloat("maxZoneMin", DEFAULT_MAX_ZONE_MINUTES);
    pulsesPerLiter = prefs.getFloat("pplitr", DEFAULT_PULSES_PER_LITER);
    seaLevelPressurePa = prefs.getFloat("seaPa", DEFAULT_SEA_LEVEL_PRESSURE_PA);
    addrSoil = prefs.getString("addrSoil", DEFAULT_ADDR_SOIL);
    addrWater = prefs.getString("addrWater", DEFAULT_ADDR_WATER);
    lastWateredZ1 = prefs.getString("lastWaterZ1", "Never");
    lastVolumeZ1 = prefs.getFloat("lastVolZ1", 0);
    lastWateredZ2 = prefs.getString("lastWaterZ2", "Never");
    lastVolumeZ2 = prefs.getFloat("lastVolZ2", 0);
    prefs.end();
    Serial.printf("[CONFIG] Loaded settings: Z1 %s start %02u:%02u (%.1fL), Z2 %s start %02u:%02u (%.1fL), max %.0fmin/zone, %.1f pulses/L, sea level %.1fPa\n",
                  enabledZ1 ? "enabled" : "DISABLED", startHourZ1, startMinuteZ1, targetVolumeZ1,
                  enabledZ2 ? "enabled" : "DISABLED", startHourZ2, startMinuteZ2, targetVolumeZ2, maxZoneMinutes, pulsesPerLiter, seaLevelPressurePa);
    Serial.printf("[CONFIG] DS18B20 mapping: Soil=%s Water=%s\n", addrSoil.c_str(), addrWater.c_str());
}

void saveAppSettings() {
    prefs.begin("ghouse", false);
    prefs.putUChar("startHZ1", startHourZ1);
    prefs.putUChar("startMZ1", startMinuteZ1);
    prefs.putUChar("startHZ2", startHourZ2);
    prefs.putUChar("startMZ2", startMinuteZ2);
    prefs.putBool("enZ1", enabledZ1);
    prefs.putBool("enZ2", enabledZ2);
    prefs.putFloat("volZ1", targetVolumeZ1);
    prefs.putFloat("volZ2", targetVolumeZ2);
    prefs.putFloat("maxZoneMin", maxZoneMinutes);
    prefs.putFloat("pplitr", pulsesPerLiter);
    prefs.putFloat("seaPa", seaLevelPressurePa);
    prefs.end();
    Serial.printf("[CONFIG] Saved settings: Z1 %s start %02u:%02u (%.1fL), Z2 %s start %02u:%02u (%.1fL), max %.0fmin/zone, %.1f pulses/L, sea level %.1fPa\n",
                  enabledZ1 ? "enabled" : "DISABLED", startHourZ1, startMinuteZ1, targetVolumeZ1,
                  enabledZ2 ? "enabled" : "DISABLED", startHourZ2, startMinuteZ2, targetVolumeZ2, maxZoneMinutes, pulsesPerLiter, seaLevelPressurePa);
}

void saveSensorMapping(const String& soilAddr, const String& waterAddr) {
    addrSoil = soilAddr;
    addrWater = waterAddr;
    prefs.begin("ghouse", false);
    prefs.putString("addrSoil", addrSoil);
    prefs.putString("addrWater", addrWater);
    prefs.end();
    Serial.printf("[CONFIG] Saved DS18B20 mapping: Soil=%s Water=%s\n", addrSoil.c_str(), addrWater.c_str());
}

void saveLastWateredZ1(const String& timestamp, float volume) {
    lastWateredZ1 = timestamp;
    lastVolumeZ1 = volume;
    prefs.begin("ghouse", false);
    prefs.putString("lastWaterZ1", lastWateredZ1);
    prefs.putFloat("lastVolZ1", lastVolumeZ1);
    prefs.end();
    Serial.printf("[IRRIGATION] Recorded last watered Zone 1: %s (%.2fL)\n", lastWateredZ1.c_str(), lastVolumeZ1);
}

void saveLastWateredZ2(const String& timestamp, float volume) {
    lastWateredZ2 = timestamp;
    lastVolumeZ2 = volume;
    prefs.begin("ghouse", false);
    prefs.putString("lastWaterZ2", lastWateredZ2);
    prefs.putFloat("lastVolZ2", lastVolumeZ2);
    prefs.end();
    Serial.printf("[IRRIGATION] Recorded last watered Zone 2: %s (%.2fL)\n", lastWateredZ2.c_str(), lastVolumeZ2);
}
