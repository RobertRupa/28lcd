#pragma once
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <fcntl.h>
#include <errno.h>

// Any non-empty UDP datagram on port 61631 triggers the doorbell.
// Keep all audio operations in the Arduino loop (not in a network callback).
static constexpr uint16_t DOORBELL_UDP_PORT = 61631;
static const IPAddress DOORBELL_MULTICAST_GROUP(239, 255, 0, 1);
static int doorbellSocket = -1;
static uint32_t doorbellRetryAt = 0;
static IPAddress doorbellBoundIP;
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

// One IPv4 socket on INADDR_ANY receives directed unicast and subnet broadcast.
// IP_ADD_MEMBERSHIP additionally enables 239.255.0.1 multicast reception.
static void doorbellSocketClose() {
 if (doorbellSocket >= 0) {
  close(doorbellSocket);
  doorbellSocket = -1;
 }
}

static bool doorbellSocketOpen() {
 int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
 if (fd < 0) { Serial.printf("[DOORBELL] socket failed: %d\\n", errno); return false; }
 int one = 1;
 setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
 setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
 sockaddr_in addr = {};
 addr.sin_family = AF_INET;
 addr.sin_port = htons(DOORBELL_UDP_PORT);
 addr.sin_addr.s_addr = htonl(INADDR_ANY);
 if (bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0) {
  Serial.printf("[DOORBELL] bind failed: %d\\n", errno);
  close(fd); return false;
 }
 ip_mreq group = {};
 group.imr_multiaddr.s_addr = inet_addr("239.255.0.1");
 group.imr_interface.s_addr = (uint32_t)WiFi.localIP();
 if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group)) != 0) {
  Serial.printf("[DOORBELL] multicast join failed: %d (unicast/broadcast still active)\\n", errno);
 }
 int flags = fcntl(fd, F_GETFL, 0);
 if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
 doorbellSocket = fd;
 doorbellBoundIP = WiFi.localIP();
 Serial.printf("[DOORBELL] Bound 0.0.0.0:%u | IP=%s | mask=%s | broadcast=%s | multicast=239.255.0.1\\n",
  DOORBELL_UDP_PORT, WiFi.localIP().toString().c_str(),
  WiFi.subnetMask().toString().c_str(), WiFi.broadcastIP().toString().c_str());
 return true;
}

static void doorbellUdpService() {
 if (WiFi.status() != WL_CONNECTED) {
  if (doorbellSocket >= 0) {
   doorbellSocketClose();
   Serial.println("[DOORBELL] Socket closed: Wi-Fi disconnected");
  }
  doorbellToneService();
  return;
 }
 if (doorbellSocket >= 0 && doorbellBoundIP != WiFi.localIP()) {
  doorbellSocketClose(); // DHCP address changed: rejoin on the new interface
 }
 if (doorbellSocket < 0 && (doorbellRetryAt == 0 || millis()-doorbellRetryAt >= 3000)) {
  doorbellRetryAt = millis();
  doorbellSocketOpen();
 }
 if (doorbellSocket >= 0) {
  // Drain up to four queued packets without blocking LVGL or audio playback.
  for (int n = 0; n < 4; ++n) {
   char payload[64];
   sockaddr_in sender = {};
   socklen_t senderLength = sizeof(sender);
   int size = recvfrom(doorbellSocket, payload, sizeof(payload)-1, MSG_DONTWAIT,
                       (sockaddr*)&sender, &senderLength);
   if (size < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK)
     Serial.printf("[DOORBELL] recvfrom error: %d\\n", errno);
    break;
   }
   if (size == 0) continue;
   payload[size] = '\\0';
   IPAddress source(ntohl(sender.sin_addr.s_addr));
   Serial.printf("[DOORBELL] UDP from %s:%u, bytes=%d, data=%.63s\\n",
    source.toString().c_str(), ntohs(sender.sin_port), size, payload);
   uint32_t now = millis();
   if (!doorbellTriggeredBefore || now-doorbellLastTrigger >= DOORBELL_RETRIGGER_MS) {
    doorbellTriggeredBefore = true;
    doorbellLastTrigger = now;
    doorbellPlay();
   }
  }
 }
 doorbellToneService();
}
