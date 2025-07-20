#pragma once
#include <lvgl.h>
#include <vector>
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Alarm {
    String time;                // Format "HH:MM"
    bool weekdays[7];           // Sunday to Saturday
    String action_type;         // "sound", "mp3", "radio"
    String action_path;         // Path or URL
    bool enabled;
};

void GUI_CreateAlarmListScreen();
void GUI_CreateAlarmEditScreen(int alarm_index = -1);
void GUI_CreateAlarmActiveScreen();
void LoadAlarms();
void SaveAlarms();

extern std::vector<Alarm> alarm_list;
extern lv_obj_t* alarm_screen;

#ifdef __cplusplus
}
#endif
