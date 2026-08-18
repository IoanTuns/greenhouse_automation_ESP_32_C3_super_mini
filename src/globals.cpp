#include "globals.h"

RTC_DS3231 rtc;
bool rtcAvailable = false;
unsigned long lastRtcRetry = 0;

float tempSoil = NAN;
float tempWater = NAN;
float tempAir = NAN;
float humidityAir = NAN;
float pressure = NAN;
float altitude = NAN;
float tempBaro = NAN;

String statusRTC     = "Not detected";
String statusDS18B20 = "Not detected";
String statusDHT22   = "Not detected";
String statusBMP180  = "Not detected";
String statusSD      = "Not detected";

volatile uint32_t pulsesZ1 = 0;
volatile uint32_t pulsesZ2 = 0;

WateringState currentState = STOPPED;

bool irrigationFault = false;
String irrigationFaultMsg = "";
