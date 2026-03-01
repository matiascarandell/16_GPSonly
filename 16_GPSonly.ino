/*
  GPS-only (ESP32)
  - Powers the GPS module through GPS_KIM
  - GPS timeout: 3 minutes
  - Deep sleep: 30 seconds
*/

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <RTClib.h>
#include "esp_sleep.h"

namespace {
constexpr uint8_t RXPinGPS = 4;
constexpr uint8_t TXPinGPS = 2;
constexpr uint32_t kGpsBaud = 9600;

constexpr uint8_t GPSPowerPin = 13;
constexpr uint8_t ButtonPin = 25;

constexpr uint32_t kGpsTimeoutMs = 180000;
constexpr uint64_t kDeepSleepDurationUs = 30ULL * 1000000ULL;
}  // namespace

TinyGPSPlus gps;
SoftwareSerial gpsSerial(RXPinGPS, TXPinGPS);

struct GpsData {
  double latitude = 200.0;
  double longitude = 200.0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  uint16_t year = 0;
  uint32_t epochTime = 0;
  bool hasFix = false;
};

GpsData gpsData;

void configGPS() {
  const uint8_t ubxConfig[] = {
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

  for (size_t i = 0; i < sizeof(ubxConfig); ++i) {
    gpsSerial.write(ubxConfig[i]);
  }

  delay(500);
}

bool shouldContinueAcquisition(uint32_t startTimeMs) {
  const bool timeoutReached = (millis() - startTimeMs) >= kGpsTimeoutMs;
  const bool buttonStillReleased = digitalRead(ButtonPin) == HIGH;
  return !timeoutReached && buttonStillReleased;
}

void printGpsSearchStatus(uint32_t startTimeMs) {
  const uint32_t elapsedMs = millis() - startTimeMs;
  const uint32_t remainingSeconds =
      (elapsedMs >= kGpsTimeoutMs) ? 0 : (kGpsTimeoutMs - elapsedMs) / 1000;

  Serial.print("Searching... ");
  Serial.print("Sat: ");
  Serial.print(gps.satellites.isValid() ? gps.satellites.value() : 0);
  Serial.print(" | HDOP: ");
  Serial.print(gps.hdop.isValid() ? gps.hdop.hdop() : 0);
  Serial.print(" | Time left: ");
  Serial.print(remainingSeconds);
  Serial.println(" s");
}

bool updateGpsDataIfValid() {
  if (!gps.location.isValid() || !gps.date.isValid() || !gps.time.isValid()) {
    return false;
  }

  gpsData.latitude = gps.location.lat();
  gpsData.longitude = gps.location.lng();
  gpsData.year = gps.date.year();
  gpsData.month = gps.date.month();
  gpsData.day = gps.date.day();
  gpsData.hour = gps.time.hour();
  gpsData.minute = gps.time.minute();
  gpsData.second = gps.time.second();

  // Convert the GPS timestamp to Unix time for later logging or storage.
  const DateTime gpsDateTime(
      gpsData.year, gpsData.month, gpsData.day,
      gpsData.hour, gpsData.minute, gpsData.second);
  gpsData.epochTime = gpsDateTime.unixtime();
  gpsData.hasFix = true;

  return true;
}

void gpsAcquireData() {
  Serial.println("GPS acquiring data -> START");

  gps = TinyGPSPlus();
  gpsData = GpsData();
  delay(10);
  configGPS();

  const uint32_t startTimeMs = millis();
  uint32_t lastPrintMs = 0;

  while (shouldContinueAcquisition(startTimeMs)) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
    }

    // Print a short status line every second while waiting for a fix.
    if (millis() - lastPrintMs > 1000) {
      lastPrintMs = millis();
      printGpsSearchStatus(startTimeMs);
    }

    if (updateGpsDataIfValid()) {
      Serial.println("GPS FIX ACQUIRED");
      break;
    }

    delay(50);
  }

  if (!gpsData.hasFix) {
    Serial.println("GPS TIMEOUT");
  } else {
    Serial.println("GPS acquiring data -> DONE");
  }
}

void deepSleepNow() {
  Serial.println("Entering deep sleep for 30 seconds...");
  Serial.flush();

  // Turn off GPS power before entering deep sleep.
  digitalWrite(GPSPowerPin, LOW);
  delay(50);

  esp_sleep_enable_timer_wakeup(kDeepSleepDurationUs);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(ButtonPin, INPUT_PULLUP);

  pinMode(GPSPowerPin, OUTPUT);
  digitalWrite(GPSPowerPin, HIGH);
  delay(500);

  gpsSerial.begin(kGpsBaud);

  gpsAcquireData();
  deepSleepNow();
}

void loop() {}
