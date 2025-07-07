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
/*

/GUI/
├── GUI.h
├── GUI.cpp                     - Common init, message queue, shared objects
├── MainScreen.cpp              - GUI_CreateMainScreen()
├── InternetRadioScreen.cpp     - GUI_CreateInternetRadioScreen()
├── MP3PlayerScreen.cpp         - GUI_CreateSDCardMP3Screen()
├── AlarmScreen.cpp             - GUI_CreateAlarmScreen()
├── AssistantScreen.cpp         - GUI_CreateAssistantScreen()
├── ClockScreen.cpp             - GUI_CreateClockScreen()

*/

void GUI_Init(Audio& audio);
void GUI_MessageQueueInit();
void GUI_SwitchToScreen(void (*creator)(), lv_obj_t** screen_ptr);

// Clock/message updates
void GUI_UpdateClock(const struct tm& rtcTime);
void GUI_UpdateMessage(const char* msg);
void GUI_ClearMessage();
void GUI_EnqueueMessage(const char* msg);
void GUI_Tick();

// Shared audio
extern Audio* audio_ptr;
extern String filename;

// Shared styling
extern lv_style_t style_btn;
extern lv_style_t style_btn_pressed;
extern lv_style_t style_label;
extern lv_style_t style_clock;
extern lv_style_t style_volume;
extern lv_style_t style_message;

// Shared labels
extern lv_obj_t* clock_label;
extern lv_obj_t* message_label;
extern lv_obj_t* vol_text_label;

// Shared screens
extern lv_obj_t* main_screen;
extern lv_obj_t* internet_radio_screen;
extern lv_obj_t* sdcard_mp3_screen;
extern lv_obj_t* alarm_screen;
extern lv_obj_t* assistant_screen;
extern lv_obj_t* clock_screen;

// Define screen enum
typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_INTERNET,
    SCREEN_SDCARD,
    SCREEN_ALARM,
    SCREEN_CLOCK,
    SCREEN_ASSISTANT,
    SCREEN_COUNT
} ScreenIndex;

// Register swipe support
void GUI_AddSwipeSupport(lv_obj_t* screen, ScreenIndex current_screen);

