#pragma once
#include <Arduino.h>

// Each zone has its own independent daily start time — they never run
// concurrently (one shared pump feeds both valves), but each is scheduled
// on its own, not chained together as one combined cycle.
extern uint8_t startHourZ1;
extern uint8_t startMinuteZ1;
extern uint8_t startHourZ2;
extern uint8_t startMinuteZ2;
// When false, that zone runs neither on its schedule nor via its manual
// "Water Now" button — Stop Now and the other zone are unaffected.
extern bool enabledZ1;
extern bool enabledZ2;
extern float targetVolumeZ1;
extern float targetVolumeZ2;
extern float maxZoneMinutes;    // safety cutoff: force-stop a zone if it runs this long without reaching its target
extern float pulsesPerLiter;    // flow meter calibration: pulses counted per liter
extern float seaLevelPressurePa; // BMP180 altitude reference, in Pa (e.g. 101325 = standard atmosphere)
extern String addrSoil;         // DS18B20 ROM address (16 hex chars) currently mapped to Soil
extern String addrWater;        // DS18B20 ROM address (16 hex chars) currently mapped to Water

// Record of each zone's last successfully completed run — tracked
// independently since the zones no longer complete as one combined cycle.
// "Never" / 0 until that zone's first-ever completion.
extern String lastWateredZ1;
extern float lastVolumeZ1;
extern String lastWateredZ2;
extern float lastVolumeZ2;

void loadAppSettings();
void saveAppSettings();
void saveSensorMapping(const String& soilAddr, const String& waterAddr);
void saveLastWateredZ1(const String& timestamp, float volume);
void saveLastWateredZ2(const String& timestamp, float volume);
