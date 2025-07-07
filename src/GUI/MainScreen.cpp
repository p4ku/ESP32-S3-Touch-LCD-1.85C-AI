#include "MainScreen.h"
#include "GUI.h"
#include "PCM5101.h"
#include "MIC_MSM.h"

void GUI_CreateMainScreen() {
    if (main_screen) return;
    Serial.println("Creating GUI_CreateMainScreen");

    main_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(main_screen, LV_SCROLLBAR_MODE_OFF);

    // --- Clock label ---
    clock_label = lv_label_create(main_screen);
    lv_obj_add_style(clock_label, &style_clock, 0);
    lv_label_set_text(clock_label, "--:--:--");
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, -80);
    lv_obj_add_flag(clock_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(clock_label, [](lv_event_t* e) {
        GUI_CreateClockScreen();
        lv_scr_load(clock_screen);
    }, LV_EVENT_CLICKED, NULL);

    // --- Message label ---
    message_label = lv_label_create(main_screen);
    lv_obj_add_style(message_label, &style_message, 0);
    lv_label_set_text_fmt(message_label, "%s", "");
    lv_obj_align(message_label, LV_ALIGN_CENTER, 0, -40);

    // --- Pause Button ---
    lv_obj_t* btn_pause = lv_button_create(main_screen);
    lv_obj_add_style(btn_pause, &style_btn, 0);
    lv_obj_add_style(btn_pause, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_pause, 60, 60);
    lv_obj_set_pos(btn_pause, 190, 180);
    lv_obj_add_event_cb(btn_pause, [](lv_event_t* e) {
        if (audio_ptr) audio_ptr->pauseResume();
        GUI_ClearMessage();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_radius(btn_pause, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_image_src(btn_pause, LV_SYMBOL_PAUSE, 0);
    lv_obj_set_style_text_font(btn_pause, lv_theme_get_font_large(btn_pause), 0);

    // --- Stop Button ---
    lv_obj_t* btn_stop = lv_button_create(main_screen);
    lv_obj_add_style(btn_stop, &style_btn, 0);
    lv_obj_add_style(btn_stop, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_stop, 90, 90);
    lv_obj_set_pos(btn_stop, 260, 160);
    lv_obj_add_event_cb(btn_stop, [](lv_event_t* e) {
        if (audio_ptr) audio_ptr->stopSong();
        GUI_ClearMessage();
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_radius(btn_stop, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_image_src(btn_stop, LV_SYMBOL_STOP, 0);
    lv_obj_set_style_text_font(btn_stop, lv_theme_get_font_large(btn_stop), 0);

    // --- Volume Slider ---
    lv_obj_t* volume_slider = lv_slider_create(main_screen);
    lv_obj_set_width(volume_slider, 220);
    lv_obj_set_height(volume_slider, 20);
    lv_obj_set_pos(volume_slider, 70, 280);
    lv_slider_set_range(volume_slider, 0, Volume_MAX);
    lv_slider_set_value(volume_slider, GetVolume(), LV_ANIM_OFF);
    lv_obj_add_event_cb(volume_slider, [](lv_event_t* e) {
        lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
        int val = lv_slider_get_value(slider);
        SetVolume(val);
        lv_label_set_text_fmt(vol_text_label, "Volume: %d", val);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // Volume label above slider
    vol_text_label = lv_label_create(main_screen);
    lv_obj_add_style(vol_text_label, &style_volume, 0);
    lv_label_set_text_fmt(vol_text_label, "Volume: %d", GetVolume());
    lv_obj_align_to(vol_text_label, volume_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // --- Internet Radio Button ---
    lv_obj_t* btn_inet = lv_button_create(main_screen);
    lv_obj_add_style(btn_inet, &style_btn, 0);
    lv_obj_add_style(btn_inet, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_inet, 250, 50);
    lv_obj_set_pos(btn_inet, 60, 320);
    lv_obj_add_event_cb(btn_inet, [](lv_event_t* e) {
        GUI_CreateInternetRadioScreen();
        lv_scr_load(internet_radio_screen);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label_inet = lv_label_create(btn_inet);
    lv_label_set_text(label_inet, "Internet Radio");
    lv_obj_center(label_inet);

    // --- SD Card MP3 Button ---
    lv_obj_t* btn_sd = lv_button_create(main_screen);
    lv_obj_add_style(btn_sd, &style_btn, 0);
    lv_obj_add_style(btn_sd, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_sd, 70, 70);
    lv_obj_set_pos(btn_sd, 90, 170);
    lv_obj_add_event_cb(btn_sd, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateSDCardMP3Screen, &sdcard_mp3_screen);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_radius(btn_sd, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_image_src(btn_sd, LV_SYMBOL_AUDIO, 0);
    lv_obj_set_style_text_font(btn_sd, lv_theme_get_font_large(btn_sd), 0);

    // --- Assistant Button ---
    lv_obj_t* btn_assistant = lv_button_create(main_screen);
    lv_obj_add_style(btn_assistant, &style_btn, 0);
    lv_obj_add_style(btn_assistant, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_assistant, 70, 70);
    lv_obj_set_pos(btn_assistant, 10, 170);
    lv_obj_add_event_cb(btn_assistant, [](lv_event_t* e) {
        MIC_SR_Stop();
        GUI_SwitchToScreen(GUI_CreateAssistantScreen, &assistant_screen);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_radius(btn_assistant, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_image_src(btn_assistant, LV_SYMBOL_VOLUME_MAX, 0);
    lv_obj_set_style_text_font(btn_assistant, lv_theme_get_font_large(btn_assistant), 0);

    // --- Alarm Button ---
    lv_obj_t* btn_alarm = lv_button_create(main_screen);
    lv_obj_add_style(btn_alarm, &style_btn, 0);
    lv_obj_add_style(btn_alarm, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_alarm, 240, 40);
    lv_obj_set_pos(btn_alarm, 60, 0);
    lv_obj_add_event_cb(btn_alarm, [](lv_event_t* e) {
        GUI_CreateAlarmScreen();
        lv_scr_load(alarm_screen);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label_alarm = lv_label_create(btn_alarm);
    lv_label_set_text(label_alarm, "Alarm");
    lv_obj_center(label_alarm);

    // GUI_AddSwipeSupport(main_screen, SCREEN_MAIN);
}
