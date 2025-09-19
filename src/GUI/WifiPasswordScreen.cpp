#include "GUI.h"
#include "WifiPasswordScreen.h"


lv_obj_t* pass_label = nullptr;
lv_obj_t* status_label = nullptr;
lv_obj_t* key_row = nullptr;
lv_obj_t* mode_label = nullptr;
lv_timer_t* mask_timer = nullptr;

String current_pass = "";

// Modes
enum KeySetMode {
    KEYS_LOWERCASE,
    KEYS_UPPERCASE,
    KEYS_NUMERIC,
    KEYS_SYMBOLS
};
KeySetMode key_mode = KEYS_LOWERCASE;

const char* keys_alpha[] = {
    "a","b","c","d","e","f","g","h","i","j",
    "k","l","m","n","o","p","q","r","s","t",
    "u","v","w","x","y","z",
    nullptr
};

const char* keys_numbers[] = {
    "0","1","2","3","4","5","6","7","8","9", nullptr
};

const char* keys_symbols[] = {
    "!","@","#","$","%","^","&","*","(",")",
    "-","_","+","=","[","]","{","}",";",":",
    "'","\"","|","\\",",",".","<",">","/","?",
    "~","`","€","£","¥","§", nullptr
};

void update_pass_label() {
    String masked = "";
    for (int i = 0; i < current_pass.length(); i++) masked += "*";
    lv_label_set_text(pass_label, masked.c_str());
}

void store_wifi_password_to_eeprom(const String& pass) {
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(EEPROM_WIFI_PASS_ADDR + i, i < pass.length() ? pass[i] : 0);
    }
    EEPROM.commit();
}

void read_wifi_password_from_eeprom(String& out_buf) {
    char buf[EEPROM_SIZE] = {0};
    for (int i = 0; i < EEPROM_SIZE; i++) {
        buf[i] = EEPROM.read(EEPROM_WIFI_PASS_ADDR + i);
        if (buf[i] == '\0') break;
    }
    out_buf = String(buf);
}

void ConnectToWiFiFromEEPROM() {
    lv_label_set_text(status_label, "Connecting...");

    // Read SSID and password from EEPROM
    String current_pass;
    read_wifi_password_from_eeprom(current_pass);
    if (current_pass.isEmpty()) {
        Serial.println("No password stored in EEPROM.");
        lv_label_set_text(status_label, "No password stored.");
        return;
    }
    String current_ssid;
    read_wifi_ssid_from_eeprom(current_ssid);
    if (current_ssid.isEmpty()) {
        Serial.println("No SSID stored in EEPROM.");
        lv_label_set_text(status_label, "No SSID stored.");
        return;
    }

    WiFi.begin(current_ssid.c_str(), current_pass.c_str());

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 30) {
        delay(200);
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        lv_label_set_text(status_label, "Connected!");
    } else {
        lv_label_set_text(status_label, "Failed to connect.");
    }

    // Return to main screen after short delay
    lv_timer_create([](lv_timer_t* t) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
        lv_timer_del(t);
    }, 1500, NULL);
}


void rebuild_keyboard_keys() {
    lv_obj_clean(key_row);

    const char** key_set = nullptr;
    switch (key_mode) {
        case KEYS_LOWERCASE: key_set = keys_alpha; break;
        case KEYS_UPPERCASE: key_set = keys_alpha; break;
        case KEYS_NUMERIC:   key_set = keys_numbers; break;
        case KEYS_SYMBOLS:   key_set = keys_symbols; break;
    }

    for (int i = 0; key_set[i] != nullptr; i++) {
        lv_obj_t* key_btn = lv_button_create(key_row);
        lv_obj_set_size(key_btn, 50, 50);

        char display_char[2] = {0};
        const char* txt = key_set[i];

        if (key_mode == KEYS_UPPERCASE && strlen(txt) == 1 && isalpha(txt[0])) {
            display_char[0] = toupper(txt[0]);
        } else {
            display_char[0] = txt[0];
        }

        lv_obj_t* label = lv_label_create(key_btn);
        lv_label_set_text(label, display_char);
        lv_obj_center(label);

        lv_obj_add_event_cb(key_btn, [](lv_event_t* e) {
            lv_obj_t* label = lv_obj_get_child((const lv_obj_t*)lv_event_get_target(e), 0);
            const char* txt = lv_label_get_text(label);
            current_pass += txt;

            // Cancel previous mask timer if active
            if (mask_timer) {
                lv_timer_del(mask_timer);
                mask_timer = nullptr;
            }

            // Show password with last character visible
            String peek = "";
            for (int i = 0; i < current_pass.length() - 1; i++) peek += "*";
            peek += txt;  // show last typed letter
            lv_label_set_text(pass_label, peek.c_str());

            // Set a timer to mask it after 800ms
            mask_timer = lv_timer_create([](lv_timer_t* t) {
                update_pass_label();      // mask with all '*'
                lv_timer_del(mask_timer); // delete self
                mask_timer = nullptr;
            }, 800, NULL);

        }, LV_EVENT_CLICKED, NULL);
    }
}

