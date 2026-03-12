รายวิชา Hardware Dev ภาควิชา คอมพิวเตอร์ คณะวิศวกรรมศาสตร์ มหาวิทยาลัยเกษตรศาสตร์ บางเขน
กลุ่ม Maituen
1.ฐิติพัฒน์ พาแพง 6810503471
2.ธนกฤต จงวิไลเกษม
3.วศุรัฐ สิงขรณ์
4.วสิษฐ์พล หนองบัวล่าง

ไฟล์ที่ใช้
-------------------------------
- Server
เป็นไฟล์ server หลักที่ทำงานโดยใช้ node.js จะทำหน้าที่ประสานงานกับตัวของ Dashboard และ ESP32-S3 โดยจะทำการรับค่าที่อ่านได้จาก sensor ต่างๆในถุงมือและนำมาประมวลผลเป็นคำแล้วแปลงเป็นเสียงพูดผ่าน Api ของ google รวมทั้งส่งกลับข้อมูลเพื่อเล่นเสียง และยังส่งข้อมูลต่างๆเพื่อ monitor ขึ้นบน Dashboard
Require Library
-google-cloud/text-to-speech
-aedes
-net
-ws
-websocket-stream

- Dashboard
เป็นไฟล์สำหรับการรันหน้าเว็บ Dashboard ซึ่งทำงานโดยใช้ next.js ทำหน้าที่ monitor ค่าต่างๆที่อ่านได้จาก sensor ของถุงมือรวมถึงคำที่ประมวลผลได้และยังสามารถปรับแต่งค่าต่างๆของถุงมือได้อีกด้วย
Require Library
-mqtt
-next

- Speaker
เป็นไฟล์สำหรับ esp32-s3 ที่ใช้ในการรับเสียงที่ผ่านการประมูลผลจากการเก็บข้อมูลของ sensor แล้วนำมาประมวลผลเป็นคำพูด จากนั้นนำมาแสดงผลเป็นเสียง
Require Library
-Pubsubclient
-ArduinoJson
-ESP8266Audio

- Esp32_glove
เป็นไฟล์สำหรับ esp32-s3 ที่ใช้ในการอ่านค่าจาก FlexSensor และ MPU6050 จากนั้นจะทำการ publish ค่าที่อ่านได้ผ่าน mqtt broker ไปที่ server แล้วให้ server ประมวลผลต่อ
Require Library
-Pubsubclient
-ArduinoJson

- SpeechToText
เป็นไฟล์สำหรับ esp32-s3 ที่ใช้ในการรับเสียงพูดผ่านไมค์และส่ง api ไปยัง google cloud speech to text เพื่อประมวลผลเสียงพูดออกมาเป็นค
Require Library
-Pubsubclient
-ArduinoJson
-U8g2

-------------------------------
Hardware ที่ใช้
-------------------------------
-ถุงมือ
-Flex sensor 
-Esp32-S3
-MPU6050(Gyro and Accelerometer)
-Speaker (Gravity: Digital Speaker Module)
-Amplifier(MAX98357A)
-Mic(INMP441)
-OLED
-------------------------------

วิธีการใช้โปรแกรม
-------------------------------
1. ติดตั้ง Library ให้เรียบร้อย
2. run Server.js ผ่าน node.js
3. Upload โค้ด 3 ส่วนเข้าแต่ละบอร์ดของ Esp32-s3 และเชื่อมไวไฟ
4. ใช้ npm run dev ในการ run หน้า Dashboard เผื่อสังเกตค่าต่างๆ
-------------------------------
