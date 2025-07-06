#include <Wire.h>
#include "RTC_PCF85063.h"

// RTC (PCF85063) Helper Functions
uint8_t decToBcd(uint8_t val) {
  return ((val / 10 * 16) + (val % 10));
}

uint8_t bcdToDec(uint8_t val) {
  return ((val / 16 * 10) + (val % 16));
}

void RTC_SetTime(struct tm* t) {
  // Convert to UTC before saving
  time_t local = mktime(t);  // Convert to epoch
  // local -= 3600;             // Subtract your GMT offset (e.g. 3600s for UTC+1)
  struct tm* utc = gmtime(&local);

  constexpr int maxRetries = 3;
  int attempt = 0;

  while (attempt < maxRetries) {
    Wire.beginTransmission(PCF85063_ADDRESS);
    Wire.write(0x04);  // Start at seconds register
    Wire.write(decToBcd(utc->tm_sec));
    Wire.write(decToBcd(utc->tm_min));
    Wire.write(decToBcd(utc->tm_hour));
    Wire.write(decToBcd(utc->tm_mday));
    Wire.write(decToBcd(utc->tm_wday));
    Wire.write(decToBcd(utc->tm_mon + 1));
    Wire.write(decToBcd(utc->tm_year % 100));
    
    if (Wire.endTransmission() == 0) return;
    attempt++;
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  Serial.println("[RTC] Failed to set UTC time after retries");
}


bool RTC_GetTime(struct tm* t) {
  Wire.beginTransmission(PCF85063_ADDRESS);
  Wire.write(0x04);
  if (Wire.endTransmission(false) != 0){
     Serial.println("[RTC] Failed to get RTC time, no device");
     return false;
  }

  if (Wire.requestFrom(PCF85063_ADDRESS, 7) != 7){
    Serial.println("[RTC] Failed to get RTC time, wrong data");
    return false;
  }

  t->tm_sec  = bcdToDec(Wire.read() & 0x7F);
  t->tm_min  = bcdToDec(Wire.read() & 0x7F);
  t->tm_hour = bcdToDec(Wire.read() & 0x3F);
  t->tm_mday = bcdToDec(Wire.read() & 0x3F);
  t->tm_wday = bcdToDec(Wire.read() & 0x07);
  t->tm_mon  = bcdToDec(Wire.read() & 0x1F) - 1;
  t->tm_year = bcdToDec(Wire.read()) + 100;

  // Convert to epoch and apply local offset
  time_t utc = mktime(t);
  utc += 3600;  // Add timezone offset here (e.g. +1h for CET)
  struct tm *local = localtime(&utc);
  *t = *local;

  return true;
}
