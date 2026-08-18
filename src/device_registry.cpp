#include "device_registry.h"
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <vector>

static Preferences prefs;
static std::vector<String> s_connectedNow;
static std::vector<int8_t> s_connectedRssi;
static std::vector<String> s_connectedIp;

static String ip4ToString(const esp_ip4_addr_t& addr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
             (unsigned)(addr.addr & 0xFF), (unsigned)((addr.addr >> 8) & 0xFF),
             (unsigned)((addr.addr >> 16) & 0xFF), (unsigned)((addr.addr >> 24) & 0xFF));
    return String(buf);
}

static String macKey(const uint8_t* mac) {
    char buf[13];
    snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

String macToDisplay(const String& key) {
    String out;
    for (int i = 0; i < 12 && i < (int)key.length(); i += 2) {
        if (i) out += ':';
        out += key.substring(i, i + 2);
    }
    return out;
}

static std::vector<String> loadKnownMacs() {
    std::vector<String> out;
    prefs.begin("devices", true);
    String list = prefs.getString("list", "");
    prefs.end();
    int start = 0;
    while (start < (int)list.length()) {
        int comma = list.indexOf(',', start);
        if (comma < 0) comma = list.length();
        String mac = list.substring(start, comma);
        if (mac.length() == 12) out.push_back(mac);
        start = comma + 1;
    }
    return out;
}

static void saveKnownMacs(const std::vector<String>& macs) {
    String list;
    for (size_t i = 0; i < macs.size(); i++) {
        if (i) list += ',';
        list += macs[i];
    }
    prefs.begin("devices", false);
    prefs.putString("list", list);
    prefs.end();
}

void refreshConnectedDevices() {
    s_connectedNow.clear();
    s_connectedRssi.clear();
    s_connectedIp.clear();

    wifi_sta_list_t wifiStaList;
    if (esp_wifi_ap_get_sta_list(&wifiStaList) != ESP_OK) return;

    esp_netif_sta_list_t netifStaList;
    bool haveIp = (esp_netif_get_sta_list(&wifiStaList, &netifStaList) == ESP_OK);

    std::vector<String> known = loadKnownMacs();
    bool changed = false;

    for (int i = 0; i < wifiStaList.num; i++) {
        String key = macKey(wifiStaList.sta[i].mac);
        s_connectedNow.push_back(key);
        s_connectedRssi.push_back(wifiStaList.sta[i].rssi);

        String ip = "";
        if (haveIp) {
            for (int j = 0; j < netifStaList.num; j++) {
                if (macKey(netifStaList.sta[j].mac) == key) {
                    ip = ip4ToString(netifStaList.sta[j].ip);
                    break;
                }
            }
        }
        s_connectedIp.push_back(ip);

        bool found = false;
        for (auto& k : known) if (k == key) { found = true; break; }
        if (!found) {
            known.push_back(key);
            changed = true;
        }
    }
    if (changed) saveKnownMacs(known);
}

int getKnownDeviceCount() {
    return (int)loadKnownMacs().size();
}

DeviceInfo getKnownDevice(int index) {
    DeviceInfo info;
    info.rssi = 0;
    info.connectedNow = false;

    std::vector<String> known = loadKnownMacs();
    if (index < 0 || index >= (int)known.size()) return info;
    info.mac = known[index];

    prefs.begin("devices", true);
    info.name = prefs.getString(("n_" + info.mac).c_str(), "");
    info.type = prefs.getString(("t_" + info.mac).c_str(), "Unknown");
    prefs.end();

    for (size_t i = 0; i < s_connectedNow.size(); i++) {
        if (s_connectedNow[i] == info.mac) {
            info.connectedNow = true;
            info.rssi = s_connectedRssi[i];
            info.ip = s_connectedIp[i];
            break;
        }
    }
    return info;
}

void saveDeviceInfo(const String& mac, const String& name, const String& type) {
    prefs.begin("devices", false);
    prefs.putString(("n_" + mac).c_str(), name);
    prefs.putString(("t_" + mac).c_str(), type);
    prefs.end();
}

void forgetDevice(const String& mac) {
    std::vector<String> known = loadKnownMacs();
    std::vector<String> updated;
    for (auto& k : known) if (k != mac) updated.push_back(k);
    saveKnownMacs(updated);

    prefs.begin("devices", false);
    prefs.remove(("n_" + mac).c_str());
    prefs.remove(("t_" + mac).c_str());
    prefs.end();
    Serial.printf("[DEVICES] Forgot device %s\n", mac.c_str());
}
