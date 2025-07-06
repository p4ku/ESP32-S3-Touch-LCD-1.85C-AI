#include <vector> 
#include "GUI.h"
#include <ArduinoJson.h>
#include "PCM5101.h"
#include "MIC_MSM.h"


// Screens
static lv_obj_t* main_screen = nullptr;
static lv_obj_t* internet_radio_screen = nullptr;
static lv_obj_t* sdcard_mp3_screen = nullptr;
static lv_obj_t* alarm_screen = nullptr;
static lv_obj_t* assistant_screen = nullptr;
static lv_obj_t* clock_screen = nullptr;

// Alarm
bool alarm_on = false;

// Global labels and audio pointer
static lv_obj_t *clock_label = nullptr;
static lv_obj_t *message_label = nullptr;
static lv_obj_t *vol_text_label = nullptr;

static lv_obj_t* bigclock_label = nullptr;
static lv_obj_t* date_label = nullptr;

// Audio ptr
static Audio* audio_ptr = nullptr;

// Shared styles
static lv_style_t style_btn;
static lv_style_t style_btn_pressed;
static lv_style_t style_label;
static lv_style_t style_clock;
static lv_style_t style_volume;
static lv_style_t style_message;


String filename = ""; // Filename for recording from microphone to SDCard

#include <freertos/queue.h>


static QueueHandle_t msgQueue = nullptr;

void GUI_MessageQueueInit() {
    msgQueue = xQueueCreate(3, sizeof(char[64])); // 3 pending messages max
}

std::vector<String>* sdcard_mp3_file_list = new(std::nothrow) std::vector<String>();

void LoadSDCardMP3Files(std::vector<String>* fileList, const char* path = "/music/") {
    if (!fileList) return;

    fileList->clear();  // Clear previous entries

    File dir = SD_MMC.open(path);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("Failed to open directory: %s\n", path);
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        String name = file.name();
        String lower = name;
        lower.toLowerCase();
        if (!file.isDirectory() && (name.endsWith(".mp3") || name.endsWith(".wav"))) {
            // Only store relative names for consistency
            if (name.startsWith(path)) {
                name = name.substring(strlen(path));
                if (name.startsWith("/")) name = name.substring(1);
            }
            fileList->push_back(name);
        }
        file = dir.openNextFile();
    }
    dir.close();
}


struct ScreenSwitchData {
    void (*creator)();      // Function that creates the screen if needed
    lv_obj_t** screen_ptr;  // Pointer to the screen variable
};

void GUI_SwitchToScreen(void (*creator)(), lv_obj_t** screen_ptr) {
    if (screen_ptr && *screen_ptr) {
        lv_scr_load(*screen_ptr);
        return;
    }

    creator(); // this should set *screen_ptr

    if (screen_ptr && *screen_ptr) {
        lv_scr_load(*screen_ptr);
    } else {
        Serial.println("[GUI_SwitchToScreen] Error: screen_ptr is null after creation!");
    }
}

