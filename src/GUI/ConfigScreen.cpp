#include "ConfigScreen.h"
#include "GUI.h"
#include "WifiDiscoveryScreen.h"
#include "WifiInfo.h"

static lv_obj_t* config_list;

static void config_event_handler(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target_obj(e);
    const char* label = lv_list_get_button_text(config_list, obj);

    if (strcmp(label, "WIFI Configuration") == 0) {
        GUI_SwitchToScreen(GUI_CreateWifiDiscoveryScreen, &wifi_discovery_screen);
    } else if (strcmp(label, "WIFI Information") == 0) {
        GUI_SwitchToScreen(GUI_CreateWifiInfoScreen, &wifi_info_screen);
    } else {
        Serial.printf("Unknown config option: %s\n", label);
    }
}

void GUI_CreateConfigScreen() {
    if (config_screen) return;
    Serial.println("Creating GUI_CreateConfigScreen");

    config_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(config_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(config_screen, LV_SCROLLBAR_MODE_OFF);

    // --- Create list container ---
    config_list = lv_list_create(config_screen);
    lv_obj_set_size(config_list, 240, 260);
    lv_obj_center(config_list);  // center in screen
    lv_obj_set_style_pad_row(config_list, 30, 0);  // spacing between items

    // --- Add list items with icons and labels ---
    lv_list_add_text(config_list, "Configuration");

    lv_obj_t* btn;

    btn = lv_list_add_button(config_list, LV_SYMBOL_WIFI, "WIFI Configuration");
    lv_obj_add_event_cb(btn, config_event_handler, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_button(config_list, LV_SYMBOL_LIST, "WIFI Information");
    lv_obj_add_event_cb(btn, config_event_handler, LV_EVENT_CLICKED, NULL);

    lv_list_add_text(config_list, "");

    // Optional: scroll to top on first load
    lv_obj_scroll_to_view(lv_obj_get_child(config_list, 0), LV_ANIM_OFF);

    // Back button
    lv_obj_t* back_btn = lv_button_create(config_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(back_btn);
    lv_label_set_text(label, "< Back");
    lv_obj_center(label);
    Serial.println("GUI_CreateConfigScreen created");
}

void GUI_UpdateConfigScreen() {
   
}
