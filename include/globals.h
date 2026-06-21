#pragma once
#include <Arduino.h>
#include <RTClib.h>

enum StareUdare { OPRIT, UDARE_ZONA_1, UDARE_ZONA_2 };

extern RTC_DS3231 rtc;
extern bool rtcAvailable;
extern unsigned long lastRtcRetry;

extern float tempSol;
extern float tempAer;
extern float umidAer;

extern String statusRTC;
extern String statusDS18B20;
extern String statusDHT22;
extern String statusSD;

extern volatile uint32_t impulsuriZ1;
extern volatile uint32_t impulsuriZ2;

extern StareUdare stareCurenta;
