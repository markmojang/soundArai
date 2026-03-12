#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioFileSourceHTTPStream.h>

// ===== WiFi / MQTT =====
const char* ssid        = ""; // Wifi Name
const char* password    = ""; // Wifi Password
const char* mqtt_server = ""; // Mqtt Server IP
const char* client_id = "esp32_speaker"; // Client name

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ===== I2S =====
#define I2S_BCLK 40
#define I2S_LRC  39
#define I2S_DOUT 41

// ===== Audio =====
AudioGeneratorMP3*        mp3 = nullptr;
AudioOutputI2S*           out = nullptr;
AudioFileSourceHTTPStream* src = nullptr;
volatile bool audioPlaying = false;
char pendingUrl[256] = "";
TaskHandle_t audioTaskHandle = NULL;

// ============================================================
// Audio Task — Core 1
// ============================================================
void audioTask(void* param) {
  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  out->SetGain(3.0);

  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (strlen(pendingUrl) == 0) continue;

    audioPlaying = true;
    Serial.printf("🎵 %s\n", pendingUrl);

    src = new AudioFileSourceHTTPStream(pendingUrl);
    mp3 = new AudioGeneratorMP3();
    mp3->begin(src, out);

    while (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        break;
      }
    }

    delete mp3; mp3 = nullptr;
    delete src; src = nullptr;
    pendingUrl[0] = '\0';
    audioPlaying = false;
    Serial.println("✅ Done");
  }
}

// ============================================================
// MQTT
// ============================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);

  if (t == "iot/esp32s3_test/speak") {
    if (audioPlaying) {
      Serial.println("⚠️ busy");
      return;
    }
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, payload, length)) return;
    const char* url = doc["url"] | "";
    if (strlen(url) == 0) return;
    strncpy(pendingUrl, url, 255);
    xTaskNotifyGive(audioTaskHandle);
  }
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(client_id)) {
      mqttClient.subscribe("iot/esp32s3_test/speak");
      Serial.println("✅ MQTT | heap=" + String(ESP.getFreeHeap()));
    } else {
      Serial.printf("❌ rc=%d\n", mqttClient.state());
      delay(2000);
    }
  }
}

// ============================================================
// Setup / Loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.begin(ssid, password);
  Serial.print("Connecting.");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\n✅ " + WiFi.localIP().toString());

  xTaskCreatePinnedToCore(audioTask, "audio", 8192, NULL, 5, &audioTaskHandle, 1);

  mqttClient.setBufferSize(512);
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  connectMQTT();

  Serial.println("🚀 Ready");
}

void loop() {
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();
}