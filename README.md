# SoundArai

## Information
**Course:** 01204114 Introduction to Computer Hardware Development  
**Department:** Computer Engineering  
**Faculty:** Faculty of Engineering  
**University:** Kasetsart University

---

## Group: Maituen

| Name | Student ID |
|-----|-----|
| ฐิติพัฒน์ พาแพง | 6810503471 |
| ธนกฤต จงวิไลเกษม | 6810503587 |
| วศุรัฐ สิงขรณ์ | 6810503901 |
| วสิษฐ์พล หนองบัวล่าง | 6810503919 |

---

# File Informations

## 1. Server
ไฟล์ **Server** เป็นเซิร์ฟเวอร์หลักที่ทำงานด้วย **Node.js**  
มีหน้าที่เป็นตัวกลางระหว่าง **Dashboard** และ **ESP32-S3**

### Responsibilities
- รับค่าจาก sensor ที่อยู่ในถุงมือ
- ประมวลผลข้อมูลเพื่อแปลงเป็นคำ
- แปลงข้อความเป็นเสียงด้วย **Google Text-to-Speech API**
- ส่งข้อมูลเสียงกลับไปยัง Speaker
- ส่งข้อมูลต่าง ๆ ไปแสดงบน Dashboard เพื่อ Monitor

### Required Libraries
- `@google-cloud/text-to-speech`
- `aedes`
- `net`
- `ws`
- `websocket-stream`

---

## 2. Dashboard
ไฟล์สำหรับรัน **Web Dashboard** โดยใช้ **Next.js**

### Responsibilities
- แสดงค่าที่อ่านได้จาก sensor
- แสดงคำที่ถูกประมวลผลจากถุงมือ
- ใช้สำหรับ Monitor ระบบ
- สามารถปรับค่าต่าง ๆ ของถุงมือได้

### Required Libraries
- `mqtt`
- `next`

---

## 3. Speaker
ไฟล์สำหรับ **ESP32-S3** ที่ใช้ในการรับข้อมูลเสียงจาก Server

### Responsibilities
- รับเสียงที่ถูกประมวลผลจาก sensor
- แปลงข้อมูลเป็นเสียง
- เล่นเสียงผ่าน Speaker

### Required Libraries
- `PubSubClient`
- `ArduinoJson`
- `ESP8266Audio`

---

## 4. ESP32_Glove
ไฟล์สำหรับ **ESP32-S3** ที่ติดอยู่กับถุงมือ

### Responsibilities
- อ่านค่าจาก **Flex Sensor**
- อ่านค่าจาก **MPU6050**
- ส่งข้อมูลผ่าน **MQTT Broker** ไปยัง Server

### Required Libraries
- `PubSubClient`
- `ArduinoJson`

---

## 5. SpeechToText
ไฟล์สำหรับ **ESP32-S3** ที่ใช้รับเสียงจากไมโครโฟน

### Responsibilities
- รับเสียงผ่านไมโครโฟน
- ส่งข้อมูลไปยัง **Google Cloud Speech-to-Text API**
- แปลงเสียงพูดเป็นข้อความ

### Required Libraries
- `PubSubClient`
- `ArduinoJson`
- `U8g2`

---

# Hardware Used

- Glove
- Flex Sensor
- ESP32-S3
- MPU6050 (Gyroscope + Accelerometer)
- Speaker *(Gravity: Digital Speaker Module)*
- Amplifier *(MAX98357A)*
- Microphone *(INMP441)*
- OLED Display

---

# How to Run the System

### 1. Install Required Libraries
ติดตั้ง Library ทั้งหมดให้ครบก่อนเริ่มใช้งาน

### 2. Start the Server
```bash
node Server.js
```
