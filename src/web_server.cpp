#include "web_server.h"
#include "globals.h"
#include "rtc_module.h"
#include "app_settings.h"
#include "device_registry.h"
#include "sensors.h"
#include "irrigation.h"
#include "sd_logger.h"
#include "activity_log.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <SD.h>
#include <string.h>
#include "config_hardware.h"
#include "config_system.h"

static WebServer server(80);
static DNSServer dnsServer;
static const byte DNS_PORT = 53;
static bool staConnected = false;

// Escapes text that gets embedded inside single-quoted HTML attributes
// (e.g. a user-entered device name), so a name like "Ioan's phone" can't
// break out of the attribute or inject markup.
static String htmlAttrEscape(const String& in) {
    String out;
    out.reserve(in.length());
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '\'') out += "&#39;";
        else if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
    return out;
}

static String currentTimeStr() {
    if (!rtcAvailable) return "----/--/-- --:--:--";
    DateTime now = readRTC();
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    return String(buf);
}

// Shared look across every page: navbar + cards + forms.
static String pageStyle() {
    String css = "<style>";
    css += "body{font-family:'Segoe UI',Roboto,sans-serif;background:#eef2f3;margin:0;padding:0;color:#333;}";
    css += ".navbar{background:#2c3e50;color:white;padding:12px 15px;display:flex;justify-content:space-between;align-items:center;box-shadow:0 2px 5px rgba(0,0,0,0.2);flex-wrap:wrap;gap:8px;}";
    css += ".navbar .brand strong{font-size:16px;display:block;}";
    css += ".navbar .brand span{font-size:12px;opacity:0.75;}";
    css += ".navbar .nav-links{display:flex;gap:8px;flex-wrap:wrap;}";
    css += ".navbar .nav-links a{background:rgba(255,255,255,0.15);color:white;text-decoration:none;padding:8px 14px;border-radius:8px;font-size:13px;font-weight:600;white-space:nowrap;}";
    css += ".navbar .nav-links a.active{background:#3498db;}";
    css += ".container{padding:15px;max-width:500px;margin:auto;}";
    css += ".banner{padding:14px 20px;text-align:center;font-weight:600;font-size:15px;border-radius:12px;margin-bottom:15px;}";
    css += ".card-title{font-size:11px;font-weight:700;color:#7f8c8d;text-transform:uppercase;padding:12px 20px 4px;}";
    css += ".menu-card{background:white;border-radius:12px;overflow:hidden;box-shadow:0 4px 15px rgba(0,0,0,0.1);margin-bottom:5px;}";
    css += ".menu-item{display:flex;justify-content:space-between;align-items:center;padding:14px 20px;border-bottom:1px solid #eee;}";
    css += ".menu-item:last-child{border-bottom:none;}";
    css += ".label{font-weight:600;font-size:15px;}";
    css += ".badge{padding:5px 12px;border-radius:20px;font-size:12px;font-weight:bold;}";
    css += ".ok{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}";
    css += ".err{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}";
    css += ".warn{background:#fff3cd;color:#856404;border:1px solid #ffc107;}";
    css += ".info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb;}";
    css += ".btn-refresh{display:block;width:100%;padding:15px;margin-top:15px;background:#3498db;color:white;border:none;border-radius:8px;font-weight:bold;text-decoration:none;text-align:center;box-sizing:border-box;}";
    css += ".footer{text-align:center;font-size:12px;color:#7f8c8d;margin-top:15px;}";
    css += ".card{background:white;border-radius:12px;padding:20px;box-shadow:0 4px 15px rgba(0,0,0,0.1);margin-bottom:15px;}";
    css += ".card h3{margin-top:0;color:#2c3e50;}";
    css += ".hint{font-size:13px;color:#7f8c8d;}";
    css += "label{display:block;font-size:13px;font-weight:600;color:#555;margin-top:10px;}";
    css += "input[type=date],input[type=number],input[type=text],select{width:100%;padding:10px;border:1px solid #ccc;border-radius:8px;font-size:15px;box-sizing:border-box;margin:6px 0 4px;}";
    css += "button{width:100%;padding:12px;background:#27ae60;color:white;border:none;border-radius:8px;font-size:16px;font-weight:bold;margin-top:12px;}";
    css += ".back{display:block;text-align:center;margin-top:12px;color:#3498db;text-decoration:none;font-size:13px;}";
    css += ".device-row{border-bottom:1px solid #eee;padding:12px 0;}";
    css += ".device-row:last-child{border-bottom:none;}";
    css += ".mac{font-family:monospace;font-size:12px;color:#7f8c8d;}";
    css += ".dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px;}";
    css += ".dot.on{background:#2ecc71;}";
    css += ".dot.off{background:#bbb;}";
    css += ".progress-track{background:#eee;border-radius:8px;height:8px;overflow:hidden;margin:6px 20px 12px;}";
    css += ".progress-fill{background:#3498db;height:100%;transition:width 0.4s ease;}";
    css += ".progress-fill.done{background:#2ecc71;}";
    css += "</style>";
    return css;
}

