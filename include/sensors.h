#pragma once
#include <Arduino.h>

void initSensors();
void sensorsLoop();

// True once the BMP180 responds on the I2C bus (fixed address 0x77).
bool isBmp180Present();

// Live enumeration of DS18B20 sensors currently on the One-Wire bus, for the
// sensor-mapping UI on /config — lets a replaced sensor (new ROM address) be
// reassigned to Soil/Water without recompiling.
int getDS18B20Count();
String getDS18B20AddressAt(int index); // 16-char hex ROM address, no colons

// Re-reads addrSoil/addrWater (app_settings.h) and updates which physical
// sensor each role reads from. Call once at boot and again after saving a
// new mapping from the web UI, so the change takes effect immediately.
void applySensorMapping();
