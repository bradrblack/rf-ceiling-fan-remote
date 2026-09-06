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

// Frequency to listen on -- change this and reflash to try a different
// candidate frequency (e.g. 315.0 or 304.25) if nothing is received.
#define SNIFF_MHZ 433.92

// 0 = normal mode: only prints when RCSwitch recognizes one of its built-in
// protocols (as below). 1 = raw mode: bypasses RCSwitch's protocol matching
// entirely and just dumps every pulse's duration -- use this when a button
// produces no output in normal mode, to see whether anything is arriving on
// GDO0 at all and, if so, what its actual timing looks like (which can then
// be turned into a custom RCSwitch::Protocol if it doesn't match a built-in
// one).
#define RAW_MODE 0

#if RAW_MODE
#define RAW_MAX_PULSES 500
// A burst is considered finished once this many us pass with no edge --
// long enough to span the gap between repeats within one button press
// (typically a few ms) without also spanning to the *next* button press.
#define RAW_GAP_US 8000

volatile unsigned long rawDurations[RAW_MAX_PULSES];
volatile int rawCount = 0;
volatile unsigned long rawLastEdge = 0;
volatile bool rawHasData = false;

void IRAM_ATTR rawIsr() {
  unsigned long now = micros();
  unsigned long duration = now - rawLastEdge;
  rawLastEdge = now;
  if (rawCount < RAW_MAX_PULSES) {
    rawDurations[rawCount++] = duration;
    rawHasData = true;
  }
}

void dumpRawIfBurstFinished() {
  if (!rawHasData) return;
  if (micros() - rawLastEdge < RAW_GAP_US) return; // still receiving

  noInterrupts();
  int count = rawCount;
  rawCount = 0;
  rawHasData = false;
  interrupts();

  Serial.println("========== RAW BURST ==========");
  Serial.printf("Pulse count: %d\n", count);
  // Skip index 0 -- it's the gap since the *previous* burst, not a real
  // pulse width. Print the rest as a comma-separated list of durations
  // (us), alternating high/low starting from whichever level GDO0 idles
  // low or high at (not tracked here -- eyeball the pattern for repeats).
  for (int i = 1; i < count; i++) {
    Serial.print(rawDurations[i]);
    Serial.print(i + 1 < count ? "," : "\n");
  }
  Serial.println("================================");
}
#endif

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("--- RF Sniffer Initializing ---");

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

    ELECHOUSE_cc1101.setMHZ(SNIFF_MHZ);
    Serial.print("Listening at ");
    Serial.print(SNIFF_MHZ, 2);
    Serial.println(" MHz");
    ELECHOUSE_cc1101.setModulation(2); // Listen for ASK/OOK signals (standard for fan remotes)
    
    // Enable Receiver mode on the chip
    ELECHOUSE_cc1101.SetRx(); 

#if RAW_MODE
    pinMode(C3_CC1101_GDO0, INPUT);
    attachInterrupt(digitalPinToInterrupt(C3_CC1101_GDO0), rawIsr, CHANGE);
    Serial.println("RAW MODE -- dumping pulse timings, no protocol matching.");
#else
    // Connect the data pin to the rc-switch receiver engine
    // On ESP32-C3, you can attach interrupts directly to standard GPIO numbers
    mySwitch.enableReceive(C3_CC1101_GDO0);
#endif

    Serial.println("Ready! Hold your NOMA remote close to the board and press a button.");
}

void loop() {
#if RAW_MODE
    dumpRawIfBurstFinished();
#else
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
#endif
}

