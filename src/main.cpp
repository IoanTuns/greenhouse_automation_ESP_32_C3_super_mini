#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "config_hardware.h"
#include "config_sistem.h"

// Instanțiere obiecte hardware
RTC_DS3231 rtc;
OneWire oneWire(PIN_Senzori_Temp);
DallasTemperature sensors(&oneWire);
DHT dht(PIN_DHT22, DHT22);
WebServer server(80);
DNSServer dnsServer;

const byte DNS_PORT = 53;

// Variabile globale pentru status interfață web
String statusRTC = "Nedetectat";
String statusDS18B20 = "Nedetectat";
String statusDHT22 = "Nedetectat";
String statusSD = "Nedetectat";

bool rtcAvailable = false;
unsigned long lastRtcRetry = 0;
const unsigned long rtcRetryInterval = 10000; // Reîncercare la 10 secunde

// Variabile pentru debitmetre (volatile pentru a fi accesibile corect în întreruperi)
volatile uint32_t impulsuriZ1 = 0;
volatile uint32_t impulsuriZ2 = 0;

enum StareUdare { OPRIT, UDARE_ZONA_1, UDARE_ZONA_2 };
StareUdare stareCurenta = OPRIT;
unsigned long ultimulStartAttempt = 0; // Guard pentru a evita retriggerarea în același minut

// Funcții de întrerupere (ISRs) - IRAM_ATTR le stochează în memoria rapidă
void IRAM_ATTR isrDebitZ1() { impulsuriZ1++; }
void IRAM_ATTR isrDebitZ2() { impulsuriZ2++; }

// Timp pentru citire senzori (non-blocking)
unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 5000; 

void scanI2CBus() {
    Serial.println("[DEBUG] Scanare magistrală I2C...");
    bool foundAny = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t result = Wire.endTransmission();
        if (result == 0) {
            Serial.printf("[DEBUG] Dispozitiv I2C găsit la adresa 0x%02X\n", addr);
            foundAny = true;
        }
    }
    if (!foundAny) {
        Serial.println("[DEBUG] Nu a fost găsit niciun dispozitiv I2C.");
    }
}

