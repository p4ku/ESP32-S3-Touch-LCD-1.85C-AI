#include "lv_conf.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include "lvgl.h"
#include "LVGL_ST77916.h"
#include "RTC_PCF85063.h"
#include "Audio.h"
#include "PCM5101.h"
#include "GUI/GUI.h"
#include "MIC_MSM.h"
#include "Touch_CST816.h"
#include "HttpServer.h"
#include "SD_Card.h"
#include "AIAssistant.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_partition.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include "config.h"

// NTP configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;      // Adjust GMT offset (3600 for CET)
const int daylightOffset_sec = 3600;  // DST offset (adjust as needed)

TaskHandle_t guiTaskHandle = NULL;
static bool backlightAlreadyOff = false;
static String last_triggered_time = "";

struct tm rtcTime;  
portMUX_TYPE rtcMux = portMUX_INITIALIZER_UNLOCKED;


void PrintAllPartitions() {
  const esp_partition_t* part = NULL;
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);

  Serial.println("App Partitions:");
  while (it != NULL) {
    part = esp_partition_get(it);
    Serial.printf("  • Label: %-16s Addr: 0x%08x  Size: %-8d  Subtype: %02x\n",
                  part->label, part->address, part->size, part->subtype);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);

  it = esp_partition_find(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, NULL);
  Serial.println("Data Partitions:");
  while (it != NULL) {
    part = esp_partition_get(it);
    Serial.printf("  • Label: %-16s Addr: 0x%08x  Size: %-8d  Subtype: %02x\n",
                  part->label, part->address, part->size, part->subtype);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
}

// Check if the alarm is due based on current time and alarm settings
bool IsAlarmDue(const struct tm& rtcTime, const Alarm& alarm) {
    if (!alarm.enabled) return false;

    int weekday = rtcTime.tm_wday;  // 0 = Sunday
    if (!alarm.weekdays[weekday]) return false;

    char buf[6];
    strftime(buf, sizeof(buf), "%H:%M", &rtcTime);
    Serial.printf("Current time: %s\n", buf);

    char time_now[6];
    snprintf(time_now, sizeof(time_now), "%02d:%02d", rtcTime.tm_hour, rtcTime.tm_min);

    Serial.printf("Checking alarm: %s vs now %s\n", alarm.time.c_str(), time_now);


    // Serial.printf("Checking alarm: %s at %s\n", alarm.time.c_str(), buf);

    return alarm.time == String(time_now);
}

// Set the RTC time safely across tasks
void GetSafeRTC(struct tm* out) {
    portENTER_CRITICAL(&rtcMux);
    *out = rtcTime;
    portEXIT_CRITICAL(&rtcMux);
}


// Check all alarms and trigger if any are due
// This function is called periodically
void CheckAlarms() {
    struct tm now;
    GetSafeRTC(&now);
    for (const Alarm& alarm : alarm_list) {
        if (IsAlarmDue(now, alarm)) {
            String current_time = alarm.time;

            if (last_triggered_time != current_time) {
                Serial.printf("Triggering alarm: %s\n", current_time.c_str());
                last_triggered_time = current_time;

                if (audio_ptr && alarm.action_type == "mp3") {
                    // Action: Play MP3
                    if (!audio_ptr->connecttoFS(SD_MMC, alarm.action_path.c_str())) {
                        Serial.println("Failed to play alarm audio");
                    }
                } else if (audio_ptr && alarm.action_type == "sound") {
                    // Action: Play sound
                    if (!audio_ptr->connecttoFS(SD_MMC, alarm.action_path.c_str())) {
                        Serial.println("Failed to play alarm sound");
                    }
                } else if (audio_ptr && alarm.action_type == "radio") {
                    // Action: Play radio stream
                    if (!audio_ptr->connecttohost(alarm.action_path.c_str())) {
                        Serial.println("Failed to connect to radio stream");
                    }
                }

                // Optional: Play default tone
                // audio_ptr->tone(1000, 3000);  // 3 sec

                // Turn on LED
                LCD_SetBacklight(true);

                // Show screen
                GUI_SwitchToScreen(GUI_CreateAlarmActiveScreen, &alarm_screen);
                break;  // Only trigger one per check
            }
        }
    }
}

//**************** Thread GUI + RTC *****************************************
void GUITask(void *parameter) {
  static unsigned long lastGuiCheck = 0;
  char displayedTime[9] = "";
  Serial.println("Starting GUITask ...");

  while (true) {
    static unsigned long last = 0;
    if (millis() - last > 200) { // Update Clock every 200ms
        last = millis();

        if (RTC_GetTime(&rtcTime)) {
          GUI_UpdateClock(rtcTime);
        }

        GUI_Tick();  // Call this every ~200ms - read from GUI queue
    }

    // Check if backlight should be turned off
    if (backlight_on && (millis() - last_touch_time > INACTIVITY_TIMEOUT_MS)) {
      LCD_SetBacklight(false);
      MIC_SR_Start();
    }

    if (millis() - lastGuiCheck >= 60 * 1000) {  // 60 seconds
        lastGuiCheck = millis();

        if (guiTaskHandle) {
            UBaseType_t watermark = uxTaskGetStackHighWaterMark(guiTaskHandle);
            Serial.printf("[GUI Task] Min free stack: %u words (%u bytes)\n",
                          watermark, watermark * sizeof(StackType_t));
        }
    }

    lv_timer_handler(); // Call LVGL timer handler
    HttpServer_Loop();  // Process HTTP requests
    vTaskDelay(pdMS_TO_TICKS(50));  // update ~33fps
  }
}

