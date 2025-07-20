#include "GUI.h"
#include "InternetRadioScreen.h"
#include "SD_Card.h"

void GUI_CreateInternetRadioScreen() {
    if (internet_radio_screen) return;
    Serial.println("Creating GUI_CreateInternetRadioScreen");

    internet_radio_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(internet_radio_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(internet_radio_screen, LV_SCROLLBAR_MODE_OFF);

    // Title
    lv_obj_t* title = lv_label_create(internet_radio_screen);
    lv_label_set_text(title, "Internet Radio");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    // Station list
    lv_obj_t* radio_container = lv_list_create(internet_radio_screen);
    lv_obj_set_size(radio_container, 280, 225);
    lv_obj_center(radio_container);

    // Load stations
    std::vector<std::pair<String, String>> stations = ReadInternetStations();

    for (const auto& station : stations) {
        const String& name = station.first;
        const String& url = station.second;

        lv_obj_t* btn = lv_list_add_button(radio_container, LV_SYMBOL_AUDIO, name.c_str());
        lv_obj_set_style_pad_top(btn, 20, 0);
        lv_obj_set_style_pad_bottom(btn, 20, 0);

        // Allocate memory for URL copy
        char* url_copy = strdup(url.c_str());

        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            const char* url = static_cast<const char*>(lv_event_get_user_data(e));
            Serial.printf("Playing: %s\n", url);
            if (audio_ptr) {
                audio_ptr->connecttohost(url);
                GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
            }
        }, LV_EVENT_CLICKED, url_copy);

        // Optional cleanup on delete
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            void* user_data = lv_event_get_user_data(e);
            if (user_data) free(user_data);
        }, LV_EVENT_DELETE, url_copy);
    }

    // Back button
    lv_obj_t* back_btn = lv_button_create(internet_radio_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateSourceScreen, &source_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(back_btn);
    lv_label_set_text(label, "< Back");
    lv_obj_center(label);
}
