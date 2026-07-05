#include "irrigation.h"
#include "globals.h"
#include "rtc_module.h"
#include <Arduino.h>
#include "config_hardware.h"
#include "config_system.h"
#include "sd_logger.h"

static unsigned long lastStartAttempt = 0;

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

void irrigationLoop() {
    uint32_t currentPulseZ1, currentPulseZ2;
    noInterrupts();
    currentPulseZ1 = pulsesZ1;
    currentPulseZ2 = pulsesZ2;
    interrupts();

    DateTime now(2000, 1, 1, 0, 0, 0);
    if (rtcAvailable) now = readRTC();

    switch (currentState) {
        case STOPPED:
            if (rtcAvailable && now.hour() == START_HOUR && now.minute() == START_MINUTE
                    && millis() - lastStartAttempt >= 60000) {
                Serial.println("[AUTOMATION] Starting watering schedule. Zone 1 active.");
                noInterrupts();
                pulsesZ1 = 0;
                interrupts();
                digitalWrite(PIN_VALVE_Z2, RELAY_OFF);
                digitalWrite(PIN_VALVE_Z1, RELAY_ON);
                digitalWrite(PIN_PUMP, RELAY_ON);
                currentState = WATERING_ZONE_1;
                lastStartAttempt = millis();
            }
            break;

        case WATERING_ZONE_1:
            if (((float)currentPulseZ1 / PULSES_PER_LITER) >= TARGET_VOLUME_Z1) {
                Serial.println("[AUTOMATION] Zone 1 complete, switching to Zone 2.");
                digitalWrite(PIN_VALVE_Z1, RELAY_OFF);
                noInterrupts();
                pulsesZ2 = 0;
                interrupts();
                digitalWrite(PIN_VALVE_Z2, RELAY_ON);
                currentState = WATERING_ZONE_2;
            }
            break;

        case WATERING_ZONE_2:
            if (((float)currentPulseZ2 / PULSES_PER_LITER) >= TARGET_VOLUME_Z2) {
                Serial.println("[AUTOMATION] Zone 2 complete, stopping everything.");
                saveSDReport(
                    (float)currentPulseZ1 / PULSES_PER_LITER,
                    (float)currentPulseZ2 / PULSES_PER_LITER
                );
                stopAll();
            }
            break;
    }
}
