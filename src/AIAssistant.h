#ifndef AI_ASSISTANT_H
#define AI_ASSISTANT_H

#include "Audio.h"
#include <ArduinoWebsockets.h>

using namespace websockets;

extern WebsocketsClient client;

// Websocket communication
void AIAssistant_Init(Audio& audio);
void AIAssistant_StartStream();
void AIAssistant_SendAudioChunk(const void* buffer, size_t byteCount);
void AIAssistant_StopStream();
void AIAssistant_Stop();
// void WebsocketServer_Loop();


// HTTP communication
void AIAssistant_UploadFile(const char* filepath);
void UploadFileTask(void* param);

#endif