// Main Screen with clock, message label and volume
void GUI_CreateMainScreen() {
    if (main_screen) return;  // Prevent re-creating if it already exists
    Serial.println("Creating GUI_CreateMainScreen");

    main_screen = lv_obj_create(NULL); // create new blank screen
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(main_screen, LV_SCROLLBAR_MODE_OFF);

    // --- Initialize styles ---
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

    // --- Clock label ---
    clock_label = lv_label_create(main_screen);
    lv_obj_add_style(clock_label, &style_clock, 0);
    lv_label_set_text(clock_label, "--:--:--");
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, -80);
    lv_obj_add_flag(clock_label, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_add_event_cb(clock_label,  [](lv_event_t* e) {
        GUI_CreateClockScreen();
        lv_scr_load(clock_screen);
    }, 
    LV_EVENT_CLICKED, NULL);

    // --- Message: Text/Song label ---
    message_label = lv_label_create(main_screen);
    lv_obj_add_style(message_label, &style_message, 0);
    lv_label_set_text_fmt(message_label, "%s", "");
    lv_obj_align(message_label, LV_ALIGN_CENTER, 0, -40);

    // --- Pause ---
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

    // --- Button: Stop ---
    lv_obj_t* btn_stop = lv_button_create(main_screen);
    lv_obj_add_style(btn_stop, &style_btn, 0);
    lv_obj_add_style(btn_stop, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_stop, 90, 90);
    lv_obj_set_pos(btn_stop, 260, 160);
    lv_obj_add_event_cb(btn_stop, [](lv_event_t *e) {
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
    lv_obj_set_pos(volume_slider, 70, 280);  // X, Y

    lv_slider_set_range(volume_slider, 0, Volume_MAX);
    lv_slider_set_value(volume_slider, GetVolume(), LV_ANIM_OFF);

    lv_obj_add_event_cb(volume_slider, [](lv_event_t* e) {
        lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
        int val = lv_slider_get_value(slider);
        SetVolume(val);
        lv_label_set_text_fmt(vol_text_label, "Volume: %d", val);
    }, LV_EVENT_VALUE_CHANGED, nullptr);

    // Optional: Add label above the slider
    vol_text_label = lv_label_create(main_screen);
    lv_obj_add_style(vol_text_label, &style_volume, 0);
    // lv_label_set_text(vol_text_label, "Volume");
    lv_label_set_text_fmt(vol_text_label, "Volume: %d", GetVolume());
    lv_obj_align_to(vol_text_label, volume_slider, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // --- Navigate to Internet Radio Screen ---
    lv_obj_t* btn_inet = lv_button_create(main_screen);
    lv_obj_add_style(btn_inet, &style_btn, 0);
    lv_obj_add_style(btn_inet, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_inet, 250, 50);
    lv_obj_set_pos(btn_inet, 60, 320);
    lv_obj_add_event_cb(btn_inet, [](lv_event_t* e) {
        GUI_CreateInternetRadioScreen();
        lv_scr_load(internet_radio_screen);
        // GUI_SwitchToScreen(GUI_CreateInternetRadioScreen, &internet_radio_screen);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label_inet = lv_label_create(btn_inet);
    // lv_obj_add_style(label_inet, &style_label_inet, 0);
    lv_label_set_text(label_inet, "Internet Radio");
    lv_obj_center(label_inet);

    // --- Navigate to SD MP3 Screen ---
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

    // --- Navigate to Assistant Screen ---
    lv_obj_t* btn_assistant = lv_button_create(main_screen);
    lv_obj_add_style(btn_assistant, &style_btn, 0);
    lv_obj_add_style(btn_assistant, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_assistant, 70, 70);
    lv_obj_set_pos(btn_assistant, 10, 170);
    lv_obj_add_event_cb(btn_assistant, [](lv_event_t* e) {
        MIC_SR_Stop(); // Stop any ongoing speech recognition
        GUI_SwitchToScreen(GUI_CreateAssistantScreen, &assistant_screen);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_set_style_radius(btn_assistant, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_image_src(btn_assistant, LV_SYMBOL_VOLUME_MAX, 0);
    lv_obj_set_style_text_font(btn_assistant, lv_theme_get_font_large(btn_assistant), 0);


    // --- Navigate to Alarm Screen ---
    lv_obj_t* btn_alarm = lv_button_create(main_screen);
    lv_obj_add_style(btn_alarm, &style_btn, 0);
    lv_obj_add_style(btn_alarm, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn_alarm, 240, 40);
    lv_obj_set_pos(btn_alarm, 60, 0);
    lv_obj_add_event_cb(btn_alarm, [](lv_event_t* e) {
        // alarm_on = true;
        GUI_CreateAlarmScreen();
        lv_scr_load(alarm_screen);
        // GUI_SwitchToScreen(GUI_CreateAlarmScreen, &alarm_screen);
    }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label_alarm = lv_label_create(btn_alarm);
    lv_label_set_text(label_alarm, "Alarm");
    lv_obj_center(label_alarm);

    GUI_AddSwipeSupport(main_screen, SCREEN_MAIN);
}

// Internet radio screen
void GUI_CreateInternetRadioScreen() {
    if (internet_radio_screen) return;  // Prevent re-creating if it already exists
    Serial.println("Creating GUI_CreateInternetRadioScreen");
    internet_radio_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(internet_radio_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(internet_radio_screen, LV_SCROLLBAR_MODE_OFF);

    // Title at the top
    lv_obj_t* title = lv_label_create(internet_radio_screen);
    lv_label_set_text(title, "Internet Radio");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);  // Optional: bigger font
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);  // Centered top, y = 20px

    lv_obj_t* radio_container = lv_list_create(internet_radio_screen);
    lv_obj_set_size(radio_container, 280, 225);
    lv_obj_center(radio_container);

    File file = SD_MMC.open("/internet_stations.txt", FILE_READ);
    if (!file || file.isDirectory()) {
        Serial.println("Failed to open /internet_stations.txt");
    } else {
        int line_num = 0;

        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.isEmpty()) continue;

            int sepIndex = line.indexOf('|');
            if (sepIndex <= 0 || sepIndex >= line.length() - 1) continue;

            String name = line.substring(0, sepIndex);

            lv_obj_t* btn = lv_list_add_button(radio_container, LV_SYMBOL_AUDIO, name.c_str());

            // Padding items - make buttons bigger
            lv_obj_set_style_pad_top(btn, 20, 0);
            lv_obj_set_style_pad_bottom(btn, 20, 0);

            // Store line number as user_data
            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                uintptr_t index = (uintptr_t)lv_event_get_user_data(e);

                File f2 = SD_MMC.open("/internet_stations.txt", FILE_READ);
                if (!f2) {
                    Serial.println("Failed to reopen station file");
                    return;
                }

                String line;
                int current_line = 0;
                while (f2.available()) {
                    line = f2.readStringUntil('\n');
                    if (current_line == index) break;
                    current_line++;
                }
                f2.close();

                line.trim();
                int sepIndex = line.indexOf('|');
                if (sepIndex > 0 && sepIndex < line.length() - 1) {
                    String url = line.substring(sepIndex + 1);
                    if (audio_ptr) {
                        Serial.printf("Playing (%d): %s\n", (int)index, url.c_str());
                        audio_ptr->connecttohost(url.c_str());
                        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
                    }
                } else {
                    Serial.printf("Invalid station format on line %d\n", (int)index);
                }
            }, LV_EVENT_CLICKED, (void*)line_num);

            line_num++;
        }
        file.close();
    }

    // Back button
    lv_obj_t* back_btn = lv_button_create(internet_radio_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(back_btn);
    lv_label_set_text(label, "< Back");
    lv_obj_center(label);

    GUI_AddSwipeSupport(internet_radio_screen, SCREEN_INTERNET);
}




// SDcard MP3
void GUI_CreateSDCardMP3Screen() {
    if (sdcard_mp3_screen) return;
    Serial.println("Creating GUI_CreateSDCardMP3Screen");

    sdcard_mp3_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(sdcard_mp3_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(sdcard_mp3_screen, LV_SCROLLBAR_MODE_OFF);

    // Title at the top
    lv_obj_t* title = lv_label_create(sdcard_mp3_screen);
    lv_label_set_text(title, "MP3");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);  // Optional: bigger font
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);  // Centered top, y = 20px

    lv_obj_t* list = lv_list_create(sdcard_mp3_screen);
    lv_obj_set_size(list, 280, 200);
    lv_obj_center(list);

    // Load SD card .mp3 files

    // Only load files if the list is currently empty
    if (sdcard_mp3_file_list->empty()) {
        Serial.println("Load files from SD");
        LoadSDCardMP3Files(sdcard_mp3_file_list);
    }

    for (const auto& name : *sdcard_mp3_file_list) {
        lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_PLAY, name.c_str());

        // Padding items - make buttons bigger
        lv_obj_set_style_pad_top(btn, 20, 0);
        lv_obj_set_style_pad_bottom(btn, 20, 0);

        // Allocate a new string that includes "/music/" prefix
        std::string fullPath = std::string("/music/") + name.c_str();
        char* pathCopy = strdup(fullPath.c_str());  // Ensure memory persists for the callback

        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            const char* path = static_cast<const char*>(lv_event_get_user_data(e));
            Serial.printf("Playing file: %s\n", path);
            if (audio_ptr && !audio_ptr->connecttoFS(SD_MMC, path)) {
                Serial.println("Failed to play MP3 file");
            }
            GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
        }, LV_EVENT_CLICKED, pathCopy);
    }


    // Back button
    lv_obj_t* back_btn = lv_button_create(sdcard_mp3_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(back_btn);
    lv_label_set_text(label, "< Back");
    lv_obj_center(label);

    GUI_AddSwipeSupport(sdcard_mp3_screen, SCREEN_SDCARD);
}

// Alarm screen
void GUI_CreateAlarmScreen() {
    if (alarm_screen) return;  // Prevent re-creating if it already exists
    Serial.println("Creating GUI_CreateAlarmScreen");

    alarm_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(alarm_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(alarm_screen, LV_SCROLLBAR_MODE_OFF);

    // Make screen black or dark red to match the alert style
    lv_obj_set_style_bg_color(alarm_screen, lv_palette_main(LV_PALETTE_RED), 0);

    // Create the big red circular STOP button
    lv_obj_t* stop_btn = lv_button_create(alarm_screen);
    lv_obj_set_size(stop_btn, 210, 210); // Large button
    lv_obj_center(stop_btn);
    // lv_obj_set_style_radius(stop_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(stop_btn, lv_palette_darken(LV_PALETTE_RED, 2), 0);
    lv_obj_set_style_bg_opa(stop_btn, LV_OPA_COVER, 0);

    // Label for STOP text
    lv_obj_t* label = lv_label_create(stop_btn);
    lv_label_set_text(label, "STOP");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_40, 0); // Large font
    lv_obj_center(label);

    lv_obj_add_event_cb(stop_btn, [](lv_event_t* e) {
        Serial.println("Alarm stopped. Stopping MIC stream.");
        //MIC_WS_StopStreaming();  // ⛔ Stop stream
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_RELEASED, NULL);

    lv_obj_add_event_cb(stop_btn, [](lv_event_t* e) {
        Serial.println("Starting MIC stream...");
        //MIC_WS_StartStreaming(); // ▶️ Start stream
    }, LV_EVENT_PRESSED, NULL);


    // Back button
    lv_obj_t* back_btn = lv_button_create(alarm_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label_back = lv_label_create(back_btn);
    lv_label_set_text(label_back, "< Back");
    lv_obj_center(label_back);

    GUI_AddSwipeSupport(alarm_screen, SCREEN_ALARM);
}


/*
           AI Assistant screen
*/
void GUI_CreateAssistantScreen() {
    if (assistant_screen) return;  // Prevent re-creating if it already exists
    Serial.println("Creating GUI_CreateAssistantScreen");

    assistant_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(assistant_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(assistant_screen, LV_SCROLLBAR_MODE_OFF);

    // Make screen black or dark red to match the alert style
    lv_obj_set_style_bg_color(assistant_screen, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);

    // Create the big red circular STOP button
    lv_obj_t* speak_btn = lv_button_create(assistant_screen);
    lv_obj_set_size(speak_btn, 210, 210); // Large button
    lv_obj_center(speak_btn);
    // lv_obj_set_style_radius(speak_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(speak_btn, lv_palette_darken(LV_PALETTE_LIGHT_BLUE, 2), 0);
    lv_obj_set_style_bg_opa(speak_btn, LV_OPA_COVER, 0);

    // Label for STOP text
    lv_obj_t* label = lv_label_create(speak_btn);
    lv_label_set_text(label, "SPEAK");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_40, 0); // Large font
    lv_obj_center(label);

    // Start recording on press
    lv_obj_add_event_cb(speak_btn, [](lv_event_t* e) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        filename = generateRotatingFileName();
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_RED), 0);
        MIC_StartRecording(filename.c_str(), 16000 /*bitRate*/, 1 /*chanels*/, 16/*bits*/, false /*strean via websocket*/);
    }, LV_EVENT_PRESSED, NULL);

    // Stop recording on release
    lv_obj_add_event_cb(speak_btn, [](lv_event_t* e) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        Serial.println("Stop and play recording...");
        MIC_StopRecording();
        lv_obj_set_style_bg_color(btn, lv_palette_darken(LV_PALETTE_LIGHT_BLUE, 2), 0);
        delay(10);
        char* fname = (char*)pvPortMalloc(filename.length() + 1);
        strcpy(fname, filename.c_str());
        xTaskCreatePinnedToCore(
            UploadFileTask,      // Task function
            "UploadFileTask",    // Name
            8192,                // Stack size
            fname,               // Parameter
            1,                   // Priority
            NULL,                // Handle (optional)
            1                    // Core
        );

        // Plat it back from SDCARD
        /*
        if (!SD_MMC.exists(filename.c_str())) {
            Serial.println("[ERR] File not saved!");
            return;
            }

        if (audio_ptr) {
            Serial.printf("Play %s\n", filename.c_str());
            audio_ptr->connecttoFS(SD_MMC, filename.c_str());
        }
        */
    }, LV_EVENT_RELEASED, NULL);


    // --- Stream Button (Top Center) ---
    lv_obj_t* stream_btn = lv_button_create(assistant_screen);
    lv_obj_set_size(stream_btn, 120, 50);
    lv_obj_align(stream_btn, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(stream_btn, lv_palette_main(LV_PALETTE_GREEN), 0);

    lv_obj_t* stream_label = lv_label_create(stream_btn);
    lv_label_set_text(stream_label, "STREAM");
    lv_obj_center(stream_label);

    // Stream start on press
    lv_obj_add_event_cb(stream_btn, [](lv_event_t* e) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        filename = generateRotatingFileName();
        Serial.printf("[STREAM] Start streaming: %s\n", filename.c_str());
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_RED), 0);
        MIC_StartRecording(filename.c_str(), 16000 /*bitRate*/, 1 /*channels*/, 16 /*bits*/, true /*stream via websocket*/);
    }, LV_EVENT_PRESSED, NULL);

    // Stream stop on release
    lv_obj_add_event_cb(stream_btn, [](lv_event_t* e) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        Serial.println("[STREAM] Stop streaming");
        MIC_StopRecording();
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    }, LV_EVENT_RELEASED, NULL);


    // --- Back button (Bottom) ----
    lv_obj_t* back_btn = lv_button_create(assistant_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label_back = lv_label_create(back_btn);
    lv_label_set_text(label_back, "< Back");
    lv_obj_center(label_back);

    // GUI_AddSwipeSupport(alarm_screen, SCREEN_ALARM);
}

