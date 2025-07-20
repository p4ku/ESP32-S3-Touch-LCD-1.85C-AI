#pragma once

#include <lvgl.h>
#include <EEPROM.h>
#include "../config.h" 

#define NUM_CHARS 38

#ifdef __cplusplus
extern "C" {
#endif

// Create wifi password screen
void GUI_CreateWifiPasswordScreen();
void read_wifi_password_from_eeprom(String& out_buf);
void store_wifi_password_to_eeprom(const String& pass);

#ifdef __cplusplus
}
#endif
