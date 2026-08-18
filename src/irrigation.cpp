#include "irrigation.h"
#include "globals.h"
#include "rtc_module.h"
#include <Arduino.h>
#include "config_hardware.h"
#include "config_system.h"
#include "sd_logger.h"
#include "app_settings.h"

static unsigned long lastStartAttemptZ1 = 0;
static unsigned long lastStartAttemptZ2 = 0;
static unsigned long zoneStartMs = 0;

void IRAM_ATTR isrFlowZ1() { pulsesZ1++; }
void IRAM_ATTR isrFlowZ2() { pulsesZ2++; }

void stopAll() {
    digitalWrite(PIN_PUMP, RELAY_OFF);
    digitalWrite(PIN_VALVE_Z1, RELAY_OFF);
    digitalWrite(PIN_VALVE_Z2, RELAY_OFF);
    currentState = STOPPED;
    Serial.println("[SYSTEM] All relays are OFF.");
}

void initRelays() {
    pinMode(PIN_PUMP, OUTPUT);
    pinMode(PIN_VALVE_Z1, OUTPUT);
    pinMode(PIN_VALVE_Z2, OUTPUT);
    digitalWrite(PIN_PUMP, RELAY_OFF);
    digitalWrite(PIN_VALVE_Z1, RELAY_OFF);
    digitalWrite(PIN_VALVE_Z2, RELAY_OFF);
    currentState = STOPPED;
    // Serial not yet started — no print here intentionally
}

void initIrrigation() {
    pinMode(PIN_FLOW_Z1, INPUT_PULLUP);
    pinMode(PIN_FLOW_Z2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_FLOW_Z1), isrFlowZ1, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_FLOW_Z2), isrFlowZ2, RISING);
    Serial.println("[OK] Relays: Initialized in OFF state.");
    Serial.println("[OK] Flow meters: Interrupts enabled (GPIO 20, 21).");
}

bool startZone1() {
    if (!enabledZ1) {
        Serial.println("[IRRIGATION] Zone 1 start ignored — zone is disabled on /config.");
        return false;
    }
    if (currentState != STOPPED) {
        Serial.println("[IRRIGATION] Zone 1 start ignored — a cycle is already running (shared pump).");
        return false;
    }
    Serial.println("[IRRIGATION] Starting Zone 1.");
    noInterrupts();
    pulsesZ1 = 0;
    interrupts();
    digitalWrite(PIN_VALVE_Z2, RELAY_OFF);
    digitalWrite(PIN_VALVE_Z1, RELAY_ON);
    digitalWrite(PIN_PUMP, RELAY_ON);
    currentState = WATERING_ZONE_1;
    lastStartAttemptZ1 = millis();
    zoneStartMs = millis();
    irrigationFault = false;
    irrigationFaultMsg = "";
    return true;
}

bool startZone2() {
    if (!enabledZ2) {
        Serial.println("[IRRIGATION] Zone 2 start ignored — zone is disabled on /config.");
        return false;
    }
    if (currentState != STOPPED) {
        Serial.println("[IRRIGATION] Zone 2 start ignored — a cycle is already running (shared pump).");
        return false;
    }
    Serial.println("[IRRIGATION] Starting Zone 2.");
    noInterrupts();
    pulsesZ2 = 0;
    interrupts();
    digitalWrite(PIN_VALVE_Z1, RELAY_OFF);
    digitalWrite(PIN_VALVE_Z2, RELAY_ON);
    digitalWrite(PIN_PUMP, RELAY_ON);
    currentState = WATERING_ZONE_2;
    lastStartAttemptZ2 = millis();
    zoneStartMs = millis();
    irrigationFault = false;
    irrigationFaultMsg = "";
    return true;
}

static void failZone(const char* zoneName) {
    irrigationFault = true;
    irrigationFaultMsg = String(zoneName) + " timed out after " + String(maxZoneMinutes, 0) +
                          " min without reaching its target volume — check the flow meter and valve wiring.";
    Serial.println("[SAFETY] " + irrigationFaultMsg);
    stopAll();
}

static String nowTimestamp() {
    if (!rtcAvailable) return "";
    DateTime now = readRTC();
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    return String(buf);
}

void irrigationLoop() {
    uint32_t currentPulseZ1, currentPulseZ2;
    noInterrupts();
    currentPulseZ1 = pulsesZ1;
    currentPulseZ2 = pulsesZ2;
    interrupts();

    DateTime now(2000, 1, 1, 0, 0, 0);
    if (rtcAvailable) now = readRTC();

    unsigned long maxZoneMs = (unsigned long)(maxZoneMinutes * 60000.0f);

    switch (currentState) {
        case STOPPED:
            // Each zone has its own independent schedule. If both happen to
            // land on the same STOPPED tick, Zone 1 takes priority this cycle
            // — Zone 2's own 60s dedupe window means it isn't lost, just
            // deferred to the next loop iteration after Zone 1 finishes (by
            // which point its own minute has usually passed, so in practice
            // treat exact-same-minute schedules as "Zone 1 wins that day").
            // Checking enabledZ1/enabledZ2 here (not just inside startZone1/2)
            // matters: without it, a disabled zone whose minute matches would
            // call startZone1()/startZone2() — and get refused — on every
            // single loop() iteration for the whole minute, since a refused
            // call never updates lastStartAttemptZ1/2 to open the 60s dedupe
            // window.
            if (enabledZ1 && rtcAvailable && now.hour() == startHourZ1 && now.minute() == startMinuteZ1
                    && millis() - lastStartAttemptZ1 >= 60000) {
                startZone1();
            } else if (enabledZ2 && rtcAvailable && now.hour() == startHourZ2 && now.minute() == startMinuteZ2
                    && millis() - lastStartAttemptZ2 >= 60000) {
                startZone2();
            }
            break;

        case WATERING_ZONE_1:
            if (millis() - zoneStartMs >= maxZoneMs) {
                failZone("Zone 1");
                break;
            }
            if (((float)currentPulseZ1 / pulsesPerLiter) >= targetVolumeZ1) {
                Serial.println("[AUTOMATION] Zone 1 complete.");
                float volZ1 = (float)currentPulseZ1 / pulsesPerLiter;
                saveSDReport(volZ1, 0); // Zone 2 didn't run in this session
                String ts = nowTimestamp();
                if (ts.length()) saveLastWateredZ1(ts, volZ1);
                stopAll();
            }
            break;

        case WATERING_ZONE_2:
            if (millis() - zoneStartMs >= maxZoneMs) {
                failZone("Zone 2");
                break;
            }
            if (((float)currentPulseZ2 / pulsesPerLiter) >= targetVolumeZ2) {
                Serial.println("[AUTOMATION] Zone 2 complete.");
                float volZ2 = (float)currentPulseZ2 / pulsesPerLiter;
                saveSDReport(0, volZ2); // Zone 1 didn't run in this session
                String ts = nowTimestamp();
                if (ts.length()) saveLastWateredZ2(ts, volZ2);
                stopAll();
            }
            break;
    }
}
