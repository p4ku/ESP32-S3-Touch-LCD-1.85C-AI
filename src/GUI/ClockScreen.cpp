#include "GUI.h"
#include "ClockScreen.h"

void GUI_CreateClockScreen() {
    if (clock_screen) return;
    Serial.println("Creating GUI_CreateClockScreen");

    static lv_obj_t* bigclock_label = nullptr;
    static lv_obj_t* date_label = nullptr;

    clock_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(clock_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(clock_screen, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_bg_color(clock_screen, lv_palette_main(LV_PALETTE_RED), 0);

    // Big clock label (HH:MM:SS)
    bigclock_label = lv_label_create(clock_screen);
    lv_label_set_text(bigclock_label, "--:--:--");
    lv_obj_set_style_text_font(bigclock_label, &lv_font_montserrat_48, 0);
    lv_obj_align(bigclock_label, LV_ALIGN_CENTER, 0, -60);

    // Date label (YYYY-MM-DD)
    date_label = lv_label_create(clock_screen);
    lv_label_set_text(date_label, "----/--/--");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_24, 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 20);

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
}
