#include "MIC_MSM.h"
// English wakeword : Hi ESP！！！！

#include "esp_dsp.h"
#include <math.h>

// ICS-43434
// https://invensense.tdk.com/wp-content/uploads/2016/02/DS-000069-ICS-43434-v1.2.pdf
// Digital I²S interface with high precision 24-bit data
// Wide frequency response from 60 Hz to 20 kHz
// High power supply rejection: −100 dB FS

I2SClass i2s;

static TaskHandle_t micTaskHandle = nullptr;
static File wavFile;
static volatile bool isRecording = false;
static bool streamToServer = false;  // Stream to server via WebSocket or save to SD card file

static uint32_t sampleRate;
static uint8_t channels;
static uint16_t bitsPerSample;

// Generated using the following command:
// python3 tools/gen_sr_commands.py "Turn on the light,Switch on the light;Turn off the light,Switch off the light,Go dark;Start fan;Stop fan"

enum {
  SR_CMD_TURN_ON_THE_BACKLIGHT,
  SR_CMD_TURN_OFF_THE_BACKLIGHT,
  SR_CMD_BACKLIGHT_IS_BRIGHTEST,
  SR_CMD_BACKLIGHT_IS_DARKEST,
  SR_CMD_PLAY_MUSIC,
};

static const sr_cmd_t sr_commands[] = {
  {0, "Turn on the backlight", "TkN nN jc BaKLiT"},                 // English
  {1, "Turn off the backlight", "TkN eF jc BaKLiT"},                // English
  {2, "backlight is brightest", "BaKLiT gZ BRiTcST"},               // English
  {3, "backlight is darkest", "BaKLiT gZ DnRKcST"},                 // English
  {4, "play music", "PLd MYoZgK"},                                  // English
};

bool play_Music_Flag = 0;
uint8_t LCD_Backlight_original = 0;


void Awaken_Event(sr_event_t event, int command_id, int phrase_id) {
  switch (event) {
    case SR_EVENT_WAKEWORD: 
      // if(ACTIVE_TRACK_CNT)
      //   _lv_demo_music_pause();
      printf("WakeWord Detected!\r\n"); 
      //LCD_Backlight_original = LCD_Backlight;
      break;
    case SR_EVENT_WAKEWORD_CHANNEL:
      printf("WakeWord Channel %d Verified!\r\n", command_id);
      // Turn on backlight
      LCD_SetBacklight(true);
      // ESP_SR.setMode(SR_MODE_COMMAND);  // Switch to Command detection
      // LCD_Backlight = 35;
      ESP_SR.setMode(SR_MODE_WAKEWORD); // Switch back to WakeWord detection
      break;
    case SR_EVENT_TIMEOUT:
      printf("Timeout Detected!\r\n");
      ESP_SR.setMode(SR_MODE_WAKEWORD);  // Switch back to WakeWord detection
      //LCD_Backlight = LCD_Backlight_original;
      if(play_Music_Flag){
        play_Music_Flag = 0;
        printf("SR_EVENT_TIMEOUT");
        // if(ACTIVE_TRACK_CNT)
        //   _lv_demo_music_resume();   
        // else
        //   printf("No MP3 file found in SD card!\r\n");    
      }
      break;
    case SR_EVENT_COMMAND:
      printf("Command %d Detected! %s\r\n", command_id, sr_commands[phrase_id].str);
      switch (command_id) {
        // case SR_CMD_HELLO:
        //   Serial.println("Hello, world!");
        //   break;
        case SR_CMD_TURN_ON_THE_BACKLIGHT:      
          // LCD_Backlight = 100;  
          break;
        case SR_CMD_TURN_OFF_THE_BACKLIGHT:     
          //LCD_Backlight = 0;    
          break;
        case SR_CMD_BACKLIGHT_IS_BRIGHTEST:     
          //LCD_Backlight = 100;  
          break;
        case SR_CMD_BACKLIGHT_IS_DARKEST:       
          //LCD_Backlight = 30;   
          break;
        case SR_CMD_PLAY_MUSIC:                 
          play_Music_Flag = 1;              
          break;
        default:                        printf("Unknown Command!\r\n"); break;
      }
      ESP_SR.setMode(SR_MODE_COMMAND);  // Allow for more commands to be given, before timeout
      // ESP_SR.setMode(SR_MODE_WAKEWORD); // Switch back to WakeWord detection
      break;
    default: printf("Unknown Event!\r\n"); break;
  }
}