bool initRTC() {
    if (!Wire.begin(PIN_SDA, PIN_SCL)) {
        statusRTC = "Eroare I2C";
        Serial.println("[EROARE] I2C: Inițializarea magistralei a eșuat!");
        return false;
    }

    if (!rtc.begin()) {
        statusRTC = "Eroare RTC";
        Serial.println("[EROARE] RTC: Senzorul DS3231 nu a fost detectat la adresa 0x68.");
        scanI2CBus();
        return false;
    }

    statusRTC = "OK";
    Serial.println("[OK] RTC: Modul detectat corect.");
    if (rtc.lostPower()) {
        Serial.println("[AVERTISMENT] RTC: Ceasul a pierdut alimentarea, se resetează timpul...");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    return true;
}

void opresteTot() {
    digitalWrite(PIN_POMPA, RELEU_OPRIT);
    digitalWrite(PIN_VALVA_Z1, RELEU_OPRIT);
    digitalWrite(PIN_VALVA_Z2, RELEU_OPRIT);
    stareCurenta = OPRIT;
    Serial.println("[SISTEM] Toate releele sunt OPRIT.");
}

// Funcție pentru generarea paginii web de status
void handleRoot() {
    // Utilizăm copii atomice ale contorilor pentru a evita citirea incoerentă în timpul întreruperilor
    uint32_t currentPulseZ1;
    uint32_t currentPulseZ2;
    noInterrupts();
    currentPulseZ1 = impulsuriZ1;
    currentPulseZ2 = impulsuriZ2;
    interrupts();

    float volumZ1 = (float)currentPulseZ1 / IMPULSURI_PER_LITRU;
    float volumZ2 = (float)currentPulseZ2 / IMPULSURI_PER_LITRU;

    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<meta name='apple-mobile-web-app-capable' content='yes'>";
    html += "<meta name='mobile-web-app-capable' content='yes'>";
    html += "<meta name='apple-mobile-web-app-status-bar-style' content='black-translucent'>";
    
    html += "<style>";
    html += "body{font-family:'Segoe UI',Roboto,sans-serif; background:#eef2f3; margin:0; padding:0; color:#333;}";
    html += ".header{background:#2c3e50; color:white; padding:20px; text-align:center; box-shadow:0 2px 5px rgba(0,0,0,0.2);}";
    html += ".container{padding:15px; max-width:500px; margin:auto;}";
    html += ".menu-card{background:white; border-radius:12px; overflow:hidden; box-shadow:0 4px 15px rgba(0,0,0,0.1); margin-top:10px;}";
    html += ".menu-item{display:flex; justify-content:space-between; align-items:center; padding:15px 20px; border-bottom:1px solid #eee; transition:background 0.3s;}";
    html += ".menu-item:last-child{border-bottom:none;}";
    html += ".menu-item:active{background:#f9f9f9;}";
    html += ".label{font-weight:600; font-size:16px;}";
    html += ".badge{padding:5px 12px; border-radius:20px; font-size:12px; font-weight:bold; text-transform:uppercase;}";
    html += ".ok{background:#d4edda; color:#155724; border:1px solid #c3e6cb;}";
    html += ".err{background:#f8d7da; color:#721c24; border:1px solid #f5c6cb;}";
    html += ".btn-refresh{display:block; width:100%; padding:15px; margin-top:20px; background:#3498db; color:white; border:none; border-radius:8px; font-weight:bold; cursor:pointer; text-decoration:none; text-align:center;}";
    html += ".footer{text-align:center; font-size:12px; color:#7f8c8d; margin-top:20px;}";
    html += "</style></head><body>";

    html += "<div class='header'><h2>Control Solar</h2></div>";
    html += "<div class='container'>";
    
    html += "<div class='menu-card'>";
    // Element Meniu RTC
    html += "<div class='menu-item'>";
    html += "<span class='label'>🕒 Ceas Sistem (RTC)</span>";
    html += "<span class='badge " + String(statusRTC == "OK" ? "ok" : "err") + "'>" + statusRTC + "</span></div>";
    
    // Element Meniu DS18B20
    html += "<div class='menu-item'>";
    html += "<span class='label'>🌡️ Senzori Sol</span>";
    html += "<span class='badge " + String(statusDS18B20.startsWith("OK") ? "ok" : "err") + "'>" + statusDS18B20 + "</span></div>";
    
    // Element Meniu DHT22
    html += "<div class='menu-item'>";
    html += "<span class='label'>☁️ Senzor Aer</span>";
    html += "<span class='badge " + String(statusDHT22 == "OK" ? "ok" : "err") + "'>" + statusDHT22 + "</span></div>";

    html += "<div class='menu-item'>";
    html += "<span class='label'>💾 Card MicroSD</span>";
    html += "<span class='badge " + String(statusSD == "OK" ? "ok" : "err") + "'>" + statusSD + "</span></div>";

    // Afișare Volume Apă
    html += "<div class='menu-item'>";
    html += "<span class='label'>💧 Volum Zona 1</span>";
    html += "<span class='badge ok'>" + String(volumZ1, 2) + " L</span></div>";
    html += "<div class='menu-item'>";
    html += "<span class='label'>💧 Volum Zona 2</span>";
    html += "<span class='badge ok'>" + String(volumZ2, 2) + " L</span></div>";
    html += "</div>";

    html += "<a href='/' class='btn-refresh'>REÎMPROSPĂTEAZĂ STATUS</a>";
    html += "<div class='footer'>IP Sistem: 192.168.4.1 | Acces Point: Greenhouse</div>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
}

// Redirecționare pentru Captive Portal
void handleNotFound() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
}

void setup() {
    // Pornire comunicație Serială
    Serial.begin(115200);
    while (!Serial) delay(10); // Așteaptă consola pe ESP32-C3 USB CDC
    Serial.println("\n--- Sistem Automatizare Solar Inteligent ---");
    
    // 0. Inițializare Wi-Fi Access Point
    IPAddress apIP(192, 168, 4, 1);
    IPAddress apGateway(192, 168, 4, 1);
    IPAddress apSubnet(255, 255, 255, 0);
    if (!WiFi.softAPConfig(apIP, apGateway, apSubnet)) {
        Serial.println("[EROARE] Wi-Fi: Configurarea IP Access Point a eșuat!");
    }
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP()); // Redirecționează toate cererile DNS către noi

    server.on("/", handleRoot);
    server.onNotFound(handleNotFound); // Orice altă pagină cerută va fi redirecționată la "/"
    server.begin();
    Serial.println("[OK] Wi-Fi: Access Point pornit (" + String(WIFI_SSID) + "). IP: " + WiFi.softAPIP().toString());

    // 1. Validare Magistrală I2C și RTC DS3231
    rtcAvailable = initRTC();

    // 1.1. Validare Card MicroSD (SPI Custom)
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("[EROARE] SD: Cardul nu a putut fi inițializat!");
        statusSD = "Eroare";
    } else {
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("[OK] SD: Card detectat (%llu MB).\n", cardSize);
        statusSD = "OK";
    }

    // 2. Validare Senzori Temperatură DS18B20 (One-Wire)
    sensors.begin();
    int deviceCount = sensors.getDeviceCount();
    if (deviceCount == 0) {
        Serial.println("[EROARE] DS18B20: Nu au fost detectați senzori pe magistrala One-Wire.");
    } else {
        statusDS18B20 = "OK (" + String(deviceCount) + " senzori)";
        Serial.printf("[OK] DS18B20: Am găsit %d senzori pe pinul %d.\n", deviceCount, PIN_Senzori_Temp);
    }

    // 3. Validare Senzor DHT22
    dht.begin();
    float testH = dht.readHumidity();
    float testT = dht.readTemperature();
    if (isnan(testH) || isnan(testT)) {
        Serial.printf("[EROARE] DHT22: Nu s-au putut citi date. Verificați conexiunea pe pinul %d.\n", PIN_DHT22);
    } else {
        statusDHT22 = "OK";
        Serial.printf("[OK] DHT22: Senzor activ (Umiditate: %.1f%%, Temp: %.1f°C).\n", testH, testT);
    }

    // 4. Configurare Relee (Logic Active LOW)
    pinMode(PIN_POMPA, OUTPUT);
    pinMode(PIN_VALVA_Z1, OUTPUT);
    pinMode(PIN_VALVA_Z2, OUTPUT);
    
    opresteTot();
    Serial.println("[OK] Relee: Inițializate în stare OPRIT.");

    // 5. Configurare Debitmetre
    pinMode(PIN_DEBIT_Z1, INPUT_PULLUP);
    pinMode(PIN_DEBIT_Z2, INPUT_PULLUP);

    // Atașare întreruperi pentru numărarea impulsurilor
    attachInterrupt(digitalPinToInterrupt(PIN_DEBIT_Z1), isrDebitZ1, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_DEBIT_Z2), isrDebitZ2, RISING);
    Serial.println("[OK] Debitmetre: Întreruperi activate (GPIO 20, 21).");

    Serial.println("--- Inițializare Completă ---\n");
}

