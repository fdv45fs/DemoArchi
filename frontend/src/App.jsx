import React, { useEffect, useState, useRef } from 'react'

// Configure this to match your C client-backend HTTP port
// Use Vite env var VITE_API_BASE (set in .env or via cross-env) or fallback to http://localhost:8000
const API_BASE = import.meta.env.VITE_API_BASE || 'http://localhost:8000'

export default function App() {
  const [value, setValue] = useState(null)
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState(null)
  const wsRef = useRef(null)

  async function fetchValue() {
    try {
      setError(null)
      const r = await fetch(`${API_BASE}/counter`)
      if (r.ok) {
        const j = await r.json()
        setValue(j.value)
      } else {
        const txt = await r.text().catch(() => r.status)
        setError(`GET failed: ${txt}`)
      }
    } catch (e) {
      setError(String(e))
    }
  }

  async function postAction(action) {
    setLoading(true)
    try {
      setError(null)
      const r = await fetch(`${API_BASE}/counter/${action}`, { method: 'POST' })
      if (r.ok) {
        const j = await r.json()
        setValue(j.value)
      } else {
        const txt = await r.text().catch(() => r.status)
        setError(`POST failed: ${txt}`)
      }
    } catch (e) {
      setError(String(e))
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => {
    // initial poll
    fetchValue()

    // try WebSocket for live updates
    try {
      const ws = new WebSocket((API_BASE.replace(/^http/, 'ws')) + '/ws')
      wsRef.current = ws
      ws.onmessage = (ev) => {
        try {
          const msg = JSON.parse(ev.data)
          if (msg.type === 'counter') setValue(msg.value)
        } catch (e) {
          console.error('ws parse', e)
        }
      }
      ws.onclose = () => { wsRef.current = null }
    } catch (e) {
      // ignore
    }

    const iv = setInterval(fetchValue, 3000)
    return () => clearInterval(iv)
  }, [])

  return (
    <div style={{ fontFamily: 'sans-serif', padding: 20 }}>
      <h1>Counter UI</h1>
  <div style={{ fontSize: 48, margin: '20px 0' }}>{value === null ? '—' : value}</div>
  {error && <div style={{ color: 'maroon', marginTop: 8 }}>{error}</div>}
      <div style={{ display: 'flex', gap: 8 }}>
  <button onClick={() => postAction('incr')} disabled={loading}>INCR</button>
  <button onClick={() => postAction('decr')} disabled={loading}>DECR</button>
  <button onClick={() => postAction('reset')} disabled={loading}>RESET</button>
  <button onClick={fetchValue} disabled={loading}>REFRESH</button>
      </div>

      <p style={{ marginTop: 20, color: '#666' }}>
        The frontend calls your client-backend at <code>{API_BASE}</code>. Ensure your C backend exposes
        these endpoints:
      </p>
      <pre style={{ background: '#f6f6f6', padding: 8 }}>
GET /counter
POST /counter/incr
POST /counter/decr
POST /counter/reset
GET /ws  (optional WebSocket)
      </pre>
    </div>
  )
}
