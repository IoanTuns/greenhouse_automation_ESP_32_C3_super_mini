#pragma once
#include <Arduino.h>
#include <RTClib.h>

enum WateringState { STOPPED, WATERING_ZONE_1, WATERING_ZONE_2 };

extern RTC_DS3231 rtc;
extern bool rtcAvailable;
extern unsigned long lastRtcRetry;

extern float tempSoil;
extern float tempWater;
extern float tempAir;
extern float humidityAir;
extern float pressure;
extern float altitude;
extern float tempBaro; // BMP180's own temperature reading (separate from the DHT22's tempAir)

extern String statusRTC;
extern String statusDS18B20;
extern String statusDHT22;
extern String statusBMP180;
extern String statusSD;

extern volatile uint32_t pulsesZ1;
extern volatile uint32_t pulsesZ2;

extern WateringState currentState;

// Set when irrigationLoop() force-stops a zone for exceeding its safety
// timeout (see maxZoneMinutes in app_settings.h) — surfaced on the dashboard
// until the next irrigation cycle starts (manual or scheduled).
extern bool irrigationFault;
extern String irrigationFaultMsg;
