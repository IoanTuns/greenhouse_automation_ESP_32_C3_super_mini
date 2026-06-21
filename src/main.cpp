#include <Arduino.h>
#include "globals.h"
#include "rtc_module.h"
#include "sensors.h"
#include "irrigation.h"
#include "sd_logger.h"
#include "web_server.h"

void setup() {
    // Relays OFF before anything else — GPIO 8/9 strapping pins have pull-up at 5V on relay module
    initRelays();

    Serial.begin(115200);
    { unsigned long t = millis(); while (!Serial && millis() - t < 3000) delay(10); }
    Serial.println("\n--- Sistem Automatizare Solar Inteligent ---");

    initWebServer();
    rtcAvailable = initRTC();
    initSD();
    initSensors();
    initIrrigation();

    Serial.println("--- Inițializare Completă ---\n");
}

void loop() {
    webServerLoop();
    rtcRetryLoop();
    irrigationLoop();
    sensorsLoop();
    sdLoggerLoop();
}
