// AIAssistant.cpp - ESP32-S3 client that sends microphone audio to Python WebSocket server
// and receives response audio back for playback
#include <WiFi.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "AIAssistant.h"

using namespace websockets;
WebsocketsClient client;
String senderSessionId = "";

static const char* websocketURL = ENV_WEBSOCKET_URL;
static const char* uploadURL = ENV_UPLOAD_URL;

// Audio ptr
static Audio* audio_ptr = nullptr;

// Mode and language settings
static String currentLang = "en";
static String currentMode = "chat";

// Streaming Microphone data state
static bool streamingWave = false;

/*
 Web Socket
*/

void onMessageCallback(WebsocketsMessage message)
{
    Serial.print("Received Websocket message");
    if (message.isText())
    {
        const String msg = message.data();

        Serial.printf("[AI Assistant] MSG: %s\n", msg.c_str());

        // Check for simple HELLO or JSON
        if (msg.startsWith("{")) {
          // Parse JSON response
          StaticJsonDocument<512> doc;
          DeserializationError error = deserializeJson(doc, msg);
          if (error) {
            Serial.printf("[AI Assistant] Failed to parse JSON: %s\n", error.c_str());
            return;
          }

          //  // sessions == id(ws)
          const char* type     = doc["type"]     | "unknown";
          const char* content  = doc["content"]  | "";
          const char* language = doc["language"] | "";
          const char* mode     = doc["mode"]     | "";
          const char* sender   = doc["sender"]   | "unspecified";
         
          Serial.println("[AI Assistant] Received assistant message");

          if (strcmp(type, "ASSISTANT_TEXT_RESPONSE") == 0) {
              Serial.println("[AI Assistant] Agent text response received:");
              Serial.printf("  Sender: %s\n", sender);
              Serial.printf("  Type: %s\n", type);
              Serial.printf("  Content: %s\n", content);
              Serial.printf("  Language: %s\n", language);
              Serial.printf("  Mode: %s\n", mode);

              if (audio_ptr) {
                  audio_ptr->connecttospeech(content, language);
              } else {
                  Serial.println("[AI Assistant] Audio pointer is null!");
              }
              return;
          } else if (strcmp(type, "HELLO") == 0) {
              Serial.printf("  Sender: %s\n", sender);
              Serial.printf("  Type: %s\n", type);
              Serial.println("[AI Assistant] Server acknowledged with HELLO"); 

              // Save sender ID globally
              senderSessionId = String(sender);
              Serial.printf("[AI Assistant] Stored session ID: %s\n", senderSessionId.c_str());
              return;
          }
        } else {
          Serial.printf("[AI Assistant] Unrecognized text: %s\n", msg.c_str());
          return;
        }

        Serial.println("Received non-binary message: " + message.data());
        return;
    }

    // Binary
    uint8_t *payload = (uint8_t *)message.c_str();
    size_t length = message.length();

    if (length == 0)
    {
        Serial.println("Received empty audio data");
        return;
    }

    Serial.printf("Received binary audio data of length: %zu bytes\n", length);
    delay(10);
}


void onEventsCallback(WebsocketsEvent event, String data)
{
    if (event == WebsocketsEvent::ConnectionOpened)
    {
        Serial.println("Connection Opened");
    }
    else if (event == WebsocketsEvent::ConnectionClosed)
    {
        Serial.println("Connection Closed");
    }
    else if (event == WebsocketsEvent::GotPing)
    {
        // Serial.println("Got a Ping!");
    }
    else if (event == WebsocketsEvent::GotPong)
    {
        Serial.println("Got a Pong!");
    }
}

// Task to handle WebSocket connection and reconnection logic
// This runs in a separate task to avoid blocking the main loop
void websocketConnectTask(void* parameter) {
  int backoff = 2000;
  const int pollIntervalMs = 10;     // call poll() every 10ms
  const int reconnectCheckMs = 5000; // check connection every 5s

  unsigned long lastReconnectCheck = millis();

  Serial.println("[WebSocket] Starting websocketConnectTask");

  while (true) {
    // Always keep polling to handle incoming data, ping/pong, etc.
    client.poll();
    vTaskDelay(pollIntervalMs / portTICK_PERIOD_MS);

    // Reconnection check (every reconnectCheckMs)
    if (millis() - lastReconnectCheck >= reconnectCheckMs) {
      lastReconnectCheck = millis();

      if (!client.available()) {
        Serial.printf("[WebSocket] Disconnected. Reconnecting... (Backoff: %dms)\n", backoff);

        if (client.connect(websocketURL)) {
          Serial.println("[WebSocket] Reconnected successfully!");
          client.send(R"({"type":"HELLO"})");
          client.ping();
          backoff = 2000;  // reset backoff
        } else {
          Serial.println("[WebSocket] Reconnect attempt failed.");
          vTaskDelay(backoff / portTICK_PERIOD_MS);
          backoff = min(backoff * 2, 60000);  // exponential backoff up to 60s
        }
      }
    }
  }
  vTaskDelete(NULL);  // not reached, safety
}



void AIAssistant_Init(Audio& audio){
  audio_ptr = &audio;

  Serial.println("[AI Assistant] Connecting to server...");

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);

  xTaskCreatePinnedToCore(websocketConnectTask, "WebSocketConnectTask", 4096, NULL, 3, NULL, 1);
  Serial.println("[AI Assistant] WebSocket client initialized");
}

