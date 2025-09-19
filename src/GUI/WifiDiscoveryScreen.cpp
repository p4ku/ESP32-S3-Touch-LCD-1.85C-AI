#include "GUI.h"
#include "WifiDiscoveryScreen.h"
#include "WifiPasswordScreen.h"

char ssid_list[MAX_SSID_LIST][MAX_SSID_LEN];
int ssid_count = -1;

lv_obj_t *wifi_list = nullptr;

void store_wifi_ssid_to_eeprom(const String& ssid) {
    for (int i = 0; i < 64; i++) {
        EEPROM.write(EEPROM_WIFI_SSID_ADDR + i, i < ssid.length() ? ssid[i] : 0);
    }
}

void read_wifi_ssid_from_eeprom(String& ssid) {
    ssid.clear();
    for (int i = 0; i < 64; i++) {
        char c = EEPROM.read(EEPROM_WIFI_SSID_ADDR + i);
        if (c == '\0') break;
        ssid += c;
    }
}

struct WiFiScanContext {
    bool was_connected;
    String prev_ssid;
};

void WiFiScanTask(void* param) {
    std::unique_ptr<WiFiScanContext> ctx((WiFiScanContext*)param);

    Serial.println("Starting WiFi scan task...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    ssid_count = WiFi.scanNetworks();

    lv_async_call([](void *user_data) {
        if (!wifi_list) return;

        lv_obj_clean(wifi_list);

        if (ssid_count <= 0) {
            lv_obj_t *lbl = lv_label_create(wifi_list);
            lv_label_set_text(lbl, "No networks found.");
            lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
            return;
        }

        for (int i = 0; i < ssid_count && i < MAX_SSID_LIST; i++) {
            String ssid = WiFi.SSID(i);
            strncpy(ssid_list[i], ssid.c_str(), MAX_SSID_LEN);

            lv_obj_t *btn = lv_list_add_button(wifi_list, NULL, ssid_list[i]);

            lv_obj_add_event_cb(btn, [](lv_event_t *e) {
                lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
                const char *selected_ssid = lv_label_get_text(lv_obj_get_child(btn, 0));

                Serial.printf("Selected SSID: %s\n", selected_ssid);
                // Store selected SSID to EEPROM
                store_wifi_ssid_to_eeprom(selected_ssid);
                
                lv_obj_t *msg = lv_label_create(lv_scr_act());
                lv_label_set_text_fmt(msg, "Selected: %s\nSaved", selected_ssid);
                lv_obj_align(msg, LV_ALIGN_BOTTOM_MID, 0, -10);

                lv_timer_t *t = lv_timer_create([](lv_timer_t *t) {
                    GUI_SwitchToScreen(GUI_CreateWifiPasswordScreen, &wifi_password_screen);
                    lv_timer_del(t);
                }, 1500, NULL);
            }, LV_EVENT_CLICKED, NULL);
        }

        WiFi.scanDelete();
    }, NULL);

    // Reconnect if needed
    if (ctx->was_connected && ctx->prev_ssid.length() > 0) {
        Serial.printf("Reconnecting to %s...\n", ctx->prev_ssid.c_str());

        String stored_pass;
        read_wifi_password_from_eeprom(stored_pass);
        WiFi.begin(ctx->prev_ssid.c_str(), stored_pass.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Reconnected to WiFi.");
        } else {
            Serial.println("Reconnection failed.");
        }
    }

    vTaskDelete(NULL);
}

void StartWiFiScanTask() {
    ssid_count = -1;

    WiFiScanContext* ctx = new WiFiScanContext;
    ctx->was_connected = WiFi.status() == WL_CONNECTED;
    ctx->prev_ssid = WiFi.SSID();

    xTaskCreatePinnedToCore(WiFiScanTask, "WiFiScanTask", 4096, ctx, 1, NULL, APP_CPU_NUM);
}

void GUI_CreateWifiDiscoveryScreen() {
    EEPROM.begin(EEPROM_SIZE);

    if (!wifi_discovery_screen) {
        Serial.println("Creating GUI_CreateWifiDiscoveryScreen");

        wifi_discovery_screen = lv_obj_create(NULL);
        lv_obj_t *screen = wifi_discovery_screen;

        lv_obj_t *title = lv_label_create(screen);
        lv_label_set_text(title, "Select WiFi");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

        wifi_list = lv_list_create(screen);
        lv_obj_set_size(wifi_list, 300, 240);
        lv_obj_align(wifi_list, LV_ALIGN_CENTER, 0, 20);

        lv_obj_t* loading = lv_label_create(wifi_list);
        lv_label_set_text(loading, "Scanning...");
        lv_obj_align(loading, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t* back_btn = lv_button_create(screen);
        lv_obj_set_size(back_btn, 260, 50);
        lv_obj_set_pos(back_btn, 55, 315);
        lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
            GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* label_back = lv_label_create(back_btn);
        lv_label_set_text(label_back, "< Back");
        lv_obj_center(label_back);
        Serial.println("GUI_CreateWifiDiscoveryScreen created");
    } else {
        // If screen already exists, refresh the list
        lv_obj_clean(wifi_list);
        lv_obj_t* loading = lv_label_create(wifi_list);
        lv_label_set_text(loading, "Scanning...");
        lv_obj_align(loading, LV_ALIGN_CENTER, 0, 0);
    }

    // Always start scan
    StartWiFiScanTask();
}

void GUI_UpdateWifiDiscoveryScreen(const struct tm& rtcTime){

}
