#pragma once
#include <RTClib.h>

void scanI2CBus();

// Resets the ESP32-C3 I2C peripheral and drains the dead zone that follows
// Wire.begin(), and clears the "next transaction NACKs" state that a prior
// requestFrom() read leaves behind on this chip. Call before any I2C
// transaction that follows other I2C activity (e.g. another peripheral's
// init right after the RTC has done a read) to avoid a spurious failure.
void recoverI2CBus();

bool initRTC();
void rtcRetryLoop();
void syncTimeManual(const DateTime& dt);  // set time from web UI or other source

// Read current time from DS3231 via stop+start (avoids iicWriteReadNonStop).
// Returns DateTime(2000,1,1,0,0,0) on bus error.
DateTime readRTC();
