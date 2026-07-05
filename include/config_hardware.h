#ifndef CONFIG_HARDWARE_H
#define CONFIG_HARDWARE_H

#include <Arduino.h>

// ============================================================
// ESP32-C3 Super Mini — GPIO allocation
// ============================================================
//
//  GPIO  | Role           | Notes
//  ------+----------------+-----------------------------------
//    0   | DHT22 data     | One-Wire-compat; any GPIO works
//    1   | DS18B20 data   | One-Wire; any GPIO works
//    2   | SPI MISO       | Hardware SPI (SD card)
//    3   | SPI MOSI       | Hardware SPI (SD card)
//    4   | SPI SCK        | Hardware SPI (SD card)
//    5   | Relay PUMP     | Free GPIO, active-LOW relay
//    6   | I2C SCL        | Pure GPIO, no special function
//    7   | I2C SDA        | Pure GPIO, no special function
//    8   | Relay VALVE_Z1 | Onboard LED lights when Z1 active
//    9   | Relay VALVE_Z2 | BOOT button (pull-up HIGH). Safe:
//         |                | RELAY_OFF=HIGH matches boot state;
//         |                | initRelays() drives HIGH before
//         |                | any other peripheral init.
//   10   | SPI CS         | Hardware SPI CS (SD card)
//   20   | Flow Z1 IRQ    | Interrupt; UART0 free because
//   21   | Flow Z2 IRQ    | ARDUINO_USB_CDC_ON_BOOT=1 routes
//         |                | Serial to USB-CDC (GPIO 18/19)
//
// WHY GPIO 8 IS NO LONGER I2C SDA:
//   The ESP32-C3 Super Mini connects the onboard LED between
//   GPIO 8 and GND via a ~330 Ω resistor (active HIGH).
//   In open-drain I2C mode, when the DS3231 releases SDA to
//   signal a '1' bit, the LED+resistor path loads the pull-up
//   line.  Measured HIGH level ≈ 2.23 V, which is below the
//   ESP32-C3 V_IH threshold (2.48 V) AND the DS3231 V_IH
//   (2.31 V).  Result: the master reads every '1' as '0',
//   requestFrom() returns 0 bytes on every call, and
//   readRTC() always returns the fallback DateTime(2000-01-01
//   00:00:00) — displayed as [00:00:00] forever.
//   GPIO 7 has no LED and no interference; use that instead.
//
// WHY GPIO 22 IS NO LONGER USED FOR RELAYS:
//   ESP32-C3 has GPIO 0–21 only.  Writing to GPIO 22 is a
//   silent no-op; every relay output was broken.
// ============================================================

// --- I2C (DS3231 RTC) ---
const uint8_t PIN_SDA = 7;   // was 8 — LED pin, loaded SDA below V_IH
const uint8_t PIN_SCL = 6;   // was 9 — BOOT button, unnecessary conflict

// --- SPI (MicroSD card) ---
const uint8_t PIN_SD_MISO = 2;
const uint8_t PIN_SD_MOSI = 3;
const uint8_t PIN_SD_SCK  = 4;
const uint8_t PIN_SD_CS   = 10;

// --- Relays (active LOW: relay energises on LOW, off on HIGH) ---
const uint8_t PIN_PUMP    = 5;   // 220 V AC pump         (was GPIO 22 — invalid)
const uint8_t PIN_VALVE_Z1 = 8;  // Zone-1 solenoid 12 V  (was GPIO 22 — invalid)
const uint8_t PIN_VALVE_Z2 = 9;  // Zone-2 solenoid 12 V  (was GPIO 22 — invalid)

// --- Digital sensors ---
const uint8_t PIN_TEMP_SENSORS = 1;  // DS18B20 One-Wire bus
const uint8_t PIN_DHT22        = 0;  // DHT22 climate sensor

// --- Flow meters ---
const uint8_t PIN_FLOW_Z1 = 20;
const uint8_t PIN_FLOW_Z2 = 21;

// --- Relay logic ---
const uint8_t RELAY_ON  = LOW;
const uint8_t RELAY_OFF = HIGH;

#endif // CONFIG_HARDWARE_H
