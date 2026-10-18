#pragma once
#include <WebServer.h>
#include <HTTPClient.h>
#include <esp_system.h>

// All URLs are user-provisioned; no default actuation endpoints.
static Preferences gatePrefs;
static WebServer gateWeb(80);
static String gateUrls[3], cameraUrls[4];
static String cameraBasicUser[4],cameraBasicPass[4];
static String gateWebPassword;
static String thermostatDeviceName;
static bool gateWebReady=false;
static bool gateBusy=false;
static uint32_t gateLastAction=0;
static const char *gateNames[3]={"Brama 1/2","Brama cala","Furtka"};
static String gateEsc(const String &s){
 String out;out.reserve(s.length()+16);
 for(size_t i=0;i<s.length();++i){
  char c=s[i];
  if(c=='&')out+="&amp;";else if(c=='<')out+="&lt;";else if(c=='>')out+="&gt;";
  else if(c=='"')out+="&quot;";else if(c=='\'')out+="&#39;";else out+=c;
 }
 return out;
}
static bool gateIsLanUrl(const String &url){
 if(!url.startsWith("http://")||url.length()>240)return false;
 int end=url.indexOf('/',7);
 String authority=url.substring(7,end<0?url.length():end);
 if(authority.length()==0||authority.indexOf('@')>=0||authority.indexOf('#')>=0)return false;
 String host=authority;int colon=host.indexOf(':');if(colon>=0)host=host.substring(0,colon);
 IPAddress ip;
 if(!ip.fromString(host))return false;
 return ip[0]==10||(ip[0]==192&&ip[1]==168)||(ip[0]==172&&ip[1]>=16&&ip[1]<=31);
}
static bool gateCameraUrlAllowed(const String &raw){
 String clean,u,p;
 return cameraResolveUrl(raw,clean,u,p)&&gateIsLanUrl(clean);
}
static bool gateRequest(uint8_t index,String &message){
 if(index>=3||gateBusy||millis()-gateLastAction<1500){message="Zajete / odczekaj";return false;}
 String url=gateUrls[index];
 if(!gateIsLanUrl(url)){message="Skonfiguruj lokalny URL HTTP";return false;}
 if(WiFi.status()!=WL_CONNECTED){message="Brak Wi-Fi";return false;}
 gateBusy=true;gateLastAction=millis();
 HTTPClient client;client.setTimeout(1800);client.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
 bool ok=false;
 if(client.begin(url)){int code=client.GET();ok=code>=200&&code<300;message="HTTP "+String(code);client.end();}
 else message="Niepoprawny URL";
 gateBusy=false;
 return ok;
}
static bool gateAuthorized(){
 if(gateWeb.authenticate("admin",gateWebPassword.c_str()))return true;
 gateWeb.requestAuthentication(BASIC_AUTH,"ES3C28P","Logowanie administratora");return false;
}
static void gatePage(){
 if(!gateAuthorized())return;
 String page=F("<!doctype html><html lang='pl'><meta name='viewport' content='width=device-width,initial-scale=1'><title>ES3C28P</title><style>body{background:#0b1018;color:#f1f5f9;font:16px system-ui;max-width:760px;margin:auto;padding:18px}input,button{font:inherit;border-radius:10px;padding:12px;background:#24364b;color:white;border:1px solid #6b8198}input{width:96%;box-sizing:border-box}button{min-height:46px;background:#2368aa}label{display:block;margin:18px 0}section{border:1px solid #354759;border-radius:14px;padding:15px;margin:15px 0}small{color:#b9c9d9}</style><h1>ES3C28P</h1><p id='status'>Odczyt...</p>");
 page+=F("<section><h2>Brama i furtka</h2>");
 for(int i=0;i<3;i++){page+="<form method='POST' action='/action'><input type='hidden' name='idx' value='"+String(i)+"'><button onclick='return confirm(&quot;Wykonac akcje?&quot;)'>"+gateEsc(gateNames[i])+"</button></form>";}
 page+=F("</section><form method='POST' action='/save'><section><h2>GET URL akcji</h2><small>Dozwolone HTTP i prywatne adresy IPv4 LAN. Kazdy przycisk ma oddzielny URL.</small>");
 for(int i=0;i<3;i++)page+="<label>"+gateEsc(gateNames[i])+"<input name='gate"+String(i)+"' maxlength='240' value='"+gateEsc(gateUrls[i])+"'></label>";
 page+=F("</section><section><h2>Kamery</h2>");
 for(int i=0;i<4;i++){
  page+="<label>Kamera "+String(i+1)+" URL<input name='cam"+String(i)+"' maxlength='240' value='"+gateEsc(cameraUrls[i])+"'></label>";
  page+="<label>Basic Auth login<input name='cuser"+String(i)+"' maxlength='48' value='"+gateEsc(cameraBasicUser[i])+"'></label>";
  page+="<label>Nowe haslo BA (puste = bez zmian)<input name='cpass"+String(i)+"' type='password' maxlength='80' autocomplete='new-password'></label>";
  page+="<button type='submit' formmethod='POST' formaction='/testcam' name='idx' value='"+String(i)+"'>Test kamera "+String(i+1)+"</button>";
 }
 page+=F("</section><section><h2>Siec Wi-Fi</h2><label>SSID<input name='ssid' maxlength='32' value='");
 page+=gateEsc(wifiPrefs.getString("ssid",""));page+=F("'></label><label>Nowe haslo Wi-Fi (puste = bez zmian)<input name='wifiPass' type='password' maxlength='63' autocomplete='new-password'></label></section>");
 page+=F("<section><h2>Termostat / Matter over Wi-Fi</h2><label>Nazwa urzadzenia<input name='matterName' maxlength='40' value='");
 page+=gateEsc(thermostatDeviceName);
 page+=F("'></label><p>Transport: Wi-Fi 2.4 GHz / IPv6</p><p>Matter: <strong>niezaimplementowany w tym firmware</strong></p><p>Kod parowania, QR, discriminator i otwarcie commissioning window beda dostepne dopiero po integracji ESP-Matter.</p><small>Nie wpisuj danych Thread ani kodow parowania recznie. Samo zapisanie tej nazwy nie tworzy urzadzenia Matter.</small></section>");
 page+=F("<section><h2>Sterowanie termostatem</h2><label>Nastawa (15-30 C)<input name='targetTemp' type='number' min='15' max='30' step='0.1' value='");
 page+=String(setPoint,1);
 page+=F("'></label><label>Histereza (0.2-2.0 C)<input name='hyst' type='number' min='0.2' max='2' step='0.1' value='");
 page+=String(hysteresis,1);
 page+=F("'></label><label>Tryb <select name='heatEnabled'><option value='1'");
 if(heatingEnabled)page+=F(" selected");
 page+=F(">AUTO</option><option value='0'");
 if(!heatingEnabled)page+=F(" selected");
 page+=F(">OFF</option></select></label><h3>Kolor LED od nastawy</h3><small>Niebieski (dolny prog) do czerwonego (gorny prog)</small><label>Temperatura niebieska<input type='number' name='ledMin' min='0' max='40' step='0.5' value='");
 page+=String(ledTempMin,1);
 page+=F("'></label><label>Temperatura czerwona<input type='number' name='ledMax' min='1' max='50' step='0.5' value='");
 page+=String(ledTempMax,1);
 page+=F("'></label><label>Jasnosc LED 0-255<input type='number' name='ledBri' min='0' max='255' step='1' value='");
 page+=String(ledBrightness);
 page+=F("'></label><p>Bateria: napiecie orientacyjne z ADC GPIO9. Poziom procentowy i stan ladowania niedostepne bez kalibracji/hardware.</p></section>");
 page+=F("<section><h2>Administrator</h2><label>Nowe haslo WWW (puste = bez zmian)<input name='webPass' type='password' minlength='12' autocomplete='new-password'></label></section><button>Zapisz ustawienia</button></form><script>setInterval(async()=>{try{const r=await fetch('/status');if(r.ok){let j=await r.json();document.getElementById('status').textContent='Temp: '+j.temperature+' C ('+j.source+')+' / Cel: '+j.target+' C / Grzanie: '+(j.heating?'TAK':'NIE')+' / Bateria ADC: '+j.battery_voltage_estimate+' V (szacunek) / Wi-Fi: '+j.ip;}}catch(e){}},2000)</script></html>");
 gateWeb.send(200,"text/html; charset=utf-8",page);
}
static void gateSave(){
 if(!gateAuthorized())return;
 String newGate[3],newCam[4];
 for(int i=0;i<3;i++){newGate[i]=gateWeb.arg("gate"+String(i));newGate[i].trim();if(newGate[i].length()&&!gateIsLanUrl(newGate[i])){gateWeb.send(400,"text/plain","Niedozwolony URL bramy");return;}}
 for(int i=0;i<4;i++){newCam[i]=gateWeb.arg("cam"+String(i));newCam[i].trim();if(newCam[i].length()&&!gateCameraUrlAllowed(newCam[i])){gateWeb.send(400,"text/plain","Niedozwolony URL kamery");return;}}
 String newMatterName=gateWeb.arg("matterName");newMatterName.trim();
 if(newMatterName.isEmpty()||newMatterName.length()>40){gateWeb.send(400,"text/plain","Nazwa termostatu: 1-40 znakow");return;}
 float target=gateWeb.arg("targetTemp").toFloat();
 float hyst=gateWeb.arg("hyst").toFloat();
 float minTemp=gateWeb.arg("ledMin").toFloat();
 float maxTemp=gateWeb.arg("ledMax").toFloat();
 int brightness=gateWeb.arg("ledBri").toInt();
 String enabled=gateWeb.arg("heatEnabled");
 if(!gateWeb.hasArg("targetTemp")||!gateWeb.hasArg("hyst")||!gateWeb.hasArg("ledMin")||
    !gateWeb.hasArg("ledMax")||!gateWeb.hasArg("ledBri")||
    !isfinite(target)||target<15||target>30||!isfinite(hyst)||hyst<0.2f||hyst>2.0f||
    !isfinite(minTemp)||minTemp<0||minTemp>40||!isfinite(maxTemp)||maxTemp>50||
    maxTemp-minTemp<0.5f||brightness<0||brightness>255||(enabled!="0"&&enabled!="1")){
    gateWeb.send(400,"text/plain","Niepoprawne parametry termostatu lub LED");return;
 }
 String newPassword=gateWeb.arg("webPass");
 if(newPassword.length()&&(newPassword.length()<12||newPassword.length()>64)){gateWeb.send(400,"text/plain","Haslo WWW musi miec 12-64 znakow");return;}
 for(int i=0;i<3;i++){gateUrls[i]=newGate[i];gatePrefs.putString(("gate"+String(i)).c_str(),gateUrls[i]);}
 for(int i=0;i<4;i++){
  cameraUrls[i]=newCam[i];gatePrefs.putString(("cam"+String(i)).c_str(),cameraUrls[i]);
  cameraBasicUser[i]=gateWeb.arg("cuser"+String(i));cameraBasicUser[i].trim();
  if(cameraBasicUser[i].length()>48){gateWeb.send(400,"text/plain","Login za dlugi");return;}
  gatePrefs.putString(("cu"+String(i)).c_str(),cameraBasicUser[i]);
  String secret=gateWeb.arg("cpass"+String(i));
  if(secret.length()){cameraBasicPass[i]=secret;gatePrefs.putString(("cp"+String(i)).c_str(),secret);}
 }
 String ssid=gateWeb.arg("ssid"),pass=gateWeb.arg("wifiPass");
 if(ssid.length()<=32&&!ssid.isEmpty())wifiPrefs.putString("ssid",ssid);
 if(!pass.isEmpty()&&pass.length()<=63)wifiPrefs.putString("password",pass);
 setPoint=target;hysteresis=hyst;heatingEnabled=(enabled=="1");
 ledTempMin=minTemp;ledTempMax=maxTemp;ledBrightness=(uint8_t)brightness;
 thermostatPrefs.putFloat("target",setPoint);
 thermostatPrefs.putFloat("hyst",hysteresis);
 thermostatPrefs.putBool("enabled",heatingEnabled);
 thermostatPrefs.putFloat("ledMin",ledTempMin);
 thermostatPrefs.putFloat("ledMax",ledTempMax);
 thermostatPrefs.putUInt("ledBri",ledBrightness);
 if(targetSlider)lv_slider_set_value(targetSlider,(int)roundf(setPoint*10),LV_ANIM_OFF);
 if(hysteresisSlider)lv_slider_set_value(hysteresisSlider,(int)roundf(hysteresis*10),LV_ANIM_OFF);
 if(heatSwitch) {
   if(heatingEnabled)lv_obj_add_state(heatSwitch,LV_STATE_CHECKED);
   else lv_obj_clear_state(heatSwitch,LV_STATE_CHECKED);
 }
 thermostatRefresh();
 thermostatDeviceName=newMatterName;gatePrefs.putString("mattername",thermostatDeviceName);
 if(newPassword.length()){gateWebPassword=newPassword;gatePrefs.putString("webpass",gateWebPassword);}
 gateWeb.sendHeader("Location","/");gateWeb.send(303,"text/plain","OK");
}
static void gateWebBegin(){
 gatePrefs.begin("gatecfg",false);
 thermostatDeviceName=gatePrefs.getString("mattername","28LCD Thermostat");
 for(int i=0;i<3;i++)gateUrls[i]=(gatePrefs.isKey(("gate"+String(i)).c_str())?gatePrefs.getString(("gate"+String(i)).c_str(),""):String(""));
 for(int i=0;i<4;i++){
  String fallback="http://192.168.68."+String(51+i)+"/cgi-bin/mjpg/video.cgi?channel=1&subtype=1";
  cameraUrls[i]=(gatePrefs.isKey(("cam"+String(i)).c_str())?gatePrefs.getString(("cam"+String(i)).c_str(),fallback):fallback);
  cameraBasicUser[i]=gatePrefs.getString(("cu"+String(i)).c_str(),"");
  cameraBasicPass[i]=gatePrefs.getString(("cp"+String(i)).c_str(),"");
 }
 gateWebPassword=gatePrefs.getString("webpass","");
 if(gateWebPassword.isEmpty()){
  char key[25];snprintf(key,sizeof(key),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
  gateWebPassword=key;gatePrefs.putString("webpass",gateWebPassword);
  Serial.printf("[WEB] Pierwsze haslo admin: %s (zapisane NVS)\n",key);
 }
 Serial.println("[WEB] Administrator: adminx");
 Serial.printf(
    "[WEB] Login: admin | Haslo: %s\n",
    gateWebPassword.c_str()
);
 Serial.println("[WEB] Haslo administratora zapisane w NVS (nie jest wyswietlane)");
 Serial.println("[WEB] Panel WWW: http://<IP-ESP32>/");
 gateWeb.on("/",HTTP_GET,gatePage);
 gateWeb.on("/save",HTTP_POST,gateSave);
 gateWeb.on("/action",HTTP_POST,[](){
  if(!gateAuthorized())return;
  int id=gateWeb.arg("idx").toInt();if(id<0||id>=3){gateWeb.send(400,"text/plain","Indeks");return;}
  String result;bool ok=gateRequest(id,result);gateWeb.send(ok?200:503,"text/plain",result);
 });
 gateWeb.on("/testcam",HTTP_POST,[](){
  if(!gateAuthorized())return;
  int idx=gateWeb.arg("idx").toInt();
  if(idx<0||idx>=4){gateWeb.send(400,"text/plain","Indeks kamery");return;}
  // Test saved connection settings; no password or URL echo in response.
  String u=cameraUrls[idx];
  if(!gateCameraUrlAllowed(u)){gateWeb.send(400,"text/plain","Nieprawidlowy lokalny URL");return;}
  String clean,parsedUser,parsedPass;
  cameraResolveUrl(u,clean,parsedUser,parsedPass);
  if(cameraBasicUser[idx].length()){parsedUser=cameraBasicUser[idx];parsedPass=cameraBasicPass[idx];}
  HTTPClient probe;probe.setTimeout(2500);
  const char *headers[]={"Content-Type"};probe.collectHeaders(headers,1);
  int code=cameraAuthenticatedGet(probe,clean,parsedUser,parsedPass);
  String type=probe.header("Content-Type");probe.end();
  gateWeb.send(200,"text/plain","Kamera "+String(idx+1)+": HTTP "+String(code)+"; Content-Type: "+(type.length()?type:"nieznany"));
 });
 gateWeb.on("/status",HTTP_GET,[](){
  if(!gateAuthorized())return;
  String temp=temperatureValid?String(roomTemp,1):"null";
#if DHT_GPIO >= 0
  const char *source="DHT";
#else
  const char *source="ESP32 die (not ambient)";
#endif
  float volts=analogReadMilliVolts(BAT_ADC)*0.001f*BATTERY_DIVIDER_RATIO;
  String body="{\"temperature\":"+temp+",\"source\":\""+String(source)+"\",\"heating\":"+(heatingDemand?"true":"false")+
    ",\"target\":"+String(setPoint,1)+",\"hysteresis\":"+String(hysteresis,1)+
    ",\"enabled\":"+(heatingEnabled?"true":"false")+
    ",\"battery_voltage_estimate\":"+String(volts,2)+
    ",\"battery_percent\":null,\"charging\":null,\"usb_power\":null"+
    ",\"led_blue_temp\":"+String(ledTempMin,1)+",\"led_red_temp\":"+String(ledTempMax,1)+
    ",\"ip\":\""+WiFi.localIP().toString()+"\"}";
  gateWeb.send(200,"application/json",body);
 });
 gateWeb.begin();gateWebReady=true;
}
static void gateWebService(){if(WiFi.status()==WL_CONNECTED&&gateWebReady)gateWeb.handleClient();}