// Clock screen
void GUI_CreateClockScreen() {
    if (clock_screen) return;  // Prevent re-creating if it already exists
    Serial.println("Creating GUI_CreateClockScreen");

    clock_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(clock_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(clock_screen, LV_SCROLLBAR_MODE_OFF);

    // Make screen black or dark red to match the alert style
    lv_obj_set_style_bg_color(clock_screen, lv_palette_main(LV_PALETTE_RED), 0);


    // Clock Label (HH:MM:SS)
    bigclock_label = lv_label_create(clock_screen);
    lv_label_set_text(bigclock_label, "--:--:--");
    lv_obj_set_style_text_font(bigclock_label, &lv_font_montserrat_48, 0);
    lv_obj_align(bigclock_label, LV_ALIGN_CENTER, 0, -60);

    // Date Label (YYYY-MM-DD)
    date_label = lv_label_create(clock_screen);
    lv_label_set_text(date_label, "----/--/--");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_24, 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 20);

    // Back button
    lv_obj_t* back_btn = lv_button_create(clock_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label_back = lv_label_create(back_btn);
    lv_label_set_text(label_back, "< Back");
    lv_obj_center(label_back);

    //GUI_AddSwipeSupport(clock_screen, SCREEN_ALARM);
}

ScreenSwitchData screen_table[SCREEN_COUNT] = {
    {GUI_CreateMainScreen, &main_screen},
    {GUI_CreateInternetRadioScreen, &internet_radio_screen},
    {GUI_CreateSDCardMP3Screen, &sdcard_mp3_screen},
    {GUI_CreateAlarmScreen, &alarm_screen}
};

void GUI_AddSwipeSupport(lv_obj_t* screen, ScreenIndex current_screen) {

    /*
    static ScreenIndex indices[SCREEN_COUNT];
    indices[current_screen] = current_screen;

    static lv_obj_t* last_touched_obj = nullptr;  // Keeps track of what was pressed

    lv_obj_add_event_cb(screen, [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);

        if (code == LV_EVENT_PRESSED) {
            // Save the original object that was pressed
            last_touched_obj = (lv_obj_t*)lv_event_get_target(e);
            return;
        }

        if (code != LV_EVENT_GESTURE) return;

        // Check if the gesture started on a slider or other excluded widget
        if (last_touched_obj &&
            (lv_obj_check_type(last_touched_obj, &lv_slider_class) ||
             lv_obj_check_type(last_touched_obj, &lv_button_class))) {
            // Optional debug
            // Serial.printf("[Swipe Blocked] Started on %s\n", lv_obj_get_class_name(last_touched_obj));
            return;
        }

        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());
        ScreenIndex* current = static_cast<ScreenIndex*>(lv_event_get_user_data(e));
        if (!current) return;

        if (dir == LV_DIR_LEFT) {
            int next = (*current + 1) % SCREEN_COUNT;
            GUI_SwitchToScreen(screen_table[next].creator, screen_table[next].screen_ptr);
        } else if (dir == LV_DIR_RIGHT) {
            int prev = (*current - 1 + SCREEN_COUNT) % SCREEN_COUNT;
            GUI_SwitchToScreen(screen_table[prev].creator, screen_table[prev].screen_ptr);
        }
    }, LV_EVENT_ALL, &indices[current_screen]);  // Use LV_EVENT_ALL so we catch PRESSED and GESTURE

    */
}

void GUI_Init(Audio& audio)
{
    audio_ptr = &audio;
    // Load the main screen
    GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);

    // Load files list from SDcart
    LoadSDCardMP3Files(sdcard_mp3_file_list);
}

