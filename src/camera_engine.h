#pragma once
#include <HTTPClient.h>
#include <JPEGDEC.h>
// HTTP multipart MJPEG or repeated JPEG endpoint; image decoded into PSRAM LVGL buffer.
// Designed for low frame rates (1-3 FPS). HTTPS/RTSP/H264 not supported.
static HTTPClient cameraHttp;
static WiFiClient *cameraStream=nullptr;
static JPEGDEC cameraDecoder;
static uint8_t *cameraFrame=nullptr;
static lv_color_t *cameraPixels=nullptr;
static lv_img_dsc_t cameraImageDescriptor={};
static lv_obj_t *cameraImageObject=nullptr;
static lv_obj_t *cameraStatusObject=nullptr;
static bool cameraActive=false;
static bool cameraCapturing=false;
static size_t cameraFrameSize=0;
static uint32_t cameraLastFrame=0;
static const size_t cameraMaxFrame=120*1024;
static const int cameraW=240,cameraH=180;
static int cameraDraw(JPEGDRAW *d){
 if(!cameraPixels)return 0;
 for(int yy=0;yy<d->iHeight;yy++){
  int y=d->y+yy;if(y<0||y>=cameraH)continue;
  for(int xx=0;xx<d->iWidth;xx++){
   int x=d->x+xx;if(x<0||x>=cameraW)continue;
   cameraPixels[y*cameraW+x].full=d->pPixels[yy*d->iWidth+xx];
  }
 }
 return 1;
}
static void cameraStop(){
 cameraActive=false;cameraCapturing=false;cameraStream=nullptr;cameraHttp.end();
 if(cameraStatusObject)lv_label_set_text(cameraStatusObject,"Podglad zatrzymany");
}
static bool cameraStart(const char *url){
 cameraStop();
 if(WiFi.status()!=WL_CONNECTED||!url||strncmp(url,"http://",7)!=0)return false;
 if(!cameraFrame)cameraFrame=(uint8_t*)heap_caps_malloc(cameraMaxFrame,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!cameraPixels)cameraPixels=(lv_color_t*)heap_caps_malloc(cameraW*cameraH*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!cameraFrame||!cameraPixels)return false;
 memset(cameraPixels,0,cameraW*cameraH*sizeof(lv_color_t));
 cameraImageDescriptor.header.always_zero=0;
 cameraImageDescriptor.header.w=cameraW;cameraImageDescriptor.header.h=cameraH;
 cameraImageDescriptor.header.cf=LV_IMG_CF_TRUE_COLOR;
 cameraImageDescriptor.data_size=cameraW*cameraH*sizeof(lv_color_t);
 cameraImageDescriptor.data=(const uint8_t*)cameraPixels;
 cameraHttp.setTimeout(1000);
 if(!cameraHttp.begin(url))return false;
 if(cameraHttp.GET()!=HTTP_CODE_OK){cameraHttp.end();return false;}
 cameraStream=cameraHttp.getStreamPtr();
 cameraFrameSize=0;cameraCapturing=false;cameraActive=true;
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
    if(w<=cameraW&&h<=cameraH){
     cameraDecoder.setPixelType(RGB565_LITTLE_ENDIAN);
     cameraDecoder.decode(0,0,0);
     lv_img_set_src(cameraImageObject,&cameraImageDescriptor);
     lv_obj_invalidate(cameraImageObject);
     cameraLastFrame=millis();
    }else if(cameraStatusObject){lv_label_set_text(cameraStatusObject,"JPEG > 240x180: zmniejsz rozdzielczosc kamery");}
    cameraDecoder.close();
   }
   cameraCapturing=false;cameraFrameSize=0;
  }
 }
 if(!cameraHttp.connected()&&!cameraStream->available())cameraStop();
}
