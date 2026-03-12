'use client'
import { useEffect, useState, useRef } from 'react'
import mqtt from 'mqtt'

const FINGER_KEYS = ['thumb', 'index', 'middle', 'ring', 'little']

function SensorBar({ label, value }) {
  const pct = Math.min(100, Math.max(0, ((value ?? 0) / 180) * 100))
  return (
    <div className="flex items-center gap-3 py-1">
      <span className="w-24 text-xs font-medium text-slate-400 capitalize">{label}</span>
      <div className="flex-1 h-2 bg-slate-700 rounded-full overflow-hidden">
        <div className="h-full rounded-full transition-all duration-300"
          style={{ width: `${pct}%`, background: `linear-gradient(90deg, #38bdf8, #818cf8)` }} />
      </div>
      <span className="w-10 text-right text-xs font-mono text-slate-300">{value ?? '--'}</span>
    </div>
  )
}

function HandPanel({ side, data }) {
  const accent = side === 'left' ? '#38bdf8' : '#a78bfa'
  return (
    <div className="rounded-2xl p-4" style={{
      background: 'rgba(255,255,255,0.03)', border: `1px solid ${accent}33`,
      boxShadow: `0 0 20px ${accent}11`, width: '220px', flexShrink: 0
    }}>
      <div className="flex items-center gap-2 mb-4">
        <span className="w-2 h-2 rounded-full animate-pulse" style={{ background: accent }} />
        <h2 className="text-sm font-semibold tracking-widest uppercase" style={{ color: accent }}>{side} Hand</h2>
      </div>
      <div className="space-y-1">
        {FINGER_KEYS.map(f => <SensorBar key={f} label={f} value={data?.fingers?.[f]} />)}
      </div>
      <div className="mt-4 pt-3 border-t border-slate-700/50 space-y-1">
        <div className="flex items-center gap-3">
          <span className="w-12 text-xs font-medium text-slate-400">Pitch</span>
          <span className="text-xs font-mono text-slate-300">{data?.pitch ? `${Number(data.pitch).toFixed(1)}°` : '--'}</span>
          <span className="w-8 text-xs font-medium text-slate-400">Roll</span>
          <span className="text-xs font-mono text-slate-300">{data?.roll ? `${Number(data.roll).toFixed(1)}°` : '--'}</span>
        </div>
        <div className="flex items-center gap-2">
          <span className="w-12 text-xs font-medium text-slate-400">Accel</span>
          <span className="text-xs font-mono text-slate-300">
            {data?.accel ? `${Number(data.accel.x).toFixed(1)}, ${Number(data.accel.y).toFixed(1)}, ${Number(data.accel.z).toFixed(1)}` : '--'}
          </span>
        </div>
      </div>
    </div>
  )
}

function ChatBubble({ msg }) {
  const isListen = msg.type === 'listen'
  return (
    <div className={`flex ${isListen ? 'justify-start' : 'justify-end'} mb-3`}>
      <div>
        <div className="text-[10px] font-semibold tracking-wider mb-1 uppercase"
          style={{ color: isListen ? '#38bdf8' : '#a78bfa', textAlign: isListen ? 'left' : 'right' }}>
          {isListen ? '🎤 Listening' : '🤟 Sign Language'}
        </div>
        <div className="px-4 py-2 rounded-2xl text-sm font-medium" style={{
          background: isListen ? 'rgba(56,189,248,0.12)' : 'rgba(167,139,250,0.12)',
          border: `1px solid ${isListen ? '#38bdf844' : '#a78bfa44'}`,
          color: isListen ? '#e0f2fe' : '#ede9fe', maxWidth: '200px'
        }}>
          {msg.text}
        </div>
        <div className="text-[9px] text-slate-600 mt-1" style={{ textAlign: isListen ? 'left' : 'right' }}>{msg.time}</div>
      </div>
    </div>
  )
}

const INTERVAL_OPTIONS = [
  { label: '100ms', value: 100 },
  { label: '250ms', value: 250 },
  { label: '500ms', value: 500 },
  { label: '1s', value: 1000 },
  { label: '2s', value: 2000 },
  { label: '5s', value: 5000 },
  { label: '10s', value: 10000 },
  { label: '30s', value: 30000 },
]