static String navbar(const String& timeStr, const char* activePage) {
    String html = "<div class='navbar'><div class='brand'><strong>🌿 Greenhouse Automation</strong><span>Last updated " + timeStr + " — refreshes automatically</span></div>";
    html += "<div class='nav-links'>";
    html += "<a href='/' class='" + String(strcmp(activePage, "home") == 0 ? "active" : "") + "'>🏠 Home</a>";
    html += "<a href='/config' class='" + String(strcmp(activePage, "config") == 0 ? "active" : "") + "'>⚙️ Config</a>";
    html += "<a href='/connections' class='" + String(strcmp(activePage, "connections") == 0 ? "active" : "") + "'>📶 Connections</a>";
    html += "<a href='/logs' class='" + String(strcmp(activePage, "logs") == 0 ? "active" : "") + "'>📄 Logs</a>";
    html += "</div></div>";
    return html;
}

static String pageHead() {
    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta name='apple-mobile-web-app-capable' content='yes'>";
    html += "<meta name='mobile-web-app-capable' content='yes'>";
    html += "<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent'>";
    html += pageStyle();
    html += "</head><body>";
    return html;
}

static void handleRoot() {
    uint32_t currentPulseZ1, currentPulseZ2;
    noInterrupts();
    currentPulseZ1 = pulsesZ1;
    currentPulseZ2 = pulsesZ2;
    interrupts();

    float volumeZ1 = (float)currentPulseZ1 / pulsesPerLiter;
    float volumeZ2 = (float)currentPulseZ2 / pulsesPerLiter;
    String timeStr = currentTimeStr();

    const char* stateCls;
    const char* bannerMsg;
    switch (currentState) {
        case WATERING_ZONE_1: stateCls = "warn"; bannerMsg = "💧 Watering Zone 1 right now"; break;
        case WATERING_ZONE_2: stateCls = "warn"; bannerMsg = "💧 Watering Zone 2 right now"; break;
        default:              stateCls = "ok"; bannerMsg = "🌿 All quiet — just keeping an eye on things"; break;
    }

    String tempSoilStr = isnan(tempSoil) ? "N/A" : String(tempSoil, 1) + " &deg;C";
    String tempWaterStr = isnan(tempWater) ? "N/A" : String(tempWater, 1) + " &deg;C";
    String tempAirStr = isnan(tempAir) ? "N/A" : String(tempAir, 1) + " &deg;C";
    String humidityAirStr = isnan(humidityAir) ? "N/A" : String(humidityAir, 0) + " %";
    String pressureStr = isnan(pressure) ? "N/A" : String(pressure, 1) + " hPa";
    String altitudeStr = isnan(altitude) ? "N/A" : String(altitude, 0) + " m";
    String tempBaroStr = isnan(tempBaro) ? "N/A" : String(tempBaro, 1) + " &deg;C";

    String html = pageHead();
    html += "<div id='page-content'>";
    html += navbar(timeStr, "home");
    html += "<div class='container'>";
    html += "<div class='banner " + String(stateCls) + "'>" + String(bannerMsg) + "</div>";
    if (irrigationFault) {
        html += "<div class='banner err'>⚠️ SAFETY STOP: " + irrigationFaultMsg + "</div>";
    }

    html += "<div class='card-title'>Live Readings</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>🌱 Soil Temperature</span><span class='badge " + String(isnan(tempSoil) ? "err" : "info") + "'>" + tempSoilStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>💧 Water Temperature</span><span class='badge " + String(isnan(tempWater) ? "err" : "info") + "'>" + tempWaterStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>🌡️ Air Temperature</span><span class='badge " + String(isnan(tempAir) ? "err" : "info") + "'>" + tempAirStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>💦 Air Humidity</span><span class='badge " + String(isnan(humidityAir) ? "err" : "info") + "'>" + humidityAirStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>🌬️ Pressure</span><span class='badge " + String(isnan(pressure) ? "err" : "info") + "'>" + pressureStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>⛰️ Altitude</span><span class='badge " + String(isnan(altitude) ? "err" : "info") + "'>" + altitudeStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>🌡️ Barometer Temperature</span><span class='badge " + String(isnan(tempBaro) ? "err" : "info") + "'>" + tempBaroStr + "</span></div>";
    html += "</div>";
    html += "<p class='hint'>Sensor/peripheral health (addresses, GPIOs, connection status) lives on the Connections page — this card is just the current values.</p>";

    float pctZ1 = (targetVolumeZ1 > 0) ? min(100.0f, (volumeZ1 / targetVolumeZ1) * 100.0f) : 0;
    float pctZ2 = (targetVolumeZ2 > 0) ? min(100.0f, (volumeZ2 / targetVolumeZ2) * 100.0f) : 0;

    html += "<div class='card-title'>Water Volumes</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>🚰 Zone 1" + String(enabledZ1 ? "" : " (disabled)") + "</span><span class='badge info'>" + String(volumeZ1, 2) + " L / " + String(targetVolumeZ1, 0) + " L</span></div>";
    html += "<div class='progress-track'><div class='progress-fill" + String(pctZ1 >= 100 ? " done" : "") + "' style='width:" + String(pctZ1, 0) + "%;'></div></div>";
    html += "<div class='menu-item'><span class='label'>🚰 Zone 2" + String(enabledZ2 ? "" : " (disabled)") + "</span><span class='badge info'>" + String(volumeZ2, 2) + " L / " + String(targetVolumeZ2, 0) + " L</span></div>";
    html += "<div class='progress-track'><div class='progress-fill" + String(pctZ2 >= 100 ? " done" : "") + "' style='width:" + String(pctZ2, 0) + "%;'></div></div>";
    html += "<div class='menu-item'><span class='label'>🕓 Last Completed — Zone 1</span><span class='badge info'>" + lastWateredZ1 +
            (lastWateredZ1 == "Never" ? "" : " (" + String(lastVolumeZ1, 1) + "L)") + "</span></div>";
    html += "<div class='menu-item'><span class='label'>🕓 Last Completed — Zone 2</span><span class='badge info'>" + lastWateredZ2 +
            (lastWateredZ2 == "Never" ? "" : " (" + String(lastVolumeZ2, 1) + "L)") + "</span></div>";
    html += "</div>";

    html += "<div class='card-title'>Manual Control</div>";
    html += "<div class='card'>";
    if (currentState == STOPPED) {
        if (enabledZ1) {
            html += "<form method='POST' action='/water-zone1' onsubmit=\"return confirm('Start watering Zone 1 now?');\">";
            html += "<button type='submit'>💧 Water Zone 1 Now</button>";
            html += "</form>";
        } else {
            html += "<p class='hint'>Zone 1 is disabled on /config — re-enable it there to water manually.</p>";
        }
        if (enabledZ2) {
            html += "<form method='POST' action='/water-zone2' onsubmit=\"return confirm('Start watering Zone 2 now?');\" style='margin-top:8px;'>";
            html += "<button type='submit'>💧 Water Zone 2 Now</button>";
            html += "</form>";
        } else {
            html += "<p class='hint'>Zone 2 is disabled on /config — re-enable it there to water manually.</p>";
        }
    } else {
        html += "<p class='hint'>A cycle is already running — use Stop Now below to abort it, or wait for it to finish.</p>";
    }
    html += "<form method='POST' action='/stop' onsubmit=\"return confirm('Stop irrigation immediately?');\" style='margin-top:8px;'>";
    html += "<button type='submit' style='background:#e74c3c;'>🛑 Stop Now</button>";
    html += "</form>";
    html += "</div>";

    String footer = "📶 AP: 192.168.4.1";
    if (staConnected) footer += " | LAN: " + WiFi.localIP().toString();
    html += "<div class='footer'>" + footer + " | " + String(WIFI_SSID) + "</div>";

    html += "</div></div>";
    // Re-fetches this same page every 10s and swaps in just the refreshed
    // content (#page-content), instead of a full-page reload — avoids the
    // flicker and lost scroll position a <meta refresh> would cause. Falls
    // back to doing nothing on a failed fetch; the next tick just retries.
    html += "<script>";
    html += "function refreshDashboard(){";
    html += "fetch(location.pathname).then(r=>r.text()).then(function(t){";
    html += "var d=new DOMParser().parseFromString(t,'text/html');";
    html += "var fresh=d.getElementById('page-content');";
    html += "var cur=document.getElementById('page-content');";
    html += "if(fresh&&cur)cur.innerHTML=fresh.innerHTML;";
    html += "}).catch(function(){});";
    html += "}";
    html += "setInterval(refreshDashboard,10000);";
    html += "</script>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

static void handleConfigGet() {
    String html = pageHead();
    html += navbar(currentTimeStr(), "config");
    html += "<div class='container'>";
    if (server.hasArg("saved")) html += "<div class='banner ok'>✅ Saved</div>";

    html += "<div class='card'><h3>⚙️ Irrigation Settings</h3>";
    html += "<p class='hint'>Each zone has its own independent daily start time and target volume. They never run at the same time (one shared pump feeds both valves) — if one zone's schedule lands while the other is still running, it's skipped for that day rather than queued. Times are 24-hour.</p>";
    html += "<form method='POST' action='/config'>";
    html += "<label><input type='checkbox' name='enabledZ1' value='1'" + String(enabledZ1 ? " checked" : "") + " style='width:auto;'> Zone 1 Enabled</label>";
    html += "<label>Zone 1 — Start Time</label>";
    html += "<div style='display:flex;gap:10px;'>";
    html += "<input type='number' min='0' max='23' name='startHourZ1' value='" + String(startHourZ1) + "' placeholder='HH' style='text-align:center;'>";
    html += "<input type='number' min='0' max='59' name='startMinuteZ1' value='" + String(startMinuteZ1) + "' placeholder='MM' style='text-align:center;'>";
    html += "</div>";
    html += "<label>Zone 1 — Target Volume (L)</label><input type='number' step='0.1' min='0' name='vol1' value='" + String(targetVolumeZ1, 1) + "'>";
    html += "<hr style='border:none;border-top:1px solid #333;margin:14px 0;'>";
    html += "<label><input type='checkbox' name='enabledZ2' value='1'" + String(enabledZ2 ? " checked" : "") + " style='width:auto;'> Zone 2 Enabled</label>";
    html += "<label>Zone 2 — Start Time</label>";
    html += "<div style='display:flex;gap:10px;'>";
    html += "<input type='number' min='0' max='23' name='startHourZ2' value='" + String(startHourZ2) + "' placeholder='HH' style='text-align:center;'>";
    html += "<input type='number' min='0' max='59' name='startMinuteZ2' value='" + String(startMinuteZ2) + "' placeholder='MM' style='text-align:center;'>";
    html += "</div>";
    html += "<label>Zone 2 — Target Volume (L)</label><input type='number' step='0.1' min='0' name='vol2' value='" + String(targetVolumeZ2, 1) + "'>";
    html += "<p class='hint'>Unchecking a zone stops it from running on its schedule or via its manual \"Water Now\" button — its saved settings and history are kept, so re-enabling it later needs no reconfiguration.</p>";
    html += "<label>Max Duration Per Zone (minutes)</label><input type='number' step='1' min='1' name='maxZoneMin' value='" + String(maxZoneMinutes, 0) + "'>";
    html += "<p class='hint'>Safety cutoff — if a zone runs this long without reaching its target volume (stuck valve, broken flow meter), it force-stops instead of running indefinitely.</p>";
    html += "<button type='submit'>💾 Save Settings</button>";
    html += "</form>";
    html += "</div>";

    html += "<div class='card'><h3>🎯 Sensor Calibration</h3>";
    html += "<p class='hint'>Flow meter volume readings and BMP180 altitude both depend on these reference values — adjust them if you've measured a discrepancy against a known-good reference.</p>";
    html += "<form method='POST' action='/config'>";
    html += "<label>Flow Meter Pulses / Liter</label>";
    html += "<input type='number' step='0.1' min='1' name='pplitr' value='" + String(pulsesPerLiter, 1) + "'>";
    html += "<p class='hint'>Run a known volume of water through a zone, divide the pulse count (visible on the dashboard) by the liters actually collected, and enter the result here. Shared by both flow meters.</p>";
    html += "<label>Local Sea-Level Pressure (hPa)</label>";
    html += "<input type='number' step='0.1' min='800' max='1100' name='seaHpa' value='" + String(seaLevelPressurePa / 100.0f, 1) + "'>";
    html += "<p class='hint'>Look up the current sea-level pressure for your area (e.g. from a local weather report) for an accurate altitude reading. Defaults to standard atmosphere (1013.25 hPa) otherwise.</p>";
    html += "<button type='submit'>💾 Save Calibration</button>";
    html += "</form>";
    html += "</div>";

    html += "<div class='card'><h3>🌡️ DS18B20 Sensor Mapping</h3>";
    int ds18Count = getDS18B20Count();
    if (ds18Count == 0) {
        html += "<p class='hint'>No DS18B20 sensors currently detected on the One-Wire bus.</p>";
    } else {
        html += "<p class='hint'>Assign each detected sensor's role. Useful when a sensor is physically replaced — the new unit has a different factory address, so it won't read correctly until re-mapped here.</p>";
        html += "<form method='POST' action='/config'>";
        for (int i = 0; i < ds18Count; i++) {
            String addr = getDS18B20AddressAt(i);
            if (addr.length() == 0) continue;
            String role = (addr == addrSoil) ? "soil" : (addr == addrWater) ? "water" : "none";
            html += "<label>" + addr + "</label>";
            html += "<select name='role_" + addr + "'>";
            html += String("<option value='none'") + (role == "none" ? " selected" : "") + ">Unassigned</option>";
            html += String("<option value='soil'") + (role == "soil" ? " selected" : "") + ">🌱 Soil</option>";
            html += String("<option value='water'") + (role == "water" ? " selected" : "") + ">💧 Water</option>";
            html += "</select>";
        }
        html += "<button type='submit'>💾 Save Mapping</button>";
        html += "</form>";
    }
    html += "</div>";

    DateTime now = rtcAvailable ? readRTC() : DateTime(2026, 1, 1, 0, 0, 0);
    char dateVal[11];
    snprintf(dateVal, sizeof(dateVal), "%04d-%02d-%02d", now.year(), now.month(), now.day());

    html += "<div class='card'><h3>🕒 System Date &amp; Time</h3>";
    html += "<p class='hint'>The clock has no battery backup, so it resets on every reboot. Set the correct date/time below (24-hour) and it'll be kept until the next power-off.</p>";
    html += "<form method='POST' action='/settime'>";
    html += "<label>Date</label><input type='date' name='date' value='" + String(dateVal) + "'>";
    html += "<label>Time (24h)</label>";
    html += "<div style='display:flex;gap:10px;'>";
    html += "<input type='number' min='0' max='23' name='hour' value='" + String(now.hour()) + "' placeholder='HH' style='text-align:center;'>";
    html += "<input type='number' min='0' max='59' name='minute' value='" + String(now.minute()) + "' placeholder='MM' style='text-align:center;'>";
    html += "</div>";
    html += "<button type='submit'>✅ Save Date &amp; Time</button>";
    html += "</form>";
    html += "</div>";

    html += "<div class='card'><h3>⚙️ System</h3>";
    html += "<div class='menu-item'><span class='label'>🏷️ Firmware Build</span><span class='badge info'>" __DATE__ " " __TIME__ "</span></div>";
    html += "<p class='hint'>Restarts the device. Irrigation, sensor readings, and Wi-Fi will briefly go offline until it boots back up (a few seconds).</p>";
    html += "<form method='POST' action='/reboot' onsubmit=\"return confirm('Reboot the device now?');\">";
    html += "<button type='submit' style='background:#e74c3c;'>🔄 Reboot Device</button>";
    html += "</form>";
    html += "</div>";

    html += "</div></body></html>";
    server.send(200, "text/html", html);
}

static void handleConfigPost() {
    bool irrigationFieldsSubmitted = server.hasArg("startHourZ1") || server.hasArg("vol1") || server.hasArg("maxZoneMin");
    bool calibrationFieldsSubmitted = server.hasArg("pplitr") || server.hasArg("seaHpa");

    if (server.hasArg("startHourZ1")) startHourZ1 = (uint8_t)constrain(server.arg("startHourZ1").toInt(), 0, 23);
    if (server.hasArg("startMinuteZ1")) startMinuteZ1 = (uint8_t)constrain(server.arg("startMinuteZ1").toInt(), 0, 59);
    if (server.hasArg("startHourZ2")) startHourZ2 = (uint8_t)constrain(server.arg("startHourZ2").toInt(), 0, 23);
    if (server.hasArg("startMinuteZ2")) startMinuteZ2 = (uint8_t)constrain(server.arg("startMinuteZ2").toInt(), 0, 59);
    // Checkboxes are only present in the POST body when checked, so an
    // unchecked box must be inferred from its absence — but only while this
    // is actually the irrigation-settings form (irrigationFieldsSubmitted),
    // otherwise a POST to /config from some other form would wrongly read as
    // "both zones just got disabled."
    if (irrigationFieldsSubmitted) {
        enabledZ1 = server.hasArg("enabledZ1");
        enabledZ2 = server.hasArg("enabledZ2");
        if (currentState == WATERING_ZONE_1 && !enabledZ1) stopAll();
        if (currentState == WATERING_ZONE_2 && !enabledZ2) stopAll();
    }
    if (server.hasArg("vol1")) targetVolumeZ1 = server.arg("vol1").toFloat();
    if (server.hasArg("vol2")) targetVolumeZ2 = server.arg("vol2").toFloat();
    if (server.hasArg("maxZoneMin")) {
        float v = server.arg("maxZoneMin").toFloat();
        if (v > 0) maxZoneMinutes = v; // guard against 0/negative — used as a timeout, not a divisor, but 0 would trip instantly
    }
    if (server.hasArg("pplitr")) {
        float v = server.arg("pplitr").toFloat();
        if (v > 0) pulsesPerLiter = v; // guard against 0/negative — it's a divisor everywhere
    }
    if (server.hasArg("seaHpa")) {
        float hpa = constrain(server.arg("seaHpa").toFloat(), 800.0f, 1100.0f);
        seaLevelPressurePa = hpa * 100.0f;
    }
    saveAppSettings();

    if (irrigationFieldsSubmitted) {
        char buf1[6], buf2[6];
        snprintf(buf1, sizeof(buf1), "%02u:%02u", startHourZ1, startMinuteZ1);
        snprintf(buf2, sizeof(buf2), "%02u:%02u", startHourZ2, startMinuteZ2);
        logActivity("Irrigation settings updated: Z1 " + String(enabledZ1 ? "enabled" : "DISABLED") + " start " + String(buf1) + " (" + String(targetVolumeZ1, 1) +
                    "L), Z2 " + String(enabledZ2 ? "enabled" : "DISABLED") + " start " + String(buf2) + " (" + String(targetVolumeZ2, 1) + "L), max " +
                    String(maxZoneMinutes, 0) + " min/zone");
    }
    if (calibrationFieldsSubmitted) {
        logActivity("Sensor calibration updated: " + String(pulsesPerLiter, 1) + " pulses/L, " +
                    String(seaLevelPressurePa / 100.0f, 1) + " hPa sea-level reference");
    }

    int ds18Count = getDS18B20Count();
    if (ds18Count > 0) {
        String newSoil = addrSoil, newWater = addrWater;
        bool mappingSubmitted = false;
        for (int i = 0; i < ds18Count; i++) {
            String addr = getDS18B20AddressAt(i);
            if (addr.length() == 0) continue;
            String field = "role_" + addr;
            if (server.hasArg(field)) {
                mappingSubmitted = true;
                String role = server.arg(field);
                if (role == "soil") {
                    newSoil = addr;
                } else if (role == "water") {
                    newWater = addr;
                } else {
                    // "Unassigned" — clear this address out of whichever role it
                    // currently holds. Without this, picking "Unassigned" for a
                    // mapped sensor had no effect: newSoil/newWater only ever got
                    // SET when role was soil/water, never CLEARED otherwise.
                    if (newSoil == addr) newSoil = "";
                    if (newWater == addr) newWater = "";
                }
            }
        }
        if (mappingSubmitted) {
            saveSensorMapping(newSoil, newWater);
            applySensorMapping();
            logActivity("DS18B20 mapping updated: Soil=" + newSoil + ", Water=" + newWater);
        }
    }

    server.sendHeader("Location", "/config?saved=1", true);
    server.send(302, "text/plain", "");
}

static const char* DEVICE_TYPES[] = { "Unknown", "Phone", "Laptop", "Tablet", "IoT/Sensor", "Other" };
static const int DEVICE_TYPE_COUNT = 6;

static void handleConnectionsGet() {
    refreshConnectedDevices();
    int count = getKnownDeviceCount();

    String html = pageHead();
    html += navbar(currentTimeStr(), "connections");
    html += "<div class='container'>";
    if (server.hasArg("saved")) html += "<div class='banner ok'>✅ Saved</div>";
    if (server.hasArg("removed")) html += "<div class='banner ok'>🗑️ Device removed</div>";

    html += "<div class='card-title'>Wi-Fi Status</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>📡 Access Point</span><span class='badge ok'>" + String(WIFI_SSID) + "</span></div>";
    html += "<div class='menu-item'><span class='label'>🌐 AP Address</span><span class='badge info'>" + WiFi.softAPIP().toString() + "</span></div>";
    html += "<div class='menu-item'><span class='label'>👥 Connected Clients</span><span class='badge info'>" + String(WiFi.softAPgetStationNum()) + "</span></div>";
    if (staConnected) {
        html += "<div class='menu-item'><span class='label'>🏠 Home Network</span><span class='badge ok'>" + WiFi.localIP().toString() + "</span></div>";
        html += "<div class='menu-item'><span class='label'>📶 Signal (RSSI)</span><span class='badge info'>" + String(WiFi.RSSI()) + " dBm</span></div>";
    } else {
        html += "<div class='menu-item'><span class='label'>🏠 Home Network</span><span class='badge err'>Not connected</span></div>";
    }
    html += "</div>";

    html += "<div class='card-title'>Local Peripherals</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>💾 MicroSD Card</span><span class='badge " + String(statusSD == "OK" ? "ok" : "err") + "'>" + statusSD + " · CS GPIO" + String(PIN_SD_CS) + "</span></div>";
    html += "<div class='menu-item'><span class='label'>🌱 Soil Sensor (DS18B20)</span><span class='badge " + String(isnan(tempSoil) ? "err" : "ok") + "'>" + String(isnan(tempSoil) ? "No reading" : "OK") + " · " + (addrSoil.length() ? addrSoil : "Unassigned") + "</span></div>";
    html += "<div class='menu-item'><span class='label'>💧 Water Sensor (DS18B20)</span><span class='badge " + String(isnan(tempWater) ? "err" : "ok") + "'>" + String(isnan(tempWater) ? "No reading" : "OK") + " · " + (addrWater.length() ? addrWater : "Unassigned") + "</span></div>";
    html += "<div class='menu-item'><span class='label'>📟 DHT22 Air Sensor</span><span class='badge " + String(statusDHT22 == "OK" ? "ok" : statusDHT22 == "Initializing..." ? "warn" : "err") + "'>" + statusDHT22 + " · GPIO" + String(PIN_DHT22) + "</span></div>";
    html += "<div class='menu-item'><span class='label'>🕒 DS3231 RTC</span><span class='badge " + String(statusRTC == "OK" ? "ok" : "err") + "'>" + statusRTC + " · I2C 0x68</span></div>";
    html += "<div class='menu-item'><span class='label'>🌬️ BMP180 Barometer</span><span class='badge " + String(statusBMP180 == "OK" ? "ok" : "err") + "'>" + statusBMP180 + " · I2C 0x77</span></div>";
    html += "</div>";

    html += "<div class='card-title'>Known Network Devices</div>";
    html += "<div class='card'>";
    if (count == 0) {
        html += "<p class='hint'>No devices have connected to the Access Point yet.</p>";
    } else {
        html += "<p class='hint'>Forgetting a device that's still connected will simply reappear on the next refresh — Forget is meant for devices that no longer connect.</p>";
        for (int i = 0; i < count; i++) {
            DeviceInfo d = getKnownDevice(i);
            html += "<div class='device-row'>";
            html += "<span class='dot " + String(d.connectedNow ? "on" : "off") + "'></span>";
            html += "<span class='mac'>" + macToDisplay(d.mac) + "</span>";
            if (d.connectedNow) {
                String ipPart = d.ip.length() ? (" · " + d.ip) : String("");
                html += " <span class='badge ok'>online" + ipPart + " · " + String(d.rssi) + " dBm</span>";
            } else {
                html += " <span class='badge err'>offline</span>";
            }
            html += "<form method='POST' action='/connections'>";
            html += "<input type='hidden' name='mac' value='" + d.mac + "'>";
            html += "<label>Name</label><input type='text' name='name' value='" + htmlAttrEscape(d.name) + "' placeholder='e.g. Phone'>";
            html += "<label>Type</label><select name='type'>";
            for (int t = 0; t < DEVICE_TYPE_COUNT; t++) {
                String sel = (d.type == DEVICE_TYPES[t]) ? " selected" : "";
                html += "<option value='" + String(DEVICE_TYPES[t]) + "'" + sel + ">" + String(DEVICE_TYPES[t]) + "</option>";
            }
            html += "</select>";
            html += "<div style='display:flex;gap:10px;'>";
            html += "<button type='submit'>💾 Save</button>";
            html += "<button type='submit' formaction='/connections/forget' style='background:#e74c3c;' onclick=\"return confirm('Forget this device?');\">🗑️ Forget</button>";
            html += "</div>";
            html += "</form>";
            html += "</div>";
        }
    }
    html += "</div>";

    html += "</div></body></html>";
    server.send(200, "text/html", html);
}

static void handleConnectionsPost() {
    if (server.hasArg("mac")) {
        String mac = server.arg("mac");
        String newName = server.hasArg("name") ? server.arg("name") : "";
        String newType = server.hasArg("type") ? server.arg("type") : "Unknown";
        saveDeviceInfo(mac, newName, newType);
        logActivity("Device " + macToDisplay(mac) + " updated: name='" + newName + "', type=" + newType);
    }
    server.sendHeader("Location", "/connections?saved=1", true);
    server.send(302, "text/plain", "");
}

static void handleConnectionsForget() {
    if (server.hasArg("mac")) {
        String mac = server.arg("mac");
        forgetDevice(mac);
        logActivity("Device " + macToDisplay(mac) + " forgotten");
    }
    server.sendHeader("Location", "/connections?removed=1", true);
    server.send(302, "text/plain", "");
}

static String formatFileSize(uint32_t bytes) {
    if (bytes >= 1024UL * 1024UL) return String(bytes / (1024.0f * 1024.0f), 2) + " MB";
    return String(bytes / 1024.0f, 1) + " KB";
}

static void handleLogsGet() {
    String html = pageHead();
    html += navbar(currentTimeStr(), "logs");
    html += "<div class='container'>";
    if (server.hasArg("removed")) html += "<div class='banner ok'>🗑️ File deleted</div>";

    html += "<div class='card-title'>Data Logs</div>";
    html += "<div class='card'>";
    int count = getLogFileCount();
    if (count == 0) {
        html += "<p class='hint'>No log files on the SD card yet — one is created automatically once irrigation runs or the periodic logger writes its first row.</p>";
    } else {
        html += "<p class='hint'>date_solar.csv is the live log. Dated/_prev files are old logs automatically archived when a firmware update changed the CSV columns.</p>";
        for (int i = 0; i < count; i++) {
            LogFileInfo f = getLogFileAt(i);
            if (f.name.length() == 0) continue;
            html += "<div class='device-row'>";
            html += "<span class='label'>" + f.name + "</span> <span class='badge info'>" + formatFileSize(f.sizeBytes) + "</span>";
            html += "<div style='display:flex;gap:10px;margin-top:8px;'>";
            html += "<a class='btn-refresh' style='margin-top:0;' href='/logs/download?file=" + f.name + "'>⬇️ Download</a>";
            html += "<form method='POST' action='/logs/delete' onsubmit=\"return confirm('Delete " + f.name + "? This cannot be undone.');\">";
            html += "<input type='hidden' name='file' value='" + f.name + "'>";
            html += "<button type='submit' style='background:#e74c3c;margin-top:0;'>🗑️ Delete</button>";
            html += "</form>";
            html += "</div>";
            html += "</div>";
        }
    }
    html += "</div>";

    html += "<div class='card-title'>Recent Activity</div>";
    html += "<div class='card'>";
    int actCount = getActivityEntryCount();
    if (actCount == 0) {
        html += "<p class='hint'>No changes logged yet — settings saves, device edits, manual watering/stop, and reboots made from this web UI will appear here.</p>";
    } else {
        html += "<p class='hint'>Audit trail of changes made from this web UI (not sensor data — that's in date_solar.csv above). Showing the most recent " +
                String(min(actCount, 20)) + " of " + String(actCount) + ".</p>";
        for (int i = 0; i < min(actCount, 20); i++) {
            String entry = getActivityEntryAt(i);
            int sep = entry.indexOf(';');
            String ts = sep >= 0 ? entry.substring(0, sep) : "N/A";
            String action = sep >= 0 ? entry.substring(sep + 1) : entry;
            html += "<div class='menu-item'><span class='label'>" + action + "</span><span class='badge info'>" + ts + "</span></div>";
        }
        html += "<form method='POST' action='/logs/activity/clear' onsubmit=\"return confirm('Clear the activity log?');\" style='margin-top:12px;'>";
        html += "<button type='submit' style='background:#e74c3c;'>🗑️ Clear Activity Log</button>";
        html += "</form>";
    }
    html += "</div>";

    html += "</div></body></html>";
    server.send(200, "text/html", html);
}

static void handleLogDownload() {
    if (!server.hasArg("file")) {
        server.send(400, "text/plain", "Missing file parameter");
        return;
    }
    String name = server.arg("file");
    if (name.indexOf('/') >= 0 || name.indexOf("..") >= 0 ||
        !name.startsWith("date_solar") || !name.endsWith(".csv")) {
        server.send(400, "text/plain", "Invalid filename");
        return;
    }
    String path = "/" + name;
    if (!SD.exists(path.c_str())) {
        server.send(404, "text/plain", "File not found");
        return;
    }
    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) {
        server.send(500, "text/plain", "Could not open file");
        return;
    }
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
    server.streamFile(f, "text/csv");
    f.close();
}

