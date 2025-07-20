#include "GUI.h"
#include "AlarmScreen.h"
#include <SD_MMC.h>
#include <ArduinoJson.h>


#define ALARM_FILE "/alarms.json" // Path to the alarm file on SD card

std::vector<Alarm> alarm_list;


// Default alarm definition
static const Alarm default_alarm = {
    "07:00",                       // time
    {0, 1, 1, 1, 1, 1, 0},         // weekdays (Sun-Sat, Mon-Fri enabled)
    "mp3",                         // action_type
    "/music/Fear of the dark.mp3", // action_path
    true                            // enabled
};

void LoadAlarms() {
    Serial.println("Loading alarms from SD card...");
    alarm_list.clear();
    File file = SD_MMC.open(ALARM_FILE, "r");
    if (!file) {
        Serial.println("Failed to open alarm file.");
        return;
    }

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, file);
    if (err) {
        Serial.println("Failed to parse alarm file");
        return;
    }

    int alarm_index = 0;

    for (JsonObject obj : doc.as<JsonArray>()) {
        Alarm a;
        a.time = obj["time"] | "00:00";
        JsonArray days = obj["weekdays"].as<JsonArray>();
        for (int i = 0; i < 7 && i < days.size(); i++)
            a.weekdays[i] = days[i];
        a.action_type = obj["action"]["type"] | "sound";
        a.action_path = obj["action"]["path"] | "";
        a.enabled = obj["enabled"] | true;

        alarm_list.push_back(a);

        // Print details
        Serial.printf("Alarm %d:\n", alarm_index++);
        Serial.printf("  Time     : %s\n", a.time.c_str());
        Serial.printf("  Enabled  : %s\n", a.enabled ? "true" : "false");
        Serial.print("  Weekdays : ");
        const char* days_abbr = "SMTWTFS";
        for (int i = 0; i < 7; i++) {
            if (a.weekdays[i]) Serial.printf("%c ", days_abbr[i]);
        }
        Serial.println();
        Serial.printf("  Action   : type = %s, path = %s\n", a.action_type.c_str(), a.action_path.c_str());
        Serial.println("--------------------------------------------------");
    }

    file.close();
}

void SaveAlarms() {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();

    for (const Alarm& a : alarm_list) {
        JsonObject obj = arr.createNestedObject();
        obj["time"] = a.time;
        JsonArray days = obj.createNestedArray("weekdays");
        for (int i = 0; i < 7; i++) days.add(a.weekdays[i]);
        JsonObject act = obj.createNestedObject("action");
        act["type"] = a.action_type;
        act["path"] = a.action_path;
        obj["enabled"] = a.enabled;
    }

    File file = SD_MMC.open(ALARM_FILE, "w");
    if (!file) {
        Serial.println("Failed to open alarm file for writing");
        return;
    }
    serializeJson(doc, file);
    file.close();
}

