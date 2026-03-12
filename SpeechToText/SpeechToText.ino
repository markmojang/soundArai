//Libraries
#include <WiFi.h> //WiFi access
#include <WiFiClientSecure.h> //access https
#include <HTTPClient.h> //send data to google api
#include <driver/i2s.h> //mic
#include <U8g2lib.h> //screen(thai font)
#include <Wire.h> //OLED
#include <ArduinoJson.h> //transcript JSON back to text
#include "mbedtls/base64.h" //turn analog audio into Base64 data

//Network Config Variables
const char* ssid = ""; // Wifi Name
const char* password = ""; // Wifi Password
const String googleApiKey = ""; // Your Google Speech to text Api Key

//Pins 
#define I2S_WS 4
#define I2S_SD 6
#define I2S_SCK 5
#define I2C_SDA 8
#define I2C_SCL 9

//Audio
#define SAMPLE_RATE 16000 //optimised for GG API
#define I2S_PORT I2S_NUM_0 //Give the mic port0
#define SILENCE_THRESHOLD 1200 //Background noise
#define SILENCE_DURATION_MS 2000 //Wait

//Display Setup
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ I2C_SCL, /* data=*/ I2C_SDA, /* reset=*/ U8X8_PIN_NONE);
//Lib_chip_res_generic_fullframebuffer_i2csw(orientation)

//Buffers & State
int16_t *audioBuffer;
size_t bufferIndex = 0;
const size_t maxBufferSize = 16000 * 15;

unsigned long lastAudioTime = 0;
bool isRecording = false;

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

void setup() {
  Serial.begin(115200);
  
  audioBuffer = (int16_t*)ps_malloc(maxBufferSize * sizeof(int16_t));
  if (audioBuffer == NULL) {
    Serial.println("Failed to allocate PSRAM! Check Tools menu.");
    while(1);
  }

  // Setup Display
  u8g2.begin();
  u8g2.enableUTF8Print(); 
  u8g2.setFont(u8g2_font_etl16thai_t); 
  
  u8g2.clearBuffer();
  u8g2.setCursor(0, 15);
  u8g2.print("Connecting WiFi...");
  u8g2.sendBuffer();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  setupI2S();
  
  u8g2.clearBuffer();
  u8g2.setCursor(0, 15);
  u8g2.print("Ready.");
  u8g2.sendBuffer();
}

void loop() {
  int32_t sample32 = 0;
  size_t bytesIn = 0;

  esp_err_t result = i2s_read(I2S_PORT, &sample32, sizeof(sample32), &bytesIn, portMAX_DELAY);
  
  if (result == ESP_OK && bytesIn > 0) {
    int16_t sample16 = sample32 >> 14; 
    
    // Calculate Absolute Amplitude for Silence Detection
    int amplitude = abs(sample16);
    
    if (amplitude > SILENCE_THRESHOLD) {
      lastAudioTime = millis();
      if (!isRecording) {
        isRecording = true;
        bufferIndex = 0; //Reset buffer
        Serial.println("Speech detected, recording...");
      }
    }

    if (isRecording) {
      if (bufferIndex < maxBufferSize) {
        audioBuffer[bufferIndex++] = sample16;
      } else {
        sendToGoogleCloud(); 
      }
      
      //Check for 2 seconds of silence
      if (millis() - lastAudioTime > SILENCE_DURATION_MS) {
        Serial.println("2 seconds of silence detected. Sending data...");
        sendToGoogleCloud();
      }
    }
  }
}