void loop() {
    // Gestionare cereri Web
    server.handleClient();
    // Gestionare cereri DNS pentru Captive Portal
    dnsServer.processNextRequest();

    if (!rtcAvailable && millis() - lastRtcRetry >= rtcRetryInterval) {
        lastRtcRetry = millis();
        if (initRTC()) {
            rtcAvailable = true;
            Serial.println("[OK] RTC: Reîncercare reușită.");
        } else {
            Serial.println("[EROARE] RTC: Reîncercare eșuată, se va încerca din nou în 10 secunde.");
        }
    }

    DateTime now;
    if (rtcAvailable) {
        now = rtc.now();
    }
    uint32_t currentPulseZ1;
    uint32_t currentPulseZ2;
    noInterrupts();
    currentPulseZ1 = impulsuriZ1;
    currentPulseZ2 = impulsuriZ2;
    interrupts();

    switch (stareCurenta) {
        case OPRIT:
            if (rtcAvailable && now.hour() == ORA_PORNIRE && now.minute() == MINUT_PORNIRE && millis() - ultimulStartAttempt >= 60000) {
                Serial.println("[AUTOMATIZARE] Pornire program udat. Zona 1 activă.");
                noInterrupts();
                impulsuriZ1 = 0;
                interrupts();
                digitalWrite(PIN_VALVA_Z2, RELEU_OPRIT);
                digitalWrite(PIN_VALVA_Z1, RELEU_PORNIT);
                digitalWrite(PIN_POMPA, RELEU_PORNIT);
                stareCurenta = UDARE_ZONA_1;
                ultimulStartAttempt = millis();
            }
            break;

        case UDARE_ZONA_1:
            if (((float)currentPulseZ1 / IMPULSURI_PER_LITRU) >= VOLUM_TINTA_Z1) {
                Serial.println("[AUTOMATIZARE] Zona 1 finalizată, trecere la Zona 2.");
                digitalWrite(PIN_VALVA_Z1, RELEU_OPRIT);
                noInterrupts();
                impulsuriZ2 = 0;
                interrupts();
                digitalWrite(PIN_VALVA_Z2, RELEU_PORNIT);
                stareCurenta = UDARE_ZONA_2;
            }
            break;

        case UDARE_ZONA_2:
            if (((float)currentPulseZ2 / IMPULSURI_PER_LITRU) >= VOLUM_TINTA_Z2) {
                Serial.println("[AUTOMATIZARE] Zona 2 finalizată, oprire generală.");
                opresteTot();
            }
            break;
    }
    
    // Monitorizare periodică non-blocking la fiecare 5 secunde
    if (millis() - lastSensorRead >= sensorInterval) {
        lastSensorRead = millis();
        
        if (rtcAvailable) {
            DateTime now = rtc.now();
            Serial.printf("Timp curent: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
        } else {
            Serial.println("[AVERTISMENT] RTC indisponibil; citire senzori fără timestamp.");
        }
        
        sensors.requestTemperatures();
        Serial.printf("Temperatura sol: %.2f°C\n", sensors.getTempCByIndex(0));

        // Log în serial pentru debug debitmetre
        Serial.printf("Debit Z1: %u impulsuri (%.2f L)\n", currentPulseZ1, (float)currentPulseZ1 / IMPULSURI_PER_LITRU);
        Serial.printf("Debit Z2: %u impulsuri (%.2f L)\n", currentPulseZ2, (float)currentPulseZ2 / IMPULSURI_PER_LITRU);
    }
}