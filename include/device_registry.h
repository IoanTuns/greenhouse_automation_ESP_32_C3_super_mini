#pragma once
#include <Arduino.h>

struct DeviceInfo {
    String mac;    // 12 hex chars, no colons, e.g. "AABBCCDDEEFF"
    String name;
    String type;
    String ip;     // dotted-quad IP while connected, "" when offline/unknown
    int8_t rssi;
    bool connectedNow;
};

// Pulls the current AP station list from the WiFi driver and merges any
// newly-seen MAC addresses into the persisted known-device list (flash/NVS).
// Call before reading device info so newly-connected devices show up.
void refreshConnectedDevices();

int getKnownDeviceCount();
DeviceInfo getKnownDevice(int index);
void saveDeviceInfo(const String& mac, const String& name, const String& type);
String macToDisplay(const String& macKey);

// Removes a device from the known list (and its saved name/type). Note: if
// the device is still actively connected to the AP, refreshConnectedDevices()
// will simply re-add it on the next call — this is meant for devices that
// no longer connect, not for kicking one off the network.
void forgetDevice(const String& mac);
