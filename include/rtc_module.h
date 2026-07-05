#pragma once
#include <RTClib.h>

void scanI2CBus();
bool initRTC();
void rtcRetryLoop();
void syncTimeManual(const DateTime& dt);  // set time from web UI or other source

// Read current time from DS3231 via stop+start (avoids iicWriteReadNonStop).
// Returns DateTime(2000,1,1,0,0,0) on bus error.
DateTime readRTC();
