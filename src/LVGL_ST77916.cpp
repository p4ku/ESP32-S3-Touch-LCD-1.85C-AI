#include "LVGL_ST77916.h"
#include "Touch_CST816.h"
#include "MIC_MSM.h"
#include "GUI/GUI.h"

Arduino_GFX* gfx;

bool backlight_on = true;
unsigned long last_touch_time = 0;
const unsigned long INACTIVITY_TIMEOUT_MS = 30000; // 30 seconds
const uint32_t ALARM_AUTO_TIMEOUT_MS = 5 * 60000;  // 5 minutes

uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;
lv_color_t *disp_draw_buf_2;


void increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

uint32_t millis_cb(void)
{
  return millis();
}

void LCD_SetBacklight(bool on) {
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, on ? HIGH : LOW);
    backlight_on = on;
    // if true,  reset the last touch time
    // to prevent the screen from going off
    if (on) {
        last_touch_time = millis();
    }
}

void Lvgl_Init()
{
    // Initialize Quad SPI bus for LCD
    Arduino_ESP32QSPI *bus = new Arduino_ESP32QSPI(
      LCD_CS,    // Chip Select pin
      LCD_SCK,   // SPI Clock pin
      LCD_D0,    // Data 0 pin (MOSI)
      LCD_D1,    // Data 1 pin (MISO/QSPI)
      LCD_D2,    // Data 2 pin (QSPI)
      LCD_D3,    // Data 3 pin (QSPI)
      true       // Enable Quad SPI mode
    );

    // Initialize ST77916 LCD driver
    // Arduino_ST77916 
    gfx = new Arduino_ST77916(
      bus,        // Data bus
      LCD_RST,    // Reset pin (-1 if external)
      0,          // Screen rotation
      true,      // ISP
      SCREEN_WIDTH, 
      SCREEN_HEIGHT
    );

    if (!gfx->begin(QSPI_FREQ)) {
      Serial.println("LCD initialization failed!");
      while (true) delay(50);
    }

    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH); // Turn on backlight

    gfx->fillScreen(BLACK);
    gfx->setTextSize(2);
    gfx->setTextColor(WHITE);
    gfx->setCursor(75, 10+40);
    gfx->println("LCD Initialized!");

    last_touch_time = millis();

    // Initialise LVGL
    lv_init();

    /*Set a tick source so that LVGL will know how much time elapsed. */
    lv_tick_set_cb(millis_cb);

    bufSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t) / 2;

    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
    disp_draw_buf_2 = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);

    Serial.println("Buffer size:");
    Serial.println(bufSize);


    if (!disp_draw_buf)
    {
      Serial.println("LVGL disp_draw_buf allocate failed!");
    }
    else
    {
      disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
      lv_display_set_flush_cb(disp, my_disp_flush);

      lv_display_set_buffers(disp, disp_draw_buf, disp_draw_buf_2, bufSize, LV_DISPLAY_RENDER_MODE_PARTIAL);

      /*Initialize the (dummy) input device driver*/
      lv_indev_t *indev = lv_indev_create();
      lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
      lv_indev_set_read_cb(indev, Lvgl_Touchpad_Read);

      // ESP-IDF hardware timer that calls increase_lvgl_tick()
      // Tell LVGL how much time has passed, crucial for animations, input timing, and scheduling internal tasks.
      const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,        // Function to call
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,      // Run in ESP-IDF timer task (not ISR)
        .name = "lvgl_tick",                    // Debug-friendly name
        .skip_unhandled_events = true           // Skip if missed (prevents backlog)
      };

      esp_timer_handle_t lvgl_tick_timer = NULL;
      ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
      ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000)); // 10ms
    }
}

void Lvgl_Touchpad_Read(lv_indev_t *indev, lv_indev_data_t *data) {
  // If a transition is in progress, ignore touches
  if (g_gui_transitioning || (millis() - g_last_screen_load_ms < 120)) {
    data->state = LV_INDEV_STATE_REL;
    data->point.x = 0;
    data->point.y = 0;
    return;
  }

  if (Touch_interrupts) {
    Touch_interrupts = false;
    detachInterrupt(CST816_INT_PIN);
    Touch_Read_Data();
    attachInterrupt(CST816_INT_PIN, Touch_CST816_ISR, FALLING);
  }

  if (touch_data.points != 0) {
    last_touch_time = millis();
    if (!backlight_on) {
      LCD_SetBacklight(true);
      MIC_SR_Stop();
      data->state = LV_INDEV_STATE_REL;
      data->point.x = data->point.y = 0;
      return;
    }
    data->point.x = touch_data.x;
    data->point.y = touch_data.y;
    data->state   = LV_INDEV_STATE_PR;
  } else {
    data->state   = LV_INDEV_STATE_REL;
  }

  // clear sampled data
  touch_data.x = touch_data.y = 0;
  touch_data.points = 0;
  touch_data.gesture = NONE;
}

/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

  /*Call it to tell LVGL you are ready*/
  lv_disp_flush_ready(disp);
}