export default function Dashboard() {
  const [client, setClient] = useState(null)
  const [connected, setConnected] = useState(false)
  const [leftData, setLeftData] = useState(null)
  const [rightData, setRightData] = useState(null)
  const [chatHistory, setChatHistory] = useState([])
  const [logs, setLogs] = useState([])
  const [ttsText, setTtsText] = useState('')
  const [interval, setIntervalVal] = useState(5000)
  const [sentInterval, setSentInterval] = useState(5000)
  const [zeroing, setZeroing] = useState(false)
  const chatRef = useRef(null)
  const logRef = useRef(null)
  const clientRef = useRef(null)

  useEffect(() => {
    const MQTT_BROKER_IP = "Your MQTT Broker IP"
    const c = mqtt.connect(`ws://${MQTT_BROKER_IP}:8888`)
    setClient(c)
    clientRef.current = c

    c.on('connect', () => {
      setConnected(true)
      c.subscribe('iot/+/sensor')
      c.subscribe('iot/+/speech')
      c.subscribe('iot/+/sign')
      c.subscribe('iot/+/log')
    })

    c.on('message', (topic, message) => {
      try {
        const text = message.toString()
        const now = new Date().toLocaleTimeString('th-TH')
        if (topic.includes('/sensor')) {
          const json = JSON.parse(text)
          if (json.hand === 'left') setLeftData(json)
          if (json.hand === 'right') setRightData(json)
        }
        if (topic.includes('/speech')) {
          setChatHistory(h => [...h, { id: Date.now(), type: 'listen', text, time: now }])
        }
        if (topic.includes('/sign')) {
          setChatHistory(h => [...h, { id: Date.now(), type: 'sign', text, time: now }])
        }
        if (topic.includes('/log')) {
          setLogs(l => [...l.slice(-199), { id: Date.now(), text, time: now }])
        }
      } catch (_) { }
    })

    c.on('close', () => setConnected(false))
    return () => c.end()
  }, [])

  useEffect(() => {
    if (chatRef.current) chatRef.current.scrollTop = chatRef.current.scrollHeight
  }, [chatHistory])

  useEffect(() => {
    if (logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight
  }, [logs])

  const sendTTS = () => {
    if (!client || !ttsText.trim()) return
    client.publish('iot/esp32s3_test/tts', ttsText)
    setTtsText('')
  }

  const applyInterval = () => {
    if (!client) return
    client.publish('iot/esp32s3_test/set_interval', String(interval))
    setSentInterval(interval)
  }

  const zeroIMU = () => {
    if (!client) return
    setZeroing(true)
    client.publish('iot/esp32s3_test/zero_imu', '1')
    setTimeout(() => setZeroing(false), 2000)
  }

  return (
    <div className="min-h-screen p-4" style={{
      background: '#0a0e1a',
      fontFamily: "'DM Mono', 'Courier New', monospace",
      color: '#e2e8f0'
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=DM+Mono:wght@400;500&family=Syne:wght@600;800&display=swap');
        * { box-sizing: border-box; }
        ::-webkit-scrollbar { width: 4px; }
        ::-webkit-scrollbar-track { background: transparent; }
        ::-webkit-scrollbar-thumb { background: #334155; border-radius: 99px; }
        @keyframes fadein { from { opacity:0; transform: translateY(8px); } to { opacity:1; transform: translateY(0); } }
        @keyframes pulse-green { 0%,100% { box-shadow: 0 0 6px #4ade80; } 50% { box-shadow: 0 0 14px #4ade80; } }
        .bubble-in { animation: fadein 0.3s ease forwards; }
      `}</style>

      {/* Header */}
      <div className="flex items-center justify-between mb-4 flex-wrap gap-3">
        <div>
          <h1 style={{
            fontFamily: "'Syne', sans-serif", fontSize: '1.8rem', fontWeight: 800,
            background: 'linear-gradient(135deg, #38bdf8, #a78bfa)',
            WebkitBackgroundClip: 'text', WebkitTextFillColor: 'transparent', lineHeight: 1.1
          }}>SoundArai</h1>
          <p className="text-xs text-slate-500 mt-1 tracking-widest uppercase">Project</p>
        </div>

        <div className="flex items-center gap-3 flex-wrap">
          {/* Interval */}
          <div className="flex items-center gap-2 px-3 py-2 rounded-2xl" style={{ background: 'rgba(255,255,255,0.04)', border: '1px solid #1e293b' }}>
            <span className="text-xs text-slate-400 tracking-widest uppercase">Interval</span>
            <div className="flex gap-1 flex-wrap">
              {INTERVAL_OPTIONS.map(opt => (
                <button key={opt.value} onClick={() => setIntervalVal(opt.value)}
                  className="px-2 py-1 rounded-lg text-[10px] font-bold transition-all"
                  style={{
                    background: interval === opt.value ? 'linear-gradient(135deg, #38bdf8, #818cf8)' : 'rgba(255,255,255,0.06)',
                    color: interval === opt.value ? '#0a0e1a' : '#94a3b8',
                    border: interval === opt.value ? 'none' : '1px solid #1e293b'
                  }}>
                  {opt.label}
                </button>
              ))}
            </div>
            <button onClick={applyInterval} className="px-3 py-1 rounded-lg text-[10px] font-bold tracking-widest uppercase transition-all"
              style={{
                background: interval !== sentInterval ? 'linear-gradient(135deg, #4ade80, #22d3ee)' : 'rgba(255,255,255,0.06)',
                color: interval !== sentInterval ? '#0a0e1a' : '#475569'
              }}>
              {interval !== sentInterval ? '● Apply' : '✓'}
            </button>
          </div>

          {/* Zero IMU */}
          <button onClick={zeroIMU} disabled={zeroing}
            className="flex items-center gap-2 px-4 py-2 rounded-2xl text-xs font-bold tracking-widest uppercase transition-all"
            style={{
              background: zeroing ? 'rgba(251,191,36,0.15)' : 'rgba(251,191,36,0.1)',
              border: `1px solid ${zeroing ? '#fbbf24aa' : '#fbbf2444'}`,
              color: zeroing ? '#fbbf24' : '#92714a',
              cursor: zeroing ? 'not-allowed' : 'pointer'
            }}>
            <span style={{ fontSize: '14px' }}>{zeroing ? '⏳' : '🧭'}</span>
            {zeroing ? 'Zeroing...' : 'Zero IMU'}
          </button>

          {/* MQTT status */}
          <div className="flex items-center gap-2 px-4 py-2 rounded-full"
            style={{ background: 'rgba(255,255,255,0.04)', border: '1px solid #1e293b' }}>
            <span className="w-2 h-2 rounded-full"
              style={{ background: connected ? '#4ade80' : '#f87171', boxShadow: connected ? '0 0 6px #4ade80' : 'none' }} />
            <span className="text-xs font-medium" style={{ color: connected ? '#4ade80' : '#f87171' }}>
              {connected ? 'Connected' : 'Disconnected'}
            </span>
          </div>
        </div>
      </div>

      {/* Main layout */}
      <div className="flex gap-3" style={{ height: 'calc(100vh - 210px)', minHeight: '480px' }}>

        {/* Left Hand */}
        <HandPanel side="left" data={leftData} />

        {/* Center: Chat + Log */}
        <div className="flex-1 flex flex-col gap-3 min-w-0">

          {/* Chat */}
          <div className="flex-1 rounded-2xl flex flex-col" style={{
            background: 'rgba(255,255,255,0.025)', border: '1px solid #1e293b', minHeight: 0
          }}>
            <div className="px-4 py-2 border-b flex justify-between items-center" style={{ borderColor: '#1e293b' }}>
              <h2 className="text-xs font-semibold tracking-widest uppercase text-slate-400">💬 Chat History</h2>
              <div className="flex gap-3">
                <span className="text-[10px] tracking-widest uppercase" style={{ color: '#38bdf8' }}>Listening</span>
                <span className="text-[10px] tracking-widest uppercase" style={{ color: '#a78bfa' }}>Sign Language</span>
              </div>
            </div>
            <div ref={chatRef} className="flex-1 overflow-y-auto px-4 py-2">
              {chatHistory.length === 0 && (
                <div className="text-center text-slate-600 text-xs mt-8">ยังไม่มีข้อความ</div>
              )}
              {chatHistory.map(msg => (
                <div key={msg.id} className="bubble-in"><ChatBubble msg={msg} /></div>
              ))}
            </div>
          </div>

          {/* Log */}
          <div className="rounded-2xl flex flex-col" style={{
            background: 'rgba(255,255,255,0.02)', border: '1px solid #1e293b', height: '160px'
          }}>
            <div className="px-4 py-2 border-b flex justify-between items-center" style={{ borderColor: '#1e293b' }}>
              <h2 className="text-xs font-semibold tracking-widest uppercase text-slate-400">🖥️ Device Log</h2>
              <button onClick={() => setLogs([])}
                className="text-[10px] text-slate-600 hover:text-slate-400 transition-colors tracking-widest uppercase">
                Clear
              </button>
            </div>
            <div ref={logRef} className="flex-1 overflow-y-auto px-4 py-2 font-mono">
              {logs.length === 0 && (
                <div className="text-slate-700 text-xs mt-2">รอรับ log จาก ESP32...</div>
              )}
              {logs.map(log => (
                <div key={log.id} className="flex gap-3 mb-1">
                  <span className="text-[10px] text-slate-600 shrink-0">{log.time}</span>
                  <span className="text-[11px] text-emerald-400">{log.text}</span>
                </div>
              ))}
            </div>
          </div>
        </div>

        {/* Right Hand */}
        <HandPanel side="right" data={rightData} />
      </div>

      {/* TTS Input */}
      <div className="mt-3 flex gap-3 items-center rounded-2xl px-4 py-3"
        style={{ background: 'rgba(255,255,255,0.03)', border: '1px solid #1e293b' }}>
        <span className="text-lg">🗣️</span>
        <input
          className="flex-1 bg-transparent text-sm outline-none placeholder-slate-600"
          placeholder="Type text for ESP32 to speak (TTS)..."
          value={ttsText}
          onChange={e => setTtsText(e.target.value)}
          onKeyDown={e => e.key === 'Enter' && sendTTS()}
          style={{ color: '#e2e8f0' }}
        />
        <button onClick={sendTTS} className="px-5 py-2 rounded-xl text-xs font-bold tracking-widest uppercase transition-all"
          style={{
            background: ttsText ? 'linear-gradient(135deg, #38bdf8, #818cf8)' : '#1e293b',
            color: ttsText ? '#0a0e1a' : '#475569',
            cursor: ttsText ? 'pointer' : 'default'
          }}>
          Send
        </button>
      </div>
    </div>
  )
}
