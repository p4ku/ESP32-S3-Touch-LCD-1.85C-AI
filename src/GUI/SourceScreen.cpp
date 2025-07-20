#include "SourceScreen.h"
#include "GUI.h"
#include "PCM5101.h"
#include "MIC_MSM.h"

static lv_obj_t* source_list;

static void source_event_handler(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    const char* label = lv_list_get_button_text(source_list, obj);

    if (strcmp(label, "Internet Radio") == 0) {
        GUI_SwitchToScreen(GUI_CreateInternetRadioScreen, &internet_radio_screen);
    } else if (strcmp(label, "SD Card MP3") == 0) {
        GUI_SwitchToScreen(GUI_CreateSDCardMP3Screen, &sdcard_mp3_screen);
    } else if (strcmp(label, "Assistant") == 0) {
        MIC_SR_Stop();
        GUI_SwitchToScreen(GUI_CreateAssistantScreen, &assistant_screen);
    } else if (strcmp(label, "Back") == 0) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }
}


void GUI_CreateSourceScreen() {
    Serial.println("Creating GUI_CreateSourceScreen");
    if (source_screen) {
        lv_obj_del(source_screen);
        source_screen = nullptr;
    }

    source_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(source_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(source_screen, LV_SCROLLBAR_MODE_OFF);

    // --- Create list container ---
    source_list = lv_list_create(source_screen);
    lv_obj_set_size(source_list, 240, 260);
    lv_obj_center(source_list);
    lv_obj_set_style_pad_row(source_list, 30, 0);

    // --- Add list items with icons and labels ---
    lv_list_add_text(source_list, "Audio Sources");

    lv_obj_t* btn;

    btn = lv_list_add_button(source_list, LV_SYMBOL_AUDIO, "Internet Radio");
    lv_obj_add_event_cb(btn, source_event_handler, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(source_list, LV_SYMBOL_SD_CARD, "SD Card MP3");
    lv_obj_add_event_cb(btn, source_event_handler, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(source_list, LV_SYMBOL_VOLUME_MAX, "Assistant");
    lv_obj_add_event_cb(btn, source_event_handler, LV_EVENT_CLICKED, NULL);

    lv_list_add_text(source_list, "");

    // Optional: scroll to top on first load
    lv_obj_scroll_to_view(lv_obj_get_child(source_list, 0), LV_ANIM_OFF);

    // Back button
    lv_obj_t* back_btn = lv_button_create(source_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(back_btn);
    lv_label_set_text(label, "< Back");
    lv_obj_center(label);
   
}

void GUI_UpdateSourceScreen() {
   
}