void _MIC_Init() {
  Serial.printf("MIC Init\n");
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, I2S_PIN_DIN);
  i2s.setTimeout(1000);

  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT)) {
    Serial.printf("[MIC_Init] I2S begin failed\n");
    return;
  }
}

void MIC_SR_Start() {
  Serial.printf("Skip starting MIC SR\n");
  return;


  // This is not fully working yet, so skip it for now
  Serial.printf("MIC SR Start\n");
  ESP_SR.onEvent(Awaken_Event);
  ESP_SR.begin(i2s, sr_commands, sizeof(sr_commands) / sizeof(sr_cmd_t), SR_CHANNELS_MONO, SR_MODE_WAKEWORD);
  ESP_SR.setMode(SR_MODE_WAKEWORD);
}

void MIC_SR_Stop() {
  Serial.printf("Skip stopping MIC SR\n");
  return;

  // This is not fully working yet, so skip it for now
  Serial.printf("MIC SR Stop\n");
  ESP_SR.end();
}


void MICTask(void *parameter) {
  _MIC_Init();
  esp_task_wdt_add(NULL);
  while(1){
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  vTaskDelete(NULL);
  
}
void MIC_Init() {
  Serial.printf("Starting MIC Task\n");
  
  xTaskCreatePinnedToCore(
    MICTask,     
    "MICTask",  
    4096,                
    NULL,                 
    5,                   
    NULL,                 
    0                     
  );
}

static void writeLE16(File &file, uint16_t value) {
    uint8_t bytes[2] = {
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF)
    };
    file.write(bytes, 2);
}

static void writeLE32(File &file, uint32_t value) {
    uint8_t bytes[4] = {
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 24) & 0xFF)
    };
    file.write(bytes, 4);
}

static void writeWavHeader(File &file) {
    uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    uint16_t blockAlign = channels * bitsPerSample / 8;

    const char riff[] = "RIFF";
    const char wave[] = "WAVE";
    const char fmt[]  = "fmt ";
    const char data[] = "data";

    file.write((const uint8_t *)riff, 4);       // ChunkID: "RIFF"
    writeLE32(file, 0);                         // ChunkSize: placeholder
    file.write((const uint8_t *)wave, 4);       // Format: "WAVE"

    file.write((const uint8_t *)fmt, 4);        // Subchunk1ID: "fmt "
    writeLE32(file, 16);                        // Subchunk1Size: 16 for PCM
    writeLE16(file, 1);                         // AudioFormat: 1 = PCM
    writeLE16(file, channels);                  // NumChannels
    writeLE32(file, sampleRate);                // SampleRate
    writeLE32(file, byteRate);                  // ByteRate
    writeLE16(file, blockAlign);                // BlockAlign
    writeLE16(file, bitsPerSample);             // BitsPerSample

    file.write((const uint8_t *)data, 4);       // Subchunk2ID: "data"
    writeLE32(file, 0);                         // Subchunk2Size: placeholder
}

static void finalizeWavFile(File &file) {
    uint32_t fileSize = file.size();
    if (fileSize < 44) return;  // File too short to be valid

    uint32_t chunkSize = fileSize - 8;
    uint32_t dataSize  = fileSize - 44;

    file.seek(4);
    writeLE32(file, chunkSize);

    file.seek(40);
    writeLE32(file, dataSize);

    file.flush();
}

