#include <Arduino.h>
#include "globals.h"
#include "rtc_module.h"
#include "sensors.h"
#include "irrigation.h"
#include "sd_logger.h"
#include "web_server.h"

void setup() {
    // Relays OFF first — relay module's 5V pull-ups are on GPIO 8/9; driving them HIGH
    // (RELAY_OFF) immediately prevents 5V from backfeeding into the 3.3V GPIO inputs.
    initRelays();

    Serial.begin(115200);
    { unsigned long t = millis(); while (!Serial && millis() - t < 3000) delay(10); }
    Serial.println("\n--- Smart Greenhouse Automation System ---");

    initWebServer();
    rtcAvailable = initRTC();
    initSD();
    initSensors();
    initIrrigation();

    Serial.println("--- Initialization Complete ---\n");
}

void loop() {
    webServerLoop();
    rtcRetryLoop();
    irrigationLoop();
    sensorsLoop();
    sdLoggerLoop();
}
