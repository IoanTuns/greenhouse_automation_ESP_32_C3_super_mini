#ifndef CONFIG_SISTEM_H
#define CONFIG_SISTEM_H

// --- Access Point (always created by the board) ---
#define WIFI_SSID     "greenhouse"   // Wi-Fi Access Point network name
#define WIFI_PASSWORD "!Green2024"   // Access Point password (minimum 8 characters)

// --- Local network (optional) — leave SSID empty to skip STA ---
#define WIFI_STA_SSID     ""         // Home router SSID
#define WIFI_STA_PASSWORD ""         // Home router password
#define WIFI_STA_TIMEOUT_MS 10000    // STA connection timeout (ms)

// NTP time sync (used when WiFi STA is connected)
#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET_SEC 0    // seconds east of UTC (e.g. 3600=UTC+1, 7200=UTC+2, 10800=UTC+3)

#define START_HOUR 8
#define START_MINUTE 0

#define TARGET_VOLUME_Z1 50.0
#define TARGET_VOLUME_Z2 50.0
#define PULSES_PER_LITER 450.0

#endif
