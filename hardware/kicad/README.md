# AI Railway Gateway V1 — KiCad Hardware Prototype

This is a first physical-interface PCB for the Raspberry Pi 5 railway multi-connectivity gateway.

## Architecture
- Raspberry Pi 5: 40-pin GPIO/control interface
- Three external modem/control headers
- GNSS UART/power header
- 5 V laboratory power input
- Input fuse and power indicator/decoupling
- Four M3 mounting-hole locations are planned in the next layout revision

## Important boundary
This is a laboratory prototype interface design, not a direct railway-power board. Do not connect it directly to train battery/traction power. Use an appropriate regulated 5 V bench supply during development.

The three cellular modems are intentionally external modules. The first prototype expects an external powered USB hub; a custom high-speed USB hub should be a later revision after the exact modem and hub IC are selected.

## Files
- `AI_Railway_Gateway_V1.kicad_pro`
- `AI_Railway_Gateway_V1.sch`
- `AI_Railway_Gateway_V1.kicad_pcb`

## Manufacturing gate
Do not fabricate this revision yet. First import/save the legacy schematic in KiCad 9, verify the exact connector/modem parts and pinouts, complete the net assignments, run ERC and PCB DRC, then generate Gerbers and drill files.
