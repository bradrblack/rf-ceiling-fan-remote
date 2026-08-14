#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include <cstdarg>
#include <cstring>
#include <esp_system.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>
#include "SinricPro.h"
#include "SinricProFanUS.h"
#include "SinricProSwitch.h"

#include "secrets.h"

// ==========================================
// CC1101 wiring (ESP32-C3 Super Mini) -- same as sniff.ino / tx_test.cpp
// ==========================================
#define C3_CC1101_CLK   4
#define C3_CC1101_MISO  5
#define C3_CC1101_MOSI  6
#define C3_CC1101_CS    7
#define C3_CC1101_GDO0  3

#define RF_MHZ          304.25
#define RF_PROTOCOL     11
#define RF_PULSE_US     412
#define RF_BITLENGTH    12

// Codes captured with sniff.ino
#define RF_CODE_FAN_LOW    2044
#define RF_CODE_FAN_MEDIUM 3836
#define RF_CODE_FAN_HIGH   3964
#define RF_CODE_FAN_OFF    4028
#define RF_CODE_LIGHT      3068

// ==========================================
// Reliability: WiFi watchdog + daily reboot
// ==========================================
// If WiFi can't (re)connect within this long, or stays disconnected this
// long during normal operation, reboot rather than sit there dead.
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 30000;
const unsigned long WIFI_DOWN_REBOOT_MS     = 60000;
unsigned long wifiDownSince = 0;

// Periodically confirms the CC1101 is still answering over SPI (same
// VERSION-register check used at boot). Catches the radio silently wedging
// mid-operation -- SinricPro/WiFi stay up (Google still hears an ack "beep")
// but sendFanCode()/onLightPowerState() stop actually transmitting anything.
const unsigned long RADIO_CHECK_INTERVAL_MS = 60000;
unsigned long lastRadioCheck = 0;

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
int fanSpeed = 0; // 0 = off, 1..3 = low/medium/high

// The physical remote only has a single LIGHT TOGGLE code, not separate
// on/off codes, so we track assumed state locally and only fire the toggle
// when the requested state actually differs from what we last sent. This
// can drift out of sync if the light is ever toggled by another remote.
bool lightState = false;

void sendFanCode(int speed) {
  switch (speed) {
    case 1: myRadio.send(RF_CODE_FAN_LOW, RF_BITLENGTH); break;
    case 2: myRadio.send(RF_CODE_FAN_MEDIUM, RF_BITLENGTH); break;
    case 3: myRadio.send(RF_CODE_FAN_HIGH, RF_BITLENGTH); break;
    default: myRadio.send(RF_CODE_FAN_OFF, RF_BITLENGTH); break;
  }
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
    myRadio.send(RF_CODE_LIGHT, RF_BITLENGTH);
    lightState = state;
  }
  return true;
}

// ==========================================
// Setup
// ==========================================

void setupRadio() {
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

// Reboots if the CC1101 stops answering over SPI mid-operation. This is the
// failure mode where SinricPro/WiFi stay connected (Google still hears an
// ack) but sendFanCode()/onLightPowerState() silently stop transmitting.
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
  myFan.onPowerState(onFanPowerState);
  myFan.onRangeValue(onFanRangeValue);
  myFan.onAdjustRangeValue(onFanAdjustRangeValue);

  SinricProSwitch &myLight = SinricPro[LIGHT_ID];
  myLight.onPowerState(onLightPowerState);

  SinricPro.onConnected([]() { logf("Connected to SinricPro"); });
  SinricPro.onDisconnected([]() { logf("Disconnected from SinricPro"); });

  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();

  // Set the timezone before any logging happens. The RTC survives a soft
  // reset (only a real power loss clears it), so time() can already return
  // a valid epoch at the very start of boot -- without the TZ set yet,
  // early log lines would misinterpret that as UTC/GMT instead of EDT.
  configTzTime(TZ_STRING, NTP_SERVER);

  logf("--- Fan/Light RF Controller ---");

  prefs.begin("fanctl", true); // read-only
  lastRebootDay = prefs.getInt("last_reboot_day", -1);
  prefs.end();

  checkUnexpectedReset();

  setupRadio();
  setupWiFi();
  setupTime();
  notifyLastReboot();
  setupSinricPro();
}

void loop() {
  SinricPro.handle();
  checkWiFiWatchdog();
  checkRadioWatchdog();
  checkDailyReboot();
}
