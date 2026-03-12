  #include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>

// ===== MPU6050 =====
#define SDA_PIN  46
#define SCL_PIN  9
#define MPU_ADDR 0x68
#define ALPHA    0.98

// ===== Flex =====
#define NUM_SENSORS 5
const int   flexPins[NUM_SENSORS] = { 4, 5, 6, 7, 8 };
int         minVal[NUM_SENSORS];
int         maxVal[NUM_SENSORS];
const char* fingerName[] = { "thumb", "index", "middle", "ring", "little" };

// ===== MPU state =====
float pitch = 0, roll = 0;
float basePitch = 0, baseRoll = 0;
float Ax = 0, Ay = 0, Az = 0;
float Gx = 0, Gy = 0, Gz = 0;
unsigned long lastIMUTime   = 0;
unsigned long lastIMUUpdate = 0;
unsigned long lastPublish   = 0;
int  PUBLISH_INTERVAL = 5000;
#define IMU_INTERVAL 1200

// ===== Still detection =====
unsigned long stillSince = 0;
#define STILL_DURATION 2000

// ===== WiFi / MQTT =====
const char* ssid        = ""; // Wifi Name
const char* password    = ""; // Wifi Password
const char* mqtt_server = ""; // Mqtt Server IP
char        client_id[32]; // Client name
const char* HAND_SIDE   = "left";

WiFiClient   espClient;
PubSubClient mqttClient(espClient);

// ============================================================
// MPU6050
// ============================================================
void writeReg(byte reg, byte data) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg); Wire.write(data);
  Wire.endTransmission();
}
int16_t read16(byte reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)2);
  if (Wire.available() < 2) return 0;
  return (Wire.read() << 8) | Wire.read();
}

void updateIMU() {
  Ax = read16(0x3B) / 16384.0;
  Ay = read16(0x3D) / 16384.0;
  Az = read16(0x3F) / 16384.0;
  Gx = read16(0x43) / 131.0;
  Gy = read16(0x45) / 131.0;
  Gz = read16(0x47) / 131.0;

  unsigned long now = millis();
  float dt = (now - lastIMUTime) / 1000.0;
  lastIMUTime = now;

  pitch = ALPHA * (pitch + Gx * dt) + (1 - ALPHA) * atan2(Ay, sqrt(Ax*Ax + Az*Az)) * 180.0 / PI;
  roll  = ALPHA * (roll  + Gy * dt) + (1 - ALPHA) * atan2(-Ax, Az) * 180.0 / PI;
}

void zeroIMU() {
  for (int i = 0; i < 50; i++) { updateIMU(); delay(20); }
  basePitch = pitch;
  baseRoll  = roll;
  Serial.printf("🧭 Zero: pitch=%.1f° roll=%.1f°\n", basePitch, baseRoll);
  mqttClient.publish("iot/esp32s3_test/log", "IMU zeroed");
}

bool isStill() {
  return sqrt(Gx*Gx + Gy*Gy + Gz*Gz) < 12.0;
}

// ============================================================
// Flex
// ============================================================
int readFlex(int pin) {
  int total = 0;
  for (int i = 0; i < 10; i++) { total += analogRead(pin); delay(2); }
  return total / 10;
}

// ============================================================
// MQTT
// ============================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);
  if (t == "iot/esp32s3_test/set_interval") {
    PUBLISH_INTERVAL = atoi((char*)payload);
    Serial.printf("⏱️ Interval: %d ms\n", PUBLISH_INTERVAL);
    return;
  }
  if (t == "iot/esp32s3_test/zero_imu") {
    zeroIMU();
    return;
  }
}

void connectMQTT() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(client_id)) {
      mqttClient.subscribe("iot/esp32s3_test/set_interval");
      mqttClient.subscribe("iot/esp32s3_test/zero_imu");
      Serial.println("✅ MQTT | heap=" + String(ESP.getFreeHeap()));
    } else {
      Serial.printf("❌ rc=%d\n", mqttClient.state());
      delay(2000);
    }
  }
}