void safe_rebuild_keyboard_keys() {
    lv_async_call([](void* user_data) {
        rebuild_keyboard_keys();
    }, NULL);
}


void GUI_CreateWifiPasswordScreen() {
    if (wifi_password_screen) return;
    Serial.println("Creating GUI_CreateWifiPasswordScreen");

    EEPROM.begin(EEPROM_SIZE);
    wifi_password_screen = lv_obj_create(NULL);
    lv_obj_t* screen = wifi_password_screen;

    // Title
    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "Enter WiFi password");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 50);

    // Password Label
    pass_label = lv_label_create(screen);
    lv_label_set_text(pass_label, "");
    lv_obj_set_style_text_font(pass_label, &lv_font_montserrat_28, 0);
    lv_obj_align(pass_label, LV_ALIGN_TOP_MID, 0, 90);

    // Scrollable keyboard row
    key_row = lv_obj_create(screen);
    lv_obj_set_size(key_row, 390, 100);
    lv_obj_set_scroll_dir(key_row, LV_DIR_HOR); // horizontal scroll only
    lv_obj_set_scroll_snap_x(key_row, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(key_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(key_row, LV_FLEX_FLOW_ROW);
    lv_obj_align(key_row, LV_ALIGN_CENTER, 0, 0);

    safe_rebuild_keyboard_keys();  // initial load

    // --- Mode toggle button (abc → ABC → 123 → !@#) ---
    lv_obj_t* mode_btn = lv_button_create(screen);
    lv_obj_set_size(mode_btn, 80, 50);
    lv_obj_align(mode_btn, LV_ALIGN_BOTTOM_LEFT, 50, -60);

    mode_label = lv_label_create(mode_btn);
    lv_label_set_text(mode_label, "abc");
    lv_obj_center(mode_label);

    lv_obj_add_event_cb(mode_btn, [](lv_event_t* e) {
        key_mode = static_cast<KeySetMode>((key_mode + 1) % 4);

        switch (key_mode) {
            case KEYS_LOWERCASE: lv_label_set_text(mode_label, "abc"); break;
            case KEYS_UPPERCASE: lv_label_set_text(mode_label, "ABC"); break;
            case KEYS_NUMERIC:   lv_label_set_text(mode_label, "123"); break;
            case KEYS_SYMBOLS:   lv_label_set_text(mode_label, "!@#"); break;
        }

        safe_rebuild_keyboard_keys();
    }, LV_EVENT_CLICKED, NULL);

    // --- DEL button ---
    lv_obj_t* del_btn = lv_button_create(screen);
    lv_obj_set_size(del_btn, 70, 50);
    lv_obj_align(del_btn, LV_ALIGN_BOTTOM_LEFT, 140, -60);
    lv_obj_t* del_label = lv_label_create(del_btn);
    lv_label_set_text(del_label, "DEL");
    lv_obj_center(del_label);
    lv_obj_add_event_cb(del_btn, [](lv_event_t* e) {
        if (!current_pass.isEmpty()) current_pass.remove(current_pass.length() - 1);
        update_pass_label();
    }, LV_EVENT_CLICKED, NULL);

    // --- ENTER button ---
    lv_obj_t* enter_btn = lv_button_create(screen);
    lv_obj_set_size(enter_btn, 80, 50);
    lv_obj_align(enter_btn, LV_ALIGN_BOTTOM_LEFT, 220, -60);
    lv_obj_t* enter_label = lv_label_create(enter_btn);
    lv_label_set_text(enter_label, "ENTER");
    lv_obj_center(enter_label);
    lv_obj_add_event_cb(enter_btn, [](lv_event_t* e) {
        store_wifi_password_to_eeprom(current_pass);
        ConnectToWiFiFromEEPROM();
    }, LV_EVENT_CLICKED, NULL);

    // --- Status label ---
    status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "");
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, 10);

    // --- Back Button ---
    lv_obj_t* back_btn = lv_button_create(screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label_back = lv_label_create(back_btn);
    lv_label_set_text(label_back, "< Back");
    lv_obj_center(label_back);
    Serial.println("GUI_CreateWifiPasswordScreen created");
}