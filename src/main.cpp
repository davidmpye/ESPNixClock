#include <nixie_i2c.h>
#include <Adafruit_MCP23017.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC
#include <Time.h>
#include <Timezone.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define ANODE_PWM_PIN 12
#define LED_PIN 2

const char *ssid = "";
const char *password = "";

const char *ota_hostname="espnixie";
const char *ota_password="";

IPAddress ip(192, 168, 1, 15);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

Nixie_i2c nix0(0);
Nixie_i2c nix1(1);
Nixie_i2c nix2(2);
Nixie_i2c nix3(3);
Nixie_i2c nix4(4);
Nixie_i2c nix5(5);

 //#define SET_TIME

//Time zone rules
 TimeChangeRule myDST = {"DST", Last, Sun, Mar, 1, 60};    //Daylight time = UTC+1 hr
 TimeChangeRule mySTD = {"UTC", Last, Sun, Oct, 2, 0};     //Standard time = UTC
 Timezone myTZ(myDST, mySTD);
 time_t currentTime;


//Descriptor for the WS2812 LEDs in the nixie boards
Adafruit_NeoPixel strip = Adafruit_NeoPixel(6, LED_PIN, NEO_GRB + NEO_KHZ800);

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}


void setNixieBrightness(int percent) {
  analogWrite(ANODE_PWM_PIN, 10.24 * percent);
}

void setup() {
  Serial.begin(115200);

  Serial.println("Initializing");

  pinMode(ANODE_PWM_PIN, OUTPUT);

  nix0.init();
  nix1.init();
  nix2.init();
  nix3.init();
  nix4.init();
  nix5.init();

  strip.begin();
  strip.setBrightness(100);
  strip.show();


  ArduinoOTA.onStart([]() {
    Serial.println("Begin");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    nix0.blank();
    nix1.blank();
    nix2.blank();
    nix3.blank();
    nix4.blank();
    nix5.blank();
    int percent = progress / (total / 100);
    nix1.displayNumber(percent / 10);
    nix0.displayNumber(percent % 10);
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(ota_hostname);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.begin();

  //Get the current time from the RTC

#ifdef SET_TIME
  tmElements_t tm;
  tm.Hour = 20;
  tm.Minute = 50;
  tm.Second = 0;
  tm.Day = 18;
  tm.Month = 9;
  tm.Year = 2016 - 1970;    //tmElements_t.Year is the offset from 1970
  RTC.write(tm);
#endif
  setSyncProvider(RTC.get);
  //tube brightness to off
  setNixieBrightness(0);

    currentTime = myTZ.toLocal(now());

  nix5.displayNumber(hour(currentTime)/10);
  nix4.displayNumber(hour(currentTime)%10);
  nix3.displayNumber(minute(currentTime)/10);
  nix2.displayNumber(minute(currentTime)%10);
  nix1.displayNumber(second(currentTime)/10);
  nix0.displayNumber(second(currentTime)%10);



}

int lastMinute = -1; //used for RGB colour updates

void loop() {
  //OTA update handler
  ArduinoOTA.handle();

  currentTime = myTZ.toLocal(now());

  int scratch;
  scratch = hour(currentTime) / 10;
  if (nix5.currentNumber() != scratch)  nix5.slotMachineDisplayNumber(scratch);
  scratch = hour(currentTime) % 10;
  if (nix4.currentNumber() != scratch) nix4.slotMachineDisplayNumber(scratch);

  scratch = minute(currentTime) / 10;
  if (nix3.currentNumber() != scratch) nix3.slotMachineDisplayNumber(scratch);
  scratch = minute(currentTime) % 10;
  if (nix2.currentNumber() != scratch) nix2.slotMachineDisplayNumber(scratch);


  scratch = second(currentTime) / 10;
  if (nix1.currentNumber() != scratch) {
    if (nix1.currentNumber() == 5) nix1.slotMachineDisplayNumber(scratch);
    else nix1.displayNumber(scratch);
  }

  scratch = second(currentTime) % 10;
  if (nix0.currentNumber() != scratch) nix0.displayNumber(scratch);


  if (minute(currentTime) != lastMinute) {
    //The color value is set by adding hours(*10) to minutes.
    //Highest val would occur at 23:59.  (230 + 59), so cap it at 255.
    byte magicColorVal = hour(currentTime) * 10 + ( minute(currentTime) / 6) > 255 ? 255 : hour(currentTime) * 10 + (minute(currentTime) / 6) ;
    uint32_t color = Wheel(magicColorVal);

    for (int i = 0; i < 6; ++i) {
      strip.setPixelColor(i, color);
    }
    strip.show();
    lastMinute = minute(currentTime);
  }

  //Handle dimming - we will dim from 11pm to 6am to 50% brightness...
  if (hour(currentTime) > 22 || hour(currentTime) < 6) {
      setNixieBrightness(10);
      strip.setBrightness(15);
      strip.show();
  }
  else {
      setNixieBrightness(100);
      strip.setBrightness(100);
      strip.show();
  }
  delay(50);
}

void setColons(bool state) {
  nix4.setRhdp(state);
  nix2.setRhdp(state);
}
