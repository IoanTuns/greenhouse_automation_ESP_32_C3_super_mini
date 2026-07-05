#pragma once
#include <Arduino.h>
#include <RTClib.h>

enum WateringState { STOPPED, WATERING_ZONE_1, WATERING_ZONE_2 };

extern RTC_DS3231 rtc;
extern bool rtcAvailable;
extern unsigned long lastRtcRetry;

extern float tempSoil;
extern float tempAir;
extern float humidityAir;

extern String statusRTC;
extern String statusDS18B20;
extern String statusDHT22;
extern String statusSD;

extern volatile uint32_t pulsesZ1;
extern volatile uint32_t pulsesZ2;

extern WateringState currentState;