void displayWrappedThai(String text) {
  u8g2.clearBuffer();
  
  int startY = 16; 
  int lineHeight = 16; 
  
  int cursorX = 0;
  int cursorY = startY; 
  
  int baseCharWidth = 0;
  uint16_t prevUnicode = 0;
  
  int i = 0;
  while (i < text.length()) {
    uint16_t unicode = 0;
    uint8_t c = text[i];
    int charLen = 1;
    
    //Decode UTF-8
    if ((c & 0x80) == 0) { unicode = c; charLen = 1; } 
    else if ((c & 0xE0) == 0xC0) { unicode = ((c & 0x1F) << 6) | (text[i+1] & 0x3F); charLen = 2; } 
    else if ((c & 0xF0) == 0xE0) { unicode = ((c & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F); charLen = 3; } 
    else { i += 1; continue; }
    
    String singleChar = text.substring(i, i + charLen);
    int myWidth = u8g2.getUTF8Width(singleChar.c_str());
    
    //thai character types
    bool isUpperVowel = (unicode == 0x0E31) || (unicode >= 0x0E34 && unicode <= 0x0E37) || unicode == 0x0E4D || unicode == 0x0E47;
    bool isLowerVowel = (unicode >= 0x0E38 && unicode <= 0x0E3A);
    bool isToneMark = (unicode >= 0x0E48 && unicode <= 0x0E4C);
    
    //position
    if (isUpperVowel || isLowerVowel || isToneMark) {
      int drawX = cursorX - (baseCharWidth / 2) - (myWidth / 2);
      int drawY = cursorY;
      
      bool prevIsUpperVowel = (prevUnicode == 0x0E31) || (prevUnicode >= 0x0E34 && prevUnicode <= 0x0E37) || prevUnicode == 0x0E4D || prevUnicode == 0x0E47;
      if (isToneMark && prevIsUpperVowel) { drawY -= 4; }
      
      u8g2.drawGlyph(drawX, drawY, unicode);
    } else {
      
      //next line
      if (cursorX + myWidth > 128) {
        cursorX = 0;
        cursorY += lineHeight;
        
        //auto scroll
        if (cursorY > 64) {
          u8g2.sendBuffer(); 
          delay(3000);       // Pause to let you read
          u8g2.clearBuffer(); 
          cursorY = startY;   
        }
      }
      
      u8g2.drawGlyph(cursorX, cursorY, unicode);
      baseCharWidth = myWidth; 
      cursorX += myWidth; 
    }
    
    prevUnicode = unicode;
    i += charLen; 
  }
  
  u8g2.sendBuffer();
}

void sendToGoogleCloud() {
  isRecording = false; //Stop recording while sending data
  
  u8g2.clearBuffer();
  u8g2.setCursor(0, 15);
  u8g2.print("Processing...");
  u8g2.sendBuffer();

  if (WiFi.status() == WL_CONNECTED) {
    // Google requires HTTPS
    WiFiClientSecure client;
    client.setInsecure(); //Skip certificate validation to save memory/time
    
    HTTPClient http;
    http.begin(client, "https://speech.googleapis.com/v1/speech:recognize?key=" + googleApiKey);
    http.setTimeout(20000);
    http.addHeader("Content-Type", "application/json");

    //size
    size_t audio_bytes = bufferIndex * sizeof(int16_t);
    size_t b64_len = 0;
    
    //base64 length (mbedtls includes a hidden null terminator in this count)
    mbedtls_base64_encode(NULL, 0, &b64_len, (const unsigned char*)audioBuffer, audio_bytes);

    String jsonStart = "{\"config\":{\"encoding\":\"LINEAR16\",\"sampleRateHertz\":16000,\"languageCode\":\"th-TH\"},\"audio\":{\"content\":\"";
    String jsonEnd = "\"}}";

    //payload size (subtracting 1 from b64_len to drop the null terminator)
    size_t exact_payload_len = jsonStart.length() + (b64_len - 1) + jsonEnd.length();
    
    //PSRAM for the JSON payload
    char* payload = (char*)ps_malloc(exact_payload_len + 1);
    if (payload == NULL) {
      Serial.println("Failed to allocate PSRAM for payload!");
      
      u8g2.clearBuffer();
      u8g2.setCursor(0, 15);
      u8g2.print("MeM Full");
      u8g2.sendBuffer();
      
      bufferIndex = 0;
      lastAudioTime = millis();
      return;
    }

    //Construct JSON
    memcpy(payload, jsonStart.c_str(), jsonStart.length());
    
    size_t written_b64 = 0;
    //Encode audio
    mbedtls_base64_encode((unsigned char*)(payload + jsonStart.length()), b64_len, &written_b64, (const unsigned char*)audioBuffer, audio_bytes);
    
    //Copy the end (This explicitly overwrites the hidden null terminator mbedtls just added)
    memcpy(payload + jsonStart.length() + (b64_len - 1), jsonEnd.c_str(), jsonEnd.length());
    
    //Terminate the entire payload safely in our memory
    payload[exact_payload_len] = '\0';

    //Send the Request
    int httpResponseCode = http.POST((uint8_t*)payload, exact_payload_len);
    
    if (httpResponseCode == 200) {
      String response = http.getString();
      Serial.println("Google's Raw Response:");
      Serial.println(response);
      
      //Parse Google's JSON response 
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, response);
      
      //Extract the raw string
      JsonVariant transcript = doc["results"][0]["alternatives"][0]["transcript"];
      
      if (!transcript.isNull()) {
        String text = transcript.as<String>();
        displayWrappedThai(text);
      } else {
         u8g2.clearBuffer();
         u8g2.setCursor(0, 16);
         u8g2.print("No Speech Detected");
         u8g2.sendBuffer();
      }
    } else {
      Serial.println("Error code: " + String(httpResponseCode));
      Serial.println(http.getString()); //debugging
    }
    
    http.end();
    free(payload); //Free the PSRAM
  }
  
  bufferIndex = 0; //Reset
  lastAudioTime = millis(); //Reset timer
}