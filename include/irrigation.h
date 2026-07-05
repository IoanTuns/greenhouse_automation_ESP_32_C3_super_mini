#pragma once

void initRelays();      // call BEFORE Serial.begin — sets all relays OFF immediately
void initIrrigation();  // call AFTER Serial.begin — attaches flow meter interrupts
void irrigationLoop();
void stopAll();
