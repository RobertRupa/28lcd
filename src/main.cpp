#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <SD_MMC.h>
#include <lvgl.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <driver/i2s.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>
#include <Preferences.h>
#include <DHT.h>
#include "board_config.h"
#include "display.h"

static LGFX lcd;
static Adafruit_NeoPixel pixel(1, RGB_GPIO, NEO_GRB + NEO_KHZ800);
static uint8_t *drawmemA=nullptr,*drawmemB=nullptr;
static lv_display_t *lvDisplay=nullptr;
static bool touchOk=false,codecOk=false,sdOk=false;
static uint32_t lastUpdate=0;
static uint8_t ledColor=0;
static lv_obj_t *statusLabel=nullptr,*sdLabel=nullptr,*wifiLabel=nullptr,*bleLabel=nullptr,*batteryLabel=nullptr;
static bool sdBusy=false;
static Preferences audioPrefs;
static Preferences wifiPrefs;
static uint8_t masterVolume=35; // 0..100%; conservative default
static uint8_t masterVolumePending=35;
static uint32_t masterVolumeChangedAt=0;
static bool masterVolumeDirty=false;
static lv_obj_t *masterVolumeLabel=nullptr;

static void bladeNavigate(int8_t direction);
static int16_t swipeStartY=0,swipeStartX=0;
static uint32_t swipeStartMs=0;
static bool swipeTracking=false;
static int16_t swipeLastY=0,swipeLastX=0;
static int8_t swipePending=0;

