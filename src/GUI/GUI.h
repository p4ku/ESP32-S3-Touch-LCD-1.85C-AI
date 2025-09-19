#pragma once
#include <lvgl.h>
#include <vector>
#include <utility>
#include <Arduino.h>
#include "Audio.h"
#include "MainScreen.h"
#include "InternetRadioScreen.h"
#include "MP3PlayerScreen.h"
#include "AlarmScreen.h"
#include "AssistantScreen.h"
#include "ClockScreen.h"
#include "SourceScreen.h"
#include "ConfigScreen.h"
#include "WifiInfo.h"
#include "WifiDiscoveryScreen.h"

/*

/GUI/
├── GUI.h
├── GUI.cpp                     - Common init, message queue, shared objects
├── AlarmScreen.cpp             - GUI_CreateAlarmScreen()
├── AssistantScreen.cpp         - GUI_CreateAssistantScreen()
├── ClockScreen.cpp             - GUI_CreateClockScreen()
├── ConfigScreen.cpp            - GUI_CreateConfigScreen()
├── InternetRadioScreen.cpp     - GUI_CreateInternetRadioScreen()
├── MainScreen.cpp              - GUI_CreateMainScreen()
├── MP3PlayerScreen.cpp         - GUI_CreateSDCardMP3Screen()
├── SourceScreen.cpp            - GUI_CreateSourceScreen()
├── WifiDiscoveryScreen.cpp     - GUI_CreateWifiDiscoveryScreen()
├── WifiInfoScreen.cpp          - GUI_CreateWifiInfoScreen()
├── WifiPasswordScreen.cpp      - GUI_CreateWifiPasswordScreen()

*/

void GUI_Init(Audio& audio);
void GUI_MessageQueueInit();
void GUI_SwitchToScreen(void (*creator)(), lv_obj_t** screen_ptr, bool render = false);
void GUI_SwitchToScreenAfter(lv_obj_t** screen_ptr);

// Clock/message updates
void GUI_UpdateClock(const struct tm& rtcTime);
void GUI_UpdateMessage(const char* msg);
void GUI_ClearMessage();
void GUI_EnqueueMessage(const char* msg);
void GUI_QueueTick();

// Alarm state
static volatile bool alarm_active = false;
static uint32_t alarm_started_ms = 0;

// Shared audio
extern Audio* audio_ptr;
extern String filename;

// Shared styling
extern lv_style_t style_btn;
extern lv_style_t style_btn_pressed;
extern lv_style_t style_label;
extern lv_style_t style_clock;
extern lv_style_t style_bigclock;
extern lv_style_t style_seconds;
extern lv_style_t style_volume;
extern lv_style_t style_message;

// Shared labels
extern lv_obj_t* bigclock_label;
extern lv_obj_t* date_label;
extern lv_obj_t* clock_label;
extern lv_obj_t* seconds_label;
extern lv_obj_t* message_label;
extern lv_obj_t* vol_text_label;
extern lv_obj_t* wifi_icon;
extern lv_obj_t* backend_status;


// Current screen pointer
extern lv_obj_t* current_screen;

// Shared screens
extern lv_obj_t* main_screen;
extern lv_obj_t* config_screen;
extern lv_obj_t* source_screen;
extern lv_obj_t* internet_radio_screen;
extern lv_obj_t* sdcard_mp3_screen;
extern lv_obj_t* alarm_screen;
extern lv_obj_t* alarm_screen_edit;
extern lv_obj_t* assistant_screen;
extern lv_obj_t* clock_screen;
extern lv_obj_t* wifi_info_screen;
extern lv_obj_t* wifi_discovery_screen;
extern lv_obj_t* wifi_password_screen;

// Define screen enum
typedef enum {
    SCREEN_MAIN = 0, // For Main screen
    SCREEN_CONFIG, // For Configuration screen
    SCREEN_SOURCE, // For Source selection screen
    SCREEN_INTERNET, // For Internet Radio screen
    SCREEN_SDCARD, // For SD Card MP3 screen
    SCREEN_ALARM, // For Alarm screen
    SCREEN_CLOCK, // For Clock screen
    SCREEN_ASSISTANT, // For Assistant screen
    SCREEN_WIFI_INFO, // For WiFi information
    SCREEN_WIFI_DISCOVERY,  // For WiFi discovery
    SCREEN_WIFI_PASSWORD    // For WiFi password entry
} ScreenIndex;

extern volatile bool last_wifi_connected;
extern volatile bool backend_connected;

extern lv_group_t* global_input_group; 

// GUI transition state
extern volatile bool g_gui_transitioning;
extern volatile uint32_t g_last_screen_load_ms;


