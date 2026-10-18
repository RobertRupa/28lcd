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
static lv_disp_draw_buf_t drawbuf;
static lv_color_t *drawmem=nullptr;
static bool touchOk=false,codecOk=false,sdOk=false;
static uint32_t lastUpdate=0;
static uint8_t ledColor=0;
static lv_obj_t *statusLabel=nullptr,*sdLabel=nullptr,*wifiLabel=nullptr,*bleLabel=nullptr,*batteryLabel=nullptr;
static bool sdBusy=false;

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
 if(Wire.endTransmission(false)!=0||Wire.requestFrom(TOUCH_ADDR, (uint8_t)1)!=(uint8_t)1)return false;
 uint8_t n=Wire.read() & 0x0F;if(n==0||n>2)return false;
 Wire.beginTransmission(TOUCH_ADDR);Wire.write(0x03);
 if(Wire.endTransmission(false)!=0||Wire.requestFrom(TOUCH_ADDR,(uint8_t)4)!=(uint8_t)4)return false;
 uint8_t xh=Wire.read(),xl=Wire.read(),yh=Wire.read(),yl=Wire.read();
 int32_t rx=((xh & 0x0F)<<8)|xl,ry=((yh & 0x0F)<<8)|yl;
 // Same mirrored transform as the reference project.
 x=constrain((int32_t)(LCD_W-1-rx),0,LCD_W-1);
 y=constrain((int32_t)(LCD_H-1-ry),0,LCD_H-1);
 return true;
}
static void flushCb(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *px){
 uint32_t w=a->x2-a->x1+1,h=a->y2-a->y1+1;
 lcd.startWrite();lcd.setAddrWindow(a->x1,a->y1,w,h);
 lcd.writePixels((lgfx::rgb565_t*)px,w*h);lcd.endWrite();lv_disp_flush_ready(d);
}
static void inputCb(lv_indev_drv_t*,lv_indev_data_t *d){
 int32_t x=0,y=0;if(touchRead(x,y)){d->state=LV_INDEV_STATE_PRESSED;d->point.x=x;d->point.y=y;}
 else d->state=LV_INDEV_STATE_RELEASED;
}
static void tick(void*){lv_tick_inc(1);}
static lv_obj_t *label(lv_obj_t *parent,const char *text){
 lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,text);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_obj_set_width(o,205);return o;
}
static lv_obj_t *button(lv_obj_t *parent,const char *txt,lv_event_cb_t callback){
 lv_obj_t *b=lv_btn_create(parent);lv_obj_set_width(b,200);lv_obj_set_height(b,36);
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
 auto *s=lv_event_get_target(e);lcd.setBrightness(lv_slider_get_value(s));
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
   samples[2*i]=samples[2*i+1]=s;phase+=delta;if(phase>2*PI)phase-=2*PI;}
   size_t sent=0;i2s_write(I2S_NUM_1,samples,sizeof(samples),&sent,portMAX_DELAY);}
 }
 i2s_zero_dma_buffer(I2S_NUM_1);i2s_driver_uninstall(I2S_NUM_1);
 digitalWrite(AMP_EN,HIGH);Serial.println("[audio] tone complete");
}
static void onI2c(lv_event_t*){
 Serial.println("[I2C] scanning bus");for(int i=1;i<127;i++)if(i2cPing(i))Serial.printf("[I2C] 0x%02X\n",i);
}

static Preferences thermostatPrefs;
static float setPoint=21.0f;
static float roomTemp=20.0f; // simulated unless DHT_GPIO >= 0
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
 bool sensorAvailable=true;
#if DHT_GPIO >= 0
 sensorAvailable=dhtOk;
