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
// guiTaskStackSize = 8 * 1024
const uint32_t guiTaskStackSize = 16 * 1024;

TaskHandle_t httpTaskHandle = NULL;
// httpTaskStackSize = 8 * 1024
const uint32_t httpTaskStackSize = 8 * 1024;

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

                alarm_active = true;
                alarm_started_ms = millis();

                MIC_SR_Stop();   
                vTaskDelay(pdMS_TO_TICKS(5));

                if (audio_ptr && alarm.action_type == "mp3") {
                    // Action: Play MP3
                    if (!audio_ptr->connecttoFS(SD_MMC, alarm.action_path.c_str())) 
                        Serial.println("Failed to play alarm audio");            
                } else if (audio_ptr && alarm.action_type == "sound") {
                    // Action: Play sound
                    if (!audio_ptr->connecttoFS(SD_MMC, alarm.action_path.c_str())) 
                        Serial.println("Failed to play alarm sound");                
                } else if (audio_ptr && alarm.action_type == "radio") {
                    // Action: Play radio stream
                    if (!audio_ptr->connecttohost(alarm.action_path.c_str())) 
                        Serial.println("Failed to connect to radio stream");                
                }

                // Optional: Play default tone
                // audio_ptr->tone(1000, 3000);  // 3 sec

                // Turn on LED
                LCD_SetBacklight(true);

                // Show alarm screen
                GUI_SwitchToScreen(GUI_CreateAlarmActiveScreen, &alarm_screen);
                break;
            }
        }
    }
}

//**************** Thread GUI + Get RTC time *****************************************
void GUITask(void *parameter) {
  static unsigned long lastGuiCheck = 0;
  char displayedTime[9] = "";
  Serial.println("Starting GUITask ...");

  const TickType_t period = pdMS_TO_TICKS(10); // 10ms -> ~100 Hz ; 50ms -> ~33fps
  while (true) {
    static unsigned long last = 0;
    if (millis() - last > 200) { // Update Clock every 200ms
        last = millis();

        if (RTC_GetTime(&rtcTime)) {
          GUI_UpdateClock(rtcTime);
        }

        GUI_QueueTick();  // Call this every ~200ms - read from GUI queue
    }

    // Check if backlight should be turned off and not alarm active
    if (!alarm_active && backlight_on && (millis() - last_touch_time > INACTIVITY_TIMEOUT_MS)) {
      LCD_SetBacklight(false);
      MIC_SR_Start();
    }

    // Alarm auto-timeout (independent timer)
    if (alarm_active) {
        // Hard stop strictly after ALARM_AUTO_TIMEOUT_MS
        if (millis() - alarm_started_ms > ALARM_AUTO_TIMEOUT_MS) {
            alarm_active = false;
            if (audio_ptr) audio_ptr->stopSong();

            LCD_SetBacklight(false);
            MIC_SR_Start();
            lv_async_call([](void*) { GUI_SwitchToScreen(GUI_CreateMainScreen, &main_screen); }, nullptr);
        }
    }

    if (millis() - lastGuiCheck >= 60 * 1000) {  // 60 seconds
        lastGuiCheck = millis();

        if (guiTaskHandle) {
            UBaseType_t watermark = uxTaskGetStackHighWaterMark(guiTaskHandle);
            Serial.printf("[GUI Task] Total stack size: %u bytes, Min free stack: %u words (%u bytes)\n",
                          guiTaskStackSize, watermark, watermark * sizeof(StackType_t));
        }
        if (httpTaskHandle) {
            UBaseType_t watermark_http = uxTaskGetStackHighWaterMark(httpTaskHandle);
            Serial.printf("[HTTP Task] Total stack size: %u bytes, Min free stack: %u words (%u bytes)\n",
                          httpTaskStackSize, watermark_http, watermark_http * sizeof(StackType_t));
        }
    }

    lv_timer_handler(); // Call LVGL timer handler - execution context
   
    vTaskDelay(period);
  }
}

//**************** Thread HTTP *****************************************
void HttpTask(void*) {
  for (;;) {
     HttpServer_Loop();  // Process HTTP requests
    vTaskDelay(pdMS_TO_TICKS(2)); // yield a bit
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

// Helper to push GUI messages safely from the audio callback
static inline void enqueue_gui_msg(const char* s) {
  if (!s) return;
  String sinfo(s);
  sinfo.replace("|", "\n");
  GUI_EnqueueMessage(sinfo.c_str());    // thread-safe (queue)
}

// Single callback for all audio events (v3.4.2+)
static void my_audio_info(Audio::msg_t m) {
  switch (m.e) {
    //Show on the GUI (scroll when long, center when short)
    case Audio::evt_id3data:        // ID3/metadata
    case Audio::evt_name:           // station name / icy-name
    case Audio::evt_streamtitle:    // stream title (current song)
      enqueue_gui_msg(m.msg);
      break;

    // Everything else -> just log
    case Audio::evt_info:
      Serial.printf("info: ............. %s\n", m.msg); break;
    case Audio::evt_eof:
      Serial.printf("end of file: ...... %s\n", m.msg);

      // Clear alarm state 
      alarm_active = false;
      break;
    case Audio::evt_bitrate:
      Serial.printf("bitrate: .......... %s\n", m.msg); break;
    case Audio::evt_icyurl:
      Serial.printf("icy URL: .......... %s\n", m.msg); break;
    case Audio::evt_lasthost:
      Serial.printf("last URL: ......... %s\n", m.msg); break;
    case Audio::evt_icylogo:
      Serial.printf("icy logo: ......... %s\n", m.msg); break;
    case Audio::evt_icydescription:
      Serial.printf("icy descr: ........ %s\n", m.msg); break;
    case Audio::evt_image:
      // APIC cover image segments info
      for (int i = 0; i < m.vec.size(); i += 2) {
        Serial.printf("cover image seg %02d, pos %07lu, len %05lu\n",
                      i/2, m.vec[i], m.vec[i+1]);
      }
      break;
    case Audio::evt_lyrics:
      Serial.printf("sync lyrics: ...... %s\n", m.msg); break;
    case Audio::evt_log:
      Serial.printf("audio log: ........ %s\n", m.msg); break;
    default:
      Serial.printf("message: .......... %s\n", m.msg); break;
  }
}

void setup() {
  Audio::audio_info_callback = my_audio_info;
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

  delay(100); // Wait before starting threads
  Serial.println("Setup new threads");

  // Create Task for HTTP Server
  xTaskCreatePinnedToCore(
    HttpTask, 
    "HttpTask",
    httpTaskStackSize,    // 8kB stack size
    nullptr,              // task parameters
    1,                    // lower priority than GUI
    &httpTaskHandle,      // task handle
    0                     // core 0
  );

  // Create Task for GUI Updates
  xTaskCreatePinnedToCore(
    GUITask,             // Task function
    "GUIUpdateTask",     // Task name
    guiTaskStackSize,    // 16kB stack size
    NULL,                // Task parameters
    2,                   // Priority
    &guiTaskHandle,      // Task handle
    1                    // Core ID (ESP32 typically uses core 1 for tasks and UI)
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
  vTaskDelay(pdMS_TO_TICKS(50));
}
