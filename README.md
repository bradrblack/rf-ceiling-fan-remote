# rf-ceiling-fan-remote

Voice control for a NOMA combo ceiling fan + light, driven by directly cloning the RF remote's signal instead of physically pressing its buttons.

## Background

The original version of this project used a relay module wired across the buttons on the fan's remote control, triggered by an ESP32 listening on an Adafruit IO feed (fed via IFTTT/Google Assistant or iOS Shortcuts). It worked, but relays clicking against physical remote buttons is mechanically unreliable, and Google later removed the IFTTT conversational-actions feature that made flexible voice phrases possible.

This version replaces the relay entirely: a CC1101 RF transceiver connected directly to the ESP32 sniffs the remote's actual RF codes once, and then re-transmits them on command. Voice control goes through [SinricPro](https://sinric.pro), which exposes the fan and light as native devices in Google Home/Assistant (no relay, no fixed IFTTT phrases, natural voice commands like "set the fan to medium").

## Hardware

- ESP32-C3 Super Mini
- CC1101 RF transceiver module (433MHz-marketed hardware; this fan's remote actually operates at 304.25MHz, which the CC1101's synthesizer can still tune to directly)
- The fan's original remote control (used once, to sniff its codes)

## Wiring

| CC1101 pin | ESP32-C3 Super Mini pin |
|---|---|
| VCC | 3V3 (**3.3V only** — do not connect to 5V) |
| GND | GND |
| SCK | GPIO4 |
| MOSI | GPIO6 |
| MISO | GPIO5 |
| CS / SS | GPIO7 |
| GDO0 | GPIO3 |

A 100µF capacitor across the CC1101's VCC/GND pins (as close to the module as possible) was needed to get reliable SPI communication once the module started drawing the higher current TX requires — worth including if you build this yourself.

## Project structure

This is a multi-sketch PlatformIO project — each `.ino`/`.cpp` lives in its own subdirectory under `src/`, with a matching `[env]` in `platformio.ini` that filters the build to just that sketch:

| Environment | Source | Purpose |
|---|---|---|
| `c3_mini` (default) | `src/sniff.ino` | Listens on the CC1101 and prints the decimal code, bit length, protocol number, and pulse length for every button pressed on the physical remote. Run this first to capture your own remote's codes. |
| `c3_mini_tx` | `src/tx_test/` | Standalone transmit test — sends a single code on a timer, used for bench-testing range and signal timing independent of SinricPro/WiFi. |
| `c3_mini_fan` | `src/fan_control/` | The production firmware: connects to WiFi and SinricPro, and re-transmits the correct RF code when a fan or light command comes in. |

Build/upload a specific environment with:
```
pio run -e c3_mini_fan -t upload
```

## This fan's captured RF codes

Captured with `sniff.ino` — protocol 11, 12-bit, ~412µs pulse length:

| Button | Decimal code |
|---|---|
| Fan low | 2044 |
| Fan medium | 3836 |
| Fan high | 3964 |
| Fan off | 4028 |
| Light toggle | 3068 |

If you're cloning this for a different remote, re-run `sniff.ino` and update the codes/protocol/pulse length in `src/fan_control/fan_control.cpp`.

## Setup

1. **Sniff your remote's codes** — flash `c3_mini`, open a serial monitor, and press each button once. Note the decimal code, protocol number, and pulse length for each.
2. **Create a SinricPro account** at [sinric.pro](https://sinric.pro) and add two devices:
   - A **Fan (US)** device — supports the 3-speed range this fan uses
   - A **Switch** device — for the light toggle
   (Free tier allows up to 3 devices.)
3. **Fill in credentials** — copy `src/fan_control/secrets.h.example` to `src/fan_control/secrets.h` (gitignored) and fill in your WiFi SSID/password, SinricPro App Key/Secret, and both device IDs.
4. **Flash the production firmware**: `pio run -e c3_mini_fan -t upload`
5. **Link SinricPro to Google Home** from the Google Home app (Works with Google / SinricPro integration) — the fan and light will show up as native devices, controllable by voice with no fixed phrase list.

## Known limitation

The remote only has a single **light toggle** button, not separate on/off codes. The firmware tracks an assumed light state locally and only fires the toggle when a voice command's requested state differs from that tracked state. If the light is ever toggled some other way (the original remote, a wall switch), the tracked state can drift out of sync with reality — there's no feedback path from the fan to detect this.
