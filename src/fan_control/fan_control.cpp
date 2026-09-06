#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp32c3/rom/rtc.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>
#include "SinricPro.h"
#include "SinricProFanUS.h"
#include "SinricProSwitch.h"

// Remote family identifiers for FAN2_REMOTE_TYPE (chosen in secrets.h),
// defined before that include so the choice reads normally top-to-bottom.
#define REMOTE_TR313A 1
#define REMOTE_SST12  2

#include "secrets.h"

// Bump this on each flash you want to be able to identify later (e.g. to
// confirm an OTA update actually took) -- format: YYYY-MM-DDrN.
#define FIRMWARE_VERSION "2026-09-05r4"

const char *BANNER =
R"(  __  __         _____
 |  \/  |_   _  |  ___|_ _ _ __
 | |\/| | | | | | |_ / _` | '_ \
 | |  | | |_| | |  _| (_| | | | |
 |_|  |_|\__, | |_|  \__,_|_| |_|
         |___/                  )";

// ==========================================
// CC1101 wiring (ESP32-C3 Super Mini) -- same as sniff.ino / tx_test.cpp
// ==========================================
#define C3_CC1101_CLK   4
#define C3_CC1101_MISO  5
#define C3_CC1101_MOSI  6
#define C3_CC1101_CS    7
#define C3_CC1101_GDO0  3

// Onboard blue LED on most ESP32-C3 Super Mini boards -- active-low (LOW =
// on). Blinks for LED_BLINK_MS every time an RF code is sent, as a visual
// "yes, it's actually transmitting" check. If your board's LED is on a
// different pin or wired active-high, adjust these two.
#define LED_PIN         8
#define LED_ON          LOW
#define LED_OFF         HIGH
#define LED_BLINK_MS    500

// TR313A family constants -- shared by any fan slot that selects
// REMOTE_TR313A. The remote's DIP switches select the address, not a
// different code family, so these values are the same regardless of
// which fan slot uses them.
#define RF_MHZ          304.25
#define RF_PROTOCOL     11
#define RF_PULSE_US     412
#define RF_BITLENGTH    12

// Base codes captured with sniff.ino, with all 4 DIP switches ON (address
// nibble 0000). This remote encodes its DIP switch address as the low 4
// bits of the 12-bit code, OFF=1/ON=0 per switch (switch 1 = bit 3 down to
// switch 4 = bit 0) -- confirmed by sniffing the same 5 buttons across 3
// different switch combinations and finding only that nibble changes.
// Re-run sniff.ino with all switches ON to recapture these for a different
// remote.
#define RF_CODE_FAN_LOW_BASE    2032
#define RF_CODE_FAN_MEDIUM_BASE 3824
#define RF_CODE_FAN_HIGH_BASE   3952
#define RF_CODE_FAN_OFF_BASE    4016
#define RF_CODE_LIGHT_BASE      3056

// SST12 family constants -- shared by any fan slot that selects
// REMOTE_SST12. Captured with sniff.ino at 433.92MHz against a real NOMA
// SST12 remote: frequency, rc-switch protocol number, pulse length, and
// bit length are properties of the SST12 chip itself (confirmed the same
// across that remote's Off/Light/Speed1-6 buttons), so they're shared
// here too -- only the actual per-button codes are unique per paired
// remote/receiver, supplied per-slot as FANn_RF_CODE_* in secrets.h.
#define SST12_RF_MHZ       433.92
#define SST12_RF_PROTOCOL  1
#define SST12_RF_PULSE_US  257
#define SST12_RF_BITLENGTH 32

// Address nibble encoded by a TR313A remote's DIP switches: each switch
// contributes one bit (1->bit3 .. 4->bit0), OFF = 1, ON = 0. OR'd with the
// base codes above to get that fan's actual RF codes.
uint8_t dipAddressNibble(bool sw1On, bool sw2On, bool sw3On, bool sw4On) {
  return (sw1On ? 0 : 8) | (sw2On ? 0 : 4) | (sw3On ? 0 : 2) | (sw4On ? 0 : 1);
}

// ==========================================
// Fan 1 and Fan 2 -- each independently picks its remote family
// ==========================================
// Every combination is supported: both TR313A, both SST12, or one of
// each. FAN1_REMOTE_TYPE defaults to REMOTE_TR313A (fan 1's original,
// still-deployed config) so existing secrets.h files keep working
// unchanged; FAN2_REMOTE_TYPE defaults to REMOTE_SST12.
#ifndef FAN1_REMOTE_TYPE
#define FAN1_REMOTE_TYPE REMOTE_TR313A
#endif
#ifndef FAN2_REMOTE_TYPE
#define FAN2_REMOTE_TYPE REMOTE_SST12
#endif

#if FAN1_REMOTE_TYPE == REMOTE_TR313A
#define FAN1_RF_MHZ       RF_MHZ
#define FAN1_RF_PROTOCOL  RF_PROTOCOL
#define FAN1_RF_PULSE_US  RF_PULSE_US
#define FAN1_RF_BITLENGTH RF_BITLENGTH
#define FAN1_MAX_SPEED    3
#elif FAN1_REMOTE_TYPE == REMOTE_SST12
#define FAN1_RF_MHZ       SST12_RF_MHZ
#define FAN1_RF_PROTOCOL  SST12_RF_PROTOCOL
#define FAN1_RF_PULSE_US  SST12_RF_PULSE_US
#define FAN1_RF_BITLENGTH SST12_RF_BITLENGTH
#define FAN1_MAX_SPEED    6
#else
#error "Unknown FAN1_REMOTE_TYPE -- must be REMOTE_TR313A or REMOTE_SST12"
#endif

#if FAN2_REMOTE_TYPE == REMOTE_TR313A
#define FAN2_RF_MHZ       RF_MHZ
#define FAN2_RF_PROTOCOL  RF_PROTOCOL
#define FAN2_RF_PULSE_US  RF_PULSE_US
#define FAN2_RF_BITLENGTH RF_BITLENGTH
#define FAN2_MAX_SPEED    3
#elif FAN2_REMOTE_TYPE == REMOTE_SST12
#define FAN2_RF_MHZ       SST12_RF_MHZ
#define FAN2_RF_PROTOCOL  SST12_RF_PROTOCOL
#define FAN2_RF_PULSE_US  SST12_RF_PULSE_US
#define FAN2_RF_BITLENGTH SST12_RF_BITLENGTH
#define FAN2_MAX_SPEED    6
#else
#error "Unknown FAN2_REMOTE_TYPE -- must be REMOTE_TR313A or REMOTE_SST12"
#endif

// Fan 1 code storage. TR313A uses DIP_SWITCH_1..4 (unchanged name, for
// backward compatibility with existing secrets.h files); SST12 uses fixed
// per-unit codes from FAN1_RF_CODE_* in secrets.h.
uint32_t rfCodeFan1Off, rfCodeFan1Light;
#if FAN1_REMOTE_TYPE == REMOTE_TR313A
uint32_t rfCodeFan1Low, rfCodeFan1Medium, rfCodeFan1High;
#else
uint32_t rfCodeFan1Speed[FAN1_MAX_SPEED]; // index 0..5 = speed 1..6
#endif

void computeRfCodesFan1() {
#if FAN1_REMOTE_TYPE == REMOTE_TR313A
  uint8_t addr1 = dipAddressNibble(DIP_SWITCH_1, DIP_SWITCH_2, DIP_SWITCH_3, DIP_SWITCH_4);
  rfCodeFan1Low    = RF_CODE_FAN_LOW_BASE    | addr1;
  rfCodeFan1Medium = RF_CODE_FAN_MEDIUM_BASE | addr1;
  rfCodeFan1High   = RF_CODE_FAN_HIGH_BASE   | addr1;
  rfCodeFan1Off    = RF_CODE_FAN_OFF_BASE    | addr1;
  rfCodeFan1Light  = RF_CODE_LIGHT_BASE      | addr1;
#else // REMOTE_SST12 -- fixed codes, no DIP address
  rfCodeFan1Off      = FAN1_RF_CODE_OFF;
  rfCodeFan1Light    = FAN1_RF_CODE_LIGHT;
  rfCodeFan1Speed[0] = FAN1_RF_CODE_SPEED1;
  rfCodeFan1Speed[1] = FAN1_RF_CODE_SPEED2;
  rfCodeFan1Speed[2] = FAN1_RF_CODE_SPEED3;
  rfCodeFan1Speed[3] = FAN1_RF_CODE_SPEED4;
  rfCodeFan1Speed[4] = FAN1_RF_CODE_SPEED5;
  rfCodeFan1Speed[5] = FAN1_RF_CODE_SPEED6;
#endif
}

// Fan 2 code storage -- same shape as fan 1's above.
uint32_t rfCodeFan2Off;
uint32_t rfCodeFan2Light; // only meaningful/sent if LIGHT2_ID is defined
#if FAN2_REMOTE_TYPE == REMOTE_TR313A
uint32_t rfCodeFan2Low, rfCodeFan2Medium, rfCodeFan2High;
#else
uint32_t rfCodeFan2Speed[FAN2_MAX_SPEED]; // index 0..5 = speed 1..6
#endif

void computeRfCodesFan2() {
#if FAN2_REMOTE_TYPE == REMOTE_TR313A
  uint8_t addr2 = dipAddressNibble(FAN2_DIP_SWITCH_1, FAN2_DIP_SWITCH_2,
                                    FAN2_DIP_SWITCH_3, FAN2_DIP_SWITCH_4);
  rfCodeFan2Low    = RF_CODE_FAN_LOW_BASE    | addr2;
  rfCodeFan2Medium = RF_CODE_FAN_MEDIUM_BASE | addr2;
  rfCodeFan2High   = RF_CODE_FAN_HIGH_BASE   | addr2;
  rfCodeFan2Off    = RF_CODE_FAN_OFF_BASE    | addr2;
  rfCodeFan2Light  = RF_CODE_LIGHT_BASE      | addr2;
#else // REMOTE_SST12 -- fixed codes, no DIP address
  rfCodeFan2Off      = FAN2_RF_CODE_OFF;
  rfCodeFan2Light    = FAN2_RF_CODE_LIGHT;
  rfCodeFan2Speed[0] = FAN2_RF_CODE_SPEED1;
  rfCodeFan2Speed[1] = FAN2_RF_CODE_SPEED2;
  rfCodeFan2Speed[2] = FAN2_RF_CODE_SPEED3;
  rfCodeFan2Speed[3] = FAN2_RF_CODE_SPEED4;
  rfCodeFan2Speed[4] = FAN2_RF_CODE_SPEED5;
  rfCodeFan2Speed[5] = FAN2_RF_CODE_SPEED6;
#endif
}

// ==========================================
// Reliability: WiFi watchdog + daily reboot
// ==========================================
// If WiFi can't (re)connect within this long, or stays disconnected this
// long during normal operation, reboot rather than sit there dead.
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;
const unsigned long WIFI_DOWN_REBOOT_MS     = 60000;
unsigned long wifiDownSince = 0;

// WiFi.status() staying connected doesn't mean SinricPro is reachable --
// seen in the field: WiFi stayed associated for over an hour while
// SinricPro itself was unreachable (DNS/TLS failures), with no watchdog
// catching it since that failure is invisible to WiFi.status(). Tracks
// time since the last successful SinricPro connection independently and
// reboots if it's been too long, regardless of WiFi state.
const unsigned long SINRIC_DOWN_REBOOT_MS = 600000; // 10 min
unsigned long sinricDownSince = 0;

// Periodically confirms the CC1101 is still answering over SPI (same
// VERSION-register check used at boot). Catches the radio silently wedging
// mid-operation -- SinricPro/WiFi stay up (Google still hears an ack "beep")
// but sendFan1Code()/sendFan2Code()/onLightPowerState() stop actually
// transmitting anything.
const unsigned long RADIO_CHECK_INTERVAL_MS = 60000;
unsigned long lastRadioCheck = 0;

// Detects the wall clock (time()) jumping relative to uptime (millis()).
// The SNTP client re-syncs periodically in the background, not just at
// boot -- if the RTC came up wrong after a hard reset, a later resync can
// silently snap the clock by minutes or hours, making every log timestamp
// since boot misleading with no visible indication it happened.
const unsigned long CLOCK_CHECK_INTERVAL_MS = 30000;
const long CLOCK_JUMP_THRESHOLD_SEC = 120; // ignore ordinary small NTP drift corrections
unsigned long lastClockCheckMillis = 0;
time_t lastClockCheckTime = 0;

// Scheduled reboot once a day, at a fixed local hour, to guard against slow
// heap fragmentation from a long-running WebSocket/TLS/JSON connection.
// POSIX TZ string (not a fixed UTC offset) so DST transitions are handled
// automatically -- US Eastern: EST=UTC-5, switches to EDT=UTC-4 on the
// 2nd Sunday of March and back on the 1st Sunday of November.
#define TZ_STRING        "EST5EDT,M3.2.0,M11.1.0/2"
#define REBOOT_HOUR      3
const char* NTP_SERVER = "pool.ntp.org";
int lastRebootDay = -1;

// The daily reboot is routine, not a symptom of a problem, so it's excluded
// from ntfy notifications by default -- only the exception reboots (WiFi
// timeout, WiFi watchdog, radio watchdog) are worth a push. Flip to true if
// you want every reboot reported.
#define NOTIFY_DAILY_REBOOT true
#define REASON_DAILY_REBOOT "Daily scheduled reboot"

// ==========================================
// Timestamped logging
// ==========================================
// Prefixes every log line with the synced wall-clock time (once NTP has
// synced) or uptime in seconds (before that / if sync ever fails), so log
// output can be correlated with real time instead of just relative order.
const char *logTimestamp() {
  static char buf[32];
  time_t now = time(nullptr);
  if (now > 100000) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
  } else {
    snprintf(buf, sizeof(buf), "+%lus", millis() / 1000);
  }
  return buf;
}

void logf(const char *fmt, ...) {
  char msg[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  Serial.printf("[%s] %s\r\n", logTimestamp(), msg);
}

// ==========================================
// Reboot reporting (ntfy.sh)
// ==========================================
// The device otherwise self-heals silently -- to see *how often* that's
// happening (rather than just "it's fine again after a power cycle"), the
// reason for each self-triggered reboot is written to NVS flash right
// before restarting, then read back and pushed to ntfy.sh once WiFi is up
// on the next boot. A manual power cycle leaves no reason behind, so it
// stays quiet -- only self-triggered reboots notify.
Preferences prefs;

void recordRebootReason(const char *reason) {
  prefs.begin("fanctl", false);
  prefs.putString("last_reason", reason);
  // The daily reboot isn't a symptom of anything -- only count exception
  // reboots, so "reboot #N" actually signals something went wrong N times.
  if (strcmp(reason, REASON_DAILY_REBOOT) != 0) {
    prefs.putUInt("reboot_count", prefs.getUInt("reboot_count", 0) + 1);
  }
  prefs.end();
}

// Detects a reboot that bypassed our own recordRebootReason() calls entirely
// -- a crash, a hung SSL/network stack triggering the hardware watchdog,
// a brownout, etc. Those resets happen before our code gets a chance to run,
// so without this they're invisible: no log line, no ntfy notification, just
// a gap. Must run before anything else in setup() could call
// recordRebootReason() for this boot cycle (e.g. a WiFi connect timeout),
// since it decides based on whether a reason is already recorded.
void checkUnexpectedReset() {
  prefs.begin("fanctl", true);
  String reason = prefs.getString("last_reason", "");
  prefs.end();
  if (reason.length() > 0) return; // a deliberate reboot already recorded this

  esp_reset_reason_t r = esp_reset_reason();
  // POWERON/EXT are manual power cycles or the reset button -- stay quiet,
  // same as any other manual power cycle. SW is our own ESP.restart(),
  // which would already have recorded a reason if it came from our code.
  if (r == ESP_RST_POWERON || r == ESP_RST_EXT || r == ESP_RST_SW) return;

  // esp_reset_reason() doesn't classify a reset triggered by a serial tool
  // toggling RTS (esptool during flashing, or opening a monitor) on this
  // IDF version -- it falls through to ESP_RST_UNKNOWN alongside genuine
  // crashes. Check the raw hardware reset-reason code directly so flashing/
  // monitoring the device doesn't generate a false "unexpected reset" alert.
  RESET_REASON raw = rtc_get_reset_reason(0);
  if (raw == USB_UART_CHIP_RESET || raw == USB_JTAG_CHIP_RESET) return;

  const char *desc;
  switch (r) {
    case ESP_RST_PANIC:     desc = "crash/panic"; break;
    case ESP_RST_INT_WDT:   desc = "interrupt watchdog"; break;
    case ESP_RST_TASK_WDT:  desc = "task watchdog"; break;
    case ESP_RST_WDT:       desc = "other watchdog"; break;
    case ESP_RST_BROWNOUT:  desc = "brownout"; break;
    case ESP_RST_DEEPSLEEP: desc = "deep sleep wake"; break;
    case ESP_RST_SDIO:      desc = "SDIO"; break;
    default:                desc = "unknown"; break;
  }
  char buf[48];
  snprintf(buf, sizeof(buf), "Unexpected reset (%s)", desc);
  recordRebootReason(buf);
}

void sendNtfy(const String &title, const String &message) {
  if (WiFi.status() != WL_CONNECTED) return;
  logf("[ntfy] Sending: \"%s\" - \"%s\"", title.c_str(), message.c_str());
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, String("https://ntfy.sh/") + NTFY_TOPIC)) return;
  http.addHeader("Title", title);
  int code = http.POST(message);
  logf("[ntfy] POST status: %d", code);
  http.end();
}

void notifyLastReboot() {
  prefs.begin("fanctl", false);
  String reason = prefs.getString("last_reason", "");
  uint32_t count = prefs.getUInt("reboot_count", 0);
  if (reason.length() > 0) {
    prefs.putString("last_reason", ""); // clear so a normal boot stays quiet
  }
  prefs.end();

  if (reason.length() == 0) return;
  if (!NOTIFY_DAILY_REBOOT && reason == REASON_DAILY_REBOOT) return;

  sendNtfy("Fan controller rebooted",
           reason + " (reboot #" + String(count) + ") at " + logTimestamp());
}

RCSwitch myRadio = RCSwitch();

// Fan speed is tracked so onAdjustRangeValue can compute a new absolute value.
int fan1Speed = 0; // 0 = off, 1..3 = low/medium/high
int fan2Speed = 0;

// The physical remote only has a single LIGHT TOGGLE code, not separate
// on/off codes, so we track assumed state locally and only fire the toggle
// when the requested state actually differs from what we last sent. This
// can drift out of sync if the light is ever toggled by another remote.
bool lightState = false;

// Lights the onboard LED for LED_BLINK_MS as visual confirmation an RF
// code was actually sent. Non-blocking: checkLedBlink() (called from
// loop()) turns it back off once the interval elapses, rather than
// stalling command processing/watchdogs with a blocking delay().
bool ledOn = false;
unsigned long ledOnSince = 0;

void ledBlinkStart() {
  digitalWrite(LED_PIN, LED_ON);
  ledOn = true;
  ledOnSince = millis();
}

void checkLedBlink() {
  if (ledOn && millis() - ledOnSince >= LED_BLINK_MS) {
    digitalWrite(LED_PIN, LED_OFF);
    ledOn = false;
  }
}

// Fan 1 and fan 2 can be on different RF families (different frequency,
// rc-switch protocol, and pulse length), so anything transmitting on
// either fan's behalf -- including the light toggles -- must re-tune the
// radio to that fan's family immediately beforehand, rather than assuming
// whatever the radio was last left configured for. Centralized here after
// a real bug: onLightPowerState() sending without re-tuning worked fine
// before fan 2 existed, but silently broke once fan 2's sends could leave
// the radio parked on a different frequency/protocol.
//
// SetTx(mhz), not setMHZ(mhz) -- setMHZ() alone only rewrites the
// frequency registers without re-strobing the chip back into TX state,
// which left the radio stuck out of TX after the first fan2 send (see
// ELECHOUSE_CC1101_SRC_DRV's SetTx(float) implementation: it does
// SIDLE -> setMHZ() -> STX as one atomic sequence).
void tuneRadioForFan1() {
  ELECHOUSE_cc1101.SetTx(FAN1_RF_MHZ);
  myRadio.setProtocol(FAN1_RF_PROTOCOL);
  myRadio.setPulseLength(FAN1_RF_PULSE_US);
}

void tuneRadioForFan2() {
  ELECHOUSE_cc1101.SetTx(FAN2_RF_MHZ);
  myRadio.setProtocol(FAN2_RF_PROTOCOL);
  myRadio.setPulseLength(FAN2_RF_PULSE_US);
}

void sendFan1Code(int speed) {
  tuneRadioForFan1();
#if FAN1_REMOTE_TYPE == REMOTE_TR313A
  switch (speed) {
    case 1: myRadio.send(rfCodeFan1Low, FAN1_RF_BITLENGTH); break;
    case 2: myRadio.send(rfCodeFan1Medium, FAN1_RF_BITLENGTH); break;
    case 3: myRadio.send(rfCodeFan1High, FAN1_RF_BITLENGTH); break;
    default: myRadio.send(rfCodeFan1Off, FAN1_RF_BITLENGTH); break;
  }
#else
  if (speed >= 1 && speed <= FAN1_MAX_SPEED) {
    myRadio.send(rfCodeFan1Speed[speed - 1], FAN1_RF_BITLENGTH);
  } else {
    myRadio.send(rfCodeFan1Off, FAN1_RF_BITLENGTH);
  }
#endif
  ledBlinkStart();
}

void sendFan2Code(int speed) {
  tuneRadioForFan2();
#if FAN2_REMOTE_TYPE == REMOTE_TR313A
  switch (speed) {
    case 1: myRadio.send(rfCodeFan2Low, FAN2_RF_BITLENGTH); break;
    case 2: myRadio.send(rfCodeFan2Medium, FAN2_RF_BITLENGTH); break;
    case 3: myRadio.send(rfCodeFan2High, FAN2_RF_BITLENGTH); break;
    default: myRadio.send(rfCodeFan2Off, FAN2_RF_BITLENGTH); break;
  }
#else
  if (speed >= 1 && speed <= FAN2_MAX_SPEED) {
    myRadio.send(rfCodeFan2Speed[speed - 1], FAN2_RF_BITLENGTH);
  } else {
    myRadio.send(rfCodeFan2Off, FAN2_RF_BITLENGTH);
  }
#endif
  ledBlinkStart();
}

// ==========================================
// SinricPro callbacks
// ==========================================

// "turn on/off the fan" -- the remote has no generic power-on code, so
// powering on defaults to LOW speed.
bool onFan1PowerState(const String &deviceId, bool &state) {
  logf("Fan 1 power: %s", state ? "ON" : "OFF");
  fan1Speed = state ? 1 : 0;
  sendFan1Code(fan1Speed);
  return true;
}

// "set the fan speed" -- range is 1..FAN1_MAX_SPEED
bool onFan1RangeValue(const String &deviceId, int &rangeValue) {
  if (rangeValue < 1) rangeValue = 1;
  if (rangeValue > FAN1_MAX_SPEED) rangeValue = FAN1_MAX_SPEED;
  fan1Speed = rangeValue;
  logf("Fan 1 speed set to %d", fan1Speed);
  sendFan1Code(fan1Speed);
  return true;
}

// relative changes, e.g. "increase the fan speed"
bool onFan1AdjustRangeValue(const String &deviceId, int &rangeValueDelta) {
  fan1Speed = constrain(fan1Speed + rangeValueDelta, 1, FAN1_MAX_SPEED);
  logf("Fan 1 speed adjusted by %d to %d", rangeValueDelta, fan1Speed);
  sendFan1Code(fan1Speed);
  rangeValueDelta = fan1Speed; // must return the new absolute value
  return true;
}

bool onFan2PowerState(const String &deviceId, bool &state) {
  logf("Fan 2 power: %s", state ? "ON" : "OFF");
  fan2Speed = state ? 1 : 0;
  sendFan2Code(fan2Speed);
  return true;
}

bool onFan2RangeValue(const String &deviceId, int &rangeValue) {
  if (rangeValue < 1) rangeValue = 1;
  if (rangeValue > FAN2_MAX_SPEED) rangeValue = FAN2_MAX_SPEED;
  fan2Speed = rangeValue;
  logf("Fan 2 speed set to %d", fan2Speed);
  sendFan2Code(fan2Speed);
  return true;
}

bool onFan2AdjustRangeValue(const String &deviceId, int &rangeValueDelta) {
  fan2Speed = constrain(fan2Speed + rangeValueDelta, 1, FAN2_MAX_SPEED);
  logf("Fan 2 speed adjusted by %d to %d", rangeValueDelta, fan2Speed);
  sendFan2Code(fan2Speed);
  rangeValueDelta = fan2Speed;
  return true;
}

// "turn on/off the light"
bool onLightPowerState(const String &deviceId, bool &state) {
  logf("Light requested: %s (currently tracked as %s)",
       state ? "ON" : "OFF", lightState ? "ON" : "OFF");
  if (state != lightState) {
    tuneRadioForFan1();
    myRadio.send(rfCodeFan1Light, FAN1_RF_BITLENGTH);
    ledBlinkStart();
    lightState = state;
  }
  return true;
}

#ifdef LIGHT2_ID
bool light2State = false;

bool onLight2PowerState(const String &deviceId, bool &state) {
  logf("Light 2 requested: %s (currently tracked as %s)",
       state ? "ON" : "OFF", light2State ? "ON" : "OFF");
  if (state != light2State) {
    tuneRadioForFan2();
    myRadio.send(rfCodeFan2Light, FAN2_RF_BITLENGTH);
    ledBlinkStart();
    light2State = state;
  }
  return true;
}
#endif

// ==========================================
// Setup
// ==========================================

void setupRadio() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);

  ELECHOUSE_cc1101.setSpiPin(C3_CC1101_CLK, C3_CC1101_MISO, C3_CC1101_MOSI, C3_CC1101_CS);

  // Init() brings up SPI.begin() on the custom pins, so it must run before
  // any register read/write, including getCC1101().
  ELECHOUSE_cc1101.Init();

  if (ELECHOUSE_cc1101.getCC1101()) {
    logf("CC1101 Connection: SUCCESS");
  } else {
    logf("CC1101 Connection: FAILED. Check your wiring!");
  }

  ELECHOUSE_cc1101.setMHZ(RF_MHZ);
  ELECHOUSE_cc1101.setModulation(2); // ASK/OOK
  ELECHOUSE_cc1101.SetTx();

  myRadio.enableTransmit(C3_CC1101_GDO0);
  myRadio.setProtocol(RF_PROTOCOL);
  myRadio.setPulseLength(RF_PULSE_US);
}

void setupWiFi() {
  logf("[WiFi]: Connecting");
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  // Cap TX power before connecting -- confirmed fix (on the same board
  // hardware, in the public/test build) for an ESP32-C3 SuperMini issue
  // where full TX power (~19.5dBm peak) browns out the board's onboard
  // regulator during transmit bursts, causing intermittent connect
  // failures. WiFi.mode(WIFI_STA) just ensures the underlying esp_wifi
  // driver is initialized before this call has something to act on.
  WiFi.mode(WIFI_STA);
  esp_wifi_set_max_tx_power(60); // 60 x 0.25dBm = 15dBm, down from ~19.5dBm peak
                                    // (was 40 = 10dBm, confirmed working on the
                                    // public/test build -- raised for more range
                                    // margin, trading back some regulator headroom)
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(250);
    if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {
      Serial.println();
      logf("[WiFi]: Could not connect in time, rebooting...");
      recordRebootReason("WiFi connect timeout at boot");
      delay(100);
      ESP.restart();
    }
  }
  Serial.println();
  logf("[WiFi]: IP-Address is %s", WiFi.localIP().toString().c_str());
}

void setupTime() {
  // Re-kick the SNTP client now that WiFi is actually up. configTzTime()
  // was already called once at the very top of setup(), before WiFi
  // connects -- that early call is there so the TZ is set before any
  // logging happens (see its own comment), but it also starts the SNTP
  // client's very first sync attempt with no network available yet. That
  // attempt fails and the client backs off, so the poll loop below can
  // end up waiting out its whole 10s window before the client's next
  // background retry ever fires, even though the network is fine by
  // then. Calling configTzTime() again here restarts the SNTP client's
  // sync cycle with WiFi already connected, so this poll actually has a
  // sync attempt to catch.
  configTzTime(TZ_STRING, NTP_SERVER);
  logf("[NTP]: Syncing time");
  time_t now = time(nullptr);
  unsigned long start = millis();
  while (now < 100000 && millis() - start < 10000) {
    Serial.print(".");
    delay(250);
    now = time(nullptr);
  }
  Serial.println();

  if (now < 100000) {
    logf("[NTP]: failed to sync (will keep retrying in the background)");
    return;
  }

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
  logf("[NTP]: synced: %s", buf);
}

// Reboots if WiFi has been disconnected for too long. WiFi.setAutoReconnect
// handles brief drops on its own; this is the backstop for when it doesn't.
void checkWiFiWatchdog() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiDownSince == 0) {
      wifiDownSince = millis();
    } else if (millis() - wifiDownSince > WIFI_DOWN_REBOOT_MS) {
      logf("WiFi down too long, rebooting...");
      recordRebootReason("WiFi down watchdog");
      delay(100);
      ESP.restart();
    }
  } else {
    wifiDownSince = 0;
  }
}

// Reboots if SinricPro has been unreachable for too long, independent of
// WiFi.status() (see comment above SINRIC_DOWN_REBOOT_MS). isConnected()
// is a live poll straight through to the underlying WebSocket client, not
// a cached flag, so there's no window where a missed callback would leave
// this stuck on stale state.
void checkSinricWatchdog() {
  if (!SinricPro.isConnected()) {
    if (sinricDownSince == 0) {
      sinricDownSince = millis();
    } else if (millis() - sinricDownSince > SINRIC_DOWN_REBOOT_MS) {
      logf("SinricPro down too long, rebooting...");
      recordRebootReason("SinricPro down watchdog");
      delay(100);
      ESP.restart();
    }
  } else {
    sinricDownSince = 0;
  }
}

// Reboots if the CC1101 stops answering over SPI mid-operation. This is the
// failure mode where SinricPro/WiFi stay connected (Google still hears an
// ack) but sendFan1Code()/sendFan2Code()/onLightPowerState() silently stop
// transmitting.
void checkRadioWatchdog() {
  if (millis() - lastRadioCheck < RADIO_CHECK_INTERVAL_MS) return;
  lastRadioCheck = millis();

  if (!ELECHOUSE_cc1101.getCC1101()) {
    logf("Radio watchdog: CC1101 not responding, rebooting...");
    recordRebootReason("Radio watchdog: CC1101 not responding");
    delay(100);
    ESP.restart();
  }
}

// Logs when the wall clock has moved by more than CLOCK_JUMP_THRESHOLD_SEC
// relative to how much uptime actually elapsed, so a background NTP
// correction is visible in the log instead of just silently making earlier
// timestamps this boot look wrong in hindsight.
void checkClockJump() {
  if (millis() - lastClockCheckMillis < CLOCK_CHECK_INTERVAL_MS) return;
  unsigned long nowMillis = millis();
  time_t now = time(nullptr);

  if (now > 100000 && lastClockCheckTime > 100000) {
    long expectedDeltaSec = (long)((nowMillis - lastClockCheckMillis) / 1000);
    long actualDeltaSec = (long)(now - lastClockCheckTime);
    long jump = actualDeltaSec - expectedDeltaSec;
    if (labs(jump) > CLOCK_JUMP_THRESHOLD_SEC) {
      logf("Clock jump detected: wall clock moved %+ld s relative to uptime", jump);
    }
  }

  lastClockCheckMillis = nowMillis;
  lastClockCheckTime = now;
}

// Reboots once per day at REBOOT_HOUR local time (see TZ_STRING above).
void checkDailyReboot() {
  time_t now = time(nullptr);
  if (now < 100000) return; // NTP hasn't synced yet

  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  if (timeinfo.tm_hour == REBOOT_HOUR && timeinfo.tm_mday != lastRebootDay) {
    lastRebootDay = timeinfo.tm_mday;
    // Persisted to NVS, not just RAM: lastRebootDay would otherwise reset to
    // -1 on the very reboot this triggers, so the next boot's first check
    // (still inside the same REBOOT_HOUR window) would see "not rebooted
    // today yet" and immediately reboot again -- looping for the rest of
    // the hour instead of reboot-once-per-day.
    prefs.begin("fanctl", false);
    prefs.putInt("last_reboot_day", lastRebootDay);
    prefs.end();
    logf("Scheduled daily reboot...");
    recordRebootReason(REASON_DAILY_REBOOT);
    delay(100);
    ESP.restart();
  }
}

void setupSinricPro() {
  SinricProFanUS &myFan = SinricPro[FAN_ID];
  myFan.onPowerState(onFan1PowerState);
  myFan.onRangeValue(onFan1RangeValue);
  myFan.onAdjustRangeValue(onFan1AdjustRangeValue);

  SinricProSwitch &myLight = SinricPro[LIGHT_ID];
  myLight.onPowerState(onLightPowerState);

  // Fan 2's fan and light devices are each independently optional --
  // define FAN2_ID and/or LIGHT2_ID in secrets.h to enable them. This
  // lets you pick fan-only, light-only, or both depending on how many
  // SinricPro devices you want to stay within (see the free-tier device
  // limit).
#ifdef FAN2_ID
  SinricProFanUS &myFan2 = SinricPro[FAN2_ID];
  myFan2.onPowerState(onFan2PowerState);
  myFan2.onRangeValue(onFan2RangeValue);
  myFan2.onAdjustRangeValue(onFan2AdjustRangeValue);
#endif

#ifdef LIGHT2_ID
  SinricProSwitch &myLight2 = SinricPro[LIGHT2_ID];
  myLight2.onPowerState(onLight2PowerState);
#endif

  SinricPro.onConnected([]() { logf("Connected to SinricPro"); });
  SinricPro.onDisconnected([]() { logf("Disconnected from SinricPro"); });

  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println(BANNER);

  // Set the timezone before any logging happens. The RTC survives a soft
  // reset (only a real power loss clears it), so time() can already return
  // a valid epoch at the very start of boot -- without the TZ set yet,
  // early log lines would misinterpret that as UTC/GMT instead of EDT.
  configTzTime(TZ_STRING, NTP_SERVER);

  logf("--- Fan/Light RF Controller v%s ---", FIRMWARE_VERSION);

  prefs.begin("fanctl", true); // read-only
  lastRebootDay = prefs.getInt("last_reboot_day", -1);
  prefs.end();

  checkUnexpectedReset();

  computeRfCodesFan1();
#if FAN1_REMOTE_TYPE == REMOTE_TR313A
  logf("Fan 1 DIP switches %d%d%d%d -> address nibble %d -> low=%u med=%u high=%u off=%u light=%u",
       DIP_SWITCH_1, DIP_SWITCH_2, DIP_SWITCH_3, DIP_SWITCH_4,
       dipAddressNibble(DIP_SWITCH_1, DIP_SWITCH_2, DIP_SWITCH_3, DIP_SWITCH_4),
       rfCodeFan1Low, rfCodeFan1Medium, rfCodeFan1High, rfCodeFan1Off, rfCodeFan1Light);
#else
  logf("Fan 1 (SST12) codes -> off=%u s1=%u s2=%u s3=%u s4=%u s5=%u s6=%u",
       rfCodeFan1Off, rfCodeFan1Speed[0], rfCodeFan1Speed[1], rfCodeFan1Speed[2],
       rfCodeFan1Speed[3], rfCodeFan1Speed[4], rfCodeFan1Speed[5]);
#endif

  computeRfCodesFan2();
#if FAN2_REMOTE_TYPE == REMOTE_TR313A
  logf("Fan 2 (TR313A) DIP switches %d%d%d%d -> low=%u med=%u high=%u off=%u",
       FAN2_DIP_SWITCH_1, FAN2_DIP_SWITCH_2, FAN2_DIP_SWITCH_3, FAN2_DIP_SWITCH_4,
       rfCodeFan2Low, rfCodeFan2Medium, rfCodeFan2High, rfCodeFan2Off);
#else
  logf("Fan 2 (SST12) codes -> off=%u s1=%u s2=%u s3=%u s4=%u s5=%u s6=%u",
       rfCodeFan2Off, rfCodeFan2Speed[0], rfCodeFan2Speed[1], rfCodeFan2Speed[2],
       rfCodeFan2Speed[3], rfCodeFan2Speed[4], rfCodeFan2Speed[5]);
#endif

  setupRadio();
  setupWiFi();
  setupTime();
  notifyLastReboot();
  setupSinricPro();
}

void loop() {
  SinricPro.handle();
  checkWiFiWatchdog();
  checkSinricWatchdog();
  checkRadioWatchdog();
  checkClockJump();
  checkDailyReboot();
  checkLedBlink();
}