/*
    List Screen
*/
void GUI_CreateAlarmListScreen() {
    Serial.println("Creating GUI_CreateAlarmListScreen");
    if (alarm_screen) lv_obj_del(alarm_screen);
    alarm_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(alarm_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(alarm_screen, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* title = lv_label_create(alarm_screen);
    lv_label_set_text(title, "Alarms");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t* list = lv_list_create(alarm_screen);
    lv_obj_set_size(list, 300, 200);
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);

    LoadAlarms();

    for (size_t i = 0; i < alarm_list.size(); ++i) {
        String label = alarm_list[i].time + (alarm_list[i].enabled ? "" : " (Off)");
        lv_obj_t* btn = lv_list_add_button(list, LV_SYMBOL_BELL, label.c_str());
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            int alarm_index = (intptr_t)lv_event_get_user_data(e);
            GUI_CreateAlarmEditScreen(alarm_index);
        }, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    // Add event to handle empty list
    lv_obj_t* add_btn = lv_button_create(alarm_screen);
    lv_obj_set_size(add_btn, 260, 50);
    lv_obj_set_pos(add_btn, 55, 260);
    lv_obj_add_event_cb(add_btn, [](lv_event_t* e) {
        GUI_CreateAlarmEditScreen(-1);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl = lv_label_create(add_btn);
    lv_label_set_text(lbl, "+ Add Alarm");
    lv_obj_center(lbl);

    // Back button
    lv_obj_t* back_btn = lv_button_create(alarm_screen);
    lv_obj_set_size(back_btn, 260, 50);
    lv_obj_set_pos(back_btn, 55, 315);
    lv_obj_add_event_cb(back_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(back_btn);
    lv_label_set_text(label, "< Back");
    lv_obj_center(label);
}

static lv_obj_t* hour_roller = nullptr;
static lv_obj_t* minute_roller = nullptr;
static lv_obj_t* weekday_checkboxes[7] = { nullptr }; 

/*
    Edit Screen
*/
void GUI_CreateAlarmEditScreen(int alarm_index) {
    Serial.printf("Editing alarm %d\n", alarm_index);
    if (alarm_screen_edit) lv_obj_del(alarm_screen_edit);
    alarm_screen_edit = lv_obj_create(NULL);
    lv_obj_clear_flag(alarm_screen_edit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(alarm_screen_edit, LV_SCROLLBAR_MODE_OFF);

    Alarm editing = (alarm_index >= 0 && alarm_index < alarm_list.size()) ? alarm_list[alarm_index] : default_alarm;

    lv_obj_t* title = lv_label_create(alarm_screen_edit);
    lv_label_set_text_fmt(title, "%s Alarm", alarm_index >= 0 ? "Edit" : "New");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // --- Time Picker Label ---
    lv_obj_t* time_title = lv_label_create(alarm_screen_edit);
    lv_label_set_text(time_title, "Time:");
    lv_obj_align(time_title, LV_ALIGN_TOP_MID, 0, 40);

    // --- Hour Roller ---
    hour_roller = lv_roller_create(alarm_screen_edit);
    lv_roller_set_options(hour_roller,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23",
        LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(hour_roller, 3);
    lv_obj_set_width(hour_roller, 60);
    lv_obj_align(hour_roller, LV_ALIGN_TOP_LEFT, 120, 70);

    // --- Minute Roller ---
    minute_roller = lv_roller_create(alarm_screen_edit);
    lv_roller_set_options(minute_roller,
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59",
        LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(minute_roller, 3);
    lv_obj_set_width(minute_roller, 60);
    lv_obj_align(minute_roller, LV_ALIGN_TOP_LEFT, 190, 70);

    // --- Set current value from loaded alarm time ---
    int current_hour = editing.time.substring(0, 2).toInt();
    int current_minute = editing.time.substring(3, 5).toInt();
    lv_roller_set_selected(hour_roller, current_hour, LV_ANIM_OFF);
    lv_roller_set_selected(minute_roller, current_minute, LV_ANIM_OFF);

    // --- Weekday Checkboxes ---
    const char* days_abbr = "SMTWTFS";

    lv_obj_t* day_row = lv_obj_create(alarm_screen_edit);
    lv_obj_set_size(day_row, 320, 70);
    lv_obj_align(day_row, LV_ALIGN_TOP_LEFT, 10, 160);
    lv_obj_set_flex_flow(day_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_row(day_row, 2, 0);
    lv_obj_set_style_pad_column(day_row, 5, 0);
    lv_obj_set_scrollbar_mode(day_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(day_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_opa(day_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(day_row, 0, 0);
    lv_obj_set_style_outline_width(day_row, 0, 0);

    for (int i = 0; i < 7; ++i) {
        lv_obj_t* col = lv_obj_create(day_row);
        lv_obj_set_size(col, 42, 60);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(col, 0, 0);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_style_border_width(col, 0, 0);
        lv_obj_set_style_outline_width(col, 0, 0);
        lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);

        // Day label
        char day[2] = { days_abbr[i], '\0' };
        lv_obj_t* lbl = lv_label_create(col);
        lv_label_set_text(lbl, day);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        // Checkbox
        lv_obj_t* cb = lv_checkbox_create(col);
        lv_checkbox_set_text(cb, "");  // hide default label
        lv_obj_set_size(cb, 46, 46);   // Larger touch area
        lv_obj_set_style_pad_all(cb, 0, 0);
        lv_obj_set_style_radius(cb, 4, 0);  // Optional rounded corners
        if (editing.weekdays[i]) lv_obj_add_state(cb, LV_STATE_CHECKED);
        weekday_checkboxes[i] = cb;

        // Use shared editing pointer
        Alarm* editing_ptr = new Alarm(editing);

        lv_obj_add_event_cb(cb, [](lv_event_t* e) {
            int i = (intptr_t)lv_event_get_user_data(e);
            lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
            Alarm* a = (Alarm*)lv_obj_get_user_data(obj);
            if (a) a->weekdays[i] = lv_obj_has_state(obj, LV_STATE_CHECKED);
        }, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);

        lv_obj_set_user_data(cb, editing_ptr);
    }

    // Save button
    lv_obj_t* save_btn = lv_button_create(alarm_screen_edit);
    lv_obj_set_size(save_btn, 120, 50);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_LEFT, 50, -60);

    // Create a copy of the alarm being edited
    Alarm* editing_ptr = new Alarm(editing);

    // Set event callback for Save
    lv_obj_add_event_cb(save_btn, [](lv_event_t* e) {
        Alarm* a = (Alarm*)lv_event_get_user_data(e);
        int alarm_index = (intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));

        // Extract hour/minute from rollers
        char hour_buf[3], min_buf[3];
        lv_roller_get_selected_str(hour_roller, hour_buf, sizeof(hour_buf));
        lv_roller_get_selected_str(minute_roller, min_buf, sizeof(min_buf));
        a->time = String(hour_buf) + ":" + String(min_buf);

        for (int i = 0; i < 7; ++i) {
            if (weekday_checkboxes[i]) {
                a->weekdays[i] = lv_obj_has_state(weekday_checkboxes[i], LV_STATE_CHECKED);
            }
        }

        // Update alarm list
        if (alarm_index >= 0 && alarm_index < alarm_list.size())
            alarm_list[alarm_index] = *a;
        else
            alarm_list.push_back(*a);

        SaveAlarms();
        GUI_SwitchToScreen(GUI_CreateAlarmListScreen, &alarm_screen, true);

        delete a;
    }, LV_EVENT_CLICKED, editing_ptr);

    // Store index as user data for event callback
    lv_obj_set_user_data(save_btn, (void*)(intptr_t)alarm_index);

    lv_obj_t* lbl = lv_label_create(save_btn);
    lv_label_set_text(lbl, "Save");
    lv_obj_center(lbl);

    // Cancel button
    lv_obj_t* cancel_btn = lv_button_create(alarm_screen_edit);
    lv_obj_set_size(cancel_btn, 120, 50);
    lv_obj_align(cancel_btn, LV_ALIGN_BOTTOM_RIGHT, -50, -60);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t* e) {
        GUI_SwitchToScreen(GUI_CreateAlarmListScreen, &alarm_screen, true);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl2 = lv_label_create(cancel_btn);
    lv_label_set_text(lbl2, "Cancel");
    lv_obj_center(lbl2);

    // Delete Button (only show for existing alarms)
    if (alarm_index >= 0 && alarm_index < alarm_list.size()) {
        lv_obj_t* delete_btn = lv_button_create(alarm_screen_edit);
        lv_obj_set_size(delete_btn, 180, 50);
        lv_obj_align(delete_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0xff3333), 0); // Red color
        lv_obj_set_style_text_color(delete_btn, lv_color_hex(0xffffff), 0);

        // Pass index as user data
        lv_obj_set_user_data(delete_btn, (void*)(intptr_t)alarm_index);

        lv_obj_add_event_cb(delete_btn, [](lv_event_t* e) {
            int idx = (intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
            if (idx >= 0 && idx < alarm_list.size()) {
                alarm_list.erase(alarm_list.begin() + idx);
                SaveAlarms();
            }
            GUI_SwitchToScreen(GUI_CreateAlarmListScreen, &alarm_screen, true);
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* delete_lbl = lv_label_create(delete_btn);
        lv_label_set_text(delete_lbl, "Delete");
        lv_obj_center(delete_lbl);
    }

    // Load the screen
    GUI_SwitchToScreenAfter(&alarm_screen_edit);
}

/*
    Active Screen
*/
void GUI_CreateAlarmActiveScreen() {
    if (alarm_screen) lv_obj_del(alarm_screen);
    alarm_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(alarm_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(alarm_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(alarm_screen, lv_palette_main(LV_PALETTE_RED), 0);

    lv_obj_t* stop_btn = lv_button_create(alarm_screen);
    lv_obj_set_size(stop_btn, 210, 210);
    lv_obj_center(stop_btn);
    lv_obj_set_style_bg_color(stop_btn, lv_palette_darken(LV_PALETTE_RED, 2), 0);
    lv_obj_set_style_bg_opa(stop_btn, LV_OPA_COVER, 0);
    lv_obj_t* label = lv_label_create(stop_btn);
    lv_label_set_text(label, "STOP");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_40, 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(stop_btn, [](lv_event_t* e) {
        Serial.println("Alarm stopped.");
        if (audio_ptr) {
            audio_ptr->stopSong(); // Stop audio playback
        }
        GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen);
    }, LV_EVENT_CLICKED, NULL);
}