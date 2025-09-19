#include "GUI.h"
#include "ClockScreen.h"

extern const uint8_t ubuntu_font[];
extern const int ubuntu_font_size;

void GUI_CreateClockScreen() {
    if (clock_screen) return;
    Serial.println("Creating GUI_CreateClockScreen");

    clock_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(clock_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(clock_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(clock_screen, lv_palette_main(LV_PALETTE_LIGHT_GREEN), 0);

    // Big clock label (HH:MM:SS)
    bigclock_label = lv_label_create(clock_screen);
    lv_label_set_text(bigclock_label, "--:--:--");
    lv_obj_add_style(bigclock_label, &style_bigclock, 0);
    lv_obj_align(bigclock_label, LV_ALIGN_CENTER, 0, -40);

    // Date label (YYYY-MM-DD)
    date_label = lv_label_create(clock_screen);
    lv_label_set_text(date_label, "--/--/----");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_28, 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 30);

    // Back button
    lv_obj_t* back_btn = lv_button_create(clock_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label_back = lv_label_create(back_btn);
    lv_label_set_text(label_back, "< Back");
    lv_obj_center(label_back);

    // GUI_AddSwipeSupport(clock_screen, SCREEN_CLOCK);  // if needed
    Serial.println("GUI_CreateClockScreen created");
}

void GUI_UpdateClockScreen(const struct tm& rtcTime) {
    if (!clock_screen || lv_scr_act() != clock_screen) return;

    if (bigclock_label)   {
        char time_str[16];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &rtcTime);
        lv_label_set_text(bigclock_label, time_str);
    }

    if (date_label) {
        char date_str[16];
        strftime(date_str, sizeof(date_str), "%d-%m-%Y", &rtcTime);
        lv_label_set_text(date_label, date_str);
    }
}
