#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H


#include <WebServer.h>
#include "Audio.h"
#include "LVGL_ST77916.h"

void HttpServer_Begin(Audio& audio);
void HttpServer_Loop();

#endif