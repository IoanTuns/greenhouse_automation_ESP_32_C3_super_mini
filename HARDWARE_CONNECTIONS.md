# Physical Wiring Guide: Smart Greenhouse Automation (Phase 1)

This document describes the updated electrical schematic and wiring layout for the project using the **ESP32-C3 Super Mini** microcontroller and the **MicroSD Card Module** for data logging. All connections must be made according to the specifications below to prevent component damage.

---

## 1. Power Distribution

The system utilizes a single main **12V DC (minimum 3A)** power source for the solenoids, which is subsequently stepped down for the logic circuits.

- **12V DC Power Source:**
  - `V+ (Plus)` -> Connects to the `IN+` input of the Step-Down module **AND** to the `COM` (Common) terminal of the power relays for the solenoid valves.
  - `V- (GND / Minus)` -> Connects to the `IN-` input of the Step-Down module **AND** to the **Minus (-)** wires of both solenoid valves.
- **Step-Down Module Output (Regulated to stable 5V):**
  - `5V` -> Connects to the `5V` pin on the ESP32-C3 **AND** the `VCC` pin on the 4-channel relay module **AND** the `VCC` pin of the MicroSD Module.
  - `GND` -> Connects to the `G` (GND) pin on the ESP32-C3 **AND** the `GND` pin on the 4-channel relay module **AND** the `GND` pin of the MicroSD Module.

---

## 2. Pin Mapping Table (ESP32-C3 Super Mini)

Follow the GPIO pin numbers labeled on the back of your board. Note that pins 20 and 21 are located on the blue solder pads at the bottom corners on the back of the board.

| Peripheral / Sensor | Component Pin | ESP32-C3 Pin | Signal Type | Important Notes |
| :--- | :--- | :--- | :--- | :--- |
| **DS3231 RTC Clock** | SDA<br>SCL<br>VCC<br>GND | **GPIO 1**<br>**GPIO 0**<br>**3.3V**<br>**G** | Digital (I2C) | Maintains offline time.<br>Must be powered at 3.3V from the ESP32. |
| **MicroSD Card Module** | MISO<br>MOSI<br>SCK<br>CS<br>VCC<br>GND | **GPIO 2**<br>**GPIO 3**<br>**GPIO 4**<br>**GPIO 10**<br>**5V**<br>**G** | Digital (Hardware SPI) | Used for data logging (.csv).<br>Connect VCC to 5V (module has a 3.3V onboard regulator). |
| **4-Channel Relay Module** | IN1 (Pump)<br>IN2 (Valve Z1)<br>IN3 (Valve Z2)<br>VCC<br>GND | **GPIO 5**<br>**GPIO 8**<br>**GPIO 9**<br>**5V**<br>**G** | Digital Output | Relays are active `LOW`. <br>Connect VCC to the 5V source (not 3.3V). |
| **Temp Sensors (DS18B20 x2)** | DATA (Signal)<br>VCC<br>GND | **GPIO 6**<br>**3.3V**<br>**G** | Digital (One-Wire) | Connect both sensors in parallel.<br>Requires a physical 4.7kΩ resistor between DATA and 3.3V. |
| **DHT22 Climate Sensor** | DATA (Signal)<br>VCC<br>GND | **GPIO 7**<br>**3.3V**<br>**G** | Digital Input | Measures greenhouse air temperature and humidity. |
| **Flow Meter Zone 1 (YF-S201)**| Yellow (Signal)<br>Red (+)<br>Black (-) | **GPIO 20**<br>**5V**<br>**G** | Interrupt Input | Located on the back-right corner pad.<br>Powered at 5V for Hall sensor accuracy. |
| **Flow Meter Zone 2 (YF-S201)**| Yellow (Signal)<br>Red (+)<br>Black (-) | **GPIO 21**<br>**5V**<br>**G** | Interrupt Input | Located on the back-left corner pad.<br>Powered at 5V for Hall sensor accuracy. |

---

## 3. Special Hardware Protection Schemes

### A. Solenoid Valve Protection (12V DC)
The inductive load from the solenoid coils can generate high-voltage spikes that damage relay contacts over time.
- **Requirement:** Install a **1N4007 diode** in parallel across the power wires of each solenoid valve.
- **Orientation:** The cathode (the silver band on the diode) must connect to the `Plus (+12V)` wire, and the Anode connects to the `Minus (GND)` wire.

### B. Water Pump Protection (220V AC)
- **Requirement:** To eliminate electric arcing inside the relay when turning the 220V pump on and off, connect an **RC Snubber filter** in parallel across the output contacts (between `COM` and `NO`) of Relay 1.

### C. Pull-Up Resistor for DS18B20
- Without a physical **$4.7\text{ k}\Omega$** resistor connected between the data wire (`GPIO 6`) and the `3.3V` rail, the One-Wire bus will fail, and the board will report faulty temperatures (e.g., `-127 °C`).