void AIAssistant_StartStream() {
  if (client.available()) {
    streamingWave = true;
    client.send(R"({"type":"START_STREAM"})");
    Serial.println("[AI Assistant] Started audio stream: JSON START_STREAM sent");
  } else {
    Serial.println("[AI Assistant] Failed to start stream — WebSocket not available");
  }
}

void AIAssistant_SendAudioChunk(const void* buffer, size_t byteCount) {
  if (client.available()) {
    client.sendBinary(reinterpret_cast<const char*>(buffer), byteCount);
  }
}

void AIAssistant_StopStream() {
  if (client.available() && streamingWave) {
    client.send(R"({"type":"STOP_STREAM"})");
    streamingWave = false;
    Serial.println("[AI Assistant] Stopped audio stream: JSON STOP_STREAM sent");
  } else if (!client.available()) {
    Serial.println("[AI Assistant] Cannot stop stream — WebSocket not available");
  } else if (!streamingWave) {
    Serial.println("[AI Assistant] Stream already stopped or not started");
  }
}

/*
void AIAssistant_SetLanguage(const char* langCode) {
  currentLang = langCode;
  if (client.available()) {
    // client.send("[LANG:" + currentLang + "]");
  }
}
*/

/*
void AIAssistant_SetMode(const char* mode) {
  currentMode = mode;
  if (client.available()) {
    // client.send("[MODE:" + currentMode + "]");
  }
}*/

void AIAssistant_Stop() {
  if (client.available()) {
    String json = R"({"type":"STOP_STREAM"})";
    client.send(json);
    Serial.println("[AI Assistant] Sent [STOP] command to server");
  } else {
    Serial.println("[AI Assistant] Cannot send [STOP] — not connected to server");
  }
}

/*
void WebsocketServer_Loop()
{
  if (client.available()) {
    client.poll();
  }
}
*/

/*
  HTTP Client
*/

void AIAssistant_UploadFile(const char* filepath) {
  if (!SD_MMC.exists(filepath)) {
    Serial.printf("[Upload] File not found: %s\n", filepath);
    return;
  }

  File file = SD_MMC.open(filepath);
  if (!file || file.isDirectory()) {
    Serial.printf("[Upload] Failed to open: %s\n", filepath);
    return;
  }

  const char* host = "5.9.104.22";
  const int port = 8766;
  const char* path = "/upload";
  const String boundary = "----ESP32FormBoundary";

  WiFiClient client;
  if (!client.connect(host, port)) {
    Serial.println("[Upload] Connection to server failed");
    file.close();
    return;
  }

  // Build initial headers
  String header = "";
  header += "POST " + String(path) + " HTTP/1.1\r\n";
  header += "Host: " + String(host) + "\r\n";
  header += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";

  // Add session_id form field
  String bodySession =
    "--" + boundary + "\r\n" +
    "Content-Disposition: form-data; name=\"session_id\"\r\n\r\n" +
    senderSessionId + "\r\n";

  // File part
  String bodyStart =
    "--" + boundary + "\r\n" +
    "Content-Disposition: form-data; name=\"file\"; filename=\"" + String(filepath) + "\"\r\n" +
    "Content-Type: audio/wav\r\n\r\n";

  String bodyEnd = "\r\n--" + boundary + "--\r\n";

  // Update content length to include session ID field
  size_t totalLength = bodySession.length() + bodyStart.length() + file.size() + bodyEnd.length();
  header += "Content-Length: " + String(totalLength) + "\r\n\r\n";

  // Send headers and body parts
  client.print(header);
  client.print(bodySession);
  client.print(bodyStart);

  // Stream file content
  uint8_t buf[512];
  while (file.available()) {
    size_t len = file.read(buf, sizeof(buf));
    client.write(buf, len);
    delay(1); // allow watchdog to breathe
  }

  // End body
  client.print(bodyEnd);
  file.close();

  // Read server response
  Serial.println("[Upload] Waiting for response...");
  String responseHeaders;
  String responseBody;
  bool inBody = false;

  unsigned long timeout = millis() + 5000;
  while (!client.available() && millis() < timeout) {
    delay(10);
  }

  // while (client.available()) {
  //   String line = client.readStringUntil('\n');
  //   Serial.print(line);
  // }

  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (!inBody && line == "\r") {
      inBody = true;
      continue;
    }

    if (inBody) {
      responseBody += line;
    } else {
      responseHeaders += line;
    }
  }

  Serial.println("[Upload] Response Headers:");
  Serial.println(responseHeaders);
  Serial.println("[Upload] Response Body:");
  Serial.println(responseBody);

  // Parse JSON if possible
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, responseBody);
  if (!error) {
    const char* status = doc["status"];
    const char* filename = doc["filename"];
    int size = doc["size"];

    Serial.printf("[Upload] Success: status=%s, filename=%s, size=%d\n", status, filename, size);
  } else {
    Serial.print("[Upload] JSON parse error: ");
    Serial.println(error.c_str());
  }

  client.stop();
  Serial.println("\n[Upload] Done.");
}



void UploadFileTask(void* param) {
  const char* fname = (const char*)param;

  Serial.printf("[Task] Uploading file in background: %s\n", fname);
  AIAssistant_UploadFile(fname);
  Serial.println("[Task] Upload complete.");

  vPortFree((void*)fname);  // free memory allocated for filename
  vTaskDelete(NULL);
}


