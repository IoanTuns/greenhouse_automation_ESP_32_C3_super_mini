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
- **Step-Down AMS1117 (Regulated to stable 3.3V):**
  - `3.3V` -> Connects to the `VCC` pin of the DS3231, DS18B20 sensors, and DHT22. The ESP32-C3's onboard `3.3V` pin and this output are electrically equivalent — either may be used for sensor VCC provided the grounds are common.

---

## 2. Pin Mapping Table (ESP32-C3 Super Mini)

Follow the GPIO pin numbers labeled on the back of your board. Note that pins 20 and 21 are located on the blue solder pads at the bottom corners on the back of the board.

| Peripheral / Sensor | Component Pin | ESP32-C3 Pin | Signal Type | Important Notes |
| :--- | :--- | :--- | :--- | :--- |
| **DS3231 RTC Clock** | SDA<br>SCL<br>VCC<br>GND | **GPIO 1**<br>**GPIO 0**<br>**3.3V**<br>**G** | Digital (I2C) | Maintains offline time.<br>Powered at 3.3V (ESP32 pin or AMS1117 output — both are equivalent). |
| **MicroSD Card Module** | MISO<br>MOSI<br>SCK<br>CS<br>VCC<br>GND | **GPIO 2**<br>**GPIO 3**<br>**GPIO 4**<br>**GPIO 10**<br>**5V**<br>**G** | Digital (Hardware SPI) | Used for data logging (.csv).<br>Connect VCC to 5V (module has a 3.3V onboard regulator).<br>**⚠ GPIO 2 is a boot strapping pin — add a 10kΩ pull-up from MISO to 3.3V. See Section 3F.** |
| **4-Channel Relay Module** | IN1 (Pump)<br>IN2 (Valve Z1)<br>IN3 (Valve Z2)<br>JD-VCC<br>VCC<br>GND | **GPIO 5** via 1kΩ<br>**GPIO 8** via 1kΩ<br>**GPIO 9** via 1kΩ<br>**5V**<br>**3.3V**<br>**G** | Digital Output | Relays are active `LOW`.<br>**⚠ Remove the VCC↔JD-VCC jumper.** Power JD-VCC from 5V (coils) and VCC from 3.3V (logic). Add a 1kΩ series resistor on each IN line. See Section 3D. |
| **Temp Sensors (DS18B20 x2)** | DATA (Signal)<br>VCC<br>GND | **GPIO 6**<br>**3.3V**<br>**G** | Digital (One-Wire) | Connect both sensors in parallel.<br>Requires a physical 4.7kΩ resistor between DATA and 3.3V. |
| **DHT22 Climate Sensor** | DATA (Signal)<br>VCC<br>GND | **GPIO 7**<br>**3.3V**<br>**G** | Digital Input | Measures greenhouse air temperature and humidity.<br>Requires a physical 10kΩ resistor between DATA and 3.3V. See Section 3C. |
| **Flow Meter Zone 1 (YF-S201)**| Yellow (Signal)<br>Red (+)<br>Black (-) | **GPIO 20**<br>**5V**<br>**G** | Interrupt Input | Located on the back-right corner pad.<br>Powered at 5V for Hall sensor accuracy.<br>**⚠ Signal is 5V — voltage divider required before GPIO 20. See Section 3E.** |
| **Flow Meter Zone 2 (YF-S201)**| Yellow (Signal)<br>Red (+)<br>Black (-) | **GPIO 21**<br>**5V**<br>**G** | Interrupt Input | Located on the back-left corner pad.<br>Powered at 5V for Hall sensor accuracy.<br>**⚠ Signal is 5V — voltage divider required before GPIO 21. See Section 3E.** |

---

## 3. Special Hardware Protection Schemes

### A. Solenoid Valve Protection (12V DC)
The inductive load from the solenoid coils can generate high-voltage spikes that damage relay contacts over time.
- **Requirement:** Install a **1N4007 diode** in parallel across the power wires of each solenoid valve.
- **Orientation:** The cathode (the silver band on the diode) must connect to the `Plus (+12V)` wire, and the Anode connects to the `Minus (GND)` wire.

