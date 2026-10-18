#pragma once
// LCDWiki ES3C28P - exact onboard pin allocation
#define LCD_W 240
#define LCD_H 320
#define LCD_CS 10
#define LCD_DC 46
#define LCD_SCK 12
#define LCD_MOSI 11
#define LCD_MISO 13
#define LCD_BL 45
#define TOUCH_SDA 16
#define TOUCH_SCL 15
#define TOUCH_RST 18
#define TOUCH_IRQ 17
#define TOUCH_ADDR 0x38
#define CODEC_ADDR 0x18
#define AMP_EN 1
#define I2S_MCLK 4
#define I2S_BCLK 5
#define I2S_WS 7
#define I2S_DOUT 8
#define I2S_DIN 6
#define SD_CLK 38
#define SD_CMD 40
#define SD_D0 39
#define SD_D1 41
#define SD_D2 48
#define SD_D3 47
#define RGB_GPIO 42
#define BAT_ADC 9
#define BOOT_GPIO 0
// Exposed expansion: GPIO2,3,14,21; UART0: RX43/TX44.
// Battery ADC is board-specific and requires divider verification.
#ifndef BATTERY_DIVIDER_RATIO
#define BATTERY_DIVIDER_RATIO 2.0f
#endif

// Optional DHT: set DHT_GPIO=2 and DHT_SENSOR_TYPE=DHT22 or DHT11.
#ifndef DHT_GPIO
#define DHT_GPIO -1
#endif
#ifndef DHT_SENSOR_TYPE
#define DHT_SENSOR_TYPE 22
#endif
