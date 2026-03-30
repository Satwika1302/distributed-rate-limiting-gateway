import { useState, useEffect, useCallback, useRef } from 'react';
import {
  LineChart, Line, AreaChart, Area, PieChart, Pie, Cell,
  XAxis, YAxis, Tooltip, ResponsiveContainer, CartesianGrid
} from 'recharts';

const GATEWAY_URL = 'http://localhost:8080';
const POLL_INTERVAL = 1500; // ms

// ── Parse Prometheus text format into a simple key→value map ────────────
function parseMetrics(text) {
  const result = {};
  for (const line of text.split('\n')) {
    if (line.startsWith('#') || !line.trim()) continue;
    const parts = line.split(' ');
    if (parts.length >= 2) result[parts[0]] = parseInt(parts[1], 10);
  }
  return result;
}

// ── Stat Card ────────────────────────────────────────────────────────────
function StatCard({ icon, label, value, sub, color }) {
  return (
    <div className={`stat-card ${color} fade-in`}>
      <div className="stat-icon">{icon}</div>
      <div className="stat-label">{label}</div>
      <div className="stat-value">{value.toLocaleString()}</div>
      <div className="stat-sub">{sub}</div>
    </div>
  );
}

// ── Custom Tooltip ───────────────────────────────────────────────────────
function CustomTip({ active, payload, label }) {
  if (!active || !payload?.length) return null;
  return (
    <div style={{ background: '#0d1724', border: '1px solid rgba(99,179,237,.25)', borderRadius: 8, padding: '8px 12px', fontSize: 12 }}>
      <p style={{ color: '#718096', marginBottom: 4 }}>T+{label}s</p>
      {payload.map(p => (
        <p key={p.dataKey} style={{ color: p.color, fontFamily: 'JetBrains Mono' }}>{p.name}: {p.value}</p>
      ))}
    </div>
  );
}

