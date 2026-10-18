#pragma once
// Local configuration UI. This does NOT implement the Matter protocol.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_system.h>

static WebServer configServer(80);
static Preferences webPrefs;
static String webAdminPassword;
static bool configServerStarted = false;
static bool configServerRoutesReady = false;
static uint32_t webLastRetry = 0;

static String webEscape(const String &v) {
  String out;
  for (size_t i=0;i<v.length();++i) {
    switch(v[i]) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += v[i];
    }
  }
  return out;
}

static bool webAuthorized() {
  if (configServer.authenticate("admin", webAdminPassword.c_str())) return true;
  configServer.requestAuthentication(BASIC_AUTH, "28LCD settings");
  return false;
}

static void webNoCache() {
  configServer.sendHeader("Cache-Control", "no-store");
  configServer.sendHeader("X-Content-Type-Options", "nosniff");
  configServer.sendHeader("X-Frame-Options", "DENY");
  configServer.sendHeader("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; base-uri 'none'");
}

static void webConfigPage() {
  if (!webAuthorized()) return;
  String name = webPrefs.getString("name", "28LCD Thermostat");
  String ssid = wifiPrefs.getString("ssid", "");
  bool connected = WiFi.status() == WL_CONNECTED;
  String html = F("<!doctype html><html lang='pl'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>28LCD konfiguracja</title><style>body{font:16px system-ui;background:#101827;color:#eff6ff;max-width:690px;margin:auto;padding:24px}section{background:#1e293b;border-radius:14px;padding:20px;margin:16px 0}input{box-sizing:border-box;width:100%;padding:11px;margin:7px 0 15px;background:#0f172a;color:white;border:1px solid #64748b;border-radius:8px}button{background:#38bdf8;color:#082f49;border:0;border-radius:8px;padding:12px 20px;font-weight:700}small{color:#cbd5e1}code{color:#7dd3fc}</style></head><body><h1>28LCD — termostat</h1>");
  html += F("<section><h2>Stan urządzenia</h2><p>Wi-Fi: ");
  html += connected ? F("połączone") : F("rozłączone");
  html += F("</p><p>Adres IP: <code>");
  html += connected ? WiFi.localIP().toString() : String("-");
  html += F("</code></p><p>MAC: <code>");
  html += webEscape(WiFi.macAddress());
  html += F("</code></p></section><section><h2>Sieć Wi-Fi</h2><form method='POST' action='/save'><label>SSID</label><input name='ssid' maxlength='32' required value='");
  html += webEscape(ssid);
  html += F("'><label>Nowe hasło Wi-Fi (puste = zachowaj dotychczasowe)</label><input name='password' type='password' maxlength='63' autocomplete='new-password'><h2>Identyfikacja urządzenia</h2><label>Nazwa termostatu</label><input name='name' maxlength='40' required value='");
  html += webEscape(name);
  html += F("'><button type='submit'>Zapisz ustawienia</button></form><small>Zmiana danych Wi-Fi może zmienić adres IP. Ustawienia zapisywane w NVS.</small></section>");
  html += F("<section><h2>Matter over Wi-Fi</h2><p>Stan: <strong>niezaimplementowane w firmware</strong></p><p>Typ docelowy: Matter Thermostat (ESP32-S3, Wi-Fi 2,4 GHz).</p><p>Kod QR, kod parowania, discriminator, commissioning window oraz fabric: niedostępne do czasu integracji stosu ESP-Matter.</p><small>Nie ma potrzeby wprowadzania danych sieci Thread. Bridge Thread nie pośredniczy w komunikacji Matter over Wi-Fi.</small></section>");
  html += F("<p><small>Panel konfiguracyjny działa lokalnie przez HTTP z uwierzytelnianiem Basic. Korzystaj wyłącznie w zaufanej sieci LAN.</small></p></body></html>");
  webNoCache();
  configServer.send(200,"text/html; charset=utf-8",html);
}

static void webConfigSave() {
  if (!webAuthorized()) return;
  if (!configServer.hasArg("ssid") || !configServer.hasArg("name")) {
    webNoCache(); configServer.send(400,"text/plain","Missing fields"); return;
  }
  String ssid=configServer.arg("ssid"), pass=configServer.arg("password"), name=configServer.arg("name");
  ssid.trim(); name.trim();
  if (ssid.isEmpty() || ssid.length()>32 || name.isEmpty() || name.length()>40 ||
      pass.length()>63 || (pass.length()>0 && pass.length()<8)) {
    webNoCache(); configServer.send(400,"text/plain","Invalid SSID, device name or Wi-Fi password"); return;
  }
  bool changed = ssid != wifiPrefs.getString("ssid","");
  wifiPrefs.putString("ssid",ssid);
  if (pass.length()) { wifiPrefs.putString("password",pass); changed=true; }
  webPrefs.putString("name",name);
  webNoCache();
  configServer.send(200,"text/plain; charset=utf-8",
                    changed ? "Zapisano. Połącz ponownie urządzenie z Wi-Fi lub uruchom je ponownie." : "Zapisano.");
}

static void webConfigBegin() {
  webPrefs.begin("webcfg",false);
  webAdminPassword=webPrefs.getString("adminpass","");
  if(webAdminPassword.length()<12) {
    char generated[25];
    uint32_t a=esp_random(),b=esp_random(),c=esp_random();
    snprintf(generated,sizeof(generated),"%08lX%08lX%08lX",
             (unsigned long)a,(unsigned long)b,(unsigned long)c);
    webAdminPassword=generated;
    webPrefs.putString("adminpass",webAdminPassword);
  }
  Serial.println("[WEB] Login: admin");
  Serial.printf("[WEB] Admin password: %s\n",webAdminPassword.c_str());
  if(!configServerRoutesReady) {
    configServer.on("/",HTTP_GET,webConfigPage);
    configServer.on("/save",HTTP_POST,webConfigSave);
    configServer.onNotFound([](){
      if (!webAuthorized()) return;
      webNoCache();configServer.send(404,"text/plain","Not found");
    });
    configServerRoutesReady=true;
  }
  String ssid=wifiPrefs.getString("ssid","");
  if (!ssid.isEmpty()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(),wifiPrefs.getString("password","").c_str());
  }
}
static void webConfigService() {
  if(WiFi.status()==WL_CONNECTED) {
    if(!configServerStarted) {
      configServer.begin(); configServerStarted=true;
      Serial.printf("[WEB] http://%s/\n",WiFi.localIP().toString().c_str());
    }
    configServer.handleClient();
  } else {
    if(configServerStarted) {configServer.stop();configServerStarted=false;}
    if(millis()-webLastRetry>15000) {
      webLastRetry=millis();
      String ssid=wifiPrefs.getString("ssid","");
      if(!ssid.isEmpty()) WiFi.reconnect();
    }
  }
}
