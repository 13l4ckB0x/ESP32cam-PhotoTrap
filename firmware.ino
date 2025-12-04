#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ----------- USER SETTINGS -----------
const char* ssid     = "YOUR_WIFI";
const char* password = "YOUR_PASS";
#define BOT_TOKEN    "123456:ABC..."
#define CHAT_ID      "12345678"
#define BUTTON_PIN   13
#define FLASH_PIN    4
// -------------------------------------

// AI-Thinker pin map
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

unsigned long lastPress = 0;

// ---------------- LED FLASH ----------------
void flashBlink(uint8_t brightness, uint16_t onMs, uint8_t times) {
  pinMode(FLASH_PIN, OUTPUT);
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(FLASH_PIN, HIGH);
    delay(8);
    digitalWrite(FLASH_PIN, LOW);
    delay(onMs);
  }
  digitalWrite(FLASH_PIN, LOW);
}

// ---------------- CAMERA ----------------
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 16500000;   // lower XCLK → less DMA load
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size   = FRAMESIZE_UXGA;  // 1600×1200
  config.jpeg_quality = 10;              // file ~80 kB
  config.fb_count     = 1;               // single buffer

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init failed: 0x%x\n", err);
    return false;
  }
  delay(200);               // let camera alloc internal buffers
  return true;
}

// ---------------- WIFI ----------------
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("📡 Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected!");
    Serial.print("📱 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi failed!");
  }
}

// ---------------- TELEGRAM ----------------
bool sendPhotoToTelegram(camera_fb_t *fb) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();
  if (!client.connect("api.telegram.org", 443)) {
    Serial.println("❌ TCP failed");
    return false;
  }

  const String B = "----ESP32CAM----" + String(millis());

  String header = "--" + B + "\r\n"
                  "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n"
                  + String(CHAT_ID) + "\r\n"
                  "--" + B + "\r\n"
                  "Content-Disposition: form-data; name=\"photo\"; filename=\"p.jpg\"\r\n"
                  "Content-Type: image/jpeg\r\n\r\n";
  String tail   = "\r\n--" + B + "--\r\n";
  size_t lenAll = header.length() + fb->len + tail.length();

  client.println("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Content-Type: multipart/form-data; boundary=" + B);
  client.println("Content-Length: " + String(lenAll));
  client.println("Connection: close");
  client.println();

  client.print(header);

  // send photo in 1 kB chunks
  const size_t CHUNK = 1024;
  for (size_t n = 0; n < fb->len; n += CHUNK) {
    size_t s = min(CHUNK, fb->len - n);
    client.write(fb->buf + n, s);
  }
  client.print(tail);

  bool ok = false;
  unsigned long t = millis();
  while (millis() - t < 10000) {
    if (client.available()) {
      if (client.readStringUntil('\n').indexOf("\"ok\":true") >= 0) ok = true;
    }
    if (ok) break;
    delay(10);
  }
  client.stop();
  return ok;
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\nESP32-CAM Telegram UXGA Photo");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(FLASH_PIN, LOW);

  if (psramFound()) {
    Serial.printf("✅ PSRAM detected: %d bytes\n", ESP.getPsramSize());
  } else {
    Serial.println("❌ PSRAM NOT found – UXGA may crash!");
  }

  if (!initCamera()) {
    Serial.println("❌ Camera failed to initialize. Halting.");
    while (true) delay(1000);
  }
  connectWiFi();
  Serial.println("System ready. Press button on GPIO13 to take photo.");
  delay(200);
  flashBlink(64, 150, 2);
  delay(2800);          // camera stack settle time
}

// ---------------- LOOP ----------------
void loop() {
  if (digitalRead(BUTTON_PIN) == LOW && millis() > 3000 && millis() - lastPress > 5000) {
    lastPress = millis();
    Serial.println("\n--- BUTTON PRESSED ---");

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb || fb->len == 0) {
      Serial.println("❌ Capture failed");
      if (fb) esp_camera_fb_return(fb);
      return;
    }
    Serial.printf("✅ Captured: %dx%d, %d bytes\n", fb->width, fb->height, fb->len);

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("🔄 WiFi lost – reconnecting");
      connectWiFi();
    }

    bool sent = false;
    if (WiFi.status() == WL_CONNECTED) {
      sent = sendPhotoToTelegram(fb);
      if (sent) {
        Serial.println("✅ Photo sent!");
        flashBlink(64, 150, 1);
      } else {
        Serial.println("❌ Telegram error");
      }
    } else {
      Serial.println("❌ WiFi still down");
    }

    esp_camera_fb_return(fb);
    delay(500);  // free memory

    Serial.printf("📊 Free heap: %d bytes | Free PSRAM: %d bytes\n",
                  ESP.getFreeHeap(), ESP.getFreePsram());

    while (digitalRead(BUTTON_PIN) == LOW) delay(10);  // wait release
    flashBlink(64, 150, 2);
  }
}
