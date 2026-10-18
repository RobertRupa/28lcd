#pragma once
#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <driver/i2s.h>
// Audio jobs are processed in loop(), never inside an LVGL callback.
static AudioFileSourceFS *audioSource=nullptr;
static AudioGeneratorMP3 *mp3=nullptr;
static AudioGeneratorWAV *wav=nullptr;
static AudioOutputI2S *audioOutput=nullptr;
static File audioRecording;
static bool audioIsRecording=false;
static uint32_t audioRecordBytes=0;
static uint32_t audioRecordStart=0;
static const uint32_t audioRecordMaxMs=10000;
static char audioMessage[128]="Ready";

static void audioStop(){
 if(mp3){mp3->stop();delete mp3;mp3=nullptr;}
 if(wav){wav->stop();delete wav;wav=nullptr;}
 if(audioOutput){audioOutput->stop();delete audioOutput;audioOutput=nullptr;}
 if(audioSource){audioSource->close();delete audioSource;audioSource=nullptr;}
 digitalWrite(AMP_EN,HIGH);
}
static bool audioPlayFile(const char *path){
 if(!sdOk||!codecOk||!path||!SD_MMC.exists(path))return false;
 audioStop();
 audioSource=new AudioFileSourceFS(SD_MMC,path);
 audioOutput=new AudioOutputI2S();
 audioOutput->SetPinout(I2S_BCLK,I2S_WS,I2S_DOUT,I2S_MCLK);
 audioOutput->SetGain(0.25f);
 bool ok=false;
 String name(path);name.toLowerCase();
 digitalWrite(AMP_EN,LOW);
 if(name.endsWith(".mp3")){mp3=new AudioGeneratorMP3();ok=mp3->begin(audioSource,audioOutput);}
 else if(name.endsWith(".wav")){wav=new AudioGeneratorWAV();ok=wav->begin(audioSource,audioOutput);}
 if(!ok){audioStop();return false;}
 snprintf(audioMessage,sizeof(audioMessage),"Playing: %s",path);return true;
}
static void audioWriteHeader(File &f,uint32_t dataSize){
 const uint16_t channels=1,bits=16;
 const uint32_t rate=16000,byteRate=rate*channels*(bits/8);
 const uint16_t align=channels*(bits/8);
 f.seek(0);
 f.write((const uint8_t*)"RIFF",4);uint32_t chunk=36+dataSize;f.write((uint8_t*)&chunk,4);
 f.write((const uint8_t*)"WAVEfmt ",8);
 uint32_t fmtSize=16;uint16_t pcm=1;f.write((uint8_t*)&fmtSize,4);
 f.write((uint8_t*)&pcm,2);f.write((uint8_t*)&channels,2);
 f.write((uint8_t*)&rate,4);f.write((uint8_t*)&byteRate,4);
 f.write((uint8_t*)&align,2);f.write((uint8_t*)&bits,2);
 f.write((const uint8_t*)"data",4);f.write((uint8_t*)&dataSize,4);
}
static void audioFinishRecord(){
 if(!audioIsRecording)return;
 audioIsRecording=false;i2s_stop(I2S_NUM_1);i2s_driver_uninstall(I2S_NUM_1);
 audioWriteHeader(audioRecording,audioRecordBytes);audioRecording.close();
 snprintf(audioMessage,sizeof(audioMessage),"Saved /record.wav (%lu bytes)",(unsigned long)audioRecordBytes);
}
static bool audioStartRecord(){
 if(!sdOk||!codecOk||audioIsRecording)return false;
 audioStop();
 audioRecording=SD_MMC.open("/record.wav",FILE_WRITE);
 if(!audioRecording)return false;
 audioRecording.seek(0);audioWriteHeader(audioRecording,0);audioRecording.seek(44);
 audioRecordBytes=0;
 i2s_config_t cfg={};
 cfg.mode=(i2s_mode_t)(I2S_MODE_MASTER|I2S_MODE_RX);
 cfg.sample_rate=16000;cfg.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT;
 cfg.channel_format=I2S_CHANNEL_FMT_ONLY_LEFT;
 cfg.communication_format=I2S_COMM_FORMAT_STAND_I2S;
 cfg.intr_alloc_flags=ESP_INTR_FLAG_LEVEL1;cfg.dma_buf_count=8;cfg.dma_buf_len=256;
 cfg.use_apll=false;cfg.mclk_multiple=I2S_MCLK_MULTIPLE_256;
 if(i2s_driver_install(I2S_NUM_1,&cfg,0,nullptr)!=ESP_OK){audioRecording.close();return false;}
 i2s_pin_config_t pins={};pins.mck_io_num=I2S_MCLK;pins.bck_io_num=I2S_BCLK;
 pins.ws_io_num=I2S_WS;pins.data_out_num=I2S_PIN_NO_CHANGE;pins.data_in_num=I2S_DIN;
 if(i2s_set_pin(I2S_NUM_1,&pins)!=ESP_OK){i2s_driver_uninstall(I2S_NUM_1);audioRecording.close();return false;}
 i2s_start(I2S_NUM_1);audioIsRecording=true;audioRecordStart=millis();
 snprintf(audioMessage,sizeof(audioMessage),"Recording 10s: /record.wav");return true;
}
static void audioService(){
 if(mp3){if(!mp3->isRunning()||!mp3->loop()){audioStop();snprintf(audioMessage,sizeof(audioMessage),"Playback finished");}}
 if(wav){if(!wav->isRunning()||!wav->loop()){audioStop();snprintf(audioMessage,sizeof(audioMessage),"Playback finished");}}
 if(audioIsRecording){
  int16_t buf[256];size_t got=0;
  if(i2s_read(I2S_NUM_1,buf,sizeof(buf),&got,0)==ESP_OK&&got){
   if(audioRecording.write((uint8_t*)buf,got)!=got){audioFinishRecord();snprintf(audioMessage,sizeof(audioMessage),"SD write error");return;}
   audioRecordBytes+=got;
  }
  if(millis()-audioRecordStart>=audioRecordMaxMs)audioFinishRecord();
 }
}
