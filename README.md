# Greenhouse Automation — ESP32-C3 Super Mini

A fully offline smart-greenhouse controller built on the ESP32-C3 Super Mini. It runs two independent irrigation zones on a schedule and target water volume, logs sensor data to a MicroSD card, and serves a local web dashboard over its own Wi-Fi access point — no internet or app required.

## Features

- **Two-zone irrigation** — each zone has its own daily start time, target volume (liters), and enable/disable switch. Zones share a single pump and never run concurrently.
- **Flow-based dosing** — YF-S201 flow meters measure actual liters delivered per zone rather than watering for a fixed duration.
- **Safety cutoff** — a configurable max-runtime timeout force-stops a zone (and raises a dashboard fault banner) if it fails to reach its target volume, protecting against a stuck valve or dead flow meter.
- **Sensors** — soil & water temperature (DS18B20), air temperature/humidity (DHT22), barometric pressure/altitude (BMP180), all with automatic retry/recovery if a peripheral drops out.
- **Offline web UI** — the board hosts its own Wi-Fi AP and a captive-portal web server at `192.168.4.1`:
  - `/` — live dashboard: sensor readings, per-zone progress, last completed cycle, manual Water Now / Stop Now controls.
  - `/config` — irrigation schedule, target volumes, safety timeout, sensor calibration, DS18B20 sensor-to-role mapping, system date/time, firmware build info.
  - `/connections` — Wi-Fi status, peripheral health, and a registry of devices that have connected to the AP (editable name/type, forget).
  - `/logs` — CSV log file browser (download/delete) plus a human-readable activity audit trail.
- **Optional home-network fallback** — can also join an existing Wi-Fi network (STA mode) alongside its own AP, with automatic reconnect.
- **Data logging** — all sensor readings and pumped volumes are appended to `/date_solar.csv` on the SD card every 60 seconds, using a semicolon/comma-decimal format that opens correctly in Excel under European locales.
- **NVS-persisted settings** — schedules, calibration, and sensor mapping survive reboots and firmware updates without a reflash.

## Hardware

ESP32-C3 Super Mini driving a relay module (pump + 2 solenoid valves), a DS3231 RTC, two DS18B20 temperature probes, a DHT22, a BMP180, two YF-S201 flow meters, and a MicroSD module — powered from a 12V supply stepped down to 5V/3.3V.

Full pin mapping, wiring diagrams, and required protection circuitry (flyback diodes, RC snubber, voltage dividers, pull-ups) are documented in [HARDWARE_CONNECTIONS.md](HARDWARE_CONNECTIONS.md). A KiCad schematic and standalone HTML reference are in [hardware/](hardware/README.md).

## Getting started

This is a [PlatformIO](https://platformio.org/) project (Arduino framework).

1. Install [VS Code](https://code.visualstudio.com/) + the [PlatformIO extension](https://platformio.org/platformio-ide).
2. Open this folder in VS Code and let PlatformIO install the `esp32-c3-devkitm-1` platform and libraries listed in [platformio.ini](platformio.ini).
3. Wire the hardware per [HARDWARE_CONNECTIONS.md](HARDWARE_CONNECTIONS.md).
4. Set `upload_port` in `platformio.ini` to match your board's serial port, then Build & Upload.
5. On first boot the board creates the `greenhouse` Wi-Fi access point (see `WIFI_SSID`/`WIFI_PASSWORD` in `include/config_system.h`). Connect to it and browse to `http://192.168.4.1`.

## Project structure

The firmware is split into single-responsibility modules under `include/`/`src/`; `main.cpp` only contains `setup()`/`loop()`. See [AI_INSTRUCTIONS.md](AI_INSTRUCTIONS.md) for the full module map, the irrigation state machine, and settings reference — it's the authoritative technical spec kept up to date alongside the code.

| Module | Responsibility |
| :--- | :--- |
| `globals` | Shared state (sensor values, status flags, irrigation state) |
| `rtc_module` | DS3231 RTC init + retry |
| `sensors` | DS18B20 / DHT22 / BMP180 reading and health |
| `irrigation` | Zone state machine, flow meter ISRs, relay control, safety timeout |
| `sd_logger` | CSV data logging, log file management |
| `activity_log` | Human-readable audit trail of settings/manual changes |
| `app_settings` | Runtime settings persisted to flash (NVS) |
| `device_registry` | Known Wi-Fi client tracking |
| `web_server` | Access point, captive portal, HTTP server, all web pages |

## Documentation

- [AI_INSTRUCTIONS.md](AI_INSTRUCTIONS.md) — architecture, pin mapping, settings reference, state machine, and planned future work.
- [HARDWARE_CONNECTIONS.md](HARDWARE_CONNECTIONS.md) — full wiring guide and protection circuitry.
- [hardware/](hardware/README.md) — KiCad schematic and standalone HTML hardware reference.

## License

MIT — see [LICENSE](LICENSE).

## Acknowledgments

This project's code was written collaboratively with Google Gemini and Claude (Anthropic).
