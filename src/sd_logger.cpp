#include "sd_logger.h"
#include "globals.h"
#include <SPI.h>
#include <SD.h>
#include "config_hardware.h"
#include "config_sistem.h"

static unsigned long lastSDLog = 0;
static const unsigned long SD_LOG_INTERVAL = 60000;

bool initSD() {
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("[EROARE] SD: Cardul nu a putut fi inițializat!");
        statusSD = "Eroare";
        return false;
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[OK] SD: Card detectat (%llu MB).\n", cardSize);
    statusSD = "OK";
    return true;
}

void salveazaRaportSD(float volZ1, float volZ2) {
    File f = SD.open("/date_solar.csv", FILE_APPEND);
    if (!f) {
        Serial.println("[EROARE] SD: Nu s-a putut deschide /date_solar.csv.");
        return;
    }
    if (f.size() == 0) {
        f.println("Data,Ora,Stare,Temp_Sol_C,Temp_Aer_C,Umiditate_%,Volum_Z1_L,Volum_Z2_L");
    }
    String data = "N/A", ora = "N/A";
    if (rtcAvailable) {
        DateTime now = rtc.now();
        char bufData[11], bufOra[9];
        snprintf(bufData, sizeof(bufData), "%04d-%02d-%02d", now.year(), now.month(), now.day());
        snprintf(bufOra,  sizeof(bufOra),  "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        data = bufData;
        ora  = bufOra;
    }
    f.printf("%s,%s,FINALIZAT,%.2f,%.2f,%.1f,%.2f,%.2f\n",
             data.c_str(), ora.c_str(), tempSol, tempAer, umidAer, volZ1, volZ2);
    f.close();
    Serial.printf("[SD] Raport sesiune: %s %s | Z1=%.2fL Z2=%.2fL | Sol=%.1f°C Aer=%.1f°C\n",
                  data.c_str(), ora.c_str(), volZ1, volZ2, tempSol, tempAer);
}

void sdLoggerLoop() {
    if (statusSD != "OK" || millis() - lastSDLog < SD_LOG_INTERVAL) return;
    lastSDLog = millis();

    String data = "N/A", ora = "N/A";
    if (rtcAvailable) {
        DateTime now = rtc.now();
        char bufData[11], bufOra[9];
        snprintf(bufData, sizeof(bufData), "%04d-%02d-%02d", now.year(), now.month(), now.day());
        snprintf(bufOra,  sizeof(bufOra),  "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        data = bufData;
        ora  = bufOra;
    }
    const char* stareStr = (stareCurenta == UDARE_ZONA_1) ? "ZONA_1" :
                           (stareCurenta == UDARE_ZONA_2) ? "ZONA_2" : "OPRIT";

    uint32_t pulseZ1, pulseZ2;
    noInterrupts();
    pulseZ1 = impulsuriZ1;
    pulseZ2 = impulsuriZ2;
    interrupts();

    File f = SD.open("/date_solar.csv", FILE_APPEND);
    if (f) {
        if (f.size() == 0) {
            f.println("Data,Ora,Stare,Temp_Sol_C,Temp_Aer_C,Umiditate_%,Volum_Z1_L,Volum_Z2_L");
        }
        f.printf("%s,%s,%s,%.2f,%.2f,%.1f,%.2f,%.2f\n",
            data.c_str(), ora.c_str(), stareStr,
            tempSol, tempAer, umidAer,
            (float)pulseZ1 / IMPULSURI_PER_LITRU,
            (float)pulseZ2 / IMPULSURI_PER_LITRU);
        f.close();
    } else {
        Serial.println("[EROARE] SD: Nu s-a putut scrie în /date_solar.csv.");
    }
}
