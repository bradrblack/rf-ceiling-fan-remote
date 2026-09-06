# Changelog

Notable changes to this project, grouped by theme rather than by individual commit. Dates are when the work landed on `main`; the two production firmware builds (`c3_mini_fan`, `c3_mini_fan_public`) each carry their own `FIRMWARE_VERSION` string (`YYYY-MM-DDrN`) bumped on every flash-worthy change, which you can cross-reference against a device's boot log.

## Unreleased

### Added
- **Fan 2 support in `c3_mini_fan`** — a second, independently-configured fan alongside the original fan+light. Each of fan 1 and fan 2 picks its remote family via `FAN1_REMOTE_TYPE`/`FAN2_REMOTE_TYPE` in `secrets.h`: `REMOTE_TR313A` (the original DIP-switch-addressed family) or `REMOTE_SST12` (a NOMA remote with a dedicated button per speed 1-6, no DIP addressing). Any combination is supported; existing `secrets.h` files keep working unchanged since fan 1 still defaults to TR313A. Fan 2's fan and light SinricPro devices (`FAN2_ID`/`LIGHT2_ID`) are each independently optional.
  - **Experimental for SST12**: only works by impersonating a remote already paired to the fan (sniff its buttons with `sniff.ino`, replay them). Pairing a brand-new remote from scratch against a bare receiver is unsolved. See the README's [Fan 2](README.md#fan-2-experimental-second-fanremote) section.
  - **Not included in `c3_mini_fan_public`** — the shareable/WiFiManager build stays TR313A-only for now.
- **`sniff.ino` raw pulse-timing mode** (`RAW_MODE`, default off) — bypasses rc-switch's built-in protocol matching entirely and dumps raw GDO0 pulse timings, for diagnosing a button that produces no decoded output (tells apart "nothing arriving" from "arriving but not matching a known protocol").
- **`sniff.ino` configurable frequency** (`SNIFF_MHZ`) — previously hardcoded to 304.25MHz; now a single constant to change before reflashing to try a different candidate frequency.

### Fixed
- **Radio left stuck out of TX mode after a frequency change.** `sendFan1Code()`/`sendFan2Code()` were calling the CC1101 driver's `setMHZ()` alone, which only rewrites the frequency registers without re-strobing the chip back into TX state (the driver's `SetTx(float)` does `SIDLE → setMHZ() → STX` as one atomic sequence — `setMHZ()` alone does not). This silently broke fan 1's previously-working transmission once fan 2 sends started retuning the radio out from under it. Centralized into `tuneRadioForFan1()`/`tuneRadioForFan2()`, called before every send (including light toggles), so this can't drift out of sync again.
- Fan 2 RF code storage was `uint16_t`, which would have silently truncated SST12's ~434-million-value codes. Switched to `uint32_t`.

## 2026-08-26 – 2026-08-30 — Custom PCB and 3D-printed case
- Parametric OpenSCAD case design for the ESP32-C3 + CC1101 dongle, iterated through several revisions (snap-fit assembly, button/vent placement, USB cutout, real component dimensions).
- Custom KiCad PCB: socketed ESP32-C3 SuperMini + CC1101 module carrier board.

## 2026-08-22 – 2026-08-25 — Shareable public build
- Added `c3_mini_fan_public` (`src/fan_control_public/`): same RF/reliability behavior as the production firmware, but collects WiFi and SinricPro credentials — plus the remote's DIP switch address — through a WiFiManager captive portal at first boot, so someone with the same fan/remote hardware can flash and configure it themselves with no code changes.
- Hostname/mDNS, ArduinoOTA (WiFi flashing), and a BOOT-button-hold factory reset for the public build.
- Made the public build tolerant of a missing/disconnected CC1101 module (reports the failure instead of hanging).

## 2026-08-10 – 2026-08-22 — Reliability hardening
- WiFi watchdog (reboot if disconnected too long) and a daily scheduled reboot to guard against slow heap fragmentation.
- SinricPro connectivity watchdog, independent of `WiFi.status()` (catches the case where WiFi stays associated but SinricPro itself is unreachable).
- Radio watchdog — periodically re-checks the CC1101 is still answering over SPI, catching the radio silently wedging mid-operation.
- Reboot reason persisted to flash and pushed via [ntfy.sh](https://ntfy.sh) on the next boot, so self-healing reboots are visible instead of silent.
- Detection for resets that bypass the app's own watchdogs entirely (crash/panic, brownout, task/interrupt watchdog).
- Timestamped log output once NTP has synced; background clock-jump detection.

## 2026-08-27 — DIP-switch addressing discovered
- Sniffing the same buttons across multiple DIP switch combinations revealed the remote encodes its 4-position DIP address as the low 4 bits of each 12-bit code. The firmware now computes a fan's actual codes at boot from a shared base table plus the configured switch positions, instead of needing a full separate code set sniffed per fan.

## 2026-08-09 — RF replay replaces the relay-based original
- Replaced the original relay-across-the-remote-buttons approach with a CC1101 RF transceiver that sniffs the remote's real RF codes once and re-transmits them directly. Voice control moved from IFTTT/Adafruit IO (discontinued) to [SinricPro](https://sinric.pro), exposing the fan and light as native Google Home/Assistant devices.

## 2024-07-06 — Original relay-based version
- Initial project: a relay module wired across the physical remote's buttons, triggered by an ESP32 listening on an Adafruit IO feed fed via IFTTT/Google Assistant or iOS Shortcuts.
