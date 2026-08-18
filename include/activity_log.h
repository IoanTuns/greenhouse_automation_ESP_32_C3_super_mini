#pragma once
#include <Arduino.h>

// Lightweight audit trail of user-initiated changes made from the web UI
// (settings saved, devices renamed/forgotten, manual watering/stop/reboot
// triggered) — separate from the sensor-data CSV in sd_logger.h. Stored as
// /activity.csv on the SD card; silently skipped if the card isn't available.
void logActivity(const String& action);

int getActivityEntryCount();          // 0 if none / no SD
String getActivityEntryAt(int index); // 0 = most recent; full "timestamp;action" line
bool clearActivityLog();
