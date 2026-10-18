#pragma once
// Modular screens for LVGL 8.x. Included after common drivers/helpers in main.cpp.
static lv_obj_t *wifiSsidInput=nullptr,*wifiPassInput=nullptr,*wifiKeyboard=nullptr;
static lv_obj_t *wifiConnectionLabel=nullptr,*powerInfoLabel=nullptr,*touchInfoLabel=nullptr;
static lv_obj_t *audioInfoLabel=nullptr;
static Preferences wifiPrefs;

static void uiTitle(lv_obj_t *parent,const char *heading,const char *sub){
 lv_obj_t *h=lv_label_create(parent);lv_label_set_text(h,heading);
 lv_obj_set_width(h,210);lv_obj_set_style_text_font(h,&lv_font_montserrat_28,0);
 lv_obj_set_style_text_color(h,lv_color_hex(0xF8FAFC),0);
 lv_obj_t *s=lv_label_create(parent);lv_label_set_text(s,sub);
 lv_obj_set_width(s,210);lv_label_set_long_mode(s,LV_LABEL_LONG_WRAP);
 lv_obj_set_style_text_color(s,lv_color_hex(0x94A3B8),0);
}
static void uiTab(lv_obj_t *tab){
 lv_obj_set_style_bg_color(tab,lv_color_hex(0x0B1220),0);
 lv_obj_set_style_pad_all(tab,10,0);
 lv_obj_set_flex_flow(tab,LV_FLEX_FLOW_COLUMN);
 lv_obj_set_flex_align(tab,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
 lv_obj_set_style_pad_row(tab,11,0);
}
static void uiOnKeyboard(lv_event_t *e){
 lv_obj_t *target=lv_event_get_target(e);
 if(lv_event_get_code(e)==LV_EVENT_FOCUSED){
   lv_keyboard_set_textarea(wifiKeyboard,target);
   lv_obj_clear_flag(wifiKeyboard,LV_OBJ_FLAG_HIDDEN);
 } else if(lv_event_get_code(e)==LV_EVENT_DEFOCUSED){
   lv_obj_add_flag(wifiKeyboard,LV_OBJ_FLAG_HIDDEN);
 }
}
static void uiOnKeyboardDone(lv_event_t *e){
 if(lv_event_get_code(e)==LV_EVENT_READY||lv_event_get_code(e)==LV_EVENT_CANCEL){
  lv_obj_add_flag(wifiKeyboard,LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_state(wifiSsidInput,LV_STATE_FOCUSED);
  lv_obj_clear_state(wifiPassInput,LV_STATE_FOCUSED);
 }
}
static void uiWifiSave(lv_event_t *){
 const char *ssid=lv_textarea_get_text(wifiSsidInput);
 const char *pass=lv_textarea_get_text(wifiPassInput);
 if(!ssid||strlen(ssid)==0||strlen(ssid)>32||strlen(pass)>63){
  lv_label_set_text(wifiConnectionLabel,"Nieprawidlowa dlugosc SSID/hasla");return;
 }
 wifiPrefs.putString("ssid",ssid);
 wifiPrefs.putString("password",pass);
 lv_label_set_text(wifiConnectionLabel,"Dane zapisane w NVS; laczenie...");
 WiFi.mode(WIFI_STA);WiFi.begin(ssid,pass);
 lv_obj_add_flag(wifiKeyboard,LV_OBJ_FLAG_HIDDEN);
}
static void uiWifiConnect(lv_event_t *){
 String ssid=wifiPrefs.getString("ssid","");
 String pass=wifiPrefs.getString("password","");
 if(ssid.isEmpty()){lv_label_set_text(wifiConnectionLabel,"Najpierw zapisz SSID");return;}
 WiFi.mode(WIFI_STA);WiFi.begin(ssid.c_str(),pass.c_str());
 lv_label_set_text(wifiConnectionLabel,"Laczenie z zapisana siecia...");
}
static void uiWifiForget(lv_event_t *){
 WiFi.disconnect(true,true);wifiPrefs.remove("ssid");wifiPrefs.remove("password");
 lv_textarea_set_text(wifiSsidInput,"");lv_textarea_set_text(wifiPassInput,"");
 lv_label_set_text(wifiConnectionLabel,"Usunieto dane Wi-Fi z NVS");
}
static void uiTouchEvent(lv_event_t *e){
 if(lv_event_get_code(e)!=LV_EVENT_PRESSED && lv_event_get_code(e)!=LV_EVENT_PRESSING)return;
 lv_point_t p;lv_indev_get_point(lv_indev_get_act(),&p);
 char msg[64];snprintf(msg,sizeof(msg),"Dotyk X=%d Y=%d",(int)p.x,(int)p.y);
 if(touchInfoLabel)lv_label_set_text(touchInfoLabel,msg);
}
static void uiAudioRecordNotice(lv_event_t *){
 if(audioIsRecording)audioFinishRecord();else if(!audioStartRecord())snprintf(audioMessage,sizeof(audioMessage),"Recording failed (SD/codec/I2S)");
 lv_label_set_text(audioInfoLabel,audioMessage);
}
static lv_obj_t *audioFileDropdown=nullptr;
static String audioPaths[24];static int audioPathCount=0;
static void uiAudioRefreshFiles(lv_event_t *){
 audioPathCount=0;String choices="No compatible files";
 if(sdOk){
  File dir=SD_MMC.open("/");File item;
  if(dir&&dir.isDirectory()){
   choices="";
   while((item=dir.openNextFile())&&audioPathCount<24){
    String name=item.name();name.toLowerCase();
    if(!item.isDirectory()&&(name.endsWith(".mp3")||name.endsWith(".wav"))){
     audioPaths[audioPathCount]=String(item.name());
     if(!audioPaths[audioPathCount].startsWith("/"))audioPaths[audioPathCount]="/"+audioPaths[audioPathCount];
     if(audioPathCount)choices+="\n";
     choices+=audioPaths[audioPathCount];audioPathCount++;
    }
    item.close();
   }
   dir.close();
  }
 }
 if(!audioPathCount)choices="No compatible files";
 if(audioFileDropdown)lv_dropdown_set_options(audioFileDropdown,choices.c_str());
}
static void uiAudioPlayNotice(lv_event_t *){
 uint16_t index=audioFileDropdown?lv_dropdown_get_selected(audioFileDropdown):0;
 if(index>=audioPathCount||!audioPlayFile(audioPaths[index].c_str()))snprintf(audioMessage,sizeof(audioMessage),"Select playable WAV/MP3 file");
 lv_label_set_text(audioInfoLabel,audioMessage);
}
static void uiMakeWifi(lv_obj_t *tab){
 uiTab(tab);uiTitle(tab,"RADIO","Wi-Fi / BLE / Thread RCP");
 wifiSsidInput=lv_textarea_create(tab);lv_obj_set_width(wifiSsidInput,205);
 lv_textarea_set_one_line(wifiSsidInput,true);lv_textarea_set_placeholder_text(wifiSsidInput,"SSID");
 wifiPassInput=lv_textarea_create(tab);lv_obj_set_width(wifiPassInput,205);
 lv_textarea_set_one_line(wifiPassInput,true);lv_textarea_set_password_mode(wifiPassInput,true);
 lv_textarea_set_placeholder_text(wifiPassInput,"Haslo Wi-Fi");
 lv_obj_add_event_cb(wifiSsidInput,uiOnKeyboard,LV_EVENT_FOCUSED,nullptr);
 lv_obj_add_event_cb(wifiSsidInput,uiOnKeyboard,LV_EVENT_DEFOCUSED,nullptr);
 lv_obj_add_event_cb(wifiPassInput,uiOnKeyboard,LV_EVENT_FOCUSED,nullptr);
 lv_obj_add_event_cb(wifiPassInput,uiOnKeyboard,LV_EVENT_DEFOCUSED,nullptr);
 String ssid=wifiPrefs.getString("ssid","");
 lv_textarea_set_text(wifiSsidInput,ssid.c_str());
 button(tab,"Zapisz i polacz",uiWifiSave);
 button(tab,"Polacz ponownie",uiWifiConnect);
 button(tab,"Usun zapisane Wi-Fi",uiWifiForget);
 wifiConnectionLabel=label(tab,"Status Wi-Fi");
 button(tab,"Skan Wi-Fi",onWifi);wifiLabel=label(tab,"");
 button(tab,"Skan Bluetooth LE",onBle);bleLabel=label(tab,"");
 label(tab,"ESP32-S3: Wi-Fi 2.4 GHz i BLE. Thread/Zigbee wymagaja zewnetrznego ESP32-C5/C6/H2 (RCP).");
 label(tab,"RCP: UART GPIO43 RX / GPIO44 TX wymaga przeniesienia logow UART0 na USB CDC i osobnego firmware.");
 wifiKeyboard=lv_keyboard_create(lv_screen_active());
 lv_obj_set_size(wifiKeyboard,LCD_W,150);lv_obj_align(wifiKeyboard,LV_ALIGN_BOTTOM_MID,0,0);
 lv_obj_add_flag(wifiKeyboard,LV_OBJ_FLAG_HIDDEN);
 lv_obj_add_event_cb(wifiKeyboard,uiOnKeyboardDone,LV_EVENT_ALL,nullptr);
}
static void uiMakeAudio(lv_obj_t *tab){
 uiTab(tab);uiTitle(tab,"AUDIO","ES8311 / microSD");
 button(tab,"Test 440 Hz",onTone);
 button(tab,"Pokaz pliki SD",onSd);
 sdLabel=label(tab,"Oczekiwanie na karte microSD");
 audioFileDropdown=lv_dropdown_create(tab);lv_obj_set_width(audioFileDropdown,205);
 button(tab,"Odswiez pliki audio",uiAudioRefreshFiles);
 button(tab,"Odtwarzaj WAV / MP3",uiAudioPlayNotice);
 button(tab,"Nagraj WAV na SD",uiAudioRecordNotice);
 audioInfoLabel=label(tab,"SD: wybierz plik lub nagraj /record.wav (10s)");
 uiAudioRefreshFiles(nullptr);
}
static lv_obj_t *cameraUrlInput=nullptr;
static void uiCameraStart(lv_event_t *){
 if(!cameraStart(lv_textarea_get_text(cameraUrlInput))&&cameraStatusObject)
  lv_label_set_text(cameraStatusObject,"Connection failed: check Wi-Fi and HTTP URL");
}
static void uiCameraStop(lv_event_t *){cameraStop();}
static void uiMakeCamera(lv_obj_t *tab){
 uiTab(tab);uiTitle(tab,"CCTV","HTTP MJPEG / JPEG (240x180)");
 cameraUrlInput=lv_textarea_create(tab);lv_obj_set_width(cameraUrlInput,205);
 lv_textarea_set_one_line(cameraUrlInput,true);
 lv_textarea_set_placeholder_text(cameraUrlInput,"http://camera/mjpeg");
 lv_obj_add_event_cb(cameraUrlInput,uiOnKeyboard,LV_EVENT_FOCUSED,nullptr);
 lv_obj_add_event_cb(cameraUrlInput,uiOnKeyboard,LV_EVENT_DEFOCUSED,nullptr);
 button(tab,"Start stream",uiCameraStart);
 button(tab,"Stop stream",uiCameraStop);
 cameraStatusObject=label(tab,"Only plain HTTP; JPEG <=240x180");
 cameraImageObject=lv_image_create(tab);
}
static void uiMakeDisplay(lv_obj_t *tab){
 uiTab(tab);uiTitle(tab,"DISPLAY","Ekran i dotyk");
 label(tab,"Jasnosc podswietlenia:");
 lv_obj_t *s=lv_slider_create(tab);lv_obj_set_width(s,195);
 lv_slider_set_range(s,15,255);lv_slider_set_value(s,160,LV_ANIM_OFF);
 lv_obj_add_event_cb(s,onBrightness,LV_EVENT_VALUE_CHANGED,nullptr);
 touchInfoLabel=label(tab,"Dotknij pola ponizej");
 lv_obj_t *pad=lv_button_create(tab);lv_obj_set_size(pad,200,115);
 lv_obj_set_style_bg_color(pad,lv_color_hex(0x1D4ED8),0);
 lv_obj_add_event_cb(pad,uiTouchEvent,LV_EVENT_PRESSED,nullptr);
 lv_obj_add_event_cb(pad,uiTouchEvent,LV_EVENT_PRESSING,nullptr);
 lv_obj_t *txt=lv_label_create(pad);lv_label_set_text(txt,"TEST DOTYKU");lv_obj_center(txt);
 label(tab,"ILI9341 240x320 / FT6336 I2C");
}
static void uiMakePower(lv_obj_t *tab){
 uiTab(tab);uiTitle(tab,"POWER","Bateria i diagnostyka");
 powerInfoLabel=label(tab,"Pomiar...");
 label(tab,"Napiecie wymaga kalibracji dzielnika BAT_ADC. Brak potwierdzonego ukladu fuel-gauge.");
 label(tab,"Stan ladowania USB i bateria %: niedostepne bez potwierdzonego ukladu pomiarowego.");
 button(tab,"Skan magistrali I2C -> USB",onI2c);
 statusLabel=label(tab,"");
}
static void uiMakeRgb(lv_obj_t *tab){
 uiTab(tab);uiTitle(tab,"WS2812","Sterowanie dioda RGB");
 button(tab,"Zmien kolor",onLed);
 label(tab,"Sekwencja: off / czerwony / zielony / niebieski / bialy.");
 label(tab,"Wbudowany WS2812 na GPIO42.");
}
static void uiSetupDashboard(){
 lv_obj_t *root=lv_screen_active();
 lv_obj_set_style_bg_color(root,lv_color_hex(0x0B1220),0);
 lv_obj_t *tabs=lv_tabview_create(root);
 lv_obj_set_size(tabs,LCD_W,LCD_H);
 lv_tabview_set_tab_bar_position(tabs,LV_DIR_BOTTOM);
 lv_tabview_set_tab_bar_size(tabs,42);
 lv_obj_set_style_bg_color(lv_tabview_get_tab_bar(tabs),lv_color_hex(0x101A2A),0);
 lv_obj_set_style_border_width(lv_tabview_get_tab_bar(tabs),0,0);
 lv_obj_set_style_text_color(lv_tabview_get_tab_bar(tabs),lv_color_hex(0xCBD5E1),0);
 lv_obj_t *thermo=lv_tabview_add_tab(tabs,"DOM");
 lv_obj_t *radio=lv_tabview_add_tab(tabs,"RADIO");
 lv_obj_t *audio=lv_tabview_add_tab(tabs,"AUDIO");
 lv_obj_t *screen=lv_tabview_add_tab(tabs,"LCD");
 lv_obj_t *power=lv_tabview_add_tab(tabs,"BAT");
 lv_obj_t *rgb=lv_tabview_add_tab(tabs,"RGB");
 lv_obj_t *admin=lv_tabview_add_tab(tabs,"ADMIN");
 lv_obj_t *camera=lv_tabview_add_tab(tabs,"CCTV");
 uiTab(thermo);makeThermostat(thermo);
 uiMakeWifi(radio);uiMakeAudio(audio);uiMakeDisplay(screen);uiMakePower(power);uiMakeRgb(rgb);
 uiTab(admin);makeAdmin(admin);uiMakeCamera(camera);
 statusUpdate();
}
static void uiPowerRefresh(){
 if(!powerInfoLabel)return;
 int raw=analogRead(BAT_ADC);
 float v=analogReadMilliVolts(BAT_ADC)*0.001f*BATTERY_DIVIDER_RATIO;
 char b[180];snprintf(b,sizeof(b),
 "ADC GPIO9: %d\nNapiecie szacunkowe: %.2f V*\nPSRAM wolne: %lu KB\nWi-Fi: %s\nSD: %s",
 raw,v,(unsigned long)(ESP.getFreePsram()/1024),
 WiFi.status()==WL_CONNECTED?"polaczone":"offline",sdOk?"OK":"BRAK");
 lv_label_set_text(powerInfoLabel,b);
 if(wifiConnectionLabel){
  if(WiFi.status()==WL_CONNECTED){
   String msg="Polaczono: "+WiFi.localIP().toString();
   lv_label_set_text(wifiConnectionLabel,msg.c_str());
  }
 }
}
