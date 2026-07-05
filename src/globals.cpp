#include "globals.h"

RTC_DS3231 rtc;
bool rtcAvailable = false;
unsigned long lastRtcRetry = 0;

float tempSoil = NAN;
float tempAir = NAN;
float humidityAir = NAN;

String statusRTC     = "Not detected";
String statusDS18B20 = "Not detected";
String statusDHT22   = "Not detected";
String statusSD      = "Not detected";

volatile uint32_t pulsesZ1 = 0;
volatile uint32_t pulsesZ2 = 0;

WateringState currentState = STOPPED;