void GUI_UpdateClock(const struct tm& rtcTime)
{
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &rtcTime);

    if (clock_label) {
        lv_label_set_text(clock_label, time_str);
    }

    if (bigclock_label) {
        lv_label_set_text(bigclock_label, time_str);
    }

    if (date_label) {
        char date_str[16];
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", &rtcTime);
        lv_label_set_text(date_label, date_str);
    }
}

void GUI_UpdateMessage(const char* msg)
{
    if (message_label) {
        lv_obj_set_width(message_label, 340);
        // lv_label_set_long_mode(message_label, LV_LABEL_LONG_MODE_WRAP); 
        lv_label_set_long_mode(message_label, LV_LABEL_LONG_SCROLL_CIRCULAR); // circular scroll
        lv_label_set_text(message_label, msg);
        lv_obj_align(message_label, LV_ALIGN_CENTER, 0, -40);
    }
    else
    {
        Serial.print("GUI_UpdateMessage, no message_label");
    }
}

void GUI_EnqueueMessage(const char* msg) {
    if (!msgQueue) return;
    char buffer[64];
    strncpy(buffer, msg, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    xQueueSend(msgQueue, buffer, 0);  // 0 = no wait

    Serial.print(">>>>>>>> ");
    Serial.println(msg);
}

void GUI_Tick() {
    if (!msgQueue) return;

    char msg[64];
    if (xQueueReceive(msgQueue, msg, 0) == pdTRUE) {
        GUI_UpdateMessage(msg);  // safe to call from GUI thread
    }
}

void GUI_ClearMessage()
{
    if (message_label) {
        lv_label_set_text(message_label, "");
    }
}




