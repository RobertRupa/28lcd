#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "board_config.h"
class LGFX : public lgfx::LGFX_Device {
 lgfx::Panel_ILI9341 panel;
 lgfx::Bus_SPI bus;
 lgfx::Light_PWM light;
 public:
 LGFX(){
  auto b=bus.config(); b.spi_host=SPI2_HOST;b.spi_mode=0;b.freq_write=60000000;b.freq_read=16000000;
  b.spi_3wire=false;b.use_lock=true;b.dma_channel=SPI_DMA_CH_AUTO;
  b.pin_sclk=LCD_SCK;b.pin_mosi=LCD_MOSI;b.pin_miso=LCD_MISO;b.pin_dc=LCD_DC;
  bus.config(b);panel.setBus(&bus);
  auto p=panel.config();p.pin_cs=LCD_CS;p.pin_rst=-1;p.pin_busy=-1;p.panel_width=LCD_W;p.panel_height=LCD_H;
  p.offset_x=0;p.offset_y=0;p.offset_rotation=0;p.readable=true;p.invert=true; // Panel inversion: fix pale/cyan rendering; validate on device
  p.rgb_order=false;p.dlen_16bit=false;p.bus_shared=false;panel.config(p);
  auto l=light.config();l.pin_bl=LCD_BL;l.invert=false;l.freq=12000;l.pwm_channel=7;
  light.config(l);panel.setLight(&light);setPanel(&panel);
 }
};
