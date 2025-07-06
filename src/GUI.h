#ifndef GUI_H
#define GUI_H

#include "lvgl.h"
#include "Audio.h"
#include "AIAssistant.h"

#ifdef __cplusplus
extern "C" {
#endif


static String pendingMessage = "";
static bool messagePending = false;

enum ScreenIndex {
    SCREEN_MAIN = 0,
    SCREEN_INTERNET,
    SCREEN_SDCARD,
    SCREEN_ALARM,
    SCREEN_COUNT
};

// Initializes the GUI elements (buttons, labels, etc.)
void GUI_Init(Audio &audio);

// Updates the clock label with the given time string (format: HH:MM:SS)
void GUI_UpdateClock(const struct tm& rtcTime);

void GUI_UpdateMessage(const char* msg);
void GUI_UpdateAudioMessage(Audio* audio);
void GUI_ClearMessage();

// Queue messages
void GUI_MessageQueueInit();
void GUI_EnqueueMessage(const char* msg);
void GUI_Tick();

// Screens
void GUI_CreateMainScreen();
void GUI_CreateInternetRadioScreen();
void GUI_CreateSDCardMP3Screen();
void GUI_CreateAlarmScreen();
void GUI_CreateAssistantScreen();
void GUI_CreateClockScreen();

void GUI_AddSwipeSupport(lv_obj_t* screen, ScreenIndex current_screen);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GUI_H
