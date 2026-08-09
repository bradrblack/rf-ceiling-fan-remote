#include <Arduino.h>
#include <WiFi.h>
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
  Serial.printf("Fan power: %s\r\n", state ? "ON" : "OFF");
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
  Serial.printf("Fan speed set to %d\r\n", fanSpeed);
  sendFanCode(fanSpeed);
  return true;
}

// relative changes, e.g. "increase the fan speed"
bool onFanAdjustRangeValue(const String &deviceId, int &rangeValueDelta) {
  fanSpeed = constrain(fanSpeed + rangeValueDelta, 1, 3);
  Serial.printf("Fan speed adjusted by %d to %d\r\n", rangeValueDelta, fanSpeed);
  sendFanCode(fanSpeed);
  rangeValueDelta = fanSpeed; // must return the new absolute value
  return true;
}

// "turn on/off the light"
bool onLightPowerState(const String &deviceId, bool &state) {
  Serial.printf("Light requested: %s (currently tracked as %s)\r\n",
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
    Serial.println("CC1101 Connection: SUCCESS");
  } else {
    Serial.println("CC1101 Connection: FAILED. Check your wiring!");
  }

  ELECHOUSE_cc1101.setMHZ(RF_MHZ);
  ELECHOUSE_cc1101.setModulation(2); // ASK/OOK
  ELECHOUSE_cc1101.SetTx();

  myRadio.enableTransmit(C3_CC1101_GDO0);
  myRadio.setProtocol(RF_PROTOCOL);
  myRadio.setPulseLength(RF_PULSE_US);
}

void setupWiFi() {
  Serial.printf("\r\n[WiFi]: Connecting");
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(250);
  }
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %s\r\n", WiFi.localIP().toString().c_str());
}

void setupSinricPro() {
  SinricProFanUS &myFan = SinricPro[FAN_ID];
  myFan.onPowerState(onFanPowerState);
  myFan.onRangeValue(onFanRangeValue);
  myFan.onAdjustRangeValue(onFanAdjustRangeValue);

  SinricProSwitch &myLight = SinricPro[LIGHT_ID];
  myLight.onPowerState(onLightPowerState);

  SinricPro.onConnected([]() { Serial.printf("Connected to SinricPro\r\n"); });
  SinricPro.onDisconnected([]() { Serial.printf("Disconnected from SinricPro\r\n"); });

  SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\r\n--- Fan/Light RF Controller ---");

  setupRadio();
  setupWiFi();
  setupSinricPro();
}

void loop() {
  SinricPro.handle();
}
