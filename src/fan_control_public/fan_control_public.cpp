#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
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

// Bump this on each flash you want to be able to identify later (e.g. to
// confirm an OTA update actually took) -- format: YYYY-MM-DDrN.
#define FIRMWARE_VERSION "2026-08-27r1"

const char *BANNER =
R"(  __  __         _____
 |  \/  |_   _  |  ___|_ _ _ __
 | |\/| | | | | | |_ / _` | '_ \
 | |  | | |_| | |  _| (_| | | | |
 |_|  |_|\__, | |_|  \__,_|_| |_|
         |___/                  )";

// ==========================================
// "For others" build
// ==========================================
// Same fan/light RF control and reliability watchdogs as c3_mini_fan
// (src/fan_control/fan_control.cpp), but no secrets.h: WiFi and SinricPro
// credentials are entered at first boot via a WiFiManager captive portal
// instead of being compiled in, so this can be shared/flashed by someone
// else with the same fan/remote hardware without editing source. No ntfy
// reboot notifications in this build -- reboot reasons still get logged to
// serial, just not pushed anywhere.

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

// BOOT button on most ESP32-C3 boards -- confirm yours has one before
// relying on this. Held for FACTORY_RESET_HOLD_MS during normal operation,
// it wipes saved WiFi + SinricPro config and reboots into the setup portal
// (e.g. to move the device to a new WiFi network, or to reach WiFiManager's
// built-in "Update" page to flash new firmware over the air with no cable).
#define FACTORY_RESET_PIN      9
#define FACTORY_RESET_HOLD_MS  10000

#define RF_MHZ          304.25
#define RF_PROTOCOL     11
#define RF_PULSE_US     412
#define RF_BITLENGTH    12

// Base codes captured with sniff.ino, with all 4 DIP switches ON (address
// nibble 0000) -- specific to this fan/remote model. If you're cloning this
// for a different remote, re-run sniff.ino with all switches ON and update
// these. This remote encodes its DIP switch address as the low 4 bits of
// the 12-bit code, OFF=1/ON=0 per switch (switch 1 = bit 3 down to switch 4
// = bit 0) -- confirmed by sniffing the same 5 buttons across 3 different
// switch combinations and finding only that nibble changes. The actual
// per-fan codes are computed once the portal's DIP switch fields are known
// (see computeRfCodes()), by OR-ing this base with the address nibble.
#define RF_CODE_FAN_LOW_BASE    2032
#define RF_CODE_FAN_MEDIUM_BASE 3824
#define RF_CODE_FAN_HIGH_BASE   3952
#define RF_CODE_FAN_OFF_BASE    4016
#define RF_CODE_LIGHT_BASE      3056

// Address nibble encoded by the remote's DIP switches: each switch
// contributes one bit (1->bit3 .. 4->bit0), OFF = 1, ON = 0. OR'd with the
// base codes above to get this fan's actual RF codes.
uint8_t dipAddressNibble(bool sw1On, bool sw2On, bool sw3On, bool sw4On) {
  return (sw1On ? 0 : 8) | (sw2On ? 0 : 4) | (sw3On ? 0 : 2) | (sw4On ? 0 : 1);
}

uint16_t rfCodeFanLow, rfCodeFanMedium, rfCodeFanHigh, rfCodeFanOff, rfCodeLight;

// ==========================================
// Reliability: WiFi + SinricPro + radio watchdogs, daily reboot
// ==========================================
const unsigned long WIFI_DOWN_REBOOT_MS = 60000;
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
// but sendFanCode()/onLightPowerState() stop actually transmitting anything.
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
// 2nd Sunday of March and back on the 1st Sunday of November. Configurable
// in the setup portal (see tzString below) like the other per-device
// fields; this is just the value a fresh device starts with.
#define DEFAULT_TZ_STRING "EST5EDT,M3.2.0,M11.1.0/2"
#define REBOOT_HOUR      3
const char* NTP_SERVER = "pool.ntp.org";
int lastRebootDay = -1;

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
// Reboot reason tracking (serial log only -- no ntfy in this build)
// ==========================================
// The device otherwise self-heals silently -- the reason for each self-
// triggered reboot is written to NVS flash right before restarting, then
// read back and logged once WiFi is up on the next boot, so you can tell
// *why* it last rebooted by checking serial. A manual power cycle leaves no
// reason behind, so it stays quiet.
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
// so without this they're invisible: no log line, just a gap. Must run
// before anything else in setup() could call recordRebootReason() for this
// boot cycle, since it decides based on whether a reason is already
// recorded.
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
  // monitoring the device doesn't generate a false "unexpected reset" log.
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

void logLastRebootReason() {
  prefs.begin("fanctl", false);
  String reason = prefs.getString("last_reason", "");
  uint32_t count = prefs.getUInt("reboot_count", 0);
  if (reason.length() > 0) {
    prefs.putString("last_reason", ""); // clear so a normal boot stays quiet
  }
  prefs.end();

  if (reason.length() == 0) return;
  logf("Last reboot reason: %s (reboot #%u)", reason.c_str(), count);
}

RCSwitch myRadio = RCSwitch();

// Set once in setupRadio() based on whether the CC1101 actually responded.
// Lets WiFi/SinricPro/portal testing proceed on a bare ESP32-C3 with no
// CC1101 attached -- fan/light RF sends become no-ops instead of calling
// into unconfigured/absent hardware, and the radio watchdog (which would
// otherwise reboot forever over a known-missing chip) stays quiet.
bool radioAvailable = false;

// Fan speed is tracked so onAdjustRangeValue can compute a new absolute value.
int fanSpeed = 0; // 0 = off, 1..3 = low/medium/high

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

// Holding FACTORY_RESET_PIN low for FACTORY_RESET_HOLD_MS wipes saved WiFi
// + SinricPro config (the "fanwm" Preferences namespace) and reboots into
// the WiFiManager setup portal. WiFi.disconnect(true, true) mirrors what
// WiFiManager's own resetSettings() does on ESP32 -- done directly here
// since the WiFiManager instance itself is local to setupWiFiManager() and
// out of scope by the time loop() is running.
unsigned long resetButtonDownSince = 0;

void checkFactoryResetButton() {
  if (digitalRead(FACTORY_RESET_PIN) == LOW) {
    if (resetButtonDownSince == 0) {
      resetButtonDownSince = millis();
      logf("Factory reset button pressed -- hold for %lus to reset", FACTORY_RESET_HOLD_MS / 1000);
    } else if (millis() - resetButtonDownSince > FACTORY_RESET_HOLD_MS) {
      logf("Factory reset button held, wiping WiFi/SinricPro config and rebooting...");
      for (int i = 0; i < 6; i++) {
        digitalWrite(LED_PIN, LED_ON);
        delay(80);
        digitalWrite(LED_PIN, LED_OFF);
        delay(80);
      }
      prefs.begin("fanwm", false);
      prefs.clear();
      prefs.end();
      // Must be in STA mode before disconnect(true, true) will actually
      // erase the saved WiFi credentials -- matches what WiFiManager's own
      // resetSettings() does internally ("must be sta to disconnect
      // erase"). Without this, the erase silently doesn't take effect and
      // the next boot reconnects using the old saved AP instead of opening
      // the portal.
      WiFi.mode(WIFI_STA);
      delay(500);
      WiFi.disconnect(true, true);
      delay(100);
      ESP.restart();
    }
  } else {
    resetButtonDownSince = 0;
  }
}

void sendFanCode(int speed) {
  if (!radioAvailable) {
    logf("Radio not available, skipping fan RF send (speed %d)", speed);
    return;
  }
  switch (speed) {
    case 1: myRadio.send(rfCodeFanLow, RF_BITLENGTH); break;
    case 2: myRadio.send(rfCodeFanMedium, RF_BITLENGTH); break;
    case 3: myRadio.send(rfCodeFanHigh, RF_BITLENGTH); break;
    default: myRadio.send(rfCodeFanOff, RF_BITLENGTH); break;
  }
  ledBlinkStart();
}

// ==========================================
// SinricPro callbacks
// ==========================================

// "turn on/off the fan" -- the remote has no generic power-on code, so
// powering on defaults to LOW speed.
bool onFanPowerState(const String &deviceId, bool &state) {
  logf("Fan power: %s", state ? "ON" : "OFF");
  if (state) {
    fanSpeed = 1;
    sendFanCode(fanSpeed);
  } else {
    fanSpeed = 0;
    sendFanCode(fanSpeed);
  }
  return true;
}

// "set the fan to low/medium/high" -- range is 1..3
bool onFanRangeValue(const String &deviceId, int &rangeValue) {
  if (rangeValue < 1) rangeValue = 1;
  if (rangeValue > 3) rangeValue = 3;
  fanSpeed = rangeValue;
  logf("Fan speed set to %d", fanSpeed);
  sendFanCode(fanSpeed);
  return true;
}

// relative changes, e.g. "increase the fan speed"
bool onFanAdjustRangeValue(const String &deviceId, int &rangeValueDelta) {
  fanSpeed = constrain(fanSpeed + rangeValueDelta, 1, 3);
  logf("Fan speed adjusted by %d to %d", rangeValueDelta, fanSpeed);
  sendFanCode(fanSpeed);
  rangeValueDelta = fanSpeed; // must return the new absolute value
  return true;
}

// "turn on/off the light"
bool onLightPowerState(const String &deviceId, bool &state) {
  logf("Light requested: %s (currently tracked as %s)",
       state ? "ON" : "OFF", lightState ? "ON" : "OFF");
  if (state != lightState) {
    if (radioAvailable) {
      myRadio.send(rfCodeLight, RF_BITLENGTH);
      ledBlinkStart();
    } else {
      logf("Radio not available, skipping light RF send");
    }
    lightState = state;
  }
  return true;
}

// ==========================================
// WiFi + SinricPro credential provisioning (WiFiManager)
// ==========================================
// Collects WiFi and SinricPro credentials at first boot via a captive
// portal instead of compiling them in from secrets.h. If saved WiFi
// credentials ever fail to connect (e.g. the device is moved to a new
// network), WiFiManager automatically reopens the portal.
#define SETUP_AP_NAME "FanControllerSetup"
#define WIFI_PORTAL_TIMEOUT_SEC 300 // give up and retry rather than hold the portal open forever
#define DEFAULT_HOSTNAME "myfan"

char appKey[40] = "";
char appSecret[96] = "";
char fanId[36] = "";
char lightId[36] = "";
// Used as both the DHCP/mDNS hostname (reachable at <hostname>.local) and
// the ArduinoOTA device name. Defaults to "myfan"; overridable in the
// portal like the SinricPro fields. A change made in the same portal
// session as initial WiFi setup won't take effect until the next connect,
// since WiFi.setHostname() has to run before autoConnect() -- i.e. before
// this field's new value is even readable.
char hostname[32] = DEFAULT_HOSTNAME;
// This fan's remote's DIP switch positions, as typed into the portal ("on"
// or "off", case-insensitive -- anything else is treated as "off"). Needed
// because different physical fans on the same remote/protocol are only
// distinguished by this address -- get it wrong and commands go to the
// wrong fan. See RF_CODE_*_BASE and dipAddressNibble() above.
char dip1[8] = "off";
char dip2[8] = "off";
char dip3[8] = "off";
char dip4[8] = "off";
// POSIX TZ string (see DEFAULT_TZ_STRING above for the format/example).
// Loaded once at the very top of setup(), before anything logs a
// timestamp, and again -- possibly updated -- once the portal has run;
// see the two configTzTime() calls in setup()/setupTime().
char tzString[48] = DEFAULT_TZ_STRING;

bool shouldSaveSinricConfig = false;
void onSaveSinricConfig() { shouldSaveSinricConfig = true; }

void loadSinricConfig() {
  prefs.begin("fanwm", true);
  prefs.getString("app_key", appKey, sizeof(appKey));
  prefs.getString("app_secret", appSecret, sizeof(appSecret));
  prefs.getString("fan_id", fanId, sizeof(fanId));
  prefs.getString("light_id", lightId, sizeof(lightId));
  prefs.getString("hostname", hostname, sizeof(hostname));
  prefs.getString("dip1", dip1, sizeof(dip1));
  prefs.getString("dip2", dip2, sizeof(dip2));
  prefs.getString("dip3", dip3, sizeof(dip3));
  prefs.getString("dip4", dip4, sizeof(dip4));
  prefs.getString("tz", tzString, sizeof(tzString));
  prefs.end();
}

void saveSinricConfig() {
  prefs.begin("fanwm", false);
  prefs.putString("app_key", appKey);
  prefs.putString("app_secret", appSecret);
  prefs.putString("fan_id", fanId);
  prefs.putString("light_id", lightId);
  prefs.putString("hostname", hostname);
  prefs.putString("dip1", dip1);
  prefs.putString("dip2", dip2);
  prefs.putString("dip3", dip3);
  prefs.putString("dip4", dip4);
  prefs.putString("tz", tzString);
  prefs.end();
}

// "on"/"1"/"true" (case-insensitive) -> true, anything else (including
// empty) -> false, so a blank/unset field defaults to OFF rather than
// silently misbehaving.
bool parseSwitchOn(const char *value) {
  return strcasecmp(value, "on") == 0 || strcasecmp(value, "1") == 0 || strcasecmp(value, "true") == 0;
}

void computeRfCodes() {
  uint8_t addr = dipAddressNibble(parseSwitchOn(dip1), parseSwitchOn(dip2),
                                    parseSwitchOn(dip3), parseSwitchOn(dip4));
  rfCodeFanLow    = RF_CODE_FAN_LOW_BASE    | addr;
  rfCodeFanMedium = RF_CODE_FAN_MEDIUM_BASE | addr;
  rfCodeFanHigh   = RF_CODE_FAN_HIGH_BASE   | addr;
  rfCodeFanOff    = RF_CODE_FAN_OFF_BASE    | addr;
  rfCodeLight     = RF_CODE_LIGHT_BASE      | addr;
  logf("DIP switches %s/%s/%s/%s -> address nibble %d -> low=%u med=%u high=%u off=%u light=%u",
       dip1, dip2, dip3, dip4, addr,
       rfCodeFanLow, rfCodeFanMedium, rfCodeFanHigh, rfCodeFanOff, rfCodeLight);
}

void setupWiFiManager() {
  loadSinricConfig();

  WiFiManagerParameter customAppKey("appkey", "SinricPro App Key", appKey, sizeof(appKey));
  WiFiManagerParameter customAppSecret("appsecret", "SinricPro App Secret", appSecret, sizeof(appSecret));
  WiFiManagerParameter customFanId("fanid", "SinricPro Fan Device ID", fanId, sizeof(fanId));
  WiFiManagerParameter customLightId("lightid", "SinricPro Light Device ID", lightId, sizeof(lightId));
  WiFiManagerParameter customHostname("hostname", "Device hostname (e.g. myfan -&gt; myfan.local)", hostname, sizeof(hostname));
  WiFiManagerParameter customDip1("dip1", "Remote DIP switch 1 (on/off)", dip1, sizeof(dip1));
  WiFiManagerParameter customDip2("dip2", "Remote DIP switch 2 (on/off)", dip2, sizeof(dip2));
  WiFiManagerParameter customDip3("dip3", "Remote DIP switch 3 (on/off)", dip3, sizeof(dip3));
  WiFiManagerParameter customDip4("dip4", "Remote DIP switch 4 (on/off)", dip4, sizeof(dip4));
  WiFiManagerParameter customTz("tz", "Timezone (POSIX TZ string, e.g. EST5EDT,M3.2.0,M11.1.0/2)", tzString, sizeof(tzString));

  WiFiManager wm;
  wm.setTitle("Fan Controller");
  wm.setSaveConfigCallback(onSaveSinricConfig);
  wm.addParameter(&customAppKey);
  wm.addParameter(&customAppSecret);
  wm.addParameter(&customFanId);
  wm.addParameter(&customLightId);
  wm.addParameter(&customHostname);
  wm.addParameter(&customDip1);
  wm.addParameter(&customDip2);
  wm.addParameter(&customDip3);
  wm.addParameter(&customDip4);
  wm.addParameter(&customTz);
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SEC);

  // Force a clean radio reset before autoConnect() decides whether to try
  // a normal STA connect or fall back to the AP/portal. Without this, a
  // phone joining the SoftAP right as it starts (transitioning from a
  // failed/nonexistent STA attempt) can time out on DHCP and self-assign a
  // 169.254.x.x link-local address instead of getting a real lease -- a
  // commonly-reported WiFiManager symptom, seen in the field on this
  // build's first setup attempt.
  WiFi.mode(WIFI_OFF);
  delay(200);

  // Cap TX power before any connection attempt. Full power (~19.5dBm
  // peak) can brown out this board's onboard voltage regulator during
  // transmit bursts -- a known cause of intermittent WiFi connect
  // failures on ESP32-C3 SuperMini boards. Grounding the floating
  // GPIO21 pin didn't fix this board's connect issue, so trying this
  // next. Setting mode to STA here (ahead of whatever mode autoConnect()
  // itself switches to) is just to get the underlying esp_wifi driver
  // initialized so this call has something to act on -- the power cap
  // itself isn't tied to a particular mode and persists through
  // whatever autoConnect() does afterward.
  WiFi.mode(WIFI_STA);
  esp_wifi_set_max_tx_power(60); // 60 x 0.25dBm = 15dBm, down from ~19.5dBm peak
                                    // (was 40 = 10dBm, confirmed working -- raised
                                    // for more range margin, trading back some of
                                    // the regulator headroom that fixed it)

  // Must happen before autoConnect(), since that's what actually associates
  // and does the DHCP handshake (which is when the hostname gets sent).
  WiFi.setHostname(hostname);

  logf("[WiFi]: Connecting (or opening \"%s\" setup portal if needed)", SETUP_AP_NAME);
  if (!wm.autoConnect(SETUP_AP_NAME)) {
    logf("[WiFi]: Setup portal timed out with no connection, rebooting...");
    recordRebootReason("WiFiManager portal timeout");
    delay(100);
    ESP.restart();
  }
  logf("[WiFi]: IP-Address is %s", WiFi.localIP().toString().c_str());

  strncpy(appKey, customAppKey.getValue(), sizeof(appKey) - 1);
  appKey[sizeof(appKey) - 1] = '\0';
  strncpy(appSecret, customAppSecret.getValue(), sizeof(appSecret) - 1);
  appSecret[sizeof(appSecret) - 1] = '\0';
  strncpy(fanId, customFanId.getValue(), sizeof(fanId) - 1);
  fanId[sizeof(fanId) - 1] = '\0';
  strncpy(lightId, customLightId.getValue(), sizeof(lightId) - 1);
  lightId[sizeof(lightId) - 1] = '\0';
  strncpy(hostname, customHostname.getValue(), sizeof(hostname) - 1);
  hostname[sizeof(hostname) - 1] = '\0';
  strncpy(dip1, customDip1.getValue(), sizeof(dip1) - 1);
  dip1[sizeof(dip1) - 1] = '\0';
  strncpy(dip2, customDip2.getValue(), sizeof(dip2) - 1);
  dip2[sizeof(dip2) - 1] = '\0';
  strncpy(dip3, customDip3.getValue(), sizeof(dip3) - 1);
  dip3[sizeof(dip3) - 1] = '\0';
  strncpy(dip4, customDip4.getValue(), sizeof(dip4) - 1);
  dip4[sizeof(dip4) - 1] = '\0';
  strncpy(tzString, customTz.getValue(), sizeof(tzString) - 1);
  tzString[sizeof(tzString) - 1] = '\0';

  if (shouldSaveSinricConfig) {
    saveSinricConfig();
  }

  computeRfCodes();

  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
}

void setupNetworkExtras() {
  if (MDNS.begin(hostname)) {
    logf("[mDNS]: responding as %s.local", hostname);
  } else {
    logf("[mDNS]: failed to start");
  }

  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.onStart([]() { logf("[OTA]: update starting..."); });
  ArduinoOTA.onEnd([]() { logf("[OTA]: update complete, rebooting..."); });
  ArduinoOTA.onError([](ota_error_t error) { logf("[OTA]: error [%u]", error); });
  ArduinoOTA.begin();
  logf("[OTA]: ready -- pio run -e c3_mini_fan_public -t upload --upload-port %s.local", hostname);
}

// ==========================================
// Setup
// ==========================================

void setupRadio() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_OFF);
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);

  ELECHOUSE_cc1101.setSpiPin(C3_CC1101_CLK, C3_CC1101_MISO, C3_CC1101_MOSI, C3_CC1101_CS);

  // Init() brings up SPI.begin() on the custom pins, so it must run before
  // any register read/write, including getCC1101().
  ELECHOUSE_cc1101.Init();

  radioAvailable = ELECHOUSE_cc1101.getCC1101();
  if (radioAvailable) {
    logf("CC1101 Connection: SUCCESS");
  } else {
    logf("CC1101 Connection: FAILED (no module attached, or check wiring) -- "
         "fan/light RF commands will be no-ops; WiFi/SinricPro/portal still work");
    return; // nothing to configure on a chip that isn't there
  }

  ELECHOUSE_cc1101.setMHZ(RF_MHZ);
  ELECHOUSE_cc1101.setModulation(2); // ASK/OOK
  ELECHOUSE_cc1101.SetTx();

  myRadio.enableTransmit(C3_CC1101_GDO0);
  myRadio.setProtocol(RF_PROTOCOL);
  myRadio.setPulseLength(RF_PULSE_US);
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
  configTzTime(tzString, NTP_SERVER);
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
// ack) but sendFanCode()/onLightPowerState() silently stop transmitting.
void checkRadioWatchdog() {
  if (!radioAvailable) return; // known-missing at boot -- don't reboot forever over it
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

// Reboots once per day at REBOOT_HOUR local time (see tzString above).
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
  SinricProFanUS &myFan = SinricPro[fanId];
  myFan.onPowerState(onFanPowerState);
  myFan.onRangeValue(onFanRangeValue);
  myFan.onAdjustRangeValue(onFanAdjustRangeValue);

  SinricProSwitch &myLight = SinricPro[lightId];
  myLight.onPowerState(onLightPowerState);

  SinricPro.onConnected([]() { logf("Connected to SinricPro"); });
  SinricPro.onDisconnected([]() { logf("Disconnected from SinricPro"); });

  SinricPro.begin(appKey, appSecret);
}

void setup() {
  // GPIO21 is broken out on this board but unconnected -- floating near
  // the antenna, it's a reported (if unconfirmed on this board) cause of
  // ESP32-C3 SuperMini WiFi connect failures elsewhere. Tried alone on
  // this board's actual touch-the-antenna-to-connect symptom and did NOT
  // fix it -- the real fix was capping TX power, see setupWiFiManager().
  // Left in as a harmless precaution (a defined pin state next to the
  // antenna is still better than a floating one), not because it's
  // doing the load-bearing work here.
  pinMode(21, OUTPUT);
  digitalWrite(21, LOW);

  Serial.begin(115200);
  delay(2000);
  Serial.println();
  Serial.println(BANNER);

  // Load the saved timezone (and the rest of the portal config, harmless
  // to read this early -- it's a flash/Preferences read, no WiFi needed)
  // and set it before any logging happens. The RTC survives a soft reset
  // (only a real power loss clears it), so time() can already return a
  // valid epoch at the very start of boot -- without the TZ set yet,
  // early log lines would misinterpret that as UTC/GMT instead of local
  // time. setupWiFiManager() reloads/re-saves this same config later
  // (and may update tzString from the portal), which is fine -- this
  // early load's only job is getting the TZ right for the first few log
  // lines before that runs.
  loadSinricConfig();
  configTzTime(tzString, NTP_SERVER);

  logf("--- Fan/Light RF Controller (setup portal build) v%s ---", FIRMWARE_VERSION);

  prefs.begin("fanctl", true); // read-only
  lastRebootDay = prefs.getInt("last_reboot_day", -1);
  prefs.end();

  checkUnexpectedReset();

  setupRadio();
  setupWiFiManager();
  setupNetworkExtras();
  setupTime();
  logLastRebootReason();
  setupSinricPro();
}

void loop() {
  SinricPro.handle();
  ArduinoOTA.handle();
  checkWiFiWatchdog();
  checkSinricWatchdog();
  checkRadioWatchdog();
  checkClockJump();
  checkDailyReboot();
  checkLedBlink();
  checkFactoryResetButton();
}
