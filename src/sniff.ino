#include <Arduino.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

// Pin mappings for ESP32-C3 Super Mini
#define C3_CC1101_CLK   4
#define C3_CC1101_MISO  5
#define C3_CC1101_MOSI  6
#define C3_CC1101_CS    7
#define C3_CC1101_GDO0  3  // Pin connected to GDO0 for capturing pulses

void setup() {
    Serial.begin(115200);
    delay(2000); 

    Serial.println("--- NOMA 304.25MHz RF Sniffer Initializing ---");
    
    // Set custom SPI pins for the C3 Super Mini
    ELECHOUSE_cc1101.setSpiPin(C3_CC1101_CLK, C3_CC1101_MISO, C3_CC1101_MOSI, C3_CC1101_CS);

    // Init() brings up SPI.begin() on the custom pins above, so it must run
    // before any register read (getCC1101 included) or the read fails.
    ELECHOUSE_cc1101.Init();

    if (ELECHOUSE_cc1101.getCC1101()) {
        Serial.println("CC1101 Connection: SUCCESS");
    } else {
        Serial.println("CC1101 Connection: FAILED. Check your wiring!");
        while (1);
    }

    ELECHOUSE_cc1101.setMHZ(304.25);   // Force physical 433MHz board to listen at 304.25 MHz
    ELECHOUSE_cc1101.setModulation(2); // Listen for ASK/OOK signals (standard for fan remotes)
    
    // Enable Receiver mode on the chip
    ELECHOUSE_cc1101.SetRx(); 

    // Connect the data pin to the rc-switch receiver engine
    // On ESP32-C3, you can attach interrupts directly to standard GPIO numbers
    mySwitch.enableReceive(C3_CC1101_GDO0);
    
    Serial.println("Ready! Hold your NOMA remote close to the board and press a button.");
}

void loop() {
    if (mySwitch.available()) {
        // A valid RF signal protocol was recognized and decoded!
        Serial.println("----------------------------------------");
        Serial.print("Received Code (Decimal): ");
        Serial.println(mySwitch.getReceivedValue());
        
        Serial.print("Bit Length: ");
        Serial.println(mySwitch.getReceivedBitlength());
        
        Serial.print("Protocol Number: ");
        Serial.println(mySwitch.getReceivedProtocol());
        
        Serial.print("Pulse Length (us): ");
        Serial.println(mySwitch.getReceivedDelay());

        // Print raw binary string - use this directly in your transmitter script
        Serial.print("Binary String: ");
        unsigned int len = mySwitch.getReceivedBitlength();
        unsigned long val = mySwitch.getReceivedValue();
        for (int i = len - 1; i >= 0; i--) {
            Serial.print((val >> i) & 1);
        }
        Serial.println("\n----------------------------------------");

        // Clear the receiver buffer to wait for the next button press
        mySwitch.resetAvailable();
    }
}

