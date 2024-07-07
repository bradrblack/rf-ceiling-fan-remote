/* 
 * Control an RF ceiling fan using a relay module & Adafruit IO plus voice integration (Google Home and Siri)
 */

#include <WiFiManager.h>
#include "AdafruitIO_WiFi.h"

char IO_USERNAME[64] = "put_IO_username_here";
char IO_KEY[64] = "put_IO_key_here";

static uint8_t objStorage[sizeof(AdafruitIO_WiFi)]; // RAM for the object
AdafruitIO_WiFi *io;                                // a pointer to the object, once it's constructed

WiFiManager wifiManager;

#define FAN_LOW 5
#define FAN_MED 6
#define FAN_HI 7
#define FAN_OFF 8
#define LIGHT_TOGGLE 9

void handleMessage(AdafruitIO_Data *data)
{

  Serial.print("received <- ");
  Serial.println(data->toString());

  if (data->toString() == "FAN_LOW") {
       digitalWrite(FAN_LOW, LOW);        
       delay(200);
       digitalWrite(FAN_LOW, HIGH);        
  } 
    if (data->toString() == "FAN_MED") {
       digitalWrite(FAN_MED, LOW);        
       delay(200);
       digitalWrite(FAN_MED, HIGH);        
  }  
    if (data->toString() == "FAN_HI") {
       digitalWrite(FAN_HI, LOW);        
       delay(200);
       digitalWrite(FAN_HI, HIGH);        
  } 
    if (data->toString() == "FAN_OFF") {
       digitalWrite(FAN_OFF, LOW);        
       delay(200);
       digitalWrite(FAN_OFF, HIGH);        
  } 
    if (data->toString() == "LIGHT_TOGGLE") {
       digitalWrite(LIGHT_TOGGLE, LOW);        
       delay(200);
       digitalWrite(LIGHT_TOGGLE, HIGH);        
  } 

} 
void setup()

{
  pinMode(FAN_LOW, OUTPUT);     
  pinMode(FAN_MED,OUTPUT);
  pinMode(FAN_HI,OUTPUT);
  pinMode(FAN_OFF,OUTPUT);
  pinMode(LIGHT_TOGGLE,OUTPUT);

  digitalWrite(FAN_LOW, HIGH);
  digitalWrite(FAN_MED, HIGH);
  digitalWrite(FAN_HI, HIGH);
  digitalWrite(FAN_OFF, HIGH);
  digitalWrite(LIGHT_TOGGLE, HIGH);

  Serial.begin(115200); // Initialize serial port for debugging.
  delay(500);
  WiFi.begin();
  delay(500);

  //wifiManager.resetSettings();  //uncomment to reset the WiFi settings

  wifiManager.setClass("invert");          // enable "dark mode" for the config portal
  wifiManager.setConfigPortalTimeout(120); // auto close configportal after n seconds
  wifiManager.setAPClientCheck(true);      // avoid timeout if client connected to softap

  
  if (!wifiManager.autoConnect("WiFi Setup")) // connect to wifi with existing setting or start config
  {
    Serial.println("Failed to connect and hit timeout");
  }
  else
  {
    // if you get here you have connected to the WiFi
    Serial.println("Connected to WiFi.");

    // connect to Adafruit IO

    io = new (objStorage) AdafruitIO_WiFi(IO_USERNAME, IO_KEY, "", "");

    Serial.printf("Connecting to Adafruit IO with User: %s Key: %s.\n", IO_USERNAME, IO_KEY);

    io->connect();

    AdafruitIO_Feed *myfeed = io->feed("fan");

    myfeed->onMessage(handleMessage);

    myfeed->get();

    // wait for a connection

    while ((io->status() < AIO_CONNECTED))
    {
      Serial.print(".");
      delay(500);
    }
    Serial.println("Connected to Adafruit IO.");
  }

} // setup()

void loop()
{

  io->run();

} // loop()
