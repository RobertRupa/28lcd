#include <Arduino.h>
// Optional standalone relay-control firmware for ESP32-C5/C6/H2.
// UART protocol described in README. Not an OpenThread RCP firmware.
// Compile with PlatformIO environment 'relay_c6' and wiring defined below.
// Relay output MUST have a normally-off driver, separate isolation and watchdog.
// Physical relay output defaults to disabled until RELAY_GPIO is configured.
#ifndef RELAY_GPIO
#define RELAY_GPIO -1
#endif
#ifndef LINK_RX_GPIO
#define LINK_RX_GPIO 6
#endif
#ifndef LINK_TX_GPIO
#define LINK_TX_GPIO 7
#endif
#ifndef RELAY_ACTIVE_LEVEL
#define RELAY_ACTIVE_LEVEL HIGH
#endif
static HardwareSerial link(1);
static uint32_t lastGood=0;
static bool relay=false;
static uint8_t rxLen=0;
static char line[80];
static uint8_t crc(const char *s){uint8_t v=0;while(*s)v^=(uint8_t)*s++;return v;}
static void apply(bool enable){
 relay=enable;
#if RELAY_GPIO >= 0
 digitalWrite(RELAY_GPIO,enable?RELAY_ACTIVE_LEVEL:!RELAY_ACTIVE_LEVEL);
#endif
}
static void reply(unsigned seq){
 char body[40],out[60];
 snprintf(body,sizeof(body),"ACK,%u,%u",seq,relay?1:0);
 snprintf(out,sizeof(out),"$%s,%02X\n",body,crc(body));
 link.print(out);
}
static void parse(){
 if(line[0]!='$')return;
 char *end=strrchr(line,',');if(!end)return;
 unsigned expected=0;
 if(sscanf(end+1,"%x",&expected)!=1)return;
 *end=0;if(crc(line+1)!=expected)return;
 unsigned seq=0,heat=0;
 if(sscanf(line+1,"TH,%u,%u",&seq,&heat)!=2||heat>1)return;
 lastGood=millis();
#if RELAY_GPIO >= 0
 apply(heat!=0);
#else
 apply(false);
#endif
 reply(seq);
}
void setup(){
#if RELAY_GPIO >= 0
 pinMode(RELAY_GPIO,OUTPUT);apply(false);
#endif
 link.begin(115200,SERIAL_8N1,LINK_RX_GPIO,LINK_TX_GPIO);
}
void loop(){
 while(link.available()){
  char c=(char)link.read();
  if(c=='\n'){line[rxLen]=0;parse();rxLen=0;}
  else if(c!='\r'){
   if(rxLen<sizeof(line)-1)line[rxLen++]=c;
   else rxLen=0;
  }
 }
 if(millis()-lastGood>1500)apply(false);
 delay(2);
}