// ============================================================
// Sensor publish
// ============================================================
void publishSensorData() {
  int angles[NUM_SENSORS];
  for (int i = 0; i < NUM_SENSORS; i++) {
    int val = readFlex(flexPins[i]);
    angles[i] = constrain(map(val, minVal[i], maxVal[i], 0, 180), 0, 180);
  }

  StaticJsonDocument<512> doc;
  doc["hand"] = HAND_SIDE;
  JsonObject fingers = doc.createNestedObject("fingers");
  for (int i = 0; i < NUM_SENSORS; i++) fingers[fingerName[i]] = angles[i];
  JsonObject gyro = doc.createNestedObject("gyro");
  gyro["x"] = serialized(String(Gx, 1));
  gyro["y"] = serialized(String(Gy, 1));
  gyro["z"] = serialized(String(Gz, 1));
  JsonObject accel = doc.createNestedObject("accel");
  accel["x"] = serialized(String(Ax, 2));
  accel["y"] = serialized(String(Ay, 2));
  accel["z"] = serialized(String(Az, 2));
  doc["pitch"] = serialized(String(pitch - basePitch, 1));
  doc["roll"]  = serialized(String(roll  - baseRoll,  1));

  char buf[512];
  serializeJson(doc, buf);
  mqttClient.publish("iot/esp32s3_test/sensor", buf);
}

// ============================================================
// Calibration
// ============================================================
void calibrate() {
  Serial.println("Relax hand (3 sec)...");
  mqttClient.publish("iot/esp32s3_test/log", "Relax hand (3 sec)...");
  delay(3000);
  for (int i = 0; i < NUM_SENSORS; i++) {
    long total = 0;
    for (int j = 0; j < 40; j++) { total += readFlex(flexPins[i]); delay(5); }
    minVal[i] = total / 40;
  }
  Serial.println("Make FIST (3 sec)...");
  mqttClient.publish("iot/esp32s3_test/log", "Make FIST (3 sec)...");
  delay(3000);
  for (int i = 0; i < NUM_SENSORS; i++) {
    long total = 0;
    for (int j = 0; j < 40; j++) { total += readFlex(flexPins[i]); delay(5); }
    maxVal[i] = total / 40;
  }
  mqttClient.publish("iot/esp32s3_test/log", "Calibration Done");
  Serial.println("✅ Calibration Done");
}

// ============================================================
// Setup / Loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.begin(ssid, password);
  Serial.print("Connecting.");
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(300); }
  Serial.println("\n✅ " + WiFi.localIP().toString());

  snprintf(client_id, sizeof(client_id), "esp32s3_%s", WiFi.macAddress().c_str());

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  writeReg(0x6B, 0x00);
  writeReg(0x1B, 0x00);
  writeReg(0x1C, 0x00);
  lastIMUTime = millis();

  analogSetAttenuation(ADC_11db);
  analogReadResolution(12);

  minVal[0] = 3020; maxVal[0] = 3267;
  minVal[1] = 2318; maxVal[1] = 2915;
  minVal[2] = 2503; maxVal[2] = 3018;
  minVal[3] = 2293; maxVal[3] = 2940;
  minVal[4] = 2559; maxVal[4] = 2851;

  mqttClient.setBufferSize(512);
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(callback);
  connectMQTT();

  calibrate();
  zeroIMU();

  Serial.println("🚀 Ready");
}

void loop() {
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  unsigned long now = millis();

  if (now - lastIMUUpdate >= IMU_INTERVAL) {
    lastIMUUpdate = now;
    updateIMU();

  //   if (isStill()) {
  //     if (now - stillSince > STILL_DURATION) {
  //       zeroIMU();
  //       stillSince = now;
  //     }
  //   } else {
  //     stillSince = now;
  //   }
  }

  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    publishSensorData();
  }
}
