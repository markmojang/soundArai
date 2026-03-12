const net = require("net")
const http = require("http")
const aedes = require("aedes")
const ws = require("websocket-stream")
const { exec } = require("child_process")
const fs = require("fs")
const path = require("path")
const os = require("os")

const broker = aedes()
const GOOGLE_API_KEY = "Google_Cloud_API_Key"
const HTTP_PORT = 8080
const SERVER_IP = "YOUR_SERVER_IP"

net.createServer(broker.handle).listen(1883, () => console.log("✅ MQTT TCP 1883"))
const wsServer = http.createServer()
ws.createServer({ server: wsServer }, s => broker.handle(s))
wsServer.listen(8888, () => console.log("✅ MQTT WS 8888"))

let latestMP3 = null
const bent = 105
const straight = 75
let playing = false
// ===== HTTP serve MP3 ให้ ESP32 =====
http.createServer((req, res) => {
  if (req.url === "/audio.mp3" && latestMP3) {
    console.log(`📡 Serving MP3: ${latestMP3.length} bytes`)
    res.writeHead(200, { "Content-Type": "audio/mpeg", "Content-Length": latestMP3.length })
    res.end(latestMP3)
  } else {
    res.writeHead(404); res.end()
  }
}).listen(HTTP_PORT, () => console.log(`✅ HTTP :${HTTP_PORT}`))

// ===== เล่นบน PC =====
function playOnPC(mp3Buf) {
  return new Promise(resolve => {
    const tmp = path.join(os.tmpdir(), `play_${Date.now()}.mp3`)
    fs.writeFileSync(tmp, mp3Buf)
    const cmd = os.platform() === "win32" ? `start "" "${tmp}"`
      : os.platform() === "darwin" ? `afplay "${tmp}"`
        : `ffplay -nodisp -autoexit "${tmp}" 2>/dev/null`
    exec(cmd, () => { setTimeout(() => fs.unlink(tmp, () => { }), 5000); resolve() })
  })
}

// ===== Google Cloud TTS =====
async function textToSpeech(text) {
  console.log("🗣️ TTS:", text)
  try {
    const res = await fetch(
      `https://texttospeech.googleapis.com/v1/text:synthesize?key=${GOOGLE_API_KEY}`,
      {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          input: { text },
          voice: {
            languageCode: "th-TH",        // ภาษาไทย
            name: "th-TH-Neural2-C",      // เสียงไทย Neural (ชัดสุด)
            ssmlGender: "FEMALE"
          },
          audioConfig: {
            audioEncoding: "MP3",
            speakingRate: 1.0,            // ความเร็ว 0.25-4.0
            pitch: 0,                     // ระดับเสียง -20 ถึง 20
            volumeGainDb: 0               // ความดัง
          }
        })
      }
    )

    if (!res.ok) {
      const err = await res.text()
      throw new Error(`Google TTS: ${res.status} — ${err}`)
    }

    const json = await res.json()
    // Google ส่งกลับเป็น base64
    const mp3Buf = Buffer.from(json.audioContent, "base64")
    console.log(`📦 MP3: ${mp3Buf.length} bytes`)

    latestMP3 = mp3Buf

    // บอก ESP32 ให้มาดึง
    broker.publish({
      topic: "iot/esp32s3_test/speak",
      payload: JSON.stringify({ url: `http://${SERVER_IP}:${HTTP_PORT}/audio.mp3` })
    })

    // เล่นบน PC พร้อมกัน
    await playOnPC(mp3Buf)

  } catch (err) {
    console.error("❌", err.message)
  }
}

// ===== แปลภาษามือ =====

// pitch: ก้มหน้าลง = ลบ, เงยขึ้น = บวก
// roll:  เอียงซ้าย = ลบ, เอียงขวา = บวก

function translate(sensorData) {
  let a = [
    sensorData["fingers"]["thumb"],
    sensorData["fingers"]["index"],
    sensorData["fingers"]["middle"],
    sensorData["fingers"]["ring"],
    sensorData["fingers"]["little"]
  ]
  //a = a.reverse() // มือซ้าย
  // a[0]=ก้อย a[1]=นาง a[2]=กลาง a[3]=ชี้ a[4]=โป้ง

  const pitch = parseFloat(sensorData["pitch"]) || 0
  const roll  = parseFloat(sensorData["roll"])  || 0

  //console.log("🔢 Sensor:", a, "pitch:", pitch, "roll:", roll)


  if (a[0] > bent && a[1] > bent && a[2] > bent && a[3] > bent && a[4] > bent)
    return "ชนหมัด"

  if (a[0] < straight && a[1] > bent && a[2] > bent && a[3] > bent && a[4] > bent && roll < -50 )
    return "เยี่ยม"

  if (a[0] < straight && a[1] > bent && a[2] > bent && a[3] > bent && a[4] > bent && roll > 50 )
    return "แย่"

  if (a[1] < straight && a[0] > bent && a[2] > bent && a[3] > bent && a[4] > bent && pitch < -20 && roll < -20)
    return "ฉัน"

  if (a[1] < straight && a[0] > bent && a[2] > bent && a[3] > bent && a[4] > bent && pitch > -15 && roll < -20)
    return "คุณ" 


  if (a[1] < straight && a[2] < straight && a[0] > bent && a[3] > bent && a[4] > bent)
    return "สันติ"

  if (a[0] > bent && a[1] > bent && a[2] < straight && a[3] < straight && a[4] < straight)
    return "โอเค"

  if (a[0] > bent && a[1] > bent && a[2] < straight && a[3] > bent && a[4] > bent)
    return "ควย"

  return null
}
// ===== MQTT =====
broker.on("publish", async (packet, client) => {
  //console.log("📡 MQTT Publish:", packet.topic, packet.payload.toString());
  if (!client || !packet?.topic || !packet?.payload) return

  let payloadText
  try { payloadText = packet.payload.toString() } catch (_) { return }
  if (!payloadText?.trim()) return

  if (packet.topic === "iot/esp32s3_test/log") {
    console.log("📝 Log:", payloadText)
    return
  }

  if (packet.topic === "iot/esp32s3_test/sensor") {
    if (playing) return
    
    let data
    try { data = JSON.parse(payloadText) } catch (_) { return }

    const translated = translate(data)
    if (!translated) return

    console.log("🔤 Gesture:", translated)
    broker.publish({ topic: "iot/esp32s3_test/sign", payload: translated })
    textToSpeech(translated)
    playing = true
    setTimeout(() => {
      console.log("⏹️ Ready for next TTS")
      playing = false
    }, 3000);
    return
  }

  if (packet.topic === "iot/esp32s3_test/tts") {
    if (playing) return
    playing = true
    textToSpeech(payloadText)
    broker.publish({ topic: "iot/esp32s3_test/sign", payload: payloadText })
    setTimeout(() => {
      console.log("⏹️ Ready for next TTS")
      playing = false
    }, 4000);
  }
})