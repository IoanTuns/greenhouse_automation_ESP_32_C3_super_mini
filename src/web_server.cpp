#include "web_server.h"
#include "globals.h"
#include "rtc_module.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config_hardware.h"
#include "config_system.h"

static WebServer server(80);
static DNSServer dnsServer;
static const byte DNS_PORT = 53;
static bool staConnected = false;

static void handleRoot() {
    uint32_t currentPulseZ1, currentPulseZ2;
    noInterrupts();
    currentPulseZ1 = pulsesZ1;
    currentPulseZ2 = pulsesZ2;
    interrupts();

    float volumeZ1 = (float)currentPulseZ1 / PULSES_PER_LITER;
    float volumeZ2 = (float)currentPulseZ2 / PULSES_PER_LITER;

    String timeStr = "--:--:--";
    if (rtcAvailable) {
        DateTime now = readRTC();
        char buf[9];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        timeStr = buf;
    }

    const char* stateStr;
    const char* stateCls;
    switch (currentState) {
        case WATERING_ZONE_1: stateStr = "WATERING ZONE 1"; stateCls = "warn"; break;
        case WATERING_ZONE_2: stateStr = "WATERING ZONE 2"; stateCls = "warn"; break;
        default:              stateStr = "STOPPED";          stateCls = "ok";   break;
    }

    String tempSoilStr = isnan(tempSoil) ? "N/A" : String(tempSoil, 1) + " &deg;C";
    String tempAirStr = isnan(tempAir) ? "N/A" : String(tempAir, 1) + " &deg;C";
    String humidityAirStr = isnan(humidityAir) ? "N/A" : String(humidityAir, 0) + " %";

    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta name='apple-mobile-web-app-capable' content='yes'>";
    html += "<meta name='mobile-web-app-capable' content='yes'>";
    html += "<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent'>";
    html += "<style>";
    html += "body{font-family:'Segoe UI',Roboto,sans-serif;background:#eef2f3;margin:0;padding:0;color:#333;}";
    html += ".header{background:#2c3e50;color:white;padding:20px;text-align:center;box-shadow:0 2px 5px rgba(0,0,0,0.2);}";
    html += ".header p{margin:4px 0 0;font-size:14px;opacity:0.75;}";
    html += ".container{padding:15px;max-width:500px;margin:auto;}";
    html += ".card-title{font-size:11px;font-weight:700;color:#7f8c8d;text-transform:uppercase;padding:12px 20px 4px;}";
    html += ".menu-card{background:white;border-radius:12px;overflow:hidden;box-shadow:0 4px 15px rgba(0,0,0,0.1);margin-bottom:5px;}";
    html += ".menu-item{display:flex;justify-content:space-between;align-items:center;padding:14px 20px;border-bottom:1px solid #eee;}";
    html += ".menu-item:last-child{border-bottom:none;}";
    html += ".label{font-weight:600;font-size:15px;}";
    html += ".badge{padding:5px 12px;border-radius:20px;font-size:12px;font-weight:bold;}";
    html += ".ok{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}";
    html += ".err{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}";
    html += ".warn{background:#fff3cd;color:#856404;border:1px solid #ffc107;}";
    html += ".info{background:#d1ecf1;color:#0c5460;border:1px solid #bee5eb;}";
    html += ".btn-refresh{display:block;width:100%;padding:15px;margin-top:15px;background:#3498db;color:white;border:none;border-radius:8px;font-weight:bold;text-decoration:none;text-align:center;box-sizing:border-box;}";
    html += ".footer{text-align:center;font-size:12px;color:#7f8c8d;margin-top:15px;}";
    html += "</style></head><body>";

    html += "<div class='header'><h2>Greenhouse Automation</h2><p>" + timeStr + "</p></div>";
    html += "<div class='container'>";

    html += "<div class='card-title'>System Status</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>Irrigation</span><span class='badge " + String(stateCls) + "'>" + String(stateStr) + "</span></div>";
    html += "<div class='menu-item'><span class='label'>RTC Clock</span><span class='badge " + String(statusRTC == "OK" ? "ok" : "err") + "'>" + statusRTC + "</span></div>";
    html += "<div class='menu-item'><span class='label'>MicroSD Card</span><span class='badge " + String(statusSD == "OK" ? "ok" : "err") + "'>" + statusSD + "</span></div>";
    html += "</div>";

    html += "<div class='card-title'>Sensors</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>Soil Temperature</span><span class='badge " + String(isnan(tempSoil) ? "err" : "info") + "'>" + tempSoilStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>Air Temperature</span><span class='badge " + String(isnan(tempAir) ? "err" : "info") + "'>" + tempAirStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>Air Humidity</span><span class='badge " + String(isnan(humidityAir) ? "err" : "info") + "'>" + humidityAirStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>DS18B20</span><span class='badge " + String(statusDS18B20.startsWith("OK") ? "ok" : "err") + "'>" + statusDS18B20 + "</span></div>";
    html += "<div class='menu-item'><span class='label'>DHT22</span><span class='badge " + String(statusDHT22 == "OK" ? "ok" : statusDHT22 == "Initializing..." ? "warn" : "err") + "'>" + statusDHT22 + "</span></div>";
    html += "</div>";

    html += "<div class='card-title'>Water Volumes</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>Zone 1</span><span class='badge info'>" + String(volumeZ1, 2) + " L / " + String(TARGET_VOLUME_Z1, 0) + " L</span></div>";
    html += "<div class='menu-item'><span class='label'>Zone 2</span><span class='badge info'>" + String(volumeZ2, 2) + " L / " + String(TARGET_VOLUME_Z2, 0) + " L</span></div>";
    html += "</div>";

    html += "<a href='/' class='btn-refresh'>REFRESH</a>";
    html += "<a href='/settime' class='btn-refresh' style='background:#27ae60;margin-top:6px;'>SET TIME</a>";

    String footer = "AP: 192.168.4.1";
    if (staConnected) footer += " | LAN: " + WiFi.localIP().toString();
    html += "<div class='footer'>" + footer + " | AP: " + String(WIFI_SSID) + "</div>";

    html += "</div></body></html>";
    server.send(200, "text/html", html);
}