### B. Water Pump Protection (220V AC)
- **Requirement:** To eliminate electric arcing inside the relay when turning the 220V pump on and off, connect an **RC Snubber filter** in parallel across the output contacts (between `COM` and `NO`) of Relay 1.

### C. Pull-Up Resistors for DS18B20 and DHT22
Both sensors use single-wire protocols that require an external pull-up resistor on the DATA line. The ESP32-C3's internal pull-up (~45 kΩ) is too weak for either protocol and must not be relied upon.

- **DS18B20 (GPIO 6):** Install a **4.7 kΩ** resistor between the DATA wire and the `3.3V` rail. Without it, the One-Wire bus will fail and the board will report faulty temperatures (e.g., `-127 °C`).
- **DHT22 (GPIO 7):** Install a **10 kΩ** resistor between the DATA wire and the `3.3V` rail. Without it, the DATA line floats between transmissions, causing CRC errors, all-zero returns, or read timeouts.

### D. Relay Module 3.3V Logic Isolation (JD-VCC Split + Series Resistors)
The relay module's IN pins have onboard ~1 kΩ pull-up resistors to its VCC rail. If VCC is 5V, these pull-ups drive GPIO 8 and GPIO 9 to 5V during the boot window (before firmware configures them as outputs), exceeding the ESP32-C3's 3.6V absolute maximum. GPIO 8 and 9 are also ESP32-C3 boot strapping pins.

**Step 1 — Split JD-VCC from VCC:**
- Remove the jumper bridging VCC and JD-VCC on the relay module.
- Connect **JD-VCC → 5V** (relay coil power, unchanged).
- Connect **VCC → 3.3V** (logic side; pull-ups now reference 3.3V, safe for ESP32-C3).

**Step 2 — Add 1 kΩ series resistors on all IN lines:**

```
GPIO 5  ── R (1kΩ) ── IN1 (Pump)
GPIO 8  ── R (1kΩ) ── IN2 (Valve Z1)
GPIO 9  ── R (1kΩ) ── IN3 (Valve Z2)
```

The series resistors limit ESD clamp current and protect against any residual overvoltage during transitions. Relay triggering is unaffected — total series impedance is well below the optocoupler's threshold.

### E. Voltage Divider for YF-S201 Flow Meter Signal Lines
The YF-S201, when powered at 5V, outputs a signal that swings to 5V HIGH. The ESP32-C3 GPIO input pins have an absolute maximum of 3.6V — connecting the signal wire directly **will destroy the GPIO**. A resistor voltage divider must be installed on each flow meter signal wire before it reaches the ESP32-C3.

- **Components (per flow meter, 4 resistors total):** R1 = **10 kΩ**, R2 = **20 kΩ**
- **Wiring (repeat for both Zone 1 and Zone 2):**

```
YF-S201 Yellow ── R1 (10kΩ) ──┬── GPIO 20 / GPIO 21
                               │
                            R2 (20kΩ)
                               │
                              GND
```

- **Result:** `5V × 20kΩ / (10kΩ + 20kΩ) = 3.33V` at the GPIO pin — within the safe 3.3V operating range.

### F. Pull-Up Resistor on MicroSD MISO (GPIO 2 Strapping Pin)
GPIO 2 on the ESP32-C3 is a boot strapping pin: HIGH at reset = boot from SPI flash (normal); LOW at reset = ROM download mode (firmware never starts). GPIO 2 is also the SPI MISO line connected to the MicroSD module. Before firmware runs and asserts CS, the SD card's MISO output is tri-stated (floating). If the trace floats LOW, the ESP32-C3 enters download mode and appears dead.

- **Requirement:** Add a **10 kΩ** resistor between the MISO line and the 3.3V rail.

```
3.3V ── R (10kΩ) ──┬── GPIO 2 (MISO)
                   │
              SD card DO pin
```

- **At boot:** holds GPIO 2 HIGH → normal firmware boot every time.
- **During SPI operation (CS asserted):** SD card actively drives MISO; the weak 10 kΩ pull-up is easily overridden — signal quality unaffected.
- **Between transactions (CS deasserted):** SD card tri-states MISO; pull-up holds GPIO 2 HIGH safely.