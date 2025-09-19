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

static String last_msg;
static bool is_scrolling = false;

volatile bool last_wifi_connected = false;
volatile bool backend_connected = false;

// --- Optional: filename (e.g., from assistant) ---
String filename = "";


// ISO-8859-1 → UTF-8 + control-char cleanup + CR/LF normalize + length cap
static std::string to_utf8_and_sanitize(const char* in, size_t max_len = 512) {
    if (!in) return {};
    std::string out;
    out.reserve(2 * strlen(in));
    size_t n = 0;
    const unsigned char* p = (const unsigned char*)in;

    while (*p && n < max_len) {
        unsigned c = *p++;

        // Replace most control chars with space (keep \n and \t only)
        if (c < 0x20 && c != '\n' && c != '\t') c = ' ';
        if (c == '\r') c = ' '; // normalize CR to space

        // Basic ISO-8859-1 → UTF-8 mapping
        if (c < 0x80) {
            out.push_back((char)c);
        } else {
            out.push_back((char)(0xC0 | (c >> 6)));
            out.push_back((char)(0x80 | (c & 0x3F)));
        }
        ++n;
    }

    // Optional: strip LVGL recolor sequences if you don't use recolor
    // (prevents accidental recolor parsing on '{' '#', etc.)
    // for (size_t i = 0; i + 1 < out.size(); ) { ... }

    return out;
}

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

// GUI transition state
volatile bool g_gui_transitioning = false;
volatile uint32_t g_last_screen_load_ms = 0;

static void indevs_enable(bool en) {
  for (lv_indev_t* i = nullptr; (i = lv_indev_get_next(i)); ) lv_indev_enable(i, en);
}

// Add render parameter to GUI_SwitchToScreen
void GUI_SwitchToScreen(void (*creator)(), lv_obj_t** screen_ptr, bool render) {
  if (g_gui_transitioning) return;

  // Fast path when already created (no heavy creator call)
  if (screen_ptr && *screen_ptr && !render) {
    g_gui_transitioning = true;
    indevs_enable(false);
    current_screen = *screen_ptr;
    lv_scr_load(current_screen);
    g_last_screen_load_ms = millis();
    indevs_enable(true);
    g_gui_transitioning = false;
    return;
  }

  Serial.println("[GUI_SwitchToScreen] Async creation started");
  g_gui_transitioning = true;
  indevs_enable(false);

  lv_async_call([](void* data) {
    auto args = (std::pair<void(*)(), lv_obj_t**>*)data;
    args->first();  // create
    if (*(args->second)) {
      current_screen = *(args->second);
      lv_scr_load(current_screen);
      g_last_screen_load_ms = millis();
    } else {
      Serial.println("[GUI_SwitchToScreen] Error: null screen after creation!");
    }
    indevs_enable(true);
    g_gui_transitioning = false;
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

// --- Update Message on the Main Screen ---
void GUI_UpdateMessage(const char* msg_in) {
    if (g_gui_transitioning) return;                   // drop during transitions
    if (!message_label) { Serial.println("[GUI] message_label is null"); return; }

    // Sanitize/encode first
    const std::string msg = to_utf8_and_sanitize(msg_in ? msg_in : "");

    // Skip if unchanged
    if (last_msg == msg.c_str()) return;

    // Ensure width once (idempotent)
    constexpr lv_coord_t MAX_W = 340;
    if (lv_obj_get_width(message_label) != MAX_W)
        lv_obj_set_width(message_label, MAX_W);

    // Set the text
    lv_label_set_text(message_label, msg.c_str());

    // Measure only to decide scroll state
    const lv_font_t* fnt = (const lv_font_t*)lv_obj_get_style_text_font(message_label, LV_PART_MAIN);
    if (!fnt) fnt = &lv_font_montserrat_18; // defensive fallback
    lv_coord_t ls = lv_obj_get_style_text_letter_space(message_label, LV_PART_MAIN);
    lv_coord_t ln = lv_obj_get_style_text_line_space(message_label, LV_PART_MAIN);

    lv_point_t sz;
    lv_txt_get_size(&sz, msg.c_str(), fnt, ls, ln,
                    LV_COORD_MAX,           // natural width (no wrap)
                    LV_TEXT_FLAG_NONE);     // no recolor; keep flags simple

    const bool need_scroll = (sz.x > MAX_W);
    if (need_scroll != is_scrolling) {
        is_scrolling = need_scroll;
        lv_label_set_long_mode(
            message_label,
            is_scrolling ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_CLIP
        );
        if (!is_scrolling) {
            // Only center when not scrolling (scroll ignores align)
            lv_obj_set_style_text_align(message_label, LV_TEXT_ALIGN_CENTER, 0);
        }
    }

    // Align once
    static bool aligned_once = false;
    if (!aligned_once) {
        lv_obj_align(message_label, LV_ALIGN_CENTER, 0, -40);
        aligned_once = true;
    }

    last_msg = msg.c_str();
}

void GUI_ClearMessage() {
    if (message_label) {
        lv_label_set_text(message_label, "");
    }
}

// Create Message Queue
void GUI_MessageQueueInit() {
    msgQueue = xQueueCreate(3, sizeof(char[64]));
}

// Enqueue Message
void GUI_EnqueueMessage(const char* msg) {
    if (!msgQueue) return;
    char buffer[64];
    strncpy(buffer, msg, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    xQueueSend(msgQueue, buffer, 0);
    Serial.print(">>>>>>>> ");
    Serial.println(msg);
}

// Dequeue Message and Update Message on the Main Screen
void GUI_QueueTick() {
    if (!msgQueue) return;

    static uint32_t lastUpdate = 0;
    uint32_t now = millis();

    // Only update at most every 500 ms
    if (now - lastUpdate < 500) return;

    char msg[64];
    if (xQueueReceive(msgQueue, msg, 0) == pdTRUE) {
        GUI_UpdateMessage(msg);
        lastUpdate = now;
    }
}
