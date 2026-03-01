/*
  GPS-only (ESP32)
  - Activa relé GPS_KIM
  - Timeout GPS: 3 minuts
  - Deep sleep: 30 segons -- això es un test
*/

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <RTClib.h>
#include "esp_sleep.h"

//------ GPS
#define RXPin_GPS 4
#define TXPin_GPS 2
#define GPSBaud   9600

#define GPS_KIM 13
#define PB_1 25

TinyGPSPlus gps;
SoftwareSerial gpsSerial(RXPin_GPS, TXPin_GPS);

//------ Variables
double gpsLat = 200, gpsLong = 200;
uint8_t gpsMonth = 0, gpsDay = 0, gpsHour = 0, gpsMinute = 0, gpsSecond = 0;
uint16_t gpsYear = 0;
bool gpsFix = false;
uint32_t epochTime = 0;

int maxGPSTimeout = 180000; // 3 minuts

//------------------ CONFIG GPS ------------------
void configGPS() {
  uint8_t ubxConfig[] = {
    0xB5, 0x62, 0x06, 0x24, 0x24, 0x00,
    0x01, 0x00,
    0x03, 0x00,
    0x01, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };

  for (int i = 0; i < sizeof(ubxConfig); i++) {
    gpsSerial.write(ubxConfig[i]);
  }

  delay(500);
}

//------------------ ACQUIRE ------------------
void gpsAcquireData() {

  Serial.println("GPS acquiring data------>START");

  gps = TinyGPSPlus();
  delay(10);
  configGPS();

  uint32_t initialTime = millis();
  uint32_t lastPrint = 0;
  gpsFix = false;

  while (millis() < (initialTime + maxGPSTimeout) &&
         digitalRead(PB_1) == true) {

    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
    }

    // Print cada 1 segon
    if (millis() - lastPrint > 1000) {
      lastPrint = millis();

      Serial.print("Searching... ");
      Serial.print("Sat: ");
      Serial.print(gps.satellites.isValid() ? gps.satellites.value() : 0);
      Serial.print(" | HDOP: ");
      Serial.print(gps.hdop.isValid() ? gps.hdop.hdop() : 0);
      Serial.print(" | Time left: ");
      Serial.print((maxGPSTimeout - (millis() - initialTime)) / 1000);
      Serial.println(" s");
    }

    if (gps.location.isValid() &&
        gps.date.isValid() &&
        gps.time.isValid()) {

      gpsLat = gps.location.lat();
      gpsLong = gps.location.lng();
      gpsYear = gps.date.year();
      gpsMonth = gps.date.month();
      gpsDay = gps.date.day();
      gpsHour = gps.time.hour();
      gpsMinute = gps.time.minute();
      gpsSecond = gps.time.second();

      DateTime now2(gpsYear, gpsMonth, gpsDay, gpsHour, gpsMinute, gpsSecond);
      epochTime = now2.unixtime();

      gpsFix = true;

      Serial.println("GPS FIX ACQUIRED ✅");
      break;
    }

    delay(50);
  }

  if (!gpsFix) {
    Serial.println("GPS TIMEOUT ❌");
  } else {
    Serial.println("GPS acquiring data------>DONE");
  }
}

//------------------ SLEEP ------------------
void deepSleepNow() {

  Serial.println("Entering deep sleep 30 seconds...");
  Serial.flush();

  digitalWrite(GPS_KIM, LOW);  // apaga GPS
  delay(50);

  // 30 segons
  esp_sleep_enable_timer_wakeup(30ULL * 1000000ULL);
  esp_deep_sleep_start();
}

//------------------ SETUP ------------------
void setup() {

  Serial.begin(115200);
  delay(200);

  pinMode(PB_1, INPUT_PULLUP);

  pinMode(GPS_KIM, OUTPUT);
  digitalWrite(GPS_KIM, HIGH);   // alimenta GPS
  delay(500);

  gpsSerial.begin(GPSBaud);

  gpsAcquireData();

  deepSleepNow();
}

void loop() {}