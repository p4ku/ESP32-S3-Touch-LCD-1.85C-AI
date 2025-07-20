#include "GUI.h"
#include <WiFi.h> 

extern const uint8_t ubuntu_font[];
extern const int ubuntu_font_size;

void GUI_CreateWifiInfoScreen() {
    if (wifi_info_screen) return;
    Serial.println("Creating GUI_CreateWifiInfoScreen");

    wifi_info_screen = lv_obj_create(NULL);
    lv_obj_t *screen = wifi_info_screen;

    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "WiFi Info");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // List-style layout using LVGL list
    lv_obj_t *info_list = lv_list_create(screen);
    lv_obj_set_size(info_list, 300, 220);
    lv_obj_align(info_list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_pad_row(info_list, 6, 0);

    lv_font_t* ubuntu_lvfont = lv_tiny_ttf_create_data(ubuntu_font, ubuntu_font_size, 19);
    static lv_style_t style_wifi_info;
    lv_style_init(&style_wifi_info);
    lv_style_set_text_font(&style_wifi_info, ubuntu_lvfont);

    auto add_row = [&](const char* label, const String& value) {
        char line[128];
        snprintf(line, sizeof(line), "%-10s: %s", label, value.c_str());
        // lv_list_add_text(info_list, line);
        lv_obj_t* row = lv_list_add_text(info_list, line);
        // Remove background color and border
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);

        // Optional: no padding/margin if you want tighter layout
        lv_obj_set_style_pad_all(row, 0, 0);

        // Apply Ubuntu font style
        lv_obj_add_style(row, &style_wifi_info, 0);
    };

    if (WiFi.status() == WL_CONNECTED) {
        add_row("SSID", WiFi.SSID());
        add_row("IP", WiFi.localIP().toString());
        add_row("Mask", WiFi.subnetMask().toString());
        add_row("Gateway", WiFi.gatewayIP().toString());
        add_row("RSSI", String(WiFi.RSSI()) + " dBm");

        int strength = WiFi.RSSI();
        int quality = constrain(2 * (strength + 100), 0, 100);
        add_row("Signal", String(quality) + " %");
        add_row("Channel", String(WiFi.channel()));
        add_row("Hostname", WiFi.getHostname());
    } else {
        add_row("Status", "Not Connected");
    }

    // Back Button
    lv_obj_t* back_btn = lv_button_create(screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label_back = lv_label_create(back_btn);
    lv_label_set_text(label_back, "< Back");
    lv_obj_center(label_back);
}
