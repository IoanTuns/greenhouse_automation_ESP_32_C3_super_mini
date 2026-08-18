#ifndef CONFIG_SYSTEM_H
#define CONFIG_SYSTEM_H

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

// Defaults used only the first time the board boots — after that, the live
// values are loaded from flash (NVS) and edited from the /config web page.
// Each zone has its own independent schedule (see AI_INSTRUCTIONS.md's State
// Machine section) — Z2 defaults 30 minutes after Z1 so a fresh board doesn't
// start with both zones aimed at the exact same minute.
#define DEFAULT_START_HOUR_Z1 8
#define DEFAULT_START_MINUTE_Z1 0
#define DEFAULT_START_HOUR_Z2 8
#define DEFAULT_START_MINUTE_Z2 30

// Both zones are on by default; either can be disabled independently from
// /config (e.g. a bed isn't planted yet, or a zone is under maintenance)
// without losing its saved schedule/volume/last-watered history.
#define DEFAULT_ZONE1_ENABLED true
#define DEFAULT_ZONE2_ENABLED true

#define DEFAULT_TARGET_VOLUME_Z1 50.0
#define DEFAULT_TARGET_VOLUME_Z2 50.0
#define DEFAULT_MAX_ZONE_MINUTES 30.0 // safety cutoff: stop a zone if its target volume isn't reached in this long (e.g. stuck flow meter/valve)
#define DEFAULT_PULSES_PER_LITER 450.0        // YF-S201 nominal spec; calibrate per-unit on /config
#define DEFAULT_SEA_LEVEL_PRESSURE_PA 101325.0 // standard atmosphere; set your local reference on /config for accurate altitude

// Which physical DS18B20 ROM address (16 hex chars, no colons) is Soil vs Water.
// Only used the first time the board boots — reassign from the /config page
// after that (e.g. when a sensor is physically replaced with a new unit that
// has a different factory address).
#define DEFAULT_ADDR_SOIL  "284AD27B00000045"
#define DEFAULT_ADDR_WATER "2845E07700000074"

#endif
