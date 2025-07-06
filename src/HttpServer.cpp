#include <vector>
#include <WiFi.h>
#include <time.h>
#include "ESP_I2S.h"
#include "HttpServer.h"
#include "SD_Card.h"
#include <SD.h>
#include "FS.h"
#include "PCM5101.h"
#include "MIC_MSM.h"


WebServer server(80);
static Audio* audio_ptr = nullptr;
static File uploadFile;
static unsigned long bootMillis = millis();
static I2SClass streamI2S;

bool deleteRecursive(String path) {
    File file = SD_MMC.open(path.c_str());
    if (!file) return false;

    if (!file.isDirectory()) {
        file.close();
        return SD_MMC.remove(path.c_str());
    }

    File entry;
    while ((entry = file.openNextFile())) {
        String entryPath = String(entry.name());
        entry.close();
        if (!deleteRecursive(entryPath)) {
            file.close();
            return false;
        }
    }
    file.close();
    return SD_MMC.rmdir(path.c_str());
}


void HttpServer_Begin(Audio& audio)
{
    audio_ptr = &audio;
    static File uploadFile;

    server.on("/backlight", HTTP_POST, []() {
        if (server.hasArg("on")) {
            String value = server.arg("on");
            value.toLowerCase();

            bool turnOn = (value == "1" || value == "true" || value == "on");
            LCD_SetBacklight(turnOn);

            server.send(200, "text/plain", "Backlight " + String(turnOn ? "ON" : "OFF"));
        } else {
            server.send(400, "text/plain", "Missing 'on' parameter (use: 1, true, on)");
        }
    });

    // Get volume
    server.on("/volume", HTTP_GET, []() {
        server.send(200, "text/plain", String(GetVolume()));
    });

    // Set volume
    server.on("/volume", HTTP_POST, []() {
        if (server.hasArg("value")) {
            int vol = server.arg("value").toInt();
            vol = constrain(vol, 0, 21);
            SetVolume(vol);
            server.send(200, "text/plain", "Volume set to " + String(vol));
        } else {
            server.send(400, "text/plain", "Missing 'value' parameter");
        }
    });

    // Play URL
    server.on("/play", HTTP_POST, []() {
        if (server.hasArg("url")) {
            String url = server.arg("url");
            audio_ptr->connecttohost(url.c_str());
            server.send(200, "text/plain", "Playing: " + url);
        } else {
            server.send(400, "text/plain", "Missing 'url' parameter");
        }
    });

    // Stop playig 
    server.on("/stop", HTTP_POST, []() {
        audio_ptr->stopSong();
        server.send(200, "text/plain", "Playback stopped");
    });

    // Speech
    server.on("/speech", HTTP_POST, []() {
        if (server.hasArg("text") && server.hasArg("lang")) {
            String text = server.arg("text");
            String lang = server.arg("lang");
            if (audio_ptr->connecttospeech(text.c_str(), lang.c_str())) {
                server.send(200, "text/plain", "Speaking: " + text);
            } else {
                server.send(500, "text/plain", "Failed to play speech");
            }
        } else {
            server.send(400, "text/plain", "Missing 'text' or 'lang' parameter");
        }
    });

    // Play file
    server.on("/playfile", HTTP_POST, []() {
        if (server.hasArg("path")) {
            String path = server.arg("path");

            // Just try to open the file to verify existence
            File f = SD_MMC.open(path.c_str());
            if (!f || f.isDirectory()) {
                server.send(404, "text/plain", "File not found: " + path);
                return;
            }
            f.close();

            if (audio_ptr->connecttoFS(SD_MMC, path.c_str())) {
                server.send(200, "text/plain", "Playing file: " + path);
            } else {
                server.send(500, "text/plain", "Failed to play file: " + path);
            }
        } else {
            server.send(400, "text/plain", "Missing 'path' parameter");
        }
    });

    // Upload file
    server.on("/upload", HTTP_POST,
        []() {
            server.send(200, "text/plain", "Upload complete");
        },
        []() {
            HTTPUpload& upload = server.upload();

            static String targetPath = "/";

            if (upload.status == UPLOAD_FILE_START) {
                // Determine the directory path from the POST argument (form field)
                if (server.hasArg("path")) {
                    targetPath = server.arg("path");
                    if (!targetPath.startsWith("/")) targetPath = "/" + targetPath;
                    if (!targetPath.endsWith("/")) targetPath += "/";
                } else {
                    targetPath = "/";
                }

                String fullPath = targetPath + upload.filename;
                Serial.printf("Upload Start, full path: %s\n", fullPath.c_str());

                uploadFile = SD_MMC.open(fullPath.c_str(), FILE_WRITE);
                if (!uploadFile) {
                    Serial.println("Failed to open file for writing");
                }

            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);

            } else if (upload.status == UPLOAD_FILE_END) {
                if (uploadFile) {
                    uploadFile.close();
                    Serial.println("Upload finished");
                }
            }
        }
    );

    server.on("/delete", HTTP_POST, []() {
        if (!server.hasArg("path")) {
            server.send(400, "text/plain", "Missing 'path' parameter");
            return;
        }

        String path = server.arg("path");
        if (!path.startsWith("/")) path = "/" + path;

        if (!SD_MMC.exists(path.c_str())) {
            server.send(404, "text/plain", "File not found: " + path);
            return;
        }

        if (SD_MMC.remove(path.c_str()) || SD_MMC.rmdir(path.c_str())) {
            server.send(200, "text/plain", "Deleted: " + path);
        } else {
            server.send(500, "text/plain", "Failed to delete: " + path);
        }
    });

    // List files and folders with sorting (dirs first, A-Z)
    server.on("/listfiles", HTTP_GET, []() {
        String dir = "/";
        String ext = "";
        String responseType = "plain";

        if (server.hasArg("path")) dir = server.arg("path");
        if (server.hasArg("ext")) ext = server.arg("ext");
        if (server.hasArg("type")) responseType = server.arg("type");

        ext.toLowerCase();
        responseType.toLowerCase();

        File root = SD_MMC.open(dir.c_str());
        if (!root || !root.isDirectory()) {
            server.send(404, "text/plain", "Directory not found: " + dir);
            return;
        }

        struct Entry {
            String name;
            size_t size;
            bool isDir;
        };
        std::vector<Entry>* entries = new(std::nothrow) std::vector<Entry>();
        if (!entries) {
            server.send(500, "text/plain", "Memory allocation failed");
            root.close();
            return;
        }

        root.rewindDirectory();
        while (true) {
            File entry = root.openNextFile();
            if (!entry) break;

            String name = String(entry.name());
            bool isDir = entry.isDirectory();
            size_t size = isDir ? 0 : entry.size();

            // Add trailing slash to directory names
            if (isDir && !name.endsWith("/")) name += "/";

            if (ext.isEmpty() || name.endsWith(ext)) {
                entries->push_back({name, size, isDir});
            }

            entry.close();
        }
        root.close();

        // Sort: Directories first, A-Z
        std::sort(entries->begin(), entries->end(), [](const Entry& a, const Entry& b) {
            if (a.isDir != b.isDir) return a.isDir > b.isDir; // dirs first
            return a.name.compareTo(b.name) < 0; // then sort by name A-Z
        });

        if (responseType == "json") {
            String json = "[\n";
            for (size_t i = 0; i < entries->size(); ++i) {
                const auto& e = (*entries)[i];
                json += "  {\"name\":\"" + e.name + "\", \"size\":" + String(e.size) + ", \"isDir\":" + (e.isDir ? "true" : "false") + "}";
                if (i < entries->size() - 1) json += ",";
                json += "\n";
            }
            json += "]";
            delete entries;
            server.send(200, "application/json", json);
            return;
        }

        // Plain text
        size_t maxNameLen = 0;
        for (const auto& e : *entries) {
            if (e.name.length() > maxNameLen)
                maxNameLen = e.name.length();
        }

        String response = "Files in " + dir + (ext.length() > 0 ? " (." + ext + ")" : "") + ":\n\n";
        for (const auto& e : *entries) {
            String name = e.name;
            size_t size = e.size;

            int padding = maxNameLen - name.length();
            response += "  " + name;
            for (int i = 0; i < padding; ++i) response += " ";
            response += "   " + String(size) + " bytes\n";
        }

        delete entries;
        server.send(200, "text/plain", response);
    });

    // Make directory
    server.on("/mkdir", HTTP_POST, []() {
        if (!server.hasArg("path")) {
            server.send(400, "text/plain", "Missing 'path' parameter");
            return;
        }
        String path = server.arg("path");
        if (!path.startsWith("/")) path = "/" + path;

        if (SD_MMC.mkdir(path.c_str())) {
            server.send(200, "text/plain", "Directory created: " + path);
        } else {
            server.send(500, "text/plain", "Failed to create directory: " + path);
        }
    });

    // Remove directory
    server.on("/rmdir", HTTP_POST, []() {
        if (!server.hasArg("path")) {
            server.send(400, "text/plain", "Missing 'path' parameter");
            return;
        }

        String path = server.arg("path");
        if (!path.startsWith("/")) path = "/" + path;

        if (!SD_MMC.exists(path.c_str())) {
            server.send(404, "text/plain", "Directory does not exist: " + path);
            return;
        }

        if (deleteRecursive(path)) {
            server.send(200, "text/plain", "Directory deleted: " + path);
        } else {
            server.send(500, "text/plain", "Failed to delete: " + path);
        }
    });

    // Move file
    server.on("/move", HTTP_POST, []() {
        if (!server.hasArg("from") || !server.hasArg("to")) {
            server.send(400, "text/plain", "Missing 'from' or 'to' parameter");
            return;
        }

        String from = server.arg("from");
        String to = server.arg("to");
        if (!from.startsWith("/")) from = "/" + from;
        if (!to.startsWith("/")) to = "/" + to;

        if (!SD_MMC.exists(from.c_str())) {
            server.send(404, "text/plain", "Source does not exist: " + from);
            return;
        }

        if (SD_MMC.rename(from.c_str(), to.c_str())) {
            server.send(200, "text/plain", "Moved: " + from + " → " + to);
        } else {
            server.send(500, "text/plain", "Failed to move file");
        }
    });

    // Download file
    server.on("/download", HTTP_GET, []() {
        if (!server.hasArg("path")) {
            server.send(400, "text/plain", "Missing 'path' parameter");
            return;
        }

        String path = server.arg("path");
        if (!path.startsWith("/")) path = "/" + path;

        File file = SD_MMC.open(path.c_str(), FILE_READ);
        if (!file || file.isDirectory()) {
            server.send(404, "text/plain", "File not found or is a directory: " + path);
            return;
        }

        String contentType = "application/octet-stream";
        server.streamFile(file, contentType);
        file.close();
    });

    // Preview file
    server.on("/preview", HTTP_GET, []() {
        if (!server.hasArg("path")) {
            server.send(400, "text/plain", "Missing 'path' parameter");
            return;
        }

        String path = server.arg("path");
        if (!path.startsWith("/")) path = "/" + path;

        File file = SD_MMC.open(path.c_str(), FILE_READ);
        if (!file || file.isDirectory()) {
            server.send(404, "text/plain", "File not found or is a directory: " + path);
            return;
        }

        // Infer content type
        String contentType = "text/plain";  // default
        if (path.endsWith(".html")) contentType = "text/html";
        else if (path.endsWith(".css")) contentType = "text/css";
        else if (path.endsWith(".js")) contentType = "application/javascript";
        else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
        else if (path.endsWith(".png")) contentType = "image/png";
        else if (path.endsWith(".gif")) contentType = "image/gif";
        else if (path.endsWith(".svg")) contentType = "image/svg+xml";
        else if (path.endsWith(".mp3")) contentType = "audio/mpeg";

        server.streamFile(file, contentType);
        file.close();
    });


    // Load ESP status
    server.on("/status", HTTP_GET, []() {
        String response;

        // WiFi Info
        response += "[WiFi]\n";
        response += "SSID: " + WiFi.SSID() + "\n";
        response += "IP: " + WiFi.localIP().toString() + "\n\n";

        // Time (from RTC)
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[16];
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            response += "[Time]\n";
            response += "RTC Time: " + String(timeStr) + "\n";
        } else {
            response += "[Time]\n";
            response += "RTC Time: unavailable\n";
        }

        // Uptime
        unsigned long uptime = millis() - bootMillis;
        int up_seconds = uptime / 1000;
        int hours = up_seconds / 3600;
        int minutes = (up_seconds % 3600) / 60;
        int seconds = up_seconds % 60;
        char uptimeStr[16];
        snprintf(uptimeStr, sizeof(uptimeStr), "%02d:%02d:%02d", hours, minutes, seconds);
        response += "Uptime: " + String(uptimeStr) + "\n\n";

        // Memory Info
        response += "[Memory]\n";
        response += "FreeHeap: " + String(ESP.getFreeHeap()) + " bytes\n";
        response += "MinEverHeap: " + String(ESP.getMinFreeHeap()) + " bytes\n";
        if (psramFound()) {
            response += "Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes\n";
            response += "Free PSRAM: " + String(ESP.getFreePsram()) + " bytes\n";
            response += "Total PSRAM: " + String(ESP.getPsramSize()) + " bytes\n";
        } else {
            response += "PSRAM: Not available\n";
        }

        // Chip Info
        response += "\n[Chip]\n";
        response += "SDK: " + String(ESP.getSdkVersion()) + "\n";
        response += "Chip ID: " + String((uint32_t)ESP.getEfuseMac(), HEX) + "\n";
        response += "CPU Freq: " + String(ESP.getCpuFreqMHz()) + " MHz\n";

        server.send(200, "text/plain", response);
    });

    // Endpoint to trigger srmodels.bin flashing
    server.on("/update_srmodels", HTTP_POST, []() {
        bool force = false;
        if (server.hasArg("force")) {
            String f = server.arg("force");
            f.toLowerCase();
            force = (f == "1" || f == "true" || f == "yes");
        }

        server.send(200, "text/plain", "Update started. Please wait 30s...");

        xTaskCreatePinnedToCore(
            [](void* param) {
                bool f = *(bool*)param;
                free(param);
                write_srmodels_bin_to_partition_if_needed(f);
                vTaskDelete(NULL);
            },
            "SRModelUpdater",
            8192,
            new bool(force),
            1,
            NULL,
            1
        );
    });

    // Get stations
    server.on("/stations", HTTP_GET, []() {
        File file = SD_MMC.open("/internet_stations.txt", FILE_READ);
        if (!file || file.isDirectory()) {
            server.send(404, "application/json", "[]");
            return;
        }

        String json = "[\n";
        bool first = true;

        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.isEmpty()) continue;

            int sep = line.indexOf('|');
            if (sep <= 0 || sep >= line.length() - 1) continue;

            String name = line.substring(0, sep);
            String url = line.substring(sep + 1);

            if (!first) json += ",\n";
            json += "  {\"name\":\"" + name + "\", \"url\":\"" + url + "\"}";
            first = false;
        }

        json += "\n]";
        file.close();

        server.send(200, "application/json", json);
    });

    // Stream I2S microphone data
    server.on("/stream", HTTP_GET, []() {
        WiFiClient client = server.client();
        if (!client.connected()) return;

        const uint32_t sampleRate = 16000;
        const uint16_t bitsPerSample = 16;
        const uint8_t channels = 1;

        // Send HTTP headers
        client.print("HTTP/1.1 200 OK\r\n");
        client.print("Content-Type: application/octet-stream\r\n");
        client.print("Connection: close\r\n\r\n");

        // Start I2S
        streamI2S.setPins(I2S_PIN_BCK, I2S_PIN_WS, I2S_PIN_DOUT, I2S_PIN_DIN);
        streamI2S.setTimeout(1000);

        Serial.println("[STREAM] Initializing I2S...");

        if (!streamI2S.begin(I2S_MODE_STD, sampleRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT)) {
            Serial.println("[ERR] I2S begin failed");
            client.stop();
            return;
        }

        if (!streamI2S.configureRX(sampleRate, I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO, I2S_RX_TRANSFORM_32_TO_16)) {
            Serial.println("[ERR] configureRX failed");
            streamI2S.end();
            client.stop();
            return;
        }
        Serial.println("[STREAM] Microphone PCM stream started");

        uint8_t buffer[512];
        size_t bytesRead = 0;
        uint32_t totalBytes = 0;
        unsigned long startTime = millis();
        const unsigned long maxDuration = 60000; // 60s

        while (client.connected()) {
                bytesRead = streamI2S.readBytes((char *)buffer, sizeof(buffer));
                if (bytesRead > 0) {
                    // Apply dynamic compression
                    int16_t *samples = (int16_t *)buffer;
                    size_t sampleCount = bytesRead / 2;

                    for (size_t i = 0; i < sampleCount; i++) {
                        float s = samples[i] / 32768.0f;

                        // Soft compression
                        if (fabsf(s) < 0.25f) {
                            s *= 2.5f; // boost quiet
                        } else if (fabsf(s) < 0.5f) {
                            s *= 1.5f;
                        } else if (fabsf(s) > 0.9f) {
                            s *= 0.8f; // limit loud
                        }

                        // Clamp to [-1, 1] and convert back
                        if (s > 1.0f) s = 1.0f;
                        if (s < -1.0f) s = -1.0f;

                        samples[i] = (int16_t)(s * 32767);
                    }

                    client.write(buffer, bytesRead);
                    totalBytes += bytesRead;
                }

            delay(1);  // Avoid starving WiFi stack
            // vTaskDelay(pdMS_TO_TICKS(20));

            if (millis() - startTime > maxDuration) {
                Serial.printf("[STREAM] Timeout (%lu ms), sent %lu bytes\n", millis() - startTime, totalBytes);
                break;
            }
        }

        streamI2S.end();
        client.stop();
        Serial.println("[STREAM] Stream ended and cleaned up");
    });

    // Serve any static file from SDCARD /html/ if it exists and is requested via GET
    server.onNotFound([]() {
        if (server.method() != HTTP_GET) {
            server.send(405, "text/plain", "Method Not Allowed");
            return;
        }

        String uri = server.uri();
        if (uri == "/") uri = "/index.html"; // default fallback

        String path = "/html" + uri;

        File file = SD_MMC.open(path);
        if (!file || file.isDirectory()) {
            server.send(404, "text/plain", "File not found: " + path);
            return;
        }

        String contentType = "text/plain";
        if (path.endsWith(".html")) contentType = "text/html";
        else if (path.endsWith(".css")) contentType = "text/css";
        else if (path.endsWith(".js")) contentType = "application/javascript";
        else if (path.endsWith(".json")) contentType = "application/json";
        else if (path.endsWith(".png")) contentType = "image/png";
        else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) contentType = "image/jpeg";
        else if (path.endsWith(".ico")) contentType = "image/x-icon";

        server.streamFile(file, contentType);
        file.close();
    });

    // Start server
    server.begin();
}

void HttpServer_Loop() {
    server.handleClient();
}
