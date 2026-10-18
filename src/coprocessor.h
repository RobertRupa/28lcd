#pragma once
#include <HardwareSerial.h>
// Simple application protocol for external relay controller.
// This is NOT Spinel RCP and NOT Thread Border Router.
// ESP32-C5/C6/H2 must run complementary application firmware.
// Format: $TH,seq,heat,crc\n with crc = XOR of ASCII body before final comma.
// RX: $ACK,seq,state,crc\n  state = 0 or 1
static HardwareSerial coprocessorSerial(1);
static bool coproEnabled=false;
static uint32_t coproLastSend=0,coproLastAck=0;
static uint16_t coproSeq=0;
static bool coproRelayConfirmed=false;
static char coproRx[80];static uint8_t coproRxLen=0;
static uint8_t coproCrc(const char *text){uint8_t c=0;while(*text)c^=(uint8_t)*text++;return c;}
static void coproBegin(){
 // Pin UART0 header GPIO43 RX and GPIO44 TX are also used by boot logs.
 // UART1 controller is mapped to these pins. USB CDC must be used for logs.
 coprocessorSerial.begin(115200,SERIAL_8N1,43,44);
 coproEnabled=true;coproLastAck=0;
}
static void coproSend(){
 if(!coproEnabled)return;
 char body[40],line[56];
 bool demand=heatingDemand;
#if DHT_GPIO >= 0
 demand=demand&&dhtOk;
#else
 demand=false; // Never energize relay with simulated temperature
#endif
 snprintf(body,sizeof(body),"TH,%u,%u",(unsigned)++coproSeq,demand?1:0);
 snprintf(line,sizeof(line),"$%s,%02X\n",body,coproCrc(body));
 coprocessorSerial.print(line);
 coproLastSend=millis();
}
static void coproService(){
 if(!coproEnabled)return;
 while(coprocessorSerial.available()){
  char c=(char)coprocessorSerial.read();
  if(c=='\n'){
   coproRx[coproRxLen]=0;
   if(coproRx[0]=='$'){
    char *crcPart=strrchr(coproRx,',');
    if(crcPart){
     unsigned received=0;
     if(sscanf(crcPart+1,"%x",&received)==1){
      *crcPart=0;
      if(coproCrc(coproRx+1)==received){
       unsigned seq=0,relay=0;
       if(sscanf(coproRx+1,"ACK,%u,%u",&seq,&relay)==2 && seq==coproSeq){
        coproLastAck=millis();coproRelayConfirmed=relay!=0;
       }
      }
     }
    }
   }
   coproRxLen=0;
  } else if(c!='\r'){
   if(coproRxLen<sizeof(coproRx)-1)coproRx[coproRxLen++]=c;
   else coproRxLen=0;
  }
 }
 if(millis()-coproLastSend>=500)coproSend();
 if(coproLastAck && millis()-coproLastAck>2000)coproRelayConfirmed=false;
}
