#pragma once
// Modular screens for LVGL 8.x. Included after common drivers/helpers in main.cpp.
static lv_obj_t *wifiSsidInput=nullptr,*wifiPassInput=nullptr,*wifiKeyboard=nullptr;
static lv_obj_t *wifiConnectionLabel=nullptr,*powerInfoLabel=nullptr,*touchInfoLabel=nullptr;
static lv_obj_t *audioInfoLabel=nullptr;


static void uiTitle(lv_obj_t *parent,const char *heading,const char *sub){
 lv_obj_t *h=lv_label_create(parent);lv_label_set_text(h,heading);
 lv_obj_set_width(h,210);lv_obj_set_style_text_font(h,&lv_font_montserrat_16,0);
 lv_obj_set_style_text_color(h,lv_color_hex(0xF8FAFC),0);
 lv_obj_t *s=lv_label_create(parent);lv_label_set_text(s,sub);
 lv_obj_set_width(s,210);lv_label_set_long_mode(s,LV_LABEL_LONG_WRAP);
 lv_obj_set_style_text_color(s,lv_color_hex(0xE2E8F0),0);
}
static void uiTab(lv_obj_t *tab){
 lv_obj_set_style_bg_color(tab,lv_color_hex(0x0B1018),0);
 lv_obj_set_style_bg_opa(tab,LV_OPA_COVER,0);
 lv_obj_set_style_pad_all(tab,10,0);
 lv_obj_set_flex_flow(tab,LV_FLEX_FLOW_COLUMN);
 lv_obj_set_flex_align(tab,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
 lv_obj_set_style_pad_row(tab,9,0);
 lv_obj_set_style_text_font(tab,&lv_font_montserrat_16,0);
 lv_obj_set_style_text_color(tab,lv_color_hex(0xF1F5F9),0);
}
static void uiOnKeyboard(lv_event_t *e){
 lv_obj_t *target=lv_event_get_target_obj(e);
 if(lv_event_get_code(e)==LV_EVENT_FOCUSED){
   lv_keyboard_set_textarea(wifiKeyboard,target);
   lv_obj_move_foreground(wifiKeyboard);
   lv_obj_scroll_to_view(target,LV_ANIM_OFF);
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
static void uiWifiReveal(lv_event_t *){
 bool old=lv_textarea_get_password_mode(wifiPassInput);
 if(!old && !lv_textarea_get_text(wifiPassInput)[0]){
  String stored=wifiPrefs.getString("password","");
  lv_textarea_set_text(wifiPassInput,stored.c_str());
 }
 lv_textarea_set_password_mode(wifiPassInput,!old);
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
 button(tab,"Pokaz / ukryj haslo",uiWifiReveal);
 button(tab,"Zapisz i polacz",uiWifiSave);
 button(tab,"Polacz ponownie",uiWifiConnect);
 button(tab,"Usun zapisane Wi-Fi",uiWifiForget);
 wifiConnectionLabel=label(tab,"Status Wi-Fi");
 button(tab,"Skan Wi-Fi",onWifi);wifiLabel=label(tab,"");
 button(tab,"Skan Bluetooth LE",onBle);bleLabel=label(tab,"");
 label(tab,"ESP32-S3: Wi-Fi 2.4 GHz i BLE. Thread/Zigbee wymagaja zewnetrznego ESP32-C5/C6/H2 (RCP).");
 label(tab,"RCP: UART GPIO43 RX / GPIO44 TX wymaga przeniesienia logow UART0 na USB CDC i osobnego firmware.");
 wifiKeyboard=lv_keyboard_create(lv_screen_active());
 lv_obj_set_size(wifiKeyboard,LCD_W,218);lv_obj_align(wifiKeyboard,LV_ALIGN_BOTTOM_MID,0,0);
 lv_obj_add_flag(wifiKeyboard,LV_OBJ_FLAG_HIDDEN);
 lv_obj_set_style_pad_all(wifiKeyboard,3,0);
 lv_obj_set_style_pad_row(wifiKeyboard,7,0);
 lv_obj_set_style_pad_column(wifiKeyboard,2,0);
 lv_obj_set_style_text_font(wifiKeyboard,&lv_font_montserrat_16,0);
 lv_obj_set_style_bg_color(wifiKeyboard,lv_color_hex(0x101827),0);
 lv_obj_set_style_bg_color(wifiKeyboard,lv_color_hex(0x33465B),LV_PART_ITEMS);
 lv_obj_set_style_text_color(wifiKeyboard,lv_color_hex(0xFFFFFF),LV_PART_ITEMS);
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
static lv_obj_t *cameraSelector=nullptr,*cameraUserInput=nullptr,*cameraPassInput=nullptr;
static Preferences cameraPrefs;
static uint8_t cameraIndex=0;
static void uiCameraSelect(lv_event_t *){
 cameraIndex=(uint8_t)lv_dropdown_get_selected(cameraSelector);
 char msg[80];snprintf(msg,sizeof(msg),"Kamera %u: 192.168.68.%u",cameraIndex+1,51+cameraIndex);
 if(cameraStatusObject)lv_label_set_text(cameraStatusObject,msg);
}
static void uiCameraReveal(lv_event_t *){
 bool hidden=lv_textarea_get_password_mode(cameraPassInput);
 if(hidden && strlen(lv_textarea_get_text(cameraPassInput))==0){
  String password=cameraPrefs.getString("pass","");
  lv_textarea_set_text(cameraPassInput,password.c_str());
 }
 lv_textarea_set_password_mode(cameraPassInput,!hidden);
}
static void uiCameraCredentialsSave(lv_event_t *){
 String user=lv_textarea_get_text(cameraUserInput);
 String pass=lv_textarea_get_text(cameraPassInput);
 if(user.length()>32||pass.length()>64){
  lv_label_set_text(cameraStatusObject,"Zbyt dlugie dane dostepu");return;
 }
 cameraPrefs.putString("user",user);
 cameraPrefs.putString("pass",pass);
 lv_textarea_set_text(cameraPassInput,"");
 lv_label_set_text(cameraStatusObject,"Dane dostepu zapisane lokalnie (NVS)");
}
static void uiCameraStart(lv_event_t *){
 cameraIndex=(uint8_t)lv_dropdown_get_selected(cameraSelector);
 char url[125];
 snprintf(url,sizeof(url),"%s",cameraUrls[cameraIndex].c_str());
 String user=cameraPrefs.getString("user","");
 String pass=cameraPrefs.getString("pass","");
 if(!cameraStart(url,user.c_str(),pass.c_str())&&cameraStatusObject)
  lv_label_set_text(cameraStatusObject,"Brak polaczenia: sprawdz Wi-Fi, MJPEG i login");
}
static void uiCameraStop(lv_event_t *){cameraStop();}
static void uiMakeCamera(lv_obj_t *tab){
 uiTab(tab);uiTitle(tab,"KAMERY IP","Wybierz strumien MJPEG");
 cameraPrefs.begin("ipcamera",false);
 cameraSelector=lv_dropdown_create(tab);
 lv_obj_set_width(cameraSelector,205);
 lv_dropdown_set_options(cameraSelector,
  "Kamera 1  /  .51\nKamera 2  /  .52\nKamera 3  /  .53\nKamera 4  /  .54");
 lv_obj_add_event_cb(cameraSelector,uiCameraSelect,LV_EVENT_VALUE_CHANGED,nullptr);
 cameraUserInput=lv_textarea_create(tab);lv_obj_set_width(cameraUserInput,205);
 lv_textarea_set_one_line(cameraUserInput,true);
 lv_textarea_set_placeholder_text(cameraUserInput,"Login kamer");
 String username=cameraPrefs.getString("user","");
 lv_textarea_set_text(cameraUserInput,username.c_str());
 cameraPassInput=lv_textarea_create(tab);lv_obj_set_width(cameraPassInput,205);
 lv_textarea_set_one_line(cameraPassInput,true);
 lv_textarea_set_password_mode(cameraPassInput,true);
 lv_textarea_set_placeholder_text(cameraPassInput,"Haslo (nie wyswietlane)");
 lv_obj_add_event_cb(cameraUserInput,uiOnKeyboard,LV_EVENT_FOCUSED,nullptr);
 lv_obj_add_event_cb(cameraUserInput,uiOnKeyboard,LV_EVENT_DEFOCUSED,nullptr);
 lv_obj_add_event_cb(cameraPassInput,uiOnKeyboard,LV_EVENT_FOCUSED,nullptr);
 lv_obj_add_event_cb(cameraPassInput,uiOnKeyboard,LV_EVENT_DEFOCUSED,nullptr);
 button(tab,"Pokaz / ukryj haslo",uiCameraReveal);
 button(tab,"Zapisz dane kamer",uiCameraCredentialsSave);
 button(tab,"Uruchom wybrana",uiCameraStart);
 button(tab,"Zatrzymaj",uiCameraStop);
 cameraStatusObject=label(tab,"192.168.68.51-54 / MJPEG subtype=1");
 cameraImageObject=lv_image_create(tab);
 label(tab,"Jeden podglad na raz. JPEG maks. 240x180.");
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
// Blade-style navigation: four readable actions rather than eight squeezed tabs.
static lv_obj_t *bladePages[10]={};
static lv_obj_t *bladeHint=nullptr;
static uint8_t bladePage=0;
static void bladeShow(uint8_t page){
 if(page>=10)return;
 bladePage=page;
 for(uint8_t i=0;i<10;i++){
  if(bladePages[i]){
   if(i==page)lv_obj_remove_flag(bladePages[i],LV_OBJ_FLAG_HIDDEN);
   else lv_obj_add_flag(bladePages[i],LV_OBJ_FLAG_HIDDEN);
  }
 }
 static const char *pageNames[]={"DOM","RADIO","AUDIO","MENU","EKRAN","BATERIA","RGB","ADMIN","KAMERA","BRAMA"};
 if(bladeHint){
  char hint[88];
  const uint8_t prev=(page+9)%10,next=(page+1)%10;
  snprintf(hint,sizeof(hint),"< %s  |  %s >",pageNames[prev],pageNames[next]);
  lv_label_set_text(bladeHint,hint);
 }
}
static void bladeNavigate(int8_t direction){
 bladeShow((uint8_t)((bladePage+10+(direction>0?1:-1))%10));
}
static void bladeNavEvent(lv_event_t *e){bladeShow((uint8_t)(uintptr_t)lv_event_get_user_data(e));}
static lv_obj_t *bladeButton(lv_obj_t *parent,const char *txt,uint8_t page){
 lv_obj_t *b=lv_button_create(parent);
 lv_obj_set_style_radius(b,10,0);
 lv_obj_set_style_border_width(b,0,0);
 lv_obj_set_style_shadow_width(b,0,0);
 lv_obj_add_event_cb(b,bladeNavEvent,LV_EVENT_CLICKED,(void*)(uintptr_t)page);
 lv_obj_t *t=lv_label_create(b);lv_label_set_text(t,txt);
 lv_obj_set_style_text_font(t,&lv_font_montserrat_14,0);
 lv_obj_center(t);
 return b;
}
static lv_obj_t *bladePageCreate(lv_obj_t *root){
 lv_obj_t *p=lv_obj_create(root);
 lv_obj_set_pos(p,0,0);lv_obj_set_size(p,LCD_W,LCD_H-35);
 lv_obj_set_style_bg_color(p,lv_color_hex(0x0B1018),0);
 lv_obj_set_style_border_width(p,0,0);
 lv_obj_set_style_radius(p,0,0);
 lv_obj_set_style_pad_all(p,0,0);
 lv_obj_set_scroll_dir(p,LV_DIR_VER);
 return p;
}
static void bladeSection(lv_obj_t *page){
 lv_obj_t *back=bladeButton(page,"<  MENU",3);
 lv_obj_set_size(back,185,39);
 lv_obj_set_style_bg_color(back,lv_color_hex(0x263B50),0);
}
static lv_obj_t *gateUiStatus=nullptr;
static void gateUiAction(lv_event_t *e){
 uint8_t id=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
 String result;bool ok=gateRequest(id,result);
 if(gateUiStatus)lv_label_set_text(gateUiStatus,(String(ok?"Wyslano: ":"Blad: ")+result).c_str());
}
static void gateUiConfirm(lv_event_t *e){
 uint8_t id=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
 lv_obj_t *modal=lv_obj_create(lv_screen_active());
 lv_obj_set_size(modal,218,146);lv_obj_center(modal);
 lv_obj_set_style_bg_color(modal,lv_color_hex(0x182536),0);
 lv_obj_set_style_text_color(modal,lv_color_hex(0xFFFFFF),0);
 lv_obj_set_style_border_color(modal,lv_color_hex(0x5DA7E1),0);
 lv_obj_t *question=lv_label_create(modal);
 lv_label_set_text_fmt(question,"Potwierdz: %s?",gateNames[id]);
 lv_obj_align(question,LV_ALIGN_TOP_MID,0,4);
 lv_obj_t *yes=lv_button_create(modal);lv_obj_set_size(yes,80,46);
 lv_obj_align(yes,LV_ALIGN_BOTTOM_LEFT,0,0);
 lv_obj_t *yt=lv_label_create(yes);lv_label_set_text(yt,"OTWORZ");lv_obj_center(yt);
 lv_obj_add_event_cb(yes,gateUiAction,LV_EVENT_CLICKED,(void*)(uintptr_t)id);
 lv_obj_add_event_cb(yes,[](lv_event_t *e){lv_obj_delete((lv_obj_t*)lv_event_get_user_data(e));},LV_EVENT_CLICKED,modal);
 lv_obj_t *no=lv_button_create(modal);lv_obj_set_size(no,80,46);
 lv_obj_align(no,LV_ALIGN_BOTTOM_RIGHT,0,0);
 lv_obj_t *nt=lv_label_create(no);lv_label_set_text(nt,"ANULUJ");lv_obj_center(nt);
 lv_obj_add_event_cb(no,[](lv_event_t *e){lv_obj_delete((lv_obj_t*)lv_event_get_user_data(e));},LV_EVENT_CLICKED,modal);
}
static void uiMakeGate(lv_obj_t *tab){
 uiTab(tab);uiTitle(tab,"BRAMA","Sterowanie przez Wi-Fi (HTTP GET)");
 static const char *titles[]={"OTWORZ 1/2","OTWORZ CALA","OTWORZ FURTKE"};
 for(uint8_t i=0;i<3;i++){
  lv_obj_t *b=button(tab,titles[i],gateUiConfirm);
  lv_obj_set_height(b,51);
  lv_obj_set_style_bg_color(b,lv_color_hex(i==2?0x245D56:0x23517D),0);
  // button() attaches an earlier callback without user data; replace it.
  lv_obj_remove_event_cb(b,gateUiConfirm);
  lv_obj_add_event_cb(b,gateUiConfirm,LV_EVENT_CLICKED,(void*)(uintptr_t)i);
 }
 gateUiStatus=label(tab,"Konfiguracja URL: panel WWW, adres IP urzadzenia.");
 label(tab,"Przyciski aktywne po skonfigurowaniu osobnych URL GET.");
}
static void uiSetupDashboard(){
 lv_obj_t *root=lv_screen_active();
 lv_obj_set_style_bg_color(root,lv_color_hex(0x0B1018),0);
 lv_obj_set_style_border_width(root,0,0);
 lv_obj_set_style_pad_all(root,0,0);
 for(uint8_t i=0;i<10;i++)bladePages[i]=bladePageCreate(root);
 uiTab(bladePages[0]);makeThermostat(bladePages[0]);
 uiMakeWifi(bladePages[1]);uiMakeAudio(bladePages[2]);
 uiTab(bladePages[3]);uiTitle(bladePages[3],"MENU","Ustawienia i narzedzia");
 static const char *names[]={"EKRAN I DOTYK","BATERIA","RGB","ADMIN","KAMERA","BRAMA"};
 for(uint8_t i=0;i<5;i++){
  lv_obj_t *b=bladeButton(bladePages[3],names[i],(uint8_t)(i+4));
  lv_obj_set_size(b,205,40);
  lv_obj_set_style_bg_color(b,lv_color_hex(0x1E3347),0);
 }
 bladeSection(bladePages[4]);uiMakeDisplay(bladePages[4]);
 bladeSection(bladePages[5]);uiMakePower(bladePages[5]);
 bladeSection(bladePages[6]);uiMakeRgb(bladePages[6]);
 bladeSection(bladePages[7]);uiTab(bladePages[7]);makeAdmin(bladePages[7]);
 bladeSection(bladePages[8]);uiMakeCamera(bladePages[8]);
 uiMakeGate(bladePages[9]);
 // Small directional preview only; no tab bar, no overlays on primary cards.
 bladeHint=lv_label_create(root);
 lv_obj_set_size(bladeHint,LCD_W-12,27);
 lv_obj_align(bladeHint,LV_ALIGN_BOTTOM_MID,0,-3);
 lv_obj_set_style_text_align(bladeHint,LV_TEXT_ALIGN_CENTER,0);
 lv_obj_set_style_text_color(bladeHint,lv_color_hex(0xE9F2FF),0);
 lv_obj_set_style_text_font(bladeHint,&lv_font_montserrat_14,0);
 lv_obj_set_style_bg_color(bladeHint,lv_color_hex(0x1A2A3B),0);
 lv_obj_set_style_bg_opa(bladeHint,LV_OPA_COVER,0);
 lv_obj_set_style_radius(bladeHint,7,0);
 lv_obj_set_style_pad_top(bladeHint,4,0);
 bladeShow(0);
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
