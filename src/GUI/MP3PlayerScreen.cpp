#include "GUI.h"
#include "MP3PlayerScreen.h"
#include "SD_Card.h"

void GUI_CreateSDCardMP3Screen() {
    if (sdcard_mp3_screen) return;
    Serial.println("Creating GUI_CreateSDCardMP3Screen");

    // Load MP3 files from SD
    std::vector<String>* file_list = new(std::nothrow) std::vector<String>();
    LoadSDCardMP3Files(file_list);

    sdcard_mp3_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(sdcard_mp3_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(sdcard_mp3_screen, LV_SCROLLBAR_MODE_OFF);

    // Title
    lv_obj_t* title = lv_label_create(sdcard_mp3_screen);
    lv_label_set_text(title, "MP3");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    // File list container
    lv_obj_t* list = lv_list_create(sdcard_mp3_screen);
    lv_obj_set_size(list, 280, 200);
    lv_obj_center(list);

    for (const auto& name : *file_list) {
        std::string* path_ptr = new std::string("/music/" + std::string(name.c_str()));

        lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_PLAY, name.c_str());
        lv_obj_set_style_pad_top(btn, 20, 0);
        lv_obj_set_style_pad_bottom(btn, 20, 0);

        // Play file on click
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            // Prevent accidental touch during scroll
            lv_indev_t* indev = lv_indev_get_act();
            if (indev && lv_indev_get_state(indev) == LV_INDEV_STATE_PR) return;

            std::string* path = static_cast<std::string*>(lv_event_get_user_data(e));
            Serial.printf("Playing file: %s\n", path->c_str());

            if (audio_ptr && !audio_ptr->connecttoFS(SD_MMC, path->c_str())) {
                Serial.println("Failed to play MP3 file");
            }

            GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
        }, LV_EVENT_CLICKED, path_ptr);

        // Free path memory when button is deleted
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            void* ud = lv_event_get_user_data(e);
            if (ud) {
                delete static_cast<std::string*>(ud);
            }
        }, LV_EVENT_DELETE, path_ptr);
    }

    // Back button
    lv_obj_t* back_btn = lv_button_create(sdcard_mp3_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateSourceScreen, &source_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(back_btn);
    lv_label_set_text(label, "< Back");
    lv_obj_center(label);

    // Cleanup list object
    delete file_list;

    Serial.println("GUI_CreateSDCardMP3Screen created");
}
