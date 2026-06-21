#include "web_server.h"
#include "globals.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config_hardware.h"
#include "config_sistem.h"

static WebServer server(80);
static DNSServer dnsServer;
static const byte DNS_PORT = 53;

static void handleRoot() {
    uint32_t currentPulseZ1, currentPulseZ2;
    noInterrupts();
    currentPulseZ1 = impulsuriZ1;
    currentPulseZ2 = impulsuriZ2;
    interrupts();

    float volumZ1 = (float)currentPulseZ1 / IMPULSURI_PER_LITRU;
    float volumZ2 = (float)currentPulseZ2 / IMPULSURI_PER_LITRU;

    String oraStr = "--:--:--";
    if (rtcAvailable) {
        DateTime now = rtc.now();
        char buf[9];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        oraStr = buf;
    }

    const char* stareStr;
    const char* stareCls;
    switch (stareCurenta) {
        case UDARE_ZONA_1: stareStr = "UDARE ZONA 1"; stareCls = "warn"; break;
        case UDARE_ZONA_2: stareStr = "UDARE ZONA 2"; stareCls = "warn"; break;
        default:           stareStr = "OPRIT";         stareCls = "ok";   break;
    }

    String tempSolStr = isnan(tempSol) ? "N/A" : String(tempSol, 1) + " &deg;C";
    String tempAerStr = isnan(tempAer) ? "N/A" : String(tempAer, 1) + " &deg;C";
    String umidAerStr = isnan(umidAer) ? "N/A" : String(umidAer, 0) + " %";

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

    html += "<div class='header'><h2>Greenhouse Automation</h2><p>" + oraStr + "</p></div>";
    html += "<div class='container'>";

    html += "<div class='card-title'>Stare Sistem</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>Irigare</span><span class='badge " + String(stareCls) + "'>" + String(stareStr) + "</span></div>";
    html += "<div class='menu-item'><span class='label'>Ceas RTC</span><span class='badge " + String(statusRTC == "OK" ? "ok" : "err") + "'>" + statusRTC + "</span></div>";
    html += "<div class='menu-item'><span class='label'>Card MicroSD</span><span class='badge " + String(statusSD == "OK" ? "ok" : "err") + "'>" + statusSD + "</span></div>";
    html += "</div>";

    html += "<div class='card-title'>Senzori</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>Temperatura Sol</span><span class='badge " + String(isnan(tempSol) ? "err" : "info") + "'>" + tempSolStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>Temperatura Aer</span><span class='badge " + String(isnan(tempAer) ? "err" : "info") + "'>" + tempAerStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>Umiditate Aer</span><span class='badge " + String(isnan(umidAer) ? "err" : "info") + "'>" + umidAerStr + "</span></div>";
    html += "<div class='menu-item'><span class='label'>DS18B20</span><span class='badge " + String(statusDS18B20.startsWith("OK") ? "ok" : "err") + "'>" + statusDS18B20 + "</span></div>";
    html += "<div class='menu-item'><span class='label'>DHT22</span><span class='badge " + String(statusDHT22 == "OK" ? "ok" : statusDHT22 == "Initializare..." ? "warn" : "err") + "'>" + statusDHT22 + "</span></div>";
    html += "</div>";

    html += "<div class='card-title'>Volume Apă</div>";
    html += "<div class='menu-card'>";
    html += "<div class='menu-item'><span class='label'>Zona 1</span><span class='badge info'>" + String(volumZ1, 2) + " L / " + String(VOLUM_TINTA_Z1, 0) + " L</span></div>";
    html += "<div class='menu-item'><span class='label'>Zona 2</span><span class='badge info'>" + String(volumZ2, 2) + " L / " + String(VOLUM_TINTA_Z2, 0) + " L</span></div>";
    html += "</div>";

    html += "<a href='/' class='btn-refresh'>REÎMPROSPĂTEAZĂ</a>";
    html += "<div class='footer'>IP: 192.168.4.1 | AP: " + String(WIFI_SSID) + "</div>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
}

static void handleNotFound() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
}

void initWebServer() {
    IPAddress apIP(192, 168, 4, 1);
    IPAddress apGateway(192, 168, 4, 1);
    IPAddress apSubnet(255, 255, 255, 0);
    if (!WiFi.softAPConfig(apIP, apGateway, apSubnet)) {
        Serial.println("[EROARE] Wi-Fi: Configurarea IP Access Point a eșuat!");
    }
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    server.on("/", handleRoot);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("[OK] Wi-Fi: Access Point pornit (" + String(WIFI_SSID) + "). IP: " + WiFi.softAPIP().toString());
}

void webServerLoop() {
    server.handleClient();
    dnsServer.processNextRequest();
}