static void handleLogDelete() {
    if (server.hasArg("file")) {
        String name = server.arg("file");
        if (deleteLogFile(name)) logActivity("Log file deleted: " + name);
    }
    server.sendHeader("Location", "/logs?removed=1", true);
    server.send(302, "text/plain", "");
}

static void handleClearActivity() {
    clearActivityLog();
    logActivity("Activity log cleared"); // first entry in the fresh file — keeps a continuity marker
    server.sendHeader("Location", "/logs?removed=1", true);
    server.send(302, "text/plain", "");
}

// Date & time setting now lives inline on /config — this stays only so old
// bookmarks/links to /settime still land somewhere sensible.
static void handleSetTimeGetRedirect() {
    server.sendHeader("Location", "/config", true);
    server.send(302, "text/plain", "");
}

static void handleSetTimePost() {
    if (!server.hasArg("date") || server.arg("date").length() < 10 ||
        !server.hasArg("hour") || !server.hasArg("minute")) {
        server.send(400, "text/plain", "Missing date/hour/minute parameter");
        return;
    }
    String date = server.arg("date"); // YYYY-MM-DD
    int y  = date.substring(0, 4).toInt();
    int mo = date.substring(5, 7).toInt();
    int d  = date.substring(8, 10).toInt();
    int h  = server.arg("hour").toInt();
    int mi = server.arg("minute").toInt();
    if (y < 2020 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59) {
        server.send(400, "text/plain", "Date/time values out of range");
        return;
    }
    syncTimeManual(DateTime((uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, 0));
    logActivity("System time set manually to " + date + " " + server.arg("hour") + ":" + server.arg("minute"));
    server.sendHeader("Location", "/config?saved=1", true);
    server.send(302, "text/plain", "");
}

static void handleWaterZone1() {
    if (startZone1()) logActivity("Manual watering started: Zone 1"); // no-op (and no log) if a cycle is already running
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

static void handleWaterZone2() {
    if (startZone2()) logActivity("Manual watering started: Zone 2");
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

static void handleStopNow() {
    stopAll();
    logActivity("Manual stop requested from web UI");
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

static void handleReboot() {
    logActivity("Reboot requested from web UI");
    server.send(200, "text/html",
        "<html><body style='font-family:sans-serif;text-align:center;padding-top:60px;'>"
        "<h2>Rebooting...</h2><p>The device is restarting. This page will not refresh automatically.</p>"
        "</body></html>");
    Serial.println("[SYSTEM] Reboot requested from web UI.");
    delay(300); // let the HTTP response flush before the reset tears down Wi-Fi
    ESP.restart();
}

static void handleNotFound() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
}

void initWebServer() {
    // Try STA if credentials are configured
    if (strlen(WIFI_STA_SSID) > 0) {
        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);
        Serial.print("[WiFi] Connecting to '" WIFI_STA_SSID "'");
        unsigned long t = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_STA_TIMEOUT_MS) {
            delay(200);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED) {
            staConnected = true;
            Serial.println("\n[OK] Wi-Fi STA: Connected. IP: " + WiFi.localIP().toString());
        } else {
            // Deliberately NOT switching to WIFI_AP here — staying in AP_STA lets
            // staRetryLoop() keep trying in the background without disrupting the
            // AP. The AP itself is unaffected either way.
            Serial.println("\n[WARNING] Wi-Fi STA: Connection failed, will keep retrying in the background.");
        }
    }

    // AP always starts regardless of STA result
    IPAddress apIP(192, 168, 4, 1);
    if (!WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0))) {
        Serial.println("[ERROR] Wi-Fi: Access Point IP configuration failed!");
    }
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/config", HTTP_GET, handleConfigGet);
    server.on("/config", HTTP_POST, handleConfigPost);
    server.on("/connections", HTTP_GET, handleConnectionsGet);
    server.on("/connections", HTTP_POST, handleConnectionsPost);
    server.on("/connections/forget", HTTP_POST, handleConnectionsForget);
    server.on("/logs", HTTP_GET, handleLogsGet);
    server.on("/logs/download", HTTP_GET, handleLogDownload);
    server.on("/logs/delete", HTTP_POST, handleLogDelete);
    server.on("/logs/activity/clear", HTTP_POST, handleClearActivity);
    server.on("/settime", HTTP_GET,  handleSetTimeGetRedirect);
    server.on("/settime", HTTP_POST, handleSetTimePost);
    server.on("/reboot", HTTP_POST, handleReboot);
    server.on("/water-zone1", HTTP_POST, handleWaterZone1);
    server.on("/water-zone2", HTTP_POST, handleWaterZone2);
    server.on("/stop", HTTP_POST, handleStopNow);
    // Captive-portal probes sent by Android, iOS/macOS, Windows, and Firefox.
    // Adding explicit handlers stops the WebServer from logging
    // "request handler not found" before falling through to onNotFound.
    server.on("/generate_204",        []() { server.send(204, "text/plain", ""); });
    server.on("/connecttest.txt",     []() { server.send(200, "text/plain", "Microsoft NCSI"); });
    server.on("/ncsi.txt",            []() { server.send(200, "text/plain", "Microsoft NCSI"); });
    server.on("/hotspot-detect.html", handleRoot);
    server.on("/success.txt",         []() { server.send(200, "text/plain", "success\n"); });
    // Browsers request this automatically on every page load; an explicit empty
    // reply avoids it falling through to onNotFound()'s redirect (harmless, but
    // wasted request + log noise on every dashboard visit).
    server.on("/favicon.ico",         []() { server.send(204, "text/plain", ""); });
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("[OK] Wi-Fi AP: Started (" + String(WIFI_SSID) + "). IP: " + WiFi.softAPIP().toString());
}

// Keeps retrying the optional home-network (STA) link in the background —
// covers both "never connected at boot" and "connected, then the router
// dropped it later" without disrupting the AP, which is unaffected either way.
static void staRetryLoop() {
    if (strlen(WIFI_STA_SSID) == 0) return;

    if (WiFi.status() == WL_CONNECTED) {
        if (!staConnected) {
            staConnected = true;
            Serial.println("[OK] Wi-Fi STA: (Re)connected. IP: " + WiFi.localIP().toString());
        }
        return;
    }

    if (staConnected) {
        staConnected = false;
        Serial.println("[WARNING] Wi-Fi STA: Connection lost, will retry.");
    }

    static unsigned long lastStaRetry = 0;
    static const unsigned long STA_RETRY_INTERVAL = 30000;
    if (millis() - lastStaRetry < STA_RETRY_INTERVAL) return;
    lastStaRetry = millis();
    Serial.println("[WiFi] Retrying Wi-Fi STA connection...");
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);
}

void webServerLoop() {
    server.handleClient();
    dnsServer.processNextRequest();
    staRetryLoop();
}
