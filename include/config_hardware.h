#ifndef CONFIG_HARDWARE_H
#define CONFIG_HARDWARE_H

#include <Arduino.h>

// --- Conectori I2C (Ceas RTC DS3231) ---
// GPIO 6/7 folosite pentru I2C; GPIO 0/1 evitate deoarece perifericul I2C
// al ESP32-C3 nu functioneaza corect pe acele pini pe aceasta placa.
const uint8_t PIN_SDA = 7;
const uint8_t PIN_SCL = 6;

// --- Conectori SPI dedicați Modulului MicroSD ---
const uint8_t PIN_SD_MISO = 2;  // Hardware SPI MISO nativ
const uint8_t PIN_SD_MOSI = 3;  // Hardware SPI MOSI nativ
const uint8_t PIN_SD_SCK  = 4;  // Hardware SPI SCK nativ
const uint8_t PIN_SD_CS   = 10; // Chip Select pentru MicroSD

// --- Conectori Actuatoare (Module Relee - Repoziționați din cauza SPI) ---
const uint8_t PIN_POMPA     = 5;  // Releu 1 - Pompa 220V AC
const uint8_t PIN_VALVA_Z1  = 8;  // Releu 2 - Electrovalva 1 (12V)
const uint8_t PIN_VALVA_Z2  = 9;  // Releu 3 - Electrovalva 2 (12V)

// --- Conectori Senzori Digitali ---
// GPIO 0/1 liberi dupa mutarea I2C; protocoalele One-Wire si DHT functioneaza
// pe orice GPIO, spre deosebire de perifericul hardware I2C.
const uint8_t PIN_Senzori_Temp  = 1;  // One-Wire (Cei doi senzori DS18B20) - GPIO 1
const uint8_t PIN_DHT22         = 0;  // Senzor climat aer DHT22 - GPIO 0

// --- Conectori Debitmetre (Repoziționați pe pinii de pe spatele/colțul plăcii) ---
const uint8_t PIN_DEBIT_Z1  = 20; // Debitmetru Zona 1 (Suportă întreruperi)
const uint8_t PIN_DEBIT_Z2  = 21; // Debitmetru Zona 2 (Suportă întreruperi)

// --- Logică Relee (Active pe LOW) ---
const uint8_t RELEU_PORNIT  = LOW;
const uint8_t RELEU_OPRIT   = HIGH;

#endif // CONFIG_HARDWARE_H