#endif
 if(heatingEnabled && sensorAvailable){
  if(heatingDemand && roomTemp >= setPoint+hysteresis/2.0f) heatingDemand=false;
  else if(!heatingDemand && roomTemp <= setPoint-hysteresis/2.0f) heatingDemand=true;
 } else heatingDemand=false;
 char b[96];
 if(roomLabel){snprintf(b,sizeof(b),"%.1f°C",roomTemp);lv_label_set_text(roomLabel,b);}
 if(targetLabel){snprintf(b,sizeof(b),"Cel: %.1f°C",setPoint);lv_label_set_text(targetLabel,b);}
 if(heatLabel)lv_label_set_text(heatLabel,heatingDemand?"GRZANIE: ZADANE":"GRZANIE: OFF");
 if(adminLabel){snprintf(b,sizeof(b),"Histereza %.1f°C | %s\nTemperatura symulowana\nWyjscie przekaznika: NIEPRZYPISANE",hysteresis,heatingEnabled?"AUTO":"OFF");lv_label_set_text(adminLabel,b);}
}
static void onTarget(lv_event_t *e){
 setPoint=lv_slider_get_value(lv_event_get_target(e))/10.0f;
 thermostatPrefs.putFloat("target",setPoint);thermostatRefresh();
}
static void onSimulation(lv_event_t *e){
#if DHT_GPIO >= 0
 return; // Live sensor owns the temperature, simulation disabled.
#endif
 roomTemp=lv_slider_get_value(lv_event_get_target(e))/10.0f;thermostatRefresh();
}
static void onHeatEnable(lv_event_t *e){
 heatingEnabled=lv_obj_has_state(lv_event_get_target(e),LV_STATE_CHECKED);
 thermostatPrefs.putBool("enabled",heatingEnabled);thermostatRefresh();
}
static void onHysteresis(lv_event_t *e){
 hysteresis=lv_slider_get_value(lv_event_get_target(e))/10.0f;
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
 lv_obj_set_style_bg_color(tab,lv_color_hex(0x111827),0);
 lv_obj_set_style_pad_all(tab,8,0);
 lv_obj_set_flex_flow(tab,LV_FLEX_FLOW_COLUMN);
 lv_obj_set_flex_align(tab,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
 makeHeading(tab,"TERMOSTAT POKOJOWY");
 makeHeading(tab,"TEMP. W POMIESZCZENIU");
 roomLabel=makeHeading(tab,"20.0°C");
 lv_obj_set_style_text_font(roomLabel,&lv_font_montserrat_28,0);
 targetLabel=makeHeading(tab,"Cel: 21.0°C");
 targetSlider=lv_slider_create(tab);lv_obj_set_width(targetSlider,195);
 lv_slider_set_range(targetSlider,150,300);
 lv_slider_set_value(targetSlider,(int)(setPoint*10),LV_ANIM_OFF);
 lv_obj_add_event_cb(targetSlider,onTarget,LV_EVENT_VALUE_CHANGED,nullptr);
 heatLabel=makeHeading(tab,"GRZANIE: OFF");
 thermoNote=makeHeading(tab,
#if DHT_GPIO >= 0
 "Czujnik DHT GPIO2 (odczyt co 2.5s)"
#else
 "DEMO - temperatura symulowana"
#endif
 );
 lv_obj_set_style_text_color(thermoNote,lv_color_hex(0xFBBF24),0);
 thermostatRefresh();
}
static void makeAdmin(lv_obj_t *tab){
 lv_obj_set_flex_flow(tab,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_all(tab,8,0);
 label(tab,"Panel administratora");
 label(tab,"Termostat aktywny:");
 heatSwitch=lv_switch_create(tab);
 if(heatingEnabled)lv_obj_add_state(heatSwitch,LV_STATE_CHECKED);
 lv_obj_add_event_cb(heatSwitch,onHeatEnable,LV_EVENT_VALUE_CHANGED,nullptr);
 label(tab,"Histereza: 0.2 - 2.0 C");
 hysteresisSlider=lv_slider_create(tab);lv_obj_set_width(hysteresisSlider,195);
 lv_slider_set_range(hysteresisSlider,2,20);
 lv_slider_set_value(hysteresisSlider,(int)(hysteresis*10),LV_ANIM_OFF);
 lv_obj_add_event_cb(hysteresisSlider,onHysteresis,LV_EVENT_VALUE_CHANGED,nullptr);
 label(tab,"Symulacja temperatury pomieszczenia (10-35 C):");
 simulationSlider=lv_slider_create(tab);lv_obj_set_width(simulationSlider,195);
 lv_slider_set_range(simulationSlider,100,350);
 lv_slider_set_value(simulationSlider,(int)(roomTemp*10),LV_ANIM_OFF);
 lv_obj_add_event_cb(simulationSlider,onSimulation,LV_EVENT_VALUE_CHANGED,nullptr);
 adminLabel=label(tab,"");
 label(tab,"Ustawienia celu, histerezy i trybu sa zapisywane w NVS.");
 label(tab,"UWAGA: brak czujnika i wyjscia grzewczego. To panel demonstracyjny.");
 statusLabel=label(tab,"");
 batteryLabel=label(tab,"");
 thermostatRefresh();
}

#include "ui_dashboard.h"
static void setupUI(){uiSetupDashboard();}

void setup(){
 Serial.begin(115200);delay(500);
 thermostatPrefs.begin("thermostat",false);
 wifiPrefs.begin("wifi",false);
 setPoint=constrain(thermostatPrefs.getFloat("target",21.0f),15.0f,30.0f);
 hysteresis=constrain(thermostatPrefs.getFloat("hyst",0.5f),0.2f,2.0f);
 heatingEnabled=thermostatPrefs.getBool("enabled",true);
 Serial.println("\nES3C28P LVGL Peripheral Lab");
#if DHT_GPIO >= 0
 dht.begin();
#endif
 pinMode(BOOT_GPIO,INPUT_PULLUP);pinMode(AMP_EN,OUTPUT);digitalWrite(AMP_EN,HIGH);
 pixel.begin();pixel.clear();pixel.show();
 Wire.begin(TOUCH_SDA,TOUCH_SCL,400000);
 pinMode(TOUCH_RST,OUTPUT);digitalWrite(TOUCH_RST,LOW);delay(15);digitalWrite(TOUCH_RST,HIGH);delay(300);
 pinMode(TOUCH_IRQ,INPUT_PULLUP);
 touchOk=i2cPing(TOUCH_ADDR);
 if(touchOk){Wire.beginTransmission(TOUCH_ADDR);Wire.write(0xA4);Wire.write(0x00);Wire.endTransmission();}
 codecBegin();
 lcd.init();lcd.setRotation(0);lcd.setBrightness(160);lcd.fillScreen(TFT_BLACK);
 // Shared with SDIO pins only, never configure them as GPIO elsewhere.
 SD_MMC.setPins(SD_CLK,SD_CMD,SD_D0,SD_D1,SD_D2,SD_D3);
 sdOk=SD_MMC.begin("/sdcard",false,false,20000,5);
 WiFi.mode(WIFI_OFF);
 lv_init();
 drawmem=(lv_color_t*)heap_caps_malloc(LCD_W*24*sizeof(lv_color_t),MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
 if(!drawmem){Serial.println("[fatal] No DMA draw buffer");while(true)delay(1000);}
 lv_disp_draw_buf_init(&drawbuf,drawmem,nullptr,LCD_W*24);
 static lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=LCD_W;dd.ver_res=LCD_H;dd.flush_cb=flushCb;dd.draw_buf=&drawbuf;lv_disp_drv_register(&dd);
 static lv_indev_drv_t id;lv_indev_drv_init(&id);id.type=LV_INDEV_TYPE_POINTER;id.read_cb=inputCb;lv_indev_drv_register(&id);
 const esp_timer_create_args_t args={.callback=&tick,.arg=nullptr,.dispatch_method=ESP_TIMER_TASK,.name="lv_tick",.skip_unhandled_events=true};
 esp_timer_handle_t th;esp_timer_create(&args,&th);esp_timer_start_periodic(th,1000);
 setupUI();
 Serial.printf("Touch=%d Codec=%d SD=%d PSRAM=%d\n",touchOk,codecOk,sdOk,psramFound());
}
void loop(){
#if DHT_GPIO >= 0
 if(millis()-lastDhtRead>=2500){lastDhtRead=millis();float t=dht.readTemperature();dhtOk=isfinite(t);if(dhtOk){roomTemp=t;thermostatRefresh();}else{heatingDemand=false;thermostatRefresh();}}
#endif
 lv_timer_handler();if(millis()-lastUpdate>500){lastUpdate=millis();statusUpdate();uiPowerRefresh();
 int raw=analogRead(BAT_ADC);float voltage=(raw/4095.0f)*3.3f*BATTERY_DIVIDER_RATIO;
 char b[96];snprintf(b,sizeof(b),"BAT ADC: %d (szac. %.2f V*)",raw,voltage);
 if(batteryLabel)lv_label_set_text(batteryLabel,b);
 }delay(5);}