static bool i2cPing(uint8_t addr){Wire.beginTransmission(addr);return Wire.endTransmission()==0;}
static bool codecReg(uint8_t r,uint8_t v){Wire.beginTransmission(CODEC_ADDR);Wire.write(r);Wire.write(v);return Wire.endTransmission()==0;}
static void codecBegin(){
 codecOk=i2cPing(CODEC_ADDR);if(!codecOk)return;
 // Playback path based on paeqe/es3c28p-dap src/es8311.cpp
 codecReg(0x00,0x1F);delay(20);codecReg(0x00,0x00);codecReg(0x00,0x80);delay(10);
 codecReg(0x01,0x30);codecReg(0x02,0x00);codecReg(0x03,0x10);
 codecReg(0x04,0x10);codecReg(0x05,0x00);codecReg(0x01,0x3F);
 codecReg(0x09,0x0C);codecReg(0x0A,0x0C);
 codecReg(0x0D,0x01);codecReg(0x0E,0x02);codecReg(0x12,0x00);
 codecReg(0x13,0x10);codecReg(0x0B,0x00);codecReg(0x0C,0x00);
 codecReg(0x10,0x1F);codecReg(0x11,0x7F);
 codecReg(0x31,0x00);codecReg(0x07,0x00);codecReg(0x08,0xFF);codecReg(0x06,0x04);
 codecReg(0x32,0x9F); // conservative playback volume
}
static bool touchRead(int32_t &x,int32_t &y){
 if(!touchOk)return false;
 Wire.beginTransmission(TOUCH_ADDR);Wire.write(0x02);
 if(Wire.endTransmission(false)!=0||Wire.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)1)!=(uint8_t)1)return false;
 uint8_t n=Wire.read() & 0x0F;if(n==0||n>2)return false;
 Wire.beginTransmission(TOUCH_ADDR);Wire.write(0x03);
 if(Wire.endTransmission(false)!=0||Wire.requestFrom((uint8_t)TOUCH_ADDR,(uint8_t)4)!=(uint8_t)4)return false;
 uint8_t xh=Wire.read(),xl=Wire.read(),yh=Wire.read(),yl=Wire.read();
 int32_t rx=((xh & 0x0F)<<8)|xl,ry=((yh & 0x0F)<<8)|yl;
 // Same mirrored transform as the reference project.
 x=constrain((int32_t)(LCD_W-1-rx),0,LCD_W-1);
 y=constrain((int32_t)(LCD_H-1-ry),0,LCD_H-1);
 return true;
}
static void flushCb(lv_display_t *d,const lv_area_t *a,uint8_t *px){
 uint32_t w=a->x2-a->x1+1,h=a->y2-a->y1+1;
 lcd.startWrite();lcd.setAddrWindow(a->x1,a->y1,w,h);
 // LovyanGFX handles RGB565 transfer ordering: do not byte-swap LVGL buffer.
 lcd.writePixels((lgfx::rgb565_t*)px,w*h);lcd.endWrite();lv_display_flush_ready(d);
}
static void inputCb(lv_indev_t*,lv_indev_data_t *d){
 int32_t x=0,y=0;
 if(touchRead(x,y)){
  d->state=LV_INDEV_STATE_PRESSED;d->point.x=x;d->point.y=y;
  swipeLastX=x;swipeLastY=y;
  if(!swipeTracking){swipeTracking=true;swipeStartX=x;swipeStartY=y;swipeStartMs=millis();}
 }else{
  d->state=LV_INDEV_STATE_RELEASED;
  if(swipeTracking){
   // A deliberate vertical gesture; horizontal sliders do not trigger navigation.
   int32_t dx=swipeLastX-swipeStartX;
   int32_t dy=swipeLastY-swipeStartY;
   if(abs(dx)>65 && abs(dx)>abs(dy)*2 && millis()-swipeStartMs<1100)
    swipePending=(dx<0)?1:-1;
   swipeTracking=false;
  }
 }
}
static void tick(void*){lv_tick_inc(1);}
static lv_obj_t *label(lv_obj_t *parent,const char *text){
 lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,text);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_obj_set_width(o,205);return o;
}
static lv_obj_t *button(lv_obj_t *parent,const char *txt,lv_event_cb_t callback){
 lv_obj_t *b=lv_button_create(parent);lv_obj_set_width(b,200);lv_obj_set_height(b,36);
 lv_obj_add_event_cb(b,callback,LV_EVENT_CLICKED,nullptr);
 lv_obj_t *t=lv_label_create(b);lv_label_set_text(t,txt);lv_obj_center(t);return b;
}
static void statusUpdate(){
 if(!statusLabel)return;
 char buf[360];
 snprintf(buf,sizeof(buf),"LCD: ILI9341V 240x320\nDotyk FT6336: %s\nKodek ES8311: %s\nMicroSD SDIO 4-bit: %s\nPSRAM: %s (%lu KB wolne)\nBOOT GPIO0: %s\nWIFI: %s\nBLE: skaner z zakladki SD/WIFI\nUART0: GPIO43/44\nI2C: SDA16 SCL15",
 touchOk?"OK":"BRAK",codecOk?"OK":"BRAK",sdOk?"OK":"BRAK",
 psramFound()?"OK":"BRAK",(unsigned long)(ESP.getFreePsram()/1024),
 digitalRead(BOOT_GPIO)==LOW?"WCISNIETY":"zwolniony",
 WiFi.status()==WL_CONNECTED?"polaczono":"niepolaczono");
 lv_label_set_text(statusLabel,buf);
}
static void onLed(lv_event_t*){
 ledColor=(ledColor+1)%5;
 const uint32_t colors[]={0x000000,0x300000,0x003000,0x000030,0x202020};
 pixel.setPixelColor(0,colors[ledColor]);pixel.show();
 Serial.printf("[RGB] state=%u\n",ledColor);
}
static void onBrightness(lv_event_t *e){
 auto *s=lv_event_get_target_obj(e);lcd.setBrightness(lv_slider_get_value(s));
}
static void onWifi(lv_event_t*){
 if(wifiLabel)lv_label_set_text(wifiLabel,"Skan Wi-Fi...");
 // Synchronous scan can freeze the UI briefly; result is displayed afterwards.
 WiFi.mode(WIFI_STA);WiFi.disconnect(false,false);
 int count=WiFi.scanNetworks(false,true);
 String out="Sieci Wi-Fi: "+String(count)+"\n";
 for(int i=0;i<count&&i<5;i++)out+=WiFi.SSID(i)+" ("+WiFi.RSSI(i)+" dBm)\n";
 WiFi.scanDelete();if(wifiLabel)lv_label_set_text(wifiLabel,out.c_str());
 Serial.println(out);
}
static void onBle(lv_event_t*){
 if(bleLabel)lv_label_set_text(bleLabel,"Skan BLE przez 3 sekundy...");
 BLEDevice::init("");
 BLEScan *scanner=BLEDevice::getScan();
 scanner->setActiveScan(true);
 BLEScanResults results=scanner->start(3,false);
 String out="BLE: "+String(results.getCount())+" urzadzen\n";
 for(int i=0;i<results.getCount()&&i<4;i++){
  BLEAdvertisedDevice d=results.getDevice(i);
  out+=String(d.getAddress().toString().c_str())+" "+String(d.getRSSI())+" dBm\n";
 }
 scanner->clearResults();
 if(bleLabel)lv_label_set_text(bleLabel,out.c_str());
 Serial.println(out);
}
static void onSd(lv_event_t*){
 if(sdBusy||!sdLabel)return;sdBusy=true;
 if(!sdOk){lv_label_set_text(sdLabel,"Brak karty lub blad montowania.");sdBusy=false;return;}
 File dir=SD_MMC.open("/");String txt="Pliki w /:\n";
 if(dir&&dir.isDirectory()){
  File f=dir.openNextFile();int n=0;
  while(f&&n<7){txt+=String(f.name());txt+=f.isDirectory()?"/":"";txt+="\n";f.close();n++;f=dir.openNextFile();}
  if(f)f.close();
 }else txt+="Nie mozna odczytac katalogu";
 if(dir)dir.close();lv_label_set_text(sdLabel,txt.c_str());sdBusy=false;
}
static void onTone(lv_event_t*){
 if(!codecOk){Serial.println("[audio] Brak kodeka ES8311");return;}
 // Tone test; do not start if speaker is not connected.
 pinMode(AMP_EN,OUTPUT);digitalWrite(AMP_EN,LOW);
 i2s_config_t cfg={};cfg.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX);
 cfg.sample_rate=44100;cfg.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
 cfg.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;cfg.communication_format=I2S_COMM_FORMAT_STAND_I2S;
 cfg.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1;cfg.dma_buf_count=8;cfg.dma_buf_len=256;
 cfg.use_apll=false;cfg.tx_desc_auto_clear=true;cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
 if(i2s_driver_install(I2S_NUM_1,&cfg,0,nullptr)!=ESP_OK){Serial.println("[audio] i2s install failed");return;}
 i2s_pin_config_t pins={};pins.mck_io_num=I2S_MCLK;pins.bck_io_num=I2S_BCLK;
 pins.ws_io_num=I2S_WS;pins.data_out_num=I2S_DOUT;pins.data_in_num=I2S_PIN_NO_CHANGE;
 if(i2s_set_pin(I2S_NUM_1,&pins)==ESP_OK){
  int16_t samples[512];float phase=0;const float delta=2.0f*PI*440/44100;
  for(int k=0;k<110;k++){for(int i=0;i<256;i++){int16_t s=(int16_t)(sinf(phase)*7000);
   samples[2*i]=samples[2*i+1]=(int16_t)(s*((float)masterVolume/100.0f));phase+=delta;if(phase>2*PI)phase-=2*PI;}
   size_t sent=0;i2s_write(I2S_NUM_1,samples,sizeof(samples),&sent,portMAX_DELAY);}
 }
 i2s_zero_dma_buffer(I2S_NUM_1);i2s_driver_uninstall(I2S_NUM_1);
 digitalWrite(AMP_EN,HIGH);Serial.println("[audio] tone complete");
}
// Subtle three-note boot chime through ES8311; I2S is released afterwards.
static void bootMelody(){
 if(!codecOk)return;
 i2s_config_t cfg={};
 cfg.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_TX);
 cfg.sample_rate=22050;cfg.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
 cfg.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT;
 cfg.communication_format=I2S_COMM_FORMAT_STAND_I2S;
 cfg.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1;cfg.dma_buf_count=4;cfg.dma_buf_len=128;
 cfg.use_apll=false;cfg.tx_desc_auto_clear=true;cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
 if(i2s_driver_install(I2S_NUM_1,&cfg,0,nullptr)!=ESP_OK)return;
 i2s_pin_config_t pins={};
 pins.mck_io_num=I2S_MCLK;pins.bck_io_num=I2S_BCLK;
 pins.ws_io_num=I2S_WS;pins.data_out_num=I2S_DOUT;pins.data_in_num=I2S_PIN_NO_CHANGE;
 if(i2s_set_pin(I2S_NUM_1,&pins)!=ESP_OK){i2s_driver_uninstall(I2S_NUM_1);return;}
 static const uint16_t notes[]={523,659,784};
 int16_t buf[256];float phase=0.0f;
 digitalWrite(AMP_EN,LOW);
 for(uint8_t n=0;n<3;n++){
  const uint32_t frames=2205; // 100 ms per note
  for(uint32_t pos=0;pos<frames;pos+=128){
   const uint32_t count=min((uint32_t)128,frames-pos);
   for(uint32_t i=0;i<count;i++){
    float envelope=(float)min(pos+i,150U)/150.0f;
    envelope*=min(1.0f,(float)(frames-pos-i)/200.0f);
    int16_t sample=(int16_t)(sinf(phase)*2400.0f*envelope*((float)masterVolume/100.0f));
    buf[2*i]=sample;buf[2*i+1]=sample;
    phase+=2.0f*PI*notes[n]/22050.0f;
    if(phase>2.0f*PI)phase-=2.0f*PI;
   }
   size_t written=0;i2s_write(I2S_NUM_1,buf,count*4,&written,portMAX_DELAY);
  }
 }
 i2s_zero_dma_buffer(I2S_NUM_1);i2s_driver_uninstall(I2S_NUM_1);
 digitalWrite(AMP_EN,HIGH);
}
static void onI2c(lv_event_t*){
 Serial.println("[I2C] scanning bus");for(int i=1;i<127;i++)if(i2cPing(i))Serial.printf("[I2C] 0x%02X\n",i);
}

