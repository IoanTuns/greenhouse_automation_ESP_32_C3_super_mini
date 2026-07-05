# Project Context: Smart Greenhouse Automation with MicroSD (Phase 1)

This document serves as the baseline instructions and context for any AI Agent or future programming session. The AI must strictly follow the hardware architecture and software structure described below in any code proposal or modification.

## 1. Architecture and Main Constraints
- **Microcontroller:** ESP32-C3 Super Mini (RISC-V architecture).
- **Development environment:** VS Code with the PlatformIO extension (Arduino framework).
- **Code storage:** GitHub Repository.
- **Operating mode:** Fully OFFLINE. The board generates its own local Wi-Fi (Access Point) and hosts a local web page at `192.168.4.1` for monitoring from a phone.
- **Data logging:** Periodically saves data from all sensors and the total pumped volume to the `/date_solar.csv` file on the MicroSD card (FAT32 formatted).
- **Automation logic:** State machine for sequential irrigation based on water volume (flow meters), triggered at a fixed time from the RTC.

## 2. Hardware Configuration & Pin Mapping (`include/config_hardware.h`)
All physical pins are mapped through constants and MUST be referenced in code by the variable names below:

- `PIN_SDA` (GPIO 7) & `PIN_SCL` (GPIO 6) -> I2C communication for the DS3231 RTC clock. GPIO 0/1 avoided — the ESP32-C3's hardware I2C peripheral does not work correctly on those pins on this board.
- `PIN_SD_MISO` (GPIO 2), `PIN_SD_MOSI` (GPIO 3), `PIN_SD_SCK` (GPIO 4), `PIN_SD_CS` (GPIO 10) -> Dedicated SPI bus for the MicroSD card module. GPIO 2 is a strapping pin (HIGH = normal boot) — has a physical 10kΩ pull-up resistor to 3.3V on the MISO line.
- `PIN_PUMP` (GPIO 5) -> Relay 1 (220V AC water pump).
- `PIN_VALVE_Z1` (GPIO 8) -> Relay 2 (Zone 1 solenoid valve - 12V DC).
- `PIN_VALVE_Z2` (GPIO 9) -> Relay 3 (Zone 2 solenoid valve - 12V DC).
- `PIN_TEMP_SENSORS` (GPIO 1) -> One-Wire bus for two waterproof DS18B20 sensors (Soil and Water). Requires a 4.7kΩ pull-up resistor to 3.3V.
- `PIN_DHT22` (GPIO 0) -> Digital sensor for air climate (Temperature + air humidity). Requires a physical 10kΩ pull-up resistor to 3.3V on the DATA line.
- `PIN_FLOW_Z1` (GPIO 20) -> Zone 1 flow meter (YF-S201). Uses a hardware interrupt (`RISING`). The sensor is powered at 5V, and the output signal is 5V — a physical voltage divider on the signal line (R1=10kΩ, R2=20kΩ to GND) brings the signal down to 3.33V before the GPIO.
- `PIN_FLOW_Z2` (GPIO 21) -> Zone 2 flow meter (YF-S201). Uses a hardware interrupt (`RISING`). Same voltage divider as Zone 1.

**Relay logic:** The relay modules used are active `LOW` (`0V` = On, `3.3V/5V` = Off). The code uses the constants `RELAY_ON` and `RELAY_OFF`. Physically: the VCC↔JD-VCC jumper is removed — JD-VCC is powered at 5V (relay coils) and VCC at 3.3V (logic/optocouplers). Each IN line (GPIO 5, 8, 9) has a 1kΩ series resistor to the ESP32-C3 pin.

## 3. System Parameters & Settings (`include/config_system.h`)
- `WIFI_SSID` = "greenhouse"
- `WIFI_PASSWORD` = "!Green2024"
- `START_HOUR` & `START_MINUTE` -> Daily irrigation trigger time.
- `TARGET_VOLUME_Z1` & `TARGET_VOLUME_Z2` -> Water volume in liters allocated to each zone (default 50.0L).
- `PULSES_PER_LITER` = 450.0 -> Calibration factor for the YF-S201 flow meter.

## 4. Modular Source Code Structure

The code is organized into separate modules, each with a single responsibility. `main.cpp` contains only `setup()` and `loop()`.

| Header File | Source File | Responsibility |
| :--- | :--- | :--- |
| `include/globals.h` | `src/globals.cpp` | Defines the `WateringState` enum and all shared global variables (`rtc`, `rtcAvailable`, `tempSoil`, `tempAir`, `humidityAir`, `statusRTC/DS18B20/DHT22/SD`, `pulsesZ1/Z2`, `currentState`). Any module that needs shared data includes `globals.h`. |
| `include/rtc_module.h` | `src/rtc_module.cpp` | Initialization of the I2C bus and the DS3231 RTC module, I2C bus scanning for debugging, automatic retry logic on failure (`rtcRetryLoop()`). |
| `include/sensors.h` | `src/sensors.cpp` | Non-blocking initialization and reading of the DS18B20 (One-Wire) and DHT22 sensors. Updates the global variables `tempSoil`, `tempAir`, `humidityAir` and the status strings. The `DallasTemperature` and `DHT` objects are `static` — local to the module. |
| `include/irrigation.h` | `src/irrigation.cpp` | The irrigation state machine, flow meter ISRs (`IRAM_ATTR`), relay control. Exposes `initRelays()` (called before `Serial.begin()`), `initIrrigation()` (flow meters, after `Serial.begin()`), `irrigationLoop()`, and `stopAll()`. |
| `include/sd_logger.h` | `src/sd_logger.cpp` | MicroSD card initialization, periodic CSV writes every 60s (`sdLoggerLoop()`), and the session report at the end of irrigation (`saveSDReport()`). |
| `include/web_server.h` | `src/web_server.cpp` | Wi-Fi Access Point setup, the Captive Portal DNS server, the HTTP server, and generation of the monitoring HTML page. The `WebServer` and `DNSServer` objects are `static` — local to the module. |

**Important rule:** `initRelays()` MUST be called first in `setup()`, before `Serial.begin()`, to ensure the relays are OFF from the first second of power-up (GPIO 8/9 strapping pins).

## 5. State Machine Logic
The system moves through three states (`enum WateringState`):
1. `STOPPED`: Monitors the sensors and waits for the RTC to reach the start time.
2. `WATERING_ZONE_1`: Closes valve 2, opens valve 1, and starts the pump. Counts pulses from flow meter 1 until the target volume is reached.
3. `WATERING_ZONE_2`: Closes valve 1, opens valve 2 (pump stays on). Counts pulses from flow meter 2. When the volume is reached, stops everything, immediately saves a report to the SD card, and returns to the `STOPPED` state.
