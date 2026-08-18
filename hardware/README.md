# Hardware — KiCad prototype

KiCad 8 project generated from `HARDWARE_CONNECTIONS.md` / `include/config_hardware.h`. Open `greenhouse_controller.kicad_pro`.

## What's here

- **`schematic.html`** — standalone visual reference (power tree, ESP32-C3 pinout, protection-circuit insets, BOM). Open it directly in any browser, no server needed. Same content as the version published at https://claude.ai/code/artifact/be8fac95-c5c8-4b73-a5d3-b2c1ee22a83d — this copy is the one that stays in sync with the repo.
- **Schematic (KiCad)** — every net from the wiring doc, including the protection circuits (relay 1kΩ series resistors, DS18B20/DHT22/SD pull-ups, flow-meter dividers, solenoid flyback diodes, pump RC snubber). Reference designators are already assigned (U1–U11, J1–J4, R1–R11, C1–C5, D1–D2).
- **PCB** — intentionally blank (default 2-layer board, no footprints, no board outline). It was generated as a starting file, not laid out.

Components are drawn as generic labeled boxes (`local:*` symbols cached directly in the schematic), not real vendor symbols — there's no dependency on any external KiCad library being installed. Nets are wired with global labels rather than point-to-point wires, so the ratsnest is correct even though the drawing itself is a flat functional block diagram, not a tidy hand-routed schematic. Straightening it up visually (`Tools → Clean Up Graphics`, dragging labels) is optional and safe — it won't change connectivity.

**U11 is a bare AMS1117-3.3, not a breakout module** — SOT-223, pin 1 = GND, pin 2 = VOUT, pin 3 = VIN, tab tied to VOUT for heat spreading. Its footprint is already assigned (`Package_TO_SOT_SMD:SOT-223-3_TabPin2`), since the part choice makes it unambiguous — everything else is still blank, see below. It's supported by its own caps rather than relying on a module's onboard ones:
  - C2 (10µF) + C3 (100nF) on VIN → GND — input bulk + HF bypass
  - C4 (22µF) + C5 (100nF) on VOUT → GND — output bulk (stability-critical for this regulator, not just ripple filtering) + HF bypass

## Before you can lay out the PCB

Every symbol except U11 has an **empty Footprint field** — nothing was guessed, since the right footprint depends on parts you haven't bought yet (SD module hole spacing, exact relay module outline, screw terminal pitch, the caps' package size, etc.). In KiCad:

1. `Tools → Assign Footprints` (or the footprint field in the symbol properties) — pick real footprints for each remaining part. For the breadboard-module parts (DS3231, BMP180, SD, relay, buck), the simplest option is a `Connector_PinHeader_2.54mm` footprint matching each module's pin count, mounted on 0.1″ female headers, exactly like the current build. For C2–C5, any small SMD ceramic (0805/1206) or THT electrolytic footprint works — pick whichever matches what you actually buy.
2. `Tools → Update PCB from Schematic` — pushes footprints onto the (currently empty) PCB.
3. Draw a board outline on `Edge.Cuts`, place footprints, route. Give U11's tab/VOUT pad a bit of copper pour for heat dissipation.

## If the schematic fails to open

This file was hand-generated (via a Python script, not KiCad itself) since neither this repo's environment nor the target machine had KiCad installed to round-trip it. The net list was self-checked (every net has ≥2 connections, see generation log), but the file has not been opened in real KiCad. If it throws a parse error on open, tell Claude the exact error — it's much faster to patch than to regenerate from scratch.