static void audioApplyMasterVolume();
static Preferences thermostatPrefs;
static void masterVolumeService(){
 if(masterVolumeDirty && millis()-masterVolumeChangedAt>=900){
  audioPrefs.putUChar("master",masterVolumePending);
  masterVolumeDirty=false;
 }
}
static void masterVolumeChanged(lv_event_t *e){
 masterVolume=(uint8_t)constrain((int)lv_slider_get_value(lv_event_get_target_obj(e)),0,100);
 masterVolumePending=masterVolume;
 audioApplyMasterVolume();
 masterVolumeDirty=true;
 masterVolumeChangedAt=millis();
 if(masterVolumeLabel){char buf[28];snprintf(buf,sizeof(buf),"Glosnosc: %u%%",masterVolume);lv_label_set_text(masterVolumeLabel,buf);}
}

static float setPoint=21.0f;
// An internal die temperature is NOT ambient room temperature.
static float roomTemp=NAN;
static bool temperatureValid=false;
#if DHT_GPIO < 0
static uint32_t lastInternalTempRead=0;
#endif
#if DHT_GPIO >= 0
static DHT dht(DHT_GPIO,DHT_SENSOR_TYPE);
static uint32_t lastDhtRead=0;
static bool dhtOk=false;
#endif
static float hysteresis=0.5f;
static bool heatingEnabled=true;
static bool heatingDemand=false; // Logic only; no relay GPIO is driven
static lv_obj_t *roomLabel=nullptr,*targetLabel=nullptr,*heatLabel=nullptr,*thermoNote=nullptr,*adminLabel=nullptr;
static lv_obj_t *targetSlider=nullptr,*simulationSlider=nullptr,*heatSwitch=nullptr,*hysteresisSlider=nullptr;

