#ifndef RTC_PCF85063_H
#define RTC_PCF85063_H

#include <Arduino.h>
#include <time.h>

// I2C address of PCF85063
#define PCF85063_ADDRESS 0x51

uint8_t decToBcd(uint8_t val);
uint8_t bcdToDec(uint8_t val);

void RTC_SetTime(struct tm* t);
bool RTC_GetTime(struct tm* t);

#endif