// ── Main App ─────────────────────────────────────────────────────────────
export default function App() {
  const [metrics, setMetrics] = useState({ gateway_requests_total: 0, gateway_401_total: 0, gateway_429_total: 0, gateway_404_total: 0 });
  const [history, setHistory] = useState([]); // rolling array of metric snapshots
  const [log, setLog] = useState([]);          // synthetic request log
  const [online, setOnline] = useState(false);
  const [tick, setTick] = useState(0);
  const prevMetrics = useRef(null);

  const fetchMetrics = useCallback(async () => {
    try {
      const res = await fetch(`${GATEWAY_URL}/metrics`, { cache: 'no-store' });
      if (!res.ok) throw new Error('bad');
      const text = await res.text();
      const parsed = parseMetrics(text);
      setOnline(true);
      setTick(t => t + 1);

      setMetrics(parsed);

      // Enrich history with rate-of-change per type (requests/s)
      setHistory(prev => {
        const now = Date.now();
        const last = prev[prev.length - 1];
        const entry = {
          t: (now / 1000).toFixed(0),
          total  : parsed.gateway_requests_total  ?? 0,
          r401   : parsed.gateway_401_total        ?? 0,
          r429   : parsed.gateway_429_total        ?? 0,
          r404   : parsed.gateway_404_total        ?? 0,
        };
        const next = [...prev, entry].slice(-40);
        return next;
      });

      // Synthetic log entry when total count changes
      if (prevMetrics.current !== null) {
        const delta = (parsed.gateway_requests_total ?? 0) - (prevMetrics.current.gateway_requests_total ?? 0);
        if (delta > 0) {
          const statuses = [
            ...(Array(delta - ((parsed.gateway_429_total ?? 0) - (prevMetrics.current.gateway_429_total ?? 0))
                           - ((parsed.gateway_401_total ?? 0) - (prevMetrics.current.gateway_401_total ?? 0))
                           - ((parsed.gateway_404_total ?? 0) - (prevMetrics.current.gateway_404_total ?? 0))).fill(200)),
            ...(Array((parsed.gateway_401_total ?? 0) - (prevMetrics.current.gateway_401_total ?? 0)).fill(401)),
            ...(Array((parsed.gateway_429_total ?? 0) - (prevMetrics.current.gateway_429_total ?? 0)).fill(429)),
            ...(Array((parsed.gateway_404_total ?? 0) - (prevMetrics.current.gateway_404_total ?? 0)).fill(404)),
          ];
          const paths = ['/api/users', '/api/orders'];
          const methods = ['GET', 'POST'];
          const newRows = statuses.map((status) => ({
            id: Math.random(),
            time: new Date().toLocaleTimeString(),
            method: methods[Math.floor(Math.random() * methods.length)],
            path: paths[status === 404 ? 1 : Math.floor(Math.random() * paths.length)],
            status,
            ms: (Math.random() * 4 + 0.8).toFixed(1),
          }));
          setLog(prev => [...newRows, ...prev].slice(0, 30));
        }
      }
      prevMetrics.current = parsed;
    } catch {
      setOnline(false);
    }
  }, []);

  useEffect(() => {
    fetchMetrics();
    const id = setInterval(fetchMetrics, POLL_INTERVAL);
    return () => clearInterval(id);
  }, [fetchMetrics]);

  const total  = metrics.gateway_requests_total ?? 0;
  const r401   = metrics.gateway_401_total       ?? 0;
  const r429   = metrics.gateway_429_total       ?? 0;
  const r404   = metrics.gateway_404_total       ?? 0;
  const ok     = Math.max(0, total - r401 - r429 - r404);
  const successRate = total > 0 ? ((ok / total) * 100).toFixed(1) : '100.0';

  const pieData = [
    { name: '2xx OK',  value: ok,   color: '#68d391' },
    { name: '401',     value: r401,  color: '#f6e05e' },
    { name: '429',     value: r429,  color: '#fc8181' },
    { name: '404',     value: r404,  color: '#b794f4' },
  ].filter(d => d.value > 0);

  const chartMin = history.length > 0 ? Math.min(...history.map(h => parseInt(h.t))) : 0;
  const relHistory = history.map(h => ({ ...h, t: (parseInt(h.t) - chartMin) }));

  return (
    <div className="dashboard">
      {/* ── Header ────────────────────────────────────────────────── */}
      <header>
        <div className="header-left">
          <div className="logo-icon">API</div>
          <div>
            <h1>Distributed Rate Limiting Gateway</h1>
            <p>High-Concurrency Edge Runtime • Token Bucket Enforcement</p>
          </div>
        </div>
        <div style={{ display: 'flex', gap: 12, alignItems: 'center' }}>
          <button className="refresh-btn" onClick={fetchMetrics}>Refresh</button>
          <div className={`status-pill ${online ? '' : 'offline'}`}>
            <div className={`dot ${online ? 'live' : 'offline'}`}></div>
            {online ? 'Gateway Online' : 'Gateway Offline'}
          </div>
        </div>
      </header>

      {/* KPI Row */}
      <div className="stat-grid">
        <StatCard icon="TP" label="Total Requests"    value={total}   sub="High-capacity traffic monitoring" color="blue" />
        <StatCard icon="OK" label="Authorized Throughput" value={ok}      sub="Successfully delivered to backend" color="green" />
        <StatCard icon="RL" label="429 Rate Limited"  value={r429}    sub="Strict Token Bucket protection" color="red" />
        <StatCard icon="UA" label="401 Unauthorized"  value={r401}    sub="JWT session validation"        color="yellow" />
      </div>

      {/* Charts Row */}
      <div className="charts-row">
        {/* Area Chart: cumulative traffic */}
        <div className="chart-card">
          <h3>Cumulative Traffic</h3>
          <p>Live counter values from /metrics endpoint - polling every 1.5s</p>
          <ResponsiveContainer width="100%" height={220}>
            <AreaChart data={relHistory}>
              <defs>
                {[['totalGrad','#63b3ed'],['okGrad','#68d391'],['limitGrad','#fc8181']].map(([id, c]) => (
                  <linearGradient key={id} id={id} x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%"  stopColor={c} stopOpacity={0.25} />
                    <stop offset="95%" stopColor={c} stopOpacity={0.02} />
                  </linearGradient>
                ))}
              </defs>
              <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,.04)" />
              <XAxis dataKey="t" tick={{ fill: '#4a5568', fontSize: 11 }} tickFormatter={v => `${v}s`} />
              <YAxis tick={{ fill: '#4a5568', fontSize: 11 }} allowDecimals={false} />
              <Tooltip content={<CustomTip />} />
              <Area type="monotone" dataKey="total" name="Total"    stroke="#63b3ed" fill="url(#totalGrad)" strokeWidth={2} dot={false} />
              <Area type="monotone" dataKey="r429"  name="429"      stroke="#fc8181" fill="url(#limitGrad)" strokeWidth={2} dot={false} />
            </AreaChart>
          </ResponsiveContainer>
        </div>

        {/* Pie Chart: status breakdown */}
        <div className="chart-card">
          <h3>Response Breakdown</h3>
          <p>Distribution by HTTP status class</p>
          {pieData.length > 0 ? (
            <ResponsiveContainer width="100%" height={220}>
              <PieChart>
                <Pie data={pieData} cx="50%" cy="50%" innerRadius={52} outerRadius={82}
                     dataKey="value" nameKey="name" paddingAngle={3}>
                  {pieData.map((e, i) => <Cell key={i} fill={e.color} />)}
                </Pie>
                <Tooltip formatter={(v, n) => [v, n]} contentStyle={{ background: '#0d1724', border: '1px solid #1a2744', fontSize: 12 }} />
              </PieChart>
            </ResponsiveContainer>
          ) : (
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: 220, color: '#4a5568', fontSize: 13 }}>
              No data yet — send some requests!
            </div>
          )}
          {/* Legend */}
          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px 16px', marginTop: 12 }}>
            {[{l:'2xx OK',c:'#68d391'},{l:'401 Auth',c:'#f6e05e'},{l:'429 Rate',c:'#fc8181'},{l:'404',c:'#b794f4'}].map(({l, c}) => (
              <span key={l} style={{ display:'flex', alignItems:'center', gap:5, fontSize:12, color:'#718096' }}>
                <span style={{ width:8, height:8, borderRadius:2, background:c, display:'inline-block' }}></span>{l}
              </span>
            ))}
          </div>
        </div>
      </div>

      {/* ── Request Log ───────────────────────────────────────────── */}
      <div className="log-card">
        <h3>Request Log</h3>
        <p>Synthesized from metric deltas — new rows appear live as traffic hits the gateway</p>
        {log.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '32px 0', color: '#4a5568', fontSize: 13 }}>
            Waiting for requests… Run <code style={{ color: '#63b3ed' }}>bash test_rate_limiter.sh</code> in a terminal!
          </div>
        ) : (
          <table className="log-table">
            <thead>
              <tr>
                <th>Time</th>
                <th>Method</th>
                <th>Path</th>
                <th>Status</th>
                <th>Latency</th>
              </tr>
            </thead>
            <tbody>
              {log.map(row => (
                <tr key={row.id}>
                  <td style={{ color: '#4a5568' }}>{row.time}</td>
                  <td style={{ color: '#b794f4' }}>{row.method}</td>
                  <td style={{ color: '#e2e8f0' }}>{row.path}</td>
                  <td><span className={`badge badge-${row.status}`}>{row.status}</span></td>
                  <td style={{ color: '#718096' }}>{row.ms}ms</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}