// This works and it is really good 
// Band-pass filtering (ESP-DSP)
// Automatic Gain Control (AGC)
// Send stream to Websocket or save to WAV file
// Clean I2S shutdown

static void MIC_RecordTask(void *parameter) {
    int32_t rawBuffer[256];           // 32-bit input from ICS-43434
    float floatSamples[256];          // Float samples for DSP
    float filtered[256];              // Filtered output
    int16_t finalSamples[256];        // Final 16-bit output for WAV

    Serial.println("[MIC] Recording task with bandpass + AGC started");

    // Band-pass filter setup (ESP-DSP biquad IIR)
    float coeffs[5];
    float w[2] = {0};
    const float fs = (float)sampleRate;
    const float f0 = 1000.0f;  // Center frequency
    const float Q = 0.707f;    // Quality factor

    dsps_biquad_gen_bpf_f32(coeffs, f0 / fs, Q);

    // AGC parameters
    float targetLevel = 8000.0f;
    float agcGain = 1.0f;
    float agcAttack = 0.01f;
    float agcRelease = 0.001f;
    uint32_t totalSize = 0;

    // Streaming
    // Add this near top of function:
    // Optional streaming buffer
    // int16_t* streamBuffer = (int16_t*)heap_caps_malloc(2048 * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    // int16_t streamBuffer[2048];   // 2048 * 2B (int16_t) = 4096 Bytes
    // size_t streamOffset = 0;

    while (isRecording) {
        size_t bytesRead = i2s.readBytes((char *)rawBuffer, sizeof(rawBuffer));
        size_t sampleCount = bytesRead / sizeof(int32_t);
        if (sampleCount == 0) continue;

        // Convert to float (assuming 24-bit left-justified in 32-bit)
        for (size_t i = 0; i < sampleCount; ++i) {
            floatSamples[i] = (float)(rawBuffer[i] >> 8);
        }

        // Apply band-pass filter
        dsps_biquad_f32(floatSamples, filtered, sampleCount, coeffs, w);

        // Estimate RMS for AGC
        float sumSquares = 0.0f;
        for (size_t i = 0; i < sampleCount; ++i) {
            sumSquares += filtered[i] * filtered[i];
        }
        float rms = sqrtf(sumSquares / sampleCount);

        // AGC gain update
        if (rms > 0.0f) {
            float desiredGain = targetLevel / rms;
            if (desiredGain > agcGain)
                agcGain += agcAttack * (desiredGain - agcGain);
            else
                agcGain += agcRelease * (desiredGain - agcGain);
        }

        // Apply AGC gain
        dsps_mulc_f32_ae32(filtered, filtered, sampleCount, agcGain, 1, 1);

        // Convert to int16 safely
        for (size_t i = 0; i < sampleCount; ++i) {
            float s = filtered[i];
            if (s > 32767.0f) s = 32767.0f;
            if (s < -32768.0f) s = -32768.0f;
            finalSamples[i] = (int16_t)s;
        }

        if (streamToServer) {
            // Send to WebSocket queue or buffer here
            AIAssistant_SendAudioChunk(finalSamples, sampleCount * sizeof(int16_t));

            /*
            // Check if adding the new samples would overflow the buffer
            if (streamOffset + sampleCount >= 1024) {
                // Fill up remaining space
                size_t spaceLeft = 1024 - streamOffset;
                memcpy(&streamBuffer[streamOffset], finalSamples, spaceLeft * sizeof(int16_t));

                // Send full buffer
                AIAssistant_SendAudioChunk(streamBuffer, 1024 * sizeof(int16_t));
                totalSize += 1024 * sizeof(int16_t);
                streamOffset = 0;

                // Copy remaining samples to start of buffer
                size_t remaining = sampleCount - spaceLeft;
                if (remaining > 0) {
                    memcpy(&streamBuffer[streamOffset], &finalSamples[spaceLeft], remaining * sizeof(int16_t));
                    streamOffset += remaining;
                }
            } else {
                // Enough space, just copy in
                memcpy(&streamBuffer[streamOffset], finalSamples, sampleCount * sizeof(int16_t));
                streamOffset += sampleCount;
            }
            */

        } else {
            // Write to WAV file on SD card
            wavFile.write((uint8_t *)finalSamples, sampleCount * sizeof(int16_t));
        }

        totalSize += sampleCount * sizeof(int16_t);
        Serial.printf("[AGC] RMS: %.1f, Gain: %.2f\n", rms, agcGain);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // Cleanup
    // heap_caps_free(streamBuffer);

    i2s.end();
    delay(50);
    i2s.~I2SClass();
    new (&i2s) I2SClass();

    Serial.printf("[MIC] Recording task ended, %d bytes\n", totalSize);

    if (streamToServer) {
        // Flush buffer
        /*
        if (streamOffset > 0) {
            AIAssistant_SendAudioChunk(streamBuffer, streamOffset * sizeof(int16_t));
            totalSize += streamOffset * sizeof(int16_t);
            streamOffset = 0;
        }*/
        AIAssistant_StopStream();
    } else {
      finalizeWavFile(wavFile);
      wavFile.flush();
      wavFile.close();
    }
    delay(200);
    vTaskDelete(nullptr);
}


// Start recording audio from the microphone
void MIC_StartRecording(const char* filename, uint32_t rate, uint8_t ch, uint16_t bits, bool stream) {
  //  filename.c_str(), 16000 /*bitRate*/, 1 /*chanels*/, 16 /*bits*/);
  if (isRecording) return;

  streamToServer = stream;

  Serial.printf("[MIC] Starting recording: %s at %luHz, %dch, %dbit, stream:%d \n", filename, rate, ch, bits, stream);

  // Configure and start ESP_I2S
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, I2S_PIN_DIN);  // Only DOUT or DIN needed
  i2s.setTimeout(1000);  // Optional, useful for .readBytes()

  // Select 32bit data format, Mono, right channel
  if (!i2s.begin(I2S_MODE_STD, rate, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT)) {
    Serial.println("[ERR] I2S begin() failed");
    return;
  }

  // Optionally: configure RX data transform if 32-bit mic output
  // if (!i2s.configureRX(rate, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_32_TO_16)) {
  //   Serial.println("[ERR] I2S configureRX failed");
  //   return;
  // }

  sampleRate = rate;
  channels = ch;
  bitsPerSample = bits;

  if (stream) {
    // WebSocket stream
    AIAssistant_StartStream();
    Serial.println("[MIC] Start streaming via websocket");
  }else{
    // WAV recording
    if (!SD_MMC.begin()) {
      Serial.println("[ERR] SD_MMC mount failed");
      return;
    }

    // Remove old file if it exists
    SD_MMC.remove(filename);
    wavFile = SD_MMC.open(filename, FILE_WRITE);
    if (!wavFile) {
      Serial.println("[ERR] Failed to open file for writing");
      return;
    }

    Serial.printf("[MIC] Start Recording %s\n", filename);

    // Write WAV header placeholder
    writeWavHeader(wavFile);
  }

  // Start recording task
  isRecording = true;
  BaseType_t result = xTaskCreatePinnedToCore(
    MIC_RecordTask,       // Task function
    "MIC_RecordTask",     // Name
    8192,                 // Stack size
    NULL,                 // Parameters
    2,                    // Priority
    &micTaskHandle,       // Out handle
    1                     // Core (use 1 for mic tasks)
  );

  if (result != pdPASS) {
    Serial.println("[ERR] Failed to start MIC_RecordTask");
    wavFile.close();
    isRecording = false;
  }
}


void MIC_StopRecording() {
  if (!isRecording) return;
  isRecording = false;

  if (micTaskHandle) {
    while (eTaskGetState(micTaskHandle) != eDeleted) {
      delay(10);
    }
    micTaskHandle = nullptr;
  }

  Serial.println("[MIC] Stopped recording");
}


