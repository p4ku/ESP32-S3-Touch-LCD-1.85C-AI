#include "GUI.h"
#include "AlarmScreen.h"

void GUI_CreateAlarmScreen() {
    if (alarm_screen) return;
    Serial.println("Creating GUI_CreateAlarmScreen");

    alarm_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(alarm_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(alarm_screen, LV_SCROLLBAR_MODE_OFF);

    // Background color
    lv_obj_set_style_bg_color(alarm_screen, lv_palette_main(LV_PALETTE_RED), 0);

    // --- Big STOP button ---
    lv_obj_t* stop_btn = lv_button_create(alarm_screen);
    lv_obj_set_size(stop_btn, 210, 210);
    lv_obj_center(stop_btn);
    lv_obj_set_style_bg_color(stop_btn, lv_palette_darken(LV_PALETTE_RED, 2), 0);
    lv_obj_set_style_bg_opa(stop_btn, LV_OPA_COVER, 0);

    lv_obj_t* label = lv_label_create(stop_btn);
    lv_label_set_text(label, "STOP");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_40, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(stop_btn, [](lv_event_t* e) {
        Serial.println("Alarm stopped. Stopping MIC stream.");
        // MIC_WS_StopStreaming();  // If used
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_RELEASED, NULL);

    lv_obj_add_event_cb(stop_btn, [](lv_event_t* e) {
        Serial.println("Starting MIC stream...");
        // MIC_WS_StartStreaming();  // If used
    }, LV_EVENT_PRESSED, NULL);

    // --- Back Button ---
    lv_obj_t* back_btn = lv_button_create(alarm_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label_back = lv_label_create(back_btn);
    lv_label_set_text(label_back, "< Back");
    lv_obj_center(label_back);

    // GUI_AddSwipeSupport(alarm_screen, SCREEN_ALARM);
}