static void handleSetTimeGet() {
    DateTime now = rtcAvailable ? readRTC() : DateTime(2026, 1, 1, 0, 0, 0);
    char defVal[20];
    snprintf(defVal, sizeof(defVal), "%04d-%02d-%02dT%02d:%02d:%02d",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<style>body{font-family:'Segoe UI',Roboto,sans-serif;background:#eef2f3;padding:20px;color:#333;}";
    html += ".card{background:white;border-radius:12px;padding:20px;box-shadow:0 4px 15px rgba(0,0,0,0.1);max-width:400px;margin:auto;}";
    html += "h3{margin-top:0;color:#2c3e50;}p{font-size:13px;color:#7f8c8d;}";
    html += "input[type=datetime-local]{width:100%;padding:10px;border:1px solid #ccc;border-radius:8px;font-size:16px;box-sizing:border-box;margin:10px 0;}";
    html += "button{width:100%;padding:12px;background:#27ae60;color:white;border:none;border-radius:8px;font-size:16px;font-weight:bold;}";
    html += ".back{display:block;text-align:center;margin-top:12px;color:#3498db;}</style></head><body>";
    html += "<div class='card'><h3>Set System Time</h3>";
    html += "<p>RTC has no battery backup — time resets on each reboot. Set the correct time here and it will be maintained via soft-RTC until the next reboot.</p>";
    html += "<form method='POST' action='/settime'>";
    html += "<input type='datetime-local' name='dt' value='";
    html += defVal;
    html += "' step='1'>";
    html += "<button type='submit'>Apply</button></form>";
    html += "<a class='back' href='/'>Back to Dashboard</a></div></body></html>";
    server.send(200, "text/html", html);
}

static void handleSetTimePost() {
    if (!server.hasArg("dt") || server.arg("dt").length() < 16) {
        server.send(400, "text/plain", "Missing or invalid dt parameter");
        return;
    }
    String dt = server.arg("dt");
    int y  = dt.substring(0,  4).toInt();
    int mo = dt.substring(5,  7).toInt();
    int d  = dt.substring(8,  10).toInt();
    int h  = dt.substring(11, 13).toInt();
    int mi = dt.substring(14, 16).toInt();
    int s  = (dt.length() >= 19) ? dt.substring(17, 19).toInt() : 0;
    if (y < 2020 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59) {
        server.send(400, "text/plain", "Date/time values out of range");
        return;
    }
    syncTimeManual(DateTime((uint16_t)y, (uint8_t)mo, (uint8_t)d,
                             (uint8_t)h, (uint8_t)mi, (uint8_t)s));
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
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
            WiFi.mode(WIFI_AP);
            Serial.println("\n[WARNING] Wi-Fi STA: Connection failed, AP mode enabled.");
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
    server.on("/settime", HTTP_GET,  handleSetTimeGet);
    server.on("/settime", HTTP_POST, handleSetTimePost);
    // Captive-portal probes sent by Android, iOS/macOS, Windows, and Firefox.
    // Adding explicit handlers stops the WebServer from logging
    // "request handler not found" before falling through to onNotFound.
    server.on("/generate_204",        []() { server.send(204, "text/plain", ""); });
    server.on("/connecttest.txt",     []() { server.send(200, "text/plain", "Microsoft NCSI"); });
    server.on("/ncsi.txt",            []() { server.send(200, "text/plain", "Microsoft NCSI"); });
    server.on("/hotspot-detect.html", handleRoot);
    server.on("/success.txt",         []() { server.send(200, "text/plain", "success\n"); });
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("[OK] Wi-Fi AP: Started (" + String(WIFI_SSID) + "). IP: " + WiFi.softAPIP().toString());
}

void webServerLoop() {
    server.handleClient();
    dnsServer.processNextRequest();
}
