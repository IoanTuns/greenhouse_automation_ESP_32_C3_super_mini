#include "irrigation.h"
#include "globals.h"
#include <Arduino.h>
#include "config_hardware.h"
#include "config_sistem.h"
#include "sd_logger.h"

static unsigned long ultimulStartAttempt = 0;

void IRAM_ATTR isrDebitZ1() { impulsuriZ1++; }
void IRAM_ATTR isrDebitZ2() { impulsuriZ2++; }

void opresteTot() {
    digitalWrite(PIN_POMPA, RELEU_OPRIT);
    digitalWrite(PIN_VALVA_Z1, RELEU_OPRIT);
    digitalWrite(PIN_VALVA_Z2, RELEU_OPRIT);
    stareCurenta = OPRIT;
    Serial.println("[SISTEM] Toate releele sunt OPRIT.");
}

void initRelays() {
    pinMode(PIN_POMPA, OUTPUT);
    pinMode(PIN_VALVA_Z1, OUTPUT);
    pinMode(PIN_VALVA_Z2, OUTPUT);
    digitalWrite(PIN_POMPA, RELEU_OPRIT);
    digitalWrite(PIN_VALVA_Z1, RELEU_OPRIT);
    digitalWrite(PIN_VALVA_Z2, RELEU_OPRIT);
    stareCurenta = OPRIT;
    // Serial not yet started — no print here intentionally
}

void initIrrigation() {
    pinMode(PIN_DEBIT_Z1, INPUT_PULLUP);
    pinMode(PIN_DEBIT_Z2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_DEBIT_Z1), isrDebitZ1, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_DEBIT_Z2), isrDebitZ2, RISING);
    Serial.println("[OK] Relee: Inițializate în stare OPRIT.");
    Serial.println("[OK] Debitmetre: Întreruperi activate (GPIO 20, 21).");
}

void irrigationLoop() {
    uint32_t currentPulseZ1, currentPulseZ2;
    noInterrupts();
    currentPulseZ1 = impulsuriZ1;
    currentPulseZ2 = impulsuriZ2;
    interrupts();

    DateTime now(2000, 1, 1, 0, 0, 0);
    if (rtcAvailable) now = rtc.now();

    switch (stareCurenta) {
        case OPRIT:
            if (rtcAvailable && now.hour() == ORA_PORNIRE && now.minute() == MINUT_PORNIRE
                    && millis() - ultimulStartAttempt >= 60000) {
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
                salveazaRaportSD(
                    (float)currentPulseZ1 / IMPULSURI_PER_LITRU,
                    (float)currentPulseZ2 / IMPULSURI_PER_LITRU
                );
                opresteTot();
            }
            break;
    }
}
