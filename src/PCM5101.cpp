#include "Audio.h"
#include "PCM5101.h"
#include <EEPROM.h>
#include "config.h"

#define DEFAULT_VOLUME     10      // fallback if uninitialized

static uint8_t currentVolume = DEFAULT_VOLUME;

Audio audio;

void IRAM_ATTR increase_audio_tick(void *arg)
{
  audio.loop();
}

uint8_t LoadVolumeFromEEPROM() {
  uint8_t vol = EEPROM.read(EEPROM_VOLUME_ADDR);
  Serial.printf("Load volume from EEPROM %d\n", vol);
  if (vol > Volume_MAX) vol = DEFAULT_VOLUME;  // Sanity check
  return vol;
}

void Audio_Init() {
  EEPROM.begin(EEPROM_SIZE);
  currentVolume = LoadVolumeFromEEPROM();

  // Audio
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(currentVolume); // 0...21  

  // Set up a hardware timer using ESP-IDF's esp_timer to periodically call the audio.loop() function,
  // which is critical for continuous audio playback using the ESP32-audioI2S library.
  // Using a timer for this is more efficient than doing it in loop() or polling with millis().
  esp_timer_handle_t audio_tick_timer = NULL;
  const esp_timer_create_args_t audio_tick_timer_args = {
    .callback = &increase_audio_tick,        // Function to call
    .dispatch_method = ESP_TIMER_TASK,       // Run in ESP-IDF timer task (not ISR)
    .name = "audio_tick",                    // Debug-friendly name
    .skip_unhandled_events = true            // Skip if missed (prevents backlog)
  };
  esp_timer_create(&audio_tick_timer_args, &audio_tick_timer);
  esp_timer_start_periodic(audio_tick_timer, Audio_TICK_PERIOD_MS * 1000);
}

void SetVolume(uint8_t vol) {
  if (vol > Volume_MAX) {
    printf("Audio : The volume value is incorrect. Please enter 0 to 21\r\n");
    return;
  }

  currentVolume = vol;
  audio.setVolume(currentVolume); // 0...21    
  EEPROM.write(EEPROM_VOLUME_ADDR, currentVolume);
  EEPROM.commit();
}

uint8_t GetVolume() {
  return currentVolume;
}