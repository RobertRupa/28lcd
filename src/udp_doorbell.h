#pragma once
#include <WiFiUdp.h>

// Any non-empty UDP datagram on port 61631 triggers the doorbell.
// Keep all audio operations in the Arduino loop (not in a network callback).
static constexpr uint16_t DOORBELL_UDP_PORT = 61631;
static WiFiUDP doorbellUdp;
static bool doorbellBound = false;
static bool doorbellToneActive = false;
static uint32_t doorbellToneFrame = 0;
static float doorbellTonePhase = 0.0f;
static uint32_t doorbellLastTrigger = 0;
static bool doorbellTriggeredBefore = false;
static constexpr uint32_t DOORBELL_RETRIGGER_MS = 350;

static void doorbellToneStop() {
 if (!doorbellToneActive) return;
 i2s_zero_dma_buffer(I2S_NUM_1);
 i2s_driver_uninstall(I2S_NUM_1);
 digitalWrite(AMP_EN, HIGH);
 doorbellToneActive = false;
}

static bool doorbellToneStart() {
 if (!codecOk) return false;
 i2s_config_t cfg = {};
 cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
 cfg.sample_rate = 22050;
 cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
 cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
 cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
 cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
 cfg.dma_buf_count = 4;
 cfg.dma_buf_len = 128;
 cfg.use_apll = false;
 cfg.tx_desc_auto_clear = true;
 cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
 if (i2s_driver_install(I2S_NUM_1, &cfg, 0, nullptr) != ESP_OK) return false;
 i2s_pin_config_t pins = {};
 pins.mck_io_num = I2S_MCLK;
 pins.bck_io_num = I2S_BCLK;
 pins.ws_io_num = I2S_WS;
 pins.data_out_num = I2S_DOUT;
 pins.data_in_num = I2S_PIN_NO_CHANGE;
 if (i2s_set_pin(I2S_NUM_1, &pins) != ESP_OK) {
  i2s_driver_uninstall(I2S_NUM_1);
  return false;
 }
 doorbellToneFrame = 0;
 doorbellTonePhase = 0;
 doorbellToneActive = true;
 digitalWrite(AMP_EN, LOW);
 return true;
}

// Non-blocking two-tone chime (D5 followed by G4, fading naturally).
static void doorbellToneService() {
 if (!doorbellToneActive) return;
 constexpr uint32_t rate = 22050;
 constexpr uint32_t toneFrames = rate * 45 / 100; // 450 ms per note
 constexpr uint32_t gapFrames = rate * 6 / 100;   // 60 ms silence
 constexpr uint32_t totalFrames = toneFrames * 2 + gapFrames;
 if (doorbellToneFrame >= totalFrames) {
  doorbellToneStop();
  return;
 }
 int16_t buffer[256];
 uint32_t count = min((uint32_t)128, totalFrames - doorbellToneFrame);
 for (uint32_t i = 0; i < count; ++i) {
  uint32_t pos = doorbellToneFrame + i;
  int16_t sample = 0;
  if (pos < toneFrames || pos >= toneFrames + gapFrames) {
   bool second = pos >= toneFrames + gapFrames;
   uint32_t offset = second ? pos - toneFrames - gapFrames : pos;
   float freq = second ? 392.0f : 587.33f;
   float attack = min(1.0f, (float)offset / 120.0f);
   float fade = 1.0f - (float)offset / (float)toneFrames;
   float gain = 6500.0f * attack * fade * fade * ((float)masterVolume / 100.0f);
   sample = (int16_t)(sinf(doorbellTonePhase) * gain);
   doorbellTonePhase += 2.0f * PI * freq / (float)rate;
   if (doorbellTonePhase >= 2.0f * PI) doorbellTonePhase -= 2.0f * PI;
  } else {
   doorbellTonePhase = 0;
  }
  buffer[2*i] = buffer[2*i+1] = sample;
 }
 size_t written = 0;
 esp_err_t result = i2s_write(I2S_NUM_1, buffer, count * 4, &written, 0);
 if (result == ESP_OK && written) doorbellToneFrame += written / 4;
}

static void doorbellPlay() {
 if (!codecOk) {
  Serial.println("[DOORBELL] ES8311 not detected; cannot play");
  return;
 }
 doorbellToneStop();
 if (audioIsRecording) audioFinishRecord();
 audioStop();
 if (sdOk && SD_MMC.exists("/dingdong.wav") && audioPlayFile("/dingdong.wav")) {
  Serial.println("[DOORBELL] Playing /dingdong.wav");
  return;
 }
 if (doorbellToneStart()) Serial.println("[DOORBELL] Playing synthesized ding-dong");
 else Serial.println("[DOORBELL] I2S startup failed");
}

static void doorbellUdpService() {
 if (WiFi.status() != WL_CONNECTED) {
  if (doorbellBound) {
   doorbellUdp.stop();
   doorbellBound = false;
   Serial.println("[DOORBELL] UDP stopped (Wi-Fi disconnected)");
  }
  doorbellToneService();
  return;
 }
 if (!doorbellBound) {
  doorbellBound = doorbellUdp.begin(DOORBELL_UDP_PORT) != 0;
  if (doorbellBound)
   Serial.printf("[DOORBELL] Listening UDP %s:%u\\n", WiFi.localIP().toString().c_str(), DOORBELL_UDP_PORT);
  else {
   static uint32_t lastError = 0;
   if (millis() - lastError > 5000) {
    lastError = millis();
    Serial.println("[DOORBELL] UDP bind failed; retrying");
   }
  }
 }
 if (doorbellBound) {
  int packetSize = doorbellUdp.parsePacket();
  if (packetSize > 0) {
   // Drain the full datagram, including packets bigger than our input buffer.
   char payload[64];
   int count = doorbellUdp.read((uint8_t*)payload, sizeof(payload) - 1);
   if (count < 0) count = 0;
   payload[count] = '\\0';
   while (doorbellUdp.available()) doorbellUdp.read();
   uint32_t now = millis();
   Serial.printf("[DOORBELL] UDP from %s:%u, bytes=%d, data=%.63s\\n",
    doorbellUdp.remoteIP().toString().c_str(), doorbellUdp.remotePort(), packetSize, payload);
   if (!doorbellTriggeredBefore || now - doorbellLastTrigger >= DOORBELL_RETRIGGER_MS) {
    doorbellTriggeredBefore = true;
    doorbellLastTrigger = now;
    doorbellPlay();
   }
  }
 }
 doorbellToneService();
}
