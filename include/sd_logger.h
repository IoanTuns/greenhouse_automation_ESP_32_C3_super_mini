#pragma once
#include <Arduino.h>

bool initSD();
void saveSDReport(float volZ1, float volZ2);
void sdLoggerLoop();

// Retries initSD() every 10s while statusSD != "OK" — without this, a card
// that failed at boot (or a card inserted after boot) would stay in an
// error state forever until a reboot. Call from the main loop().
void sdRetryLoop();

// Log file management for the web UI's /logs page. Deliberately scoped to
// only the CSV logs this project creates (date_solar*.csv) — not a general
// SD file browser — so it can't be used to touch unrelated files on the card.
struct LogFileInfo {
    String name; // basename only, e.g. "date_solar.csv" (no leading slash)
    uint32_t sizeBytes;
};
int getLogFileCount();
LogFileInfo getLogFileAt(int index);
bool deleteLogFile(const String& name);
