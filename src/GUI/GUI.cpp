#include "GUI.h"
#include <freertos/queue.h>
#include <Arduino.h>

// --- Screen pointers ---
lv_obj_t* main_screen = nullptr;
lv_obj_t* internet_radio_screen = nullptr;
lv_obj_t* sdcard_mp3_screen = nullptr;
lv_obj_t* alarm_screen = nullptr;
lv_obj_t* assistant_screen = nullptr;
lv_obj_t* clock_screen = nullptr;

// --- Global shared widgets ---
lv_obj_t* clock_label = nullptr;
lv_obj_t* message_label = nullptr;
lv_obj_t* vol_text_label = nullptr;
lv_obj_t* bigclock_label = nullptr;
lv_obj_t* date_label = nullptr;

// --- Shared audio ---
Audio* audio_ptr = nullptr;

// --- Shared styles ---
lv_style_t style_btn;
lv_style_t style_btn_pressed;
lv_style_t style_label;
lv_style_t style_clock;
lv_style_t style_volume;
lv_style_t style_message;

// --- Message Queue ---
static QueueHandle_t msgQueue = nullptr;

// --- Optional: filename (e.g., from assistant) ---
String filename = "";

// --- Screen Switching ---
struct ScreenSwitchData {
    void (*creator)();
    lv_obj_t** screen_ptr;
};

#define SCREEN_COUNT 4

ScreenSwitchData screen_table[SCREEN_COUNT] = {
    {GUI_CreateMainScreen,        &main_screen},
    {GUI_CreateInternetRadioScreen, &internet_radio_screen},
    {GUI_CreateSDCardMP3Screen,   &sdcard_mp3_screen},
    {GUI_CreateAlarmScreen,       &alarm_screen}
};

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

    lv_style_init(&style_clock);
    lv_style_set_text_font(&style_clock, &lv_font_montserrat_48);
    lv_style_set_text_color(&style_clock, lv_palette_main(LV_PALETTE_GREEN));

    lv_style_init(&style_message);
    lv_style_set_text_font(&style_message, &lv_font_montserrat_26);
    lv_style_set_text_color(&style_message, lv_color_hex(0xAAAAAA));

    lv_style_init(&style_volume);
    lv_style_set_text_font(&style_volume, &lv_font_montserrat_18);
    lv_style_set_text_color(&style_volume, lv_color_hex(0xAAAAAA));

    GUI_MessageQueueInit();
    GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
}

// --- Switching Screens ---
void GUI_SwitchToScreen(void (*creator)(), lv_obj_t** screen_ptr) {
    if (screen_ptr && *screen_ptr) {
        lv_scr_load(*screen_ptr);
        return;
    }

    creator();  // expected to set *screen_ptr
    if (screen_ptr && *screen_ptr) {
        lv_scr_load(*screen_ptr);
    } else {
        Serial.println("[GUI_SwitchToScreen] Error: screen_ptr is null after creation!");
    }
}

// --- Clock Update ---
void GUI_UpdateClock(const struct tm& rtcTime) {
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &rtcTime);

    if (clock_label)       lv_label_set_text(clock_label, time_str);
    if (bigclock_label)    lv_label_set_text(bigclock_label, time_str);

    if (date_label) {
        char date_str[16];
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", &rtcTime);
        lv_label_set_text(date_label, date_str);
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

void GUI_AddSwipeSupport(lv_obj_t* screen, ScreenIndex current_screen) {
    static ScreenIndex screen_ids[SCREEN_COUNT];
    screen_ids[current_screen] = current_screen;

    static lv_obj_t* last_pressed_obj = nullptr;

    lv_obj_add_event_cb(screen, [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
        ScreenIndex* current = (ScreenIndex*)lv_event_get_user_data(e);

        if (!current) return;

        if (code == LV_EVENT_PRESSED) {
            last_pressed_obj = target;
            return;
        }

        if (code != LV_EVENT_GESTURE) return;

        // Avoid swipe conflict with sliders or buttons
        if (last_pressed_obj &&
            (lv_obj_check_type(last_pressed_obj, &lv_slider_class) ||
             lv_obj_check_type(last_pressed_obj, &lv_button_class))) {
            return;
        }

        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

        if (dir == LV_DIR_LEFT) {
            int next = (*current + 1) % SCREEN_COUNT;
            GUI_SwitchToScreen(screen_table[next].creator, screen_table[next].screen_ptr);
        } else if (dir == LV_DIR_RIGHT) {
            int prev = (*current - 1 + SCREEN_COUNT) % SCREEN_COUNT;
            GUI_SwitchToScreen(screen_table[prev].creator, screen_table[prev].screen_ptr);
        }

    }, LV_EVENT_ALL, &screen_ids[current_screen]);
}