void sr_setup()
{
  srmodel_list_t *models = esp_srmodel_init("model");
  if (models == NULL) {
    Serial.println("Failed to initialize ESP-SR models");
    return;
  }
  Serial.printf("ESP-SR models initialized with %d models\n", models->num);
  for (int i = 0; i < models->num; i++) {
    Serial.printf("Model %d: %s, Info: %s\n", i, models->model_name[i], models->model_info[i]);
  }

  // Initialize the WN interface
  Serial.println("Initializing WN interface...");
  MIC_SR_Start();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(11, 10, 400000);
  Serial.println("Starting setup...");

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize LCD
  Lvgl_Init();

  // Initialize Touch
  Touch_Init();
  Serial.println("Touch Initialized!");

  // Connect to WiFi
  gfx->setCursor(75, 40+40);
  gfx->println("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  gfx->setCursor(75, 70+40);
  gfx->print("WiFi Connected");
  gfx->setCursor(75, 100+40);
  gfx->println(ip.toString());

  // Fetch NTP time once
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.println("NTP Time fetched!");
    RTC_SetTime(&timeinfo);
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    gfx->setCursor(75, 130+40);
    gfx->print("NTP Synced: ");
    gfx->println(timeStr);
  } else {
    Serial.println("NTP Failed, using RTC");
    gfx->setCursor(75, 130+40);
    gfx->println("NTP Failed!");
  }

  // Initialize sd card
  Serial.println("Setup SD card");
  SD_Init();

  // Load alarms from SD card
  LoadAlarms();

  // Write srmodels.bin to partition if needed
  write_srmodels_bin_to_partition_if_needed();

  Serial.printf("PSRAM Size: %d bytes\n", ESP.getPsramSize());
  if (ESP.getPsramSize() < 2 * 1024 * 1024) {
    Serial.println("PSRAM not detected! Audio will crash.");
    gfx->setCursor(75, 160+40);
    gfx->println("NO PSRAM!");
    while (true) delay(100);
  }

  // Initialize Audio
  Serial.println("Setup Audio PCM5101");
  Audio_Init();

  // Initialize 
  Serial.println("Setup Microphone");
  MIC_Init();
  gfx->setCursor(75, 160+40);
  gfx->println("Audio started");

  Serial.println("Setup GUI");
  GUI_Init(audio);
  // GUI_MessageQueueInit();

  Serial.println("Setup Http Server");
  HttpServer_Begin(audio);
  gfx->setCursor(75, 190+40);
  gfx->println("HTTP started");

  // Websocket AI assistent
  AIAssistant_Init(audio);
  gfx->setCursor(75, 220+40);
  gfx->println("AI Assistent started");

  delay(100); // Wait before starting loop

  // Create Task for GUI Updates
  Serial.println("Setup new Thread 100 ms");
  xTaskCreatePinnedToCore(
    GUITask,             // Task function
    "GUIUpdateTask",     // Task name
    1024 * 12,           // 12kB
    NULL,                // Task parameters
    3,                   // Priority
    &guiTaskHandle,      // Task handle
    1                    // Core ID (ESP32 typically uses core 1 for tasks)
  );

  char taskList[512];
  vTaskList(taskList);
  Serial.println("Task Name\tStatus\tPrio\tHWM\tTask#");
  Serial.println(taskList);

  PrintAllPartitions();

  // ESP-SR (Microphone Speech Recognition) Initialization
  Serial.println("Setup ESP-SR");
  sr_setup();
}

void loop() {
    static unsigned long last = 0;
    if (millis() - last > 1000 * 60) {
        // Display every 60s
        last = millis();
        Serial.printf("[FreeHeap] %u bytes, Min Ever: %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
        Serial.printf("[PSRAM] Used: %d, Free: %d, Total: %d\n",ESP.getPsramSize()-ESP.getFreePsram(), ESP.getFreePsram(), ESP.getPsramSize());

        // Check alarms every minute
        CheckAlarms();
    }

    // Check if the backlight should be turned off
    if (digitalRead(BUTTON_PIN) == LOW && !backlightAlreadyOff) {
        backlightAlreadyOff = true;
        LCD_SetBacklight(false);
        MIC_SR_Start();
        Serial.println("🔘 Button pressed — screen turned off");
    }

    if (digitalRead(BUTTON_PIN) == HIGH) {
        backlightAlreadyOff = false;  // Reset state when button released
    }

  // audio.loop();
  vTaskDelay(pdMS_TO_TICKS(5));
}

// Audio I2S info
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);

    if (info) {
        String sinfo(info);
        sinfo.replace("|", "\n");
        GUI_EnqueueMessage(sinfo.c_str());  // non-blocking, safe
    }
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
}
void audio_showstation(const char *info){
    Serial.print("station     ");Serial.println(info);

    if (info) {
        String sinfo(info);
        sinfo.replace("|", "\n");
        GUI_EnqueueMessage(sinfo.c_str());  // non-blocking, safe
    }
}
void audio_showstreaminfo(const char *info){
    Serial.print("streaminfo  ");Serial.println(info);
    
}
void audio_showstreamtitle(const char *info){
    Serial.print("streamtitle ");Serial.println(info);

    if (info) {
        String sinfo(info);
        sinfo.replace("|", "\n");
        GUI_EnqueueMessage(sinfo.c_str());  // non-blocking, safe
    }
}
void audio_bitrate(const char *info){
    Serial.print("bitrate     ");Serial.println(info);
}
void audio_commercial(const char *info){  //duration in sec
    Serial.print("commercial  ");Serial.println(info);
}
void audio_icyurl(const char *info){  //homepage
    Serial.print("icyurl      ");Serial.println(info);
}
void audio_lasthost(const char *info){  //stream URL played
    Serial.print("lasthost    ");Serial.println(info);
}
void audio_eof_speech(const char *info){
    Serial.print("eof_speech  ");Serial.println(info);
}


