#ifndef LVGL_ST77916_H
#define LVGL_ST77916_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "lvgl.h"
#include "config.h"

extern Arduino_GFX* gfx;

extern bool backlight_on;
extern unsigned long last_touch_time;
extern const unsigned long INACTIVITY_TIMEOUT_MS;

extern uint32_t bufSize;
extern lv_display_t *disp;
extern lv_color_t *disp_draw_buf;

void increase_lvgl_tick(void *arg);
uint32_t millis_cb(void);

void Lvgl_Init();
void LCD_SetBacklight(bool on);
void Lvgl_Touchpad_Read(lv_indev_t *indev_drv, lv_indev_data_t *data);

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map);

#endif