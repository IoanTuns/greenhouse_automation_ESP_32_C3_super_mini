#pragma once

void initRelays();      // call BEFORE Serial.begin — sets all relays OFF immediately
void initIrrigation();  // call AFTER Serial.begin — attaches flow meter interrupts
void irrigationLoop();
void stopAll();

// Starts a single zone independently — each has its own schedule (see
// app_settings.h) and can also be triggered manually from the web UI
// (/water-zone1, /water-zone2). No-op (returns false, just logs) if a cycle
// is already running in either zone — the shared pump means they can never
// run concurrently.
bool startZone1();
bool startZone2();
