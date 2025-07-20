#ifndef CONFIG_H
#define CONFIG_H

// First button pin
#define BUTTON_PIN 0  // GPIO0

// WiFi credentials
#define WIFI_SSID     ENV_WIFI_SSID
#define WIFI_PASSWORD ENV_WIFI_PASSWORD

// SPI frequency (40 MHz typically stable for QSPI)
#define QSPI_FREQ 40000000

// LCD Pin definitions
#define LCD_CS    21
#define LCD_SCK   40
#define LCD_D0    46
#define LCD_D1    45
#define LCD_D2    42
#define LCD_D3    41
#define LCD_BL    5
#define LCD_TE    18
#undef LCD_RST
#define LCD_RST   -1  // External reset pin not accessible (EXIO2)


// Screen resolution
#define SCREEN_WIDTH  360
#define SCREEN_HEIGHT 360


#define GFX_BL 5                         // default backlight pin = 5

#define LVGL_TICK_PERIOD_MS  10

// EEPROM configuration
#define EEPROM_SIZE 128
#define EEPROM_VOLUME_ADDR 0             // EEPROM address where volume is stored
#define EEPROM_WIFI_SSID_ADDR 0 + 4      // 33 bytes for SSID
#define EEPROM_WIFI_PASS_ADDR 64 + 4


#endif // CONFIG_H
