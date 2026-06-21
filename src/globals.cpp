#include "globals.h"

RTC_DS3231 rtc;
bool rtcAvailable = false;
unsigned long lastRtcRetry = 0;

float tempSol = NAN;
float tempAer = NAN;
float umidAer = NAN;

String statusRTC     = "Nedetectat";
String statusDS18B20 = "Nedetectat";
String statusDHT22   = "Nedetectat";
String statusSD      = "Nedetectat";

volatile uint32_t impulsuriZ1 = 0;
volatile uint32_t impulsuriZ2 = 0;

StareUdare stareCurenta = OPRIT;
