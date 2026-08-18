#include "activity_log.h"
#include "globals.h"
#include "rtc_module.h"
#include <SD.h>
#include <vector>

static const char* ACTIVITY_PATH = "/activity.csv";
static const char* ACTIVITY_HEADER = "Timestamp;Action";

void logActivity(const String& action) {
    if (statusSD != "OK") return; // no card — nothing to persist to; caller's own Serial print still happened

    String ts = "N/A";
    if (rtcAvailable) {
        DateTime now = readRTC();
        char buf[20];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                 now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
        ts = buf;
    }

    File f = SD.open(ACTIVITY_PATH, FILE_APPEND);
    if (!f) {
        Serial.println("[ACTIVITY] Could not open /activity.csv to log: " + action);
        return;
    }
    if (f.size() == 0) f.println(ACTIVITY_HEADER);
    f.println(ts + ";" + action);
    f.close();
    Serial.println("[ACTIVITY] " + ts + " - " + action);
}

// Reads the whole (small) file per call — fine for an audit log that only
// grows on human-initiated changes, not sensor telemetry every few seconds.
static std::vector<String> readEntries() {
    std::vector<String> lines;
    if (statusSD != "OK" || !SD.exists(ACTIVITY_PATH)) return lines;
    File f = SD.open(ACTIVITY_PATH, FILE_READ);
    if (!f) return lines;
    f.readStringUntil('\n'); // skip header
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) lines.push_back(line);
    }
    f.close();
    return lines;
}

int getActivityEntryCount() {
    return (int)readEntries().size();
}

String getActivityEntryAt(int index) {
    std::vector<String> lines = readEntries();
    int actualIndex = (int)lines.size() - 1 - index; // 0 = most recent
    if (actualIndex < 0 || actualIndex >= (int)lines.size()) return "";
    return lines[actualIndex];
}

bool clearActivityLog() {
    if (!SD.exists(ACTIVITY_PATH)) return true;
    bool ok = SD.remove(ACTIVITY_PATH);
    Serial.printf("[ACTIVITY] Clear log: %s\n", ok ? "OK" : "FAILED");
    return ok;
}
