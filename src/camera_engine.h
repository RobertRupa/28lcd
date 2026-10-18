#pragma once
#include <HTTPClient.h>
#include <JPEGDEC.h>
// HTTP multipart MJPEG or repeated JPEG endpoint; image decoded into PSRAM LVGL buffer.
// Designed for low frame rates (1-3 FPS). HTTPS/RTSP/H264 not supported.
static HTTPClient cameraHttp;
static WiFiClient *cameraStream=nullptr;
static JPEGDEC cameraDecoder;
static uint8_t *cameraFrame=nullptr;
static uint16_t *cameraPixels=nullptr;
static lv_image_dsc_t cameraImageDescriptor={};
static lv_obj_t *cameraImageObject=nullptr;
static lv_obj_t *cameraStatusObject=nullptr;
static bool cameraActive=false;
static String cameraLastError;
static bool cameraCapturing=false;
static size_t cameraFrameSize=0;
static uint32_t cameraLastFrame=0;
static const size_t cameraMaxFrame=120*1024;
static const int cameraW=240,cameraH=180;
static uint16_t cameraDecodedW=240,cameraDecodedH=180;
static int cameraDraw(JPEGDRAW *d){
 if(!cameraPixels)return 0;
 for(int yy=0;yy<d->iHeight;yy++){
  int y=d->y+yy;if(y<0||y>=cameraDecodedH)continue;
  for(int xx=0;xx<d->iWidth;xx++){
   int x=d->x+xx;if(x<0||x>=cameraDecodedW)continue;
   cameraPixels[y*cameraW+x]=d->pPixels[yy*d->iWidth+xx];
  }
 }
 return 1;
}
static void cameraStop(){
 cameraActive=false;cameraCapturing=false;cameraStream=nullptr;cameraHttp.end();
 if(cameraStatusObject)lv_label_set_text(cameraStatusObject,"Podglad zatrzymany");
}
static bool cameraStart(const char *url,const char *username=nullptr,const char *password=nullptr){
 cameraStop();
 if(WiFi.status()!=WL_CONNECTED){cameraLastError="Wi-Fi offline";return false;}
 if(!url||strncmp(url,"http://",7)!=0){cameraLastError="Niepoprawny URL";return false;}
 if(!cameraFrame)cameraFrame=(uint8_t*)heap_caps_malloc(cameraMaxFrame,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!cameraPixels)cameraPixels=(uint16_t*)heap_caps_malloc(cameraW*cameraH*sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!cameraFrame||!cameraPixels){cameraLastError="Brak PSRAM";return false;}
 memset(cameraPixels,0,cameraW*cameraH*sizeof(uint16_t));
 cameraImageDescriptor.header.magic=LV_IMAGE_HEADER_MAGIC;
 cameraImageDescriptor.header.w=cameraW;cameraImageDescriptor.header.h=cameraH;
 cameraImageDescriptor.header.cf=LV_COLOR_FORMAT_RGB565;
 cameraImageDescriptor.data_size=cameraW*cameraH*sizeof(uint16_t);
 cameraImageDescriptor.data=(const uint8_t*)cameraPixels;
 cameraHttp.setTimeout(1000);
 if(!cameraHttp.begin(url)){cameraLastError="HTTP begin failed";return false;}
 cameraHttp.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
 if(username&&username[0])cameraHttp.setAuthorization(username,password?password:"");
 int httpCode=cameraHttp.GET();
 if(httpCode!=HTTP_CODE_OK){cameraLastError="HTTP "+String(httpCode)+(httpCode<0?" "+cameraHttp.errorToString(httpCode):"");cameraHttp.end();return false;}
 String contentType=cameraHttp.header("Content-Type");
 Serial.printf("[CAM] HTTP %d, content-type=%s, endpoint=%s\\n",httpCode,contentType.c_str(),url);
 cameraStream=cameraHttp.getStreamPtr();
 cameraFrameSize=0;cameraCapturing=false;cameraActive=true;cameraLastError="";
 if(cameraStatusObject)lv_label_set_text(cameraStatusObject,"Podglad aktywny (HTTP JPEG/MJPEG)");
 return true;
}
static void cameraService(){
 if(!cameraActive||!cameraStream)return;
 // Budget 2048 bytes per UI iteration; no blocking reads.
 int available=cameraStream->available();
 int n=available>2048?2048:available;
 while(n-->0){
  int c=cameraStream->read();if(c<0)break;
  uint8_t b=(uint8_t)c;
  if(!cameraCapturing){
   if(cameraFrameSize==1&&cameraFrame[0]==0xFF&&b==0xD8){
    cameraFrame[1]=b;cameraFrameSize=2;cameraCapturing=true;
   } else {cameraFrame[0]=b;cameraFrameSize=1;}
   continue;
  }
  if(cameraFrameSize>=cameraMaxFrame){cameraCapturing=false;cameraFrameSize=0;continue;}
  cameraFrame[cameraFrameSize++]=b;
  if(cameraFrameSize>=2&&cameraFrame[cameraFrameSize-2]==0xFF&&b==0xD9){
   if(millis()-cameraLastFrame>=300 && cameraDecoder.openRAM(cameraFrame,(int)cameraFrameSize,cameraDraw)){
    int w=cameraDecoder.getWidth(),h=cameraDecoder.getHeight();
    // Hardware-friendly JPEG scaling at 1/2, 1/4 or 1/8 to fit the screen.
    int scale=1;
    while(scale<8 && ((w+scale-1)/scale>cameraW || (h+scale-1)/scale>cameraH))scale*=2;
    cameraDecodedW=(uint16_t)((w+scale-1)/scale);
    cameraDecodedH=(uint16_t)((h+scale-1)/scale);
    if(cameraDecodedW<=cameraW&&cameraDecodedH<=cameraH){
     memset(cameraPixels,0,cameraW*cameraH*sizeof(uint16_t));
     cameraDecoder.setPixelType(RGB565_LITTLE_ENDIAN);
     cameraDecoder.decode(0,0,scale);
     cameraImageDescriptor.header.w=cameraW;
     cameraImageDescriptor.header.h=cameraH;
     lv_image_set_src(cameraImageObject,&cameraImageDescriptor);
     lv_obj_invalidate(cameraImageObject);
     cameraLastFrame=millis();
    }else if(cameraStatusObject){
     lv_label_set_text(cameraStatusObject,"Obraz zbyt duzy (>1920x1440)");
    }
    cameraDecoder.close();
   }
   cameraCapturing=false;cameraFrameSize=0;
  }
 }
 if(!cameraHttp.connected()&&!cameraStream->available())cameraStop();
}