static void thermostatRefresh(){
 bool sensorAvailable=temperatureValid;
#if DHT_GPIO < 0
 sensorAvailable=false; // Chip temperature must NEVER drive room heating.
#endif
 if(heatingEnabled && sensorAvailable){
  if(heatingDemand && roomTemp >= setPoint+hysteresis/2.0f) heatingDemand=false;
  else if(!heatingDemand && roomTemp <= setPoint-hysteresis/2.0f) heatingDemand=true;
 } else heatingDemand=false;
 char b[96];
 if(roomLabel){
  if(temperatureValid)snprintf(b,sizeof(b),"%.1f°C",roomTemp);
  else snprintf(b,sizeof(b),"--.-°C");
  lv_label_set_text(roomLabel,b);
 }
 if(targetLabel){snprintf(b,sizeof(b),"Cel: %.1f°C",setPoint);lv_label_set_text(targetLabel,b);}
 if(heatLabel)lv_label_set_text(heatLabel,heatingDemand?"GRZANIE: ZADANE":"GRZANIE: OFF");
 if(adminLabel){
#if DHT_GPIO >= 0
 snprintf(b,sizeof(b),"Histereza %.1f C / %s\\nCzujnik DHT: %s\\nWyjscie: nieprzypisane",hysteresis,heatingEnabled?"AUTO":"OFF",temperatureValid?"OK":"BRAK");
#else
 snprintf(b,sizeof(b),"Histereza %.1f C / %s\\nTemperatura ukladu ESP32\\nOgrzewanie zablokowane bez DHT",hysteresis,heatingEnabled?"AUTO":"OFF");
#endif
 lv_label_set_text(adminLabel,b);
}
}
static void onTarget(lv_event_t *e){
 setPoint=lv_slider_get_value(lv_event_get_target_obj(e))/10.0f;
 thermostatPrefs.putFloat("target",setPoint);thermostatRefresh();
}
static void onSimulation(lv_event_t *e){
#if DHT_GPIO >= 0
 return; // Live sensor owns the temperature, simulation disabled.
#endif
 // Fallback sensor is read-only. No simulated room temperature.
 (void)e;
}
static void onHeatEnable(lv_event_t *e){
 heatingEnabled=lv_obj_has_state(lv_event_get_target_obj(e),LV_STATE_CHECKED);
 thermostatPrefs.putBool("enabled",heatingEnabled);thermostatRefresh();
}
static void onHysteresis(lv_event_t *e){
 hysteresis=lv_slider_get_value(lv_event_get_target_obj(e))/10.0f;
 thermostatPrefs.putFloat("hyst",hysteresis);thermostatRefresh();
}
static lv_obj_t* makeHeading(lv_obj_t *parent,const char *str,uint8_t fontSize=0){
 lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,str);
 lv_obj_set_style_text_color(o,lv_color_hex(0xE2E8F0),0);
 lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
 lv_obj_set_width(o,210);
 if(fontSize==1)lv_obj_set_style_text_font(o,&lv_font_montserrat_14,0);
 return o;
}
static void makeThermostat(lv_obj_t *tab){
 lv_obj_set_style_bg_color(tab,lv_color_hex(0x0B101B),0);
 lv_obj_set_style_pad_all(tab,8,0);
 lv_obj_set_style_pad_row(tab,8,0);
 lv_obj_set_flex_flow(tab,LV_FLEX_FLOW_COLUMN);
 lv_obj_set_flex_align(tab,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
 lv_obj_t *head=lv_label_create(tab);
 lv_label_set_text(head,"KLIMAT  /  SALON");
 lv_obj_set_style_text_color(head,lv_color_hex(0x94A3B8),0);
 lv_obj_set_style_text_font(head,&lv_font_montserrat_14,0);
 lv_obj_t *card=lv_obj_create(tab);
 lv_obj_set_width(card,LV_PCT(100));
 lv_obj_set_height(card,128);
 lv_obj_set_style_radius(card,18,0);
 lv_obj_set_style_border_width(card,1,0);
 lv_obj_set_style_border_color(card,lv_color_hex(0x26344A),0);
 lv_obj_set_style_bg_color(card,lv_color_hex(0x172234),0);
 lv_obj_set_style_pad_all(card,13,0);
 lv_obj_clear_flag(card,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_set_flex_flow(card,LV_FLEX_FLOW_COLUMN);
 lv_obj_set_style_pad_row(card,5,0);
 lv_obj_t *subtitle=lv_label_create(card);
#if DHT_GPIO >= 0
 lv_label_set_text(subtitle,"TEMPERATURA / DHT");
#else
 lv_label_set_text(subtitle,"TEMP. UKLADU ESP32");
#endif
 lv_obj_set_style_text_color(subtitle,lv_color_hex(0xFFFFFF),0);
 lv_obj_set_style_text_font(subtitle,&lv_font_montserrat_16,0);
 roomLabel=lv_label_create(card);lv_label_set_text(roomLabel,"20.0°C");
 lv_obj_set_style_text_font(roomLabel,&lv_font_montserrat_28,0);
 lv_obj_set_style_text_color(roomLabel,lv_color_hex(0xF8FAFC),0);
 heatLabel=lv_label_create(card);lv_label_set_text(heatLabel,"GRZANIE: OFF");
 lv_obj_set_style_text_color(heatLabel,lv_color_hex(0xA5F3CE),0);
 lv_obj_t *set=lv_obj_create(tab);
 lv_obj_set_width(set,LV_PCT(100));lv_obj_set_height(set,82);
 lv_obj_set_style_radius(set,16,0);
 lv_obj_set_style_bg_color(set,lv_color_hex(0x172234),0);
 lv_obj_set_style_border_width(set,0,0);
 lv_obj_set_style_pad_all(set,10,0);
 targetLabel=lv_label_create(set);lv_label_set_text(targetLabel,"Cel: 21.0°C");
 lv_obj_set_style_text_color(targetLabel,lv_color_hex(0xE2E8F0),0);
 targetSlider=lv_slider_create(set);
 lv_obj_set_width(targetSlider,LV_PCT(100));
 lv_obj_set_height(targetSlider,11);
 lv_obj_align(targetSlider,LV_ALIGN_BOTTOM_MID,0,-7);
 lv_slider_set_range(targetSlider,150,300);
 lv_slider_set_value(targetSlider,(int)(setPoint*10),LV_ANIM_OFF);
 lv_obj_set_style_bg_color(targetSlider,lv_color_hex(0x37465F),LV_PART_MAIN);
 lv_obj_set_style_bg_color(targetSlider,lv_color_hex(0x38BDF8),LV_PART_INDICATOR);
 lv_obj_set_style_bg_color(targetSlider,lv_color_hex(0xF8FAFC),LV_PART_KNOB);
 lv_obj_add_event_cb(targetSlider,onTarget,LV_EVENT_VALUE_CHANGED,nullptr);
 thermoNote=nullptr; // No demo overlay on the home screen.
 thermostatRefresh();
}
static void makeAdmin(lv_obj_t *tab){
 lv_obj_set_flex_flow(tab,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(tab,8,0);
 label(tab,"Panel administratora");
 masterVolumeLabel=label(tab,"Glosnosc: 35%");
 {char b[28];snprintf(b,sizeof(b),"Glosnosc: %u%%",masterVolume);lv_label_set_text(masterVolumeLabel,b);}
 lv_obj_t *vol=lv_slider_create(tab);lv_obj_set_width(vol,195);lv_obj_set_height(vol,12);
 lv_slider_set_range(vol,0,100);lv_slider_set_value(vol,masterVolume,LV_ANIM_OFF);
 lv_obj_add_event_cb(vol,masterVolumeChanged,LV_EVENT_VALUE_CHANGED,nullptr);
 label(tab,"Master volume: melodie, test tonu i WAV/MP3. Zapis automatyczny.");
 label(tab,"Termostat aktywny:");
 heatSwitch=lv_switch_create(tab);
 if(heatingEnabled)lv_obj_add_state(heatSwitch,LV_STATE_CHECKED);
 lv_obj_add_event_cb(heatSwitch,onHeatEnable,LV_EVENT_VALUE_CHANGED,nullptr);
 label(tab,"Histereza: 0.2 - 2.0 C");
 hysteresisSlider=lv_slider_create(tab);lv_obj_set_width(hysteresisSlider,195);
 lv_slider_set_range(hysteresisSlider,2,20);
 lv_slider_set_value(hysteresisSlider,(int)(hysteresis*10),LV_ANIM_OFF);
 lv_obj_add_event_cb(hysteresisSlider,onHysteresis,LV_EVENT_VALUE_CHANGED,nullptr);
#if DHT_GPIO >= 0
 label(tab,"Temperatura pochodzi z czujnika DHT");
#else
 label(tab,"Czujnik wewnetrzny ESP32-S3 mierzy nagrzanie ukladu, a nie pokoj.");
 label(tab,"Tryb ogrzewania jest zablokowany bez DHT.");
#endif
 adminLabel=label(tab,"");
 label(tab,"Ustawienia celu, histerezy i trybu sa zapisywane w NVS.");
 label(tab,"UWAGA: brak czujnika i wyjscia grzewczego. To panel demonstracyjny.");
 statusLabel=label(tab,"");
 batteryLabel=label(tab,"");
 thermostatRefresh();
}

#include "audio_engine.h"
#include "camera_engine.h"
#include "coprocessor.h"
#include "gate_web.h"
#include "ui_dashboard.h"
static void setupUI(){uiSetupDashboard();}

void setup(){
 Serial.begin(115200);delay(500);
 thermostatPrefs.begin("thermostat",false);
 audioPrefs.begin("sound",false);
 masterVolume=(uint8_t)constrain((int)audioPrefs.getUChar("master",35),0,100);
 masterVolumePending=masterVolume;
 wifiPrefs.begin("wifi",false);
 setPoint=constrain(thermostatPrefs.getFloat("target",21.0f),15.0f,30.0f);
 hysteresis=constrain(thermostatPrefs.getFloat("hyst",0.5f),0.2f,2.0f);
 heatingEnabled=thermostatPrefs.getBool("enabled",true);
 Serial.println("\nES3C28P LVGL Peripheral Lab");
#if DHT_GPIO >= 0
 dht.begin();
#else
 roomTemp=temperatureRead();
 temperatureValid=isfinite(roomTemp);
#endif
 pinMode(BOOT_GPIO,INPUT_PULLUP);pinMode(AMP_EN,OUTPUT);digitalWrite(AMP_EN,HIGH);
 pixel.begin();pixel.clear();pixel.show();
 Wire.begin(TOUCH_SDA,TOUCH_SCL,400000);
 pinMode(TOUCH_RST,OUTPUT);digitalWrite(TOUCH_RST,LOW);delay(15);digitalWrite(TOUCH_RST,HIGH);delay(300);
 pinMode(TOUCH_IRQ,INPUT_PULLUP);
 touchOk=i2cPing(TOUCH_ADDR);
 if(touchOk){Wire.beginTransmission(TOUCH_ADDR);Wire.write(0xA4);Wire.write(0x00);Wire.endTransmission();}
 codecBegin();
 bootMelody();
 lcd.init();lcd.setRotation(0);lcd.setBrightness(160);lcd.fillScreen(TFT_BLACK);
 // Shared with SDIO pins only, never configure them as GPIO elsewhere.
 SD_MMC.setPins(SD_CLK,SD_CMD,SD_D0,SD_D1,SD_D2,SD_D3);
 sdOk=SD_MMC.begin("/sdcard",false,false,20000,5);
 WiFi.mode(WIFI_STA);
 {String ssid=wifiPrefs.getString("ssid","");if(ssid.length())WiFi.begin(ssid.c_str(),wifiPrefs.getString("password","").c_str());}
 lv_init();
 // Two 32-line internal DMA buffers. RGB565, partial redraw only.
 constexpr uint32_t bufferBytes=LCD_W*32*2;
 drawmemA=(uint8_t*)heap_caps_malloc(bufferBytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
 drawmemB=(uint8_t*)heap_caps_malloc(bufferBytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
 if(!drawmemA||!drawmemB){Serial.println("[fatal] No DMA draw buffers");while(true)delay(1000);}
 lvDisplay=lv_display_create(LCD_W,LCD_H);
 lv_display_set_color_format(lvDisplay,LV_COLOR_FORMAT_RGB565);
 lv_theme_t *darkTheme=lv_theme_default_init(lvDisplay,lv_color_hex(0x278DDA),lv_color_hex(0x42BFA9),true,&lv_font_montserrat_16);
 lv_display_set_theme(lvDisplay,darkTheme);
 lv_display_set_flush_cb(lvDisplay,flushCb);
 lv_display_set_buffers(lvDisplay,drawmemA,drawmemB,bufferBytes,LV_DISPLAY_RENDER_MODE_PARTIAL);
 lv_indev_t *touch=lv_indev_create();
 lv_indev_set_type(touch,LV_INDEV_TYPE_POINTER);
 lv_indev_set_read_cb(touch,inputCb);
 lv_indev_set_display(touch,lvDisplay);
 const esp_timer_create_args_t args={.callback=&tick,.arg=nullptr,.dispatch_method=ESP_TIMER_TASK,.name="lv_tick",.skip_unhandled_events=true};
 esp_timer_handle_t th;esp_timer_create(&args,&th);esp_timer_start_periodic(th,1000);
 gateWebBegin();
 setupUI();
#ifdef ENABLE_COPROCESSOR_UART
 coproBegin();
#endif
 Serial.printf("Touch=%d Codec=%d SD=%d PSRAM=%d\n",touchOk,codecOk,sdOk,psramFound());
}
void loop(){
 audioService();coproService();cameraService();masterVolumeService();gateWebService();
#if DHT_GPIO >= 0
 if(millis()-lastDhtRead>=2500){lastDhtRead=millis();float t=dht.readTemperature();dhtOk=isfinite(t);temperatureValid=dhtOk;if(dhtOk)roomTemp=t;else heatingDemand=false;thermostatRefresh();}
#else
 if(millis()-lastInternalTempRead>=2000){
  lastInternalTempRead=millis();
  const float t=temperatureRead();
  temperatureValid=isfinite(t)&&t>-40.0f&&t<125.0f;
  if(temperatureValid)roomTemp=t;
  heatingDemand=false;
  thermostatRefresh();
 }
#endif
 lv_timer_handler();
 if(swipePending){int8_t direction=swipePending;swipePending=0;bladeNavigate(direction);}
 if(millis()-lastUpdate>500){lastUpdate=millis();statusUpdate();uiPowerRefresh();
 int raw=analogRead(BAT_ADC);float voltage=(raw/4095.0f)*3.3f*BATTERY_DIVIDER_RATIO;
 char b[96];snprintf(b,sizeof(b),"BAT ADC: %d (szac. %.2f V*)",raw,voltage);
 if(batteryLabel)lv_label_set_text(batteryLabel,b);
 }delay(5);}
