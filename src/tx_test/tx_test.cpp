#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

// Pin mappings for ESP32-C3 Super Mini (same wiring as sniff.ino)
#define C3_CC1101_CLK   4
#define C3_CC1101_MISO  5
#define C3_CC1101_MOSI  6
#define C3_CC1101_CS    7
#define C3_CC1101_GDO0  3

// Captured from sniff.ino
#define CODE_LIGHT       3068
#define CODE_BITLENGTH   12
#define CODE_PROTOCOL    11
#define CODE_PULSE_US    412

const unsigned long TOGGLE_INTERVAL_MS = 10000;
unsigned long lastToggle = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("--- 304.25MHz Range Test: Light Toggle every 10s ---");

    ELECHOUSE_cc1101.setSpiPin(C3_CC1101_CLK, C3_CC1101_MISO, C3_CC1101_MOSI, C3_CC1101_CS);

    // Init() brings up SPI on the custom pins, so it must run before getCC1101()
    ELECHOUSE_cc1101.Init();

    if (ELECHOUSE_cc1101.getCC1101()) {
        Serial.println("CC1101 Connection: SUCCESS");
    } else {
        Serial.println("CC1101 Connection: FAILED. Check your wiring!");
        while (1);
    }

    Serial.println("Reading VERSION register (0x31) 20x rapidly:");
    for (int i = 0; i < 20; i++) {
        Serial.print("0x");
        Serial.print(ELECHOUSE_cc1101.SpiReadStatus(0x31), HEX);
        Serial.print(" ");
    }
    Serial.println();

    Serial.print("MHz right after Init(): ");
    Serial.println(ELECHOUSE_cc1101.getMHZ());

    ELECHOUSE_cc1101.setMHZ(304.25);
    Serial.print("MHz right after setMHZ(304.25): ");
    Serial.println(ELECHOUSE_cc1101.getMHZ());

    ELECHOUSE_cc1101.setModulation(2); // ASK/OOK
    Serial.print("MHz right after setModulation(2): ");
    Serial.println(ELECHOUSE_cc1101.getMHZ());

    ELECHOUSE_cc1101.SetTx();
    Serial.print("MHz right after SetTx(): ");
    Serial.println(ELECHOUSE_cc1101.getMHZ());

    mySwitch.enableTransmit(C3_CC1101_GDO0);
    mySwitch.setProtocol(CODE_PROTOCOL);
    mySwitch.setPulseLength(CODE_PULSE_US);

    // 0x13 = TX, 0x14 = TX_END. Anything else means STX didn't take.
    Serial.print("MARCSTATE after SetTx (0x35 read as status): 0x");
    Serial.println(ELECHOUSE_cc1101.SpiReadStatus(0x35), HEX);

    Serial.println("Ready. Sending LIGHT TOGGLE every 10s -- walk away and watch the light.");
}

void loop() {
    if (millis() - lastToggle >= TOGGLE_INTERVAL_MS) {
        lastToggle = millis();
        mySwitch.send(CODE_LIGHT, CODE_BITLENGTH);
        Serial.print("Sent LIGHT TOGGLE code ");
        Serial.print(CODE_LIGHT);
        Serial.print(" -- MARCSTATE: 0x");
        Serial.println(ELECHOUSE_cc1101.SpiReadStatus(0x35), HEX);
    }
}
