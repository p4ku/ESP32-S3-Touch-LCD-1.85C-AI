#include "GUI.h"
#include <freertos/queue.h>
#include <Arduino.h>

extern const uint8_t ubuntu_font[];
extern const int ubuntu_font_size;

extern const uint8_t digital7_mono_ttf[];
extern const int digital7_mono_size;

// --- Current screen pointer ---
lv_obj_t* current_screen = nullptr;

// --- Screen pointers ---
lv_obj_t* main_screen = nullptr;
lv_obj_t* config_screen = nullptr;
lv_obj_t* source_screen = nullptr;
lv_obj_t* internet_radio_screen = nullptr;
lv_obj_t* sdcard_mp3_screen = nullptr;
lv_obj_t* alarm_screen = nullptr;
lv_obj_t* alarm_screen_edit = nullptr; 
lv_obj_t* assistant_screen = nullptr;
lv_obj_t* clock_screen = nullptr;
lv_obj_t* wifi_info_screen = nullptr;
lv_obj_t* wifi_discovery_screen = nullptr;
lv_obj_t* wifi_password_screen = nullptr;

// --- Global shared widgets ---
lv_obj_t* bigclock_label = nullptr;
lv_obj_t* date_label = nullptr;
lv_obj_t* clock_label = nullptr;
lv_obj_t* seconds_label = nullptr;
lv_obj_t* message_label = nullptr;
lv_obj_t* vol_text_label = nullptr;
lv_obj_t* wifi_icon = nullptr;
lv_obj_t* backend_status = nullptr;

// --- Shared audio ---
Audio* audio_ptr = nullptr;

// --- Shared styles ---
lv_style_t style_btn;
lv_style_t style_btn_pressed;
lv_style_t style_label;
lv_style_t style_clock;
lv_style_t style_bigclock;
lv_style_t style_seconds;
lv_style_t style_volume;
lv_style_t style_message;

// --- Message Queue ---
static QueueHandle_t msgQueue = nullptr;

volatile bool last_wifi_connected = false;
volatile bool backend_connected = false;

// --- Optional: filename (e.g., from assistant) ---
String filename = "";

// --- GUI Lifecycle ---
void GUI_Init(Audio& audio) {
    audio_ptr = &audio;

    // Init styles once
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 10);
    lv_style_set_bg_color(&style_btn, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_border_color(&style_btn, lv_palette_darken(LV_PALETTE_BLUE, 3));
    lv_style_set_border_width(&style_btn, 2);

    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, lv_palette_darken(LV_PALETTE_BLUE, 2));

    lv_style_init(&style_label);
    lv_style_set_text_font(&style_label, &lv_font_montserrat_28);
    lv_style_set_text_color(&style_label, lv_color_white());

    // Main clock style HH:MM
    lv_style_init(&style_clock);
    lv_font_t * font = lv_tiny_ttf_create_data(digital7_mono_ttf, digital7_mono_size, 65);
    lv_style_set_text_font(&style_clock, font);
    lv_style_set_text_color(&style_clock, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_align(&style_clock, LV_TEXT_ALIGN_CENTER);

    // Main clock seconds style
    lv_style_init(&style_seconds);
    lv_font_t * font_seconds = lv_tiny_ttf_create_data(digital7_mono_ttf, digital7_mono_size, 35);
    lv_style_set_text_font(&style_seconds, font_seconds);
    lv_style_set_text_color(&style_seconds, lv_palette_main(LV_PALETTE_GREEN));

    // Big clock style HH:MM:SS
    lv_style_init(&style_bigclock);
    lv_font_t * font_bigclock = lv_tiny_ttf_create_data(ubuntu_font, ubuntu_font_size, 72);
    lv_style_set_text_font(&style_bigclock, font_bigclock);
    // lv_style_set_text_color(&style_bigclock, lv_palette_main(LV_PALETTE_LIGHT_GREEN));

    lv_style_init(&style_message);
    lv_font_t * font_message = lv_tiny_ttf_create_data(ubuntu_font, ubuntu_font_size, 26);
    lv_style_set_text_font(&style_message, font_message);
    //lv_style_set_text_font(&style_message, &lv_font_montserrat_26);
    lv_style_set_text_color(&style_message, lv_color_hex(0xAAAAAA));

    lv_style_init(&style_volume);
    lv_style_set_text_font(&style_volume, &lv_font_montserrat_18);
    lv_style_set_text_color(&style_volume, lv_color_hex(0xAAAAAA));

    GUI_MessageQueueInit();
    GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
}

lv_group_t* global_input_group;  // Must be defined in your GUI.cpp

// Add render parameter to GUI_SwitchToScreen
void GUI_SwitchToScreen(void (*creator)(), lv_obj_t** screen_ptr, bool render) {
    if (screen_ptr && *screen_ptr && !render) {
        current_screen = *screen_ptr;
        lv_scr_load(*screen_ptr);
        return;
    }

    Serial.println("[GUI_SwitchToScreen] Async creation started");
    lv_async_call([](void* data) {
        auto args = (std::pair<void(*)(), lv_obj_t**>*)data;
        args->first();  // creator()
        if (*(args->second)) {
            current_screen = *(args->second);
            lv_scr_load(current_screen);
        } else {
            Serial.println("[GUI_SwitchToScreen] Error: screen_ptr null after async creation!");
        }
        delete args;
    }, new std::pair<void(*)(), lv_obj_t**>(creator, screen_ptr));
}


void GUI_SwitchToScreenAfter(lv_obj_t** screen_ptr) {
    if (screen_ptr && *screen_ptr) {
        current_screen = *screen_ptr;
        lv_scr_load(*screen_ptr);
        return;
    }
}

// --- Clock Update ---
void GUI_UpdateClock(const struct tm& rtcTime) {
    if (current_screen == clock_screen) {
        GUI_UpdateClockScreen(rtcTime);
    } else if (current_screen == main_screen) {
        GUI_UpdateMainScreen(rtcTime);
    } else if (current_screen == wifi_discovery_screen) {
        GUI_UpdateWifiDiscoveryScreen(rtcTime);
    } else if (current_screen == wifi_info_screen) {
        
    } else if (current_screen == wifi_password_screen) {
       
    }
}

// --- Message ---
void GUI_UpdateMessage(const char* msg) {
    if (message_label) {
        lv_obj_set_width(message_label, 340);
        lv_label_set_long_mode(message_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_label_set_text(message_label, msg);
        lv_obj_align(message_label, LV_ALIGN_CENTER, 0, -40);
    } else {
        Serial.println("[GUI] message_label is null");
    }
}

void GUI_ClearMessage() {
    if (message_label) {
        lv_label_set_text(message_label, "");
    }
}

// --- Queue API ---
void GUI_MessageQueueInit() {
    msgQueue = xQueueCreate(3, sizeof(char[64]));
}

void GUI_EnqueueMessage(const char* msg) {
    if (!msgQueue) return;
    char buffer[64];
    strncpy(buffer, msg, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    xQueueSend(msgQueue, buffer, 0);
    Serial.print(">>>>>>>> ");
    Serial.println(msg);
}

void GUI_Tick() {
    if (!msgQueue) return;

    char msg[64];
    if (xQueueReceive(msgQueue, msg, 0) == pdTRUE) {
        GUI_UpdateMessage(msg);
    }
}
