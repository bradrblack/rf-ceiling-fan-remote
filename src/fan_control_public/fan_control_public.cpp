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
#include <esp32c3/rom/rtc.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>
#include "SinricPro.h"
#include "SinricProFanUS.h"
#include "SinricProSwitch.h"

// Bump this on each flash you want to be able to identify later (e.g. to
// confirm an OTA update actually took) -- format: YYYY-MM-DDrN.
#define FIRMWARE_VERSION "2026-08-25r5"

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

// Codes captured with sniff.ino -- these are specific to this fan/remote
// model. If you're cloning this for a different remote, re-run sniff.ino
// and update these.
#define RF_CODE_FAN_LOW    2044
#define RF_CODE_FAN_MEDIUM 3836
#define RF_CODE_FAN_HIGH   3964
#define RF_CODE_FAN_OFF    4028
#define RF_CODE_LIGHT      3068

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
// 2nd Sunday of March and back on the 1st Sunday of November. Change this
// if you're outside US Eastern.
#define TZ_STRING        "EST5EDT,M3.2.0,M11.1.0/2"
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
    case 1: myRadio.send(RF_CODE_FAN_LOW, RF_BITLENGTH); break;
    case 2: myRadio.send(RF_CODE_FAN_MEDIUM, RF_BITLENGTH); break;
    case 3: myRadio.send(RF_CODE_FAN_HIGH, RF_BITLENGTH); break;
    default: myRadio.send(RF_CODE_FAN_OFF, RF_BITLENGTH); break;
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
      myRadio.send(RF_CODE_LIGHT, RF_BITLENGTH);
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

bool shouldSaveSinricConfig = false;
void onSaveSinricConfig() { shouldSaveSinricConfig = true; }

void loadSinricConfig() {
  prefs.begin("fanwm", true);
  prefs.getString("app_key", appKey, sizeof(appKey));
  prefs.getString("app_secret", appSecret, sizeof(appSecret));
  prefs.getString("fan_id", fanId, sizeof(fanId));
  prefs.getString("light_id", lightId, sizeof(lightId));
  prefs.getString("hostname", hostname, sizeof(hostname));
  prefs.end();
}

void saveSinricConfig() {
  prefs.begin("fanwm", false);
  prefs.putString("app_key", appKey);
  prefs.putString("app_secret", appSecret);
  prefs.putString("fan_id", fanId);
  prefs.putString("light_id", lightId);
  prefs.putString("hostname", hostname);
  prefs.end();
}

void setupWiFiManager() {
  loadSinricConfig();

  WiFiManagerParameter customAppKey("appkey", "SinricPro App Key", appKey, sizeof(appKey));
  WiFiManagerParameter customAppSecret("appsecret", "SinricPro App Secret", appSecret, sizeof(appSecret));
  WiFiManagerParameter customFanId("fanid", "SinricPro Fan Device ID", fanId, sizeof(fanId));
  WiFiManagerParameter customLightId("lightid", "SinricPro Light Device ID", lightId, sizeof(lightId));
  WiFiManagerParameter customHostname("hostname", "Device hostname (e.g. myfan -&gt; myfan.local)", hostname, sizeof(hostname));

  WiFiManager wm;
  wm.setTitle("Fan Controller");
  wm.setSaveConfigCallback(onSaveSinricConfig);
  wm.addParameter(&customAppKey);
  wm.addParameter(&customAppSecret);
  wm.addParameter(&customFanId);
  wm.addParameter(&customLightId);
  wm.addParameter(&customHostname);
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

  if (shouldSaveSinricConfig) {
    saveSinricConfig();
  }

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
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println(BANNER);

  // Set the timezone before any logging happens. The RTC survives a soft
  // reset (only a real power loss clears it), so time() can already return
  // a valid epoch at the very start of boot -- without the TZ set yet,
  // early log lines would misinterpret that as UTC/GMT instead of EDT.
  configTzTime(TZ_STRING, NTP_SERVER);

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
