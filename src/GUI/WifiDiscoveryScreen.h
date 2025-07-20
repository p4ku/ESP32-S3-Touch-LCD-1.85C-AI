
#pragma once

#include <lvgl.h>
#include <WiFi.h>
#include <EEPROM.h>
#include "../config.h" 

#define MAX_SSID_LIST 20

#ifdef __cplusplus
extern "C" {
#endif

// Create wifi discovery screen
void GUI_CreateWifiDiscoveryScreen();
void GUI_UpdateWifiDiscoveryScreen(const struct tm& rtcTime);
void store_wifi_ssid_to_eeprom(const String& ssid);
void read_wifi_ssid_from_eeprom(String& ssid);

#ifdef __cplusplus
}
#endif
