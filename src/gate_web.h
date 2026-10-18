#pragma once
#include <WebServer.h>
#include <HTTPClient.h>
#include <esp_system.h>

// All URLs are user-provisioned; no default actuation endpoints.
static Preferences gatePrefs;
static WebServer gateWeb(80);
static String gateUrls[3], cameraUrls[4];
static String gateWebPassword;
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
 for(int i=0;i<4;i++)page+="<label>Kamera "+String(i+1)+"<input name='cam"+String(i)+"' maxlength='240' value='"+gateEsc(cameraUrls[i])+"'></label>";
 page+=F("</section><section><h2>Siec Wi-Fi</h2><label>SSID<input name='ssid' maxlength='32' value='");
 page+=gateEsc(wifiPrefs.getString("ssid",""));page+=F("'></label><label>Nowe haslo Wi-Fi (puste = bez zmian)<input name='wifiPass' type='password' maxlength='63' autocomplete='new-password'></label></section>");
 page+=F("<section><h2>Administrator</h2><label>Nowe haslo WWW (puste = bez zmian)<input name='webPass' type='password' minlength='12' autocomplete='new-password'></label></section><button>Zapisz ustawienia</button></form><script>setInterval(async()=>{try{const r=await fetch('/status');if(r.ok){let j=await r.json();document.getElementById('status').textContent='Temp: '+j.temperature+' C ('+j.source+') / Grzanie: '+(j.heating?'TAK':'NIE')+' / Wi-Fi: '+j.ip;}}catch(e){}},2000)</script></html>");
 gateWeb.send(200,"text/html; charset=utf-8",page);
}
static void gateSave(){
 if(!gateAuthorized())return;
 String newGate[3],newCam[4];
 for(int i=0;i<3;i++){newGate[i]=gateWeb.arg("gate"+String(i));newGate[i].trim();if(newGate[i].length()&&!gateIsLanUrl(newGate[i])){gateWeb.send(400,"text/plain","Niedozwolony URL bramy");return;}}
 for(int i=0;i<4;i++){newCam[i]=gateWeb.arg("cam"+String(i));newCam[i].trim();if(newCam[i].length()&&!gateIsLanUrl(newCam[i])){gateWeb.send(400,"text/plain","Niedozwolony URL kamery");return;}}
 String newPassword=gateWeb.arg("webPass");
 if(newPassword.length()&&(newPassword.length()<12||newPassword.length()>64)){gateWeb.send(400,"text/plain","Haslo WWW musi miec 12-64 znakow");return;}
 for(int i=0;i<3;i++){gateUrls[i]=newGate[i];gatePrefs.putString(("gate"+String(i)).c_str(),gateUrls[i]);}
 for(int i=0;i<4;i++){cameraUrls[i]=newCam[i];gatePrefs.putString(("cam"+String(i)).c_str(),cameraUrls[i]);}
 String ssid=gateWeb.arg("ssid"),pass=gateWeb.arg("wifiPass");
 if(ssid.length()<=32&&!ssid.isEmpty())wifiPrefs.putString("ssid",ssid);
 if(!pass.isEmpty()&&pass.length()<=63)wifiPrefs.putString("password",pass);
 if(newPassword.length()){gateWebPassword=newPassword;gatePrefs.putString("webpass",gateWebPassword);}
 gateWeb.sendHeader("Location","/");gateWeb.send(303,"text/plain","OK");
}
static void gateWebBegin(){
 gatePrefs.begin("gatecfg",false);
 for(int i=0;i<3;i++)gateUrls[i]=gatePrefs.getString(("gate"+String(i)).c_str(),"");
 for(int i=0;i<4;i++){
  String fallback="http://192.168.68."+String(51+i)+"/cgi-bin/mjpg/video.cgi?channel=1&subtype=1";
  cameraUrls[i]=gatePrefs.getString(("cam"+String(i)).c_str(),fallback);
 }
 gateWebPassword=gatePrefs.getString("webpass","");
 if(gateWebPassword.isEmpty()){
  char key[25];snprintf(key,sizeof(key),"%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random());
  gateWebPassword=key;gatePrefs.putString("webpass",gateWebPassword);
  Serial.printf("[WEB] Pierwsze haslo admin: %s (zapisane NVS)\n",key);
 }
 gateWeb.on("/",HTTP_GET,gatePage);
 gateWeb.on("/save",HTTP_POST,gateSave);
 gateWeb.on("/action",HTTP_POST,[](){
  if(!gateAuthorized())return;
  int id=gateWeb.arg("idx").toInt();if(id<0||id>=3){gateWeb.send(400,"text/plain","Indeks");return;}
  String result;bool ok=gateRequest(id,result);gateWeb.send(ok?200:503,"text/plain",result);
 });
 gateWeb.on("/status",HTTP_GET,[](){
  if(!gateAuthorized())return;
  String temp=temperatureValid?String(roomTemp,1):"null";
#if DHT_GPIO >= 0
  const char *source="DHT";
#else
  const char *source="ESP32 die (not ambient)";
#endif
  String body="{\"temperature\":"+temp+",\"source\":\""+String(source)+"\",\"heating\":"+(heatingDemand?"true":"false")+",\"ip\":\""+WiFi.localIP().toString()+"\"}";
  gateWeb.send(200,"application/json",body);
 });
 gateWeb.begin();gateWebReady=true;
}
static void gateWebService(){if(WiFi.status()==WL_CONNECTED&&gateWebReady)gateWeb.handleClient();}
