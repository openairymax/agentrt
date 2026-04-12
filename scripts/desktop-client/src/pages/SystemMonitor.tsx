import React, { useState, useEffect } from "react";
import {
  Cpu, HardDrive, Wifi, Activity, Clock, Zap, Server, RefreshCw, Search,
  AlertTriangle, CheckCircle2, XCircle, Globe, FolderOpen, FileCode,
  Trash2, Terminal as TerminalIcon, Copy, Download, Upload, ChevronRight,
  Monitor,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import type { SystemMonitorData, ProcessInfo, NetworkInterface, PortCheckResult, DirectoryListing } from "../services/agentos-sdk";
import { useAlert } from "../components/useAlert";

const formatBytes = (b: number) => b < 1024 ? b + "B" : b < 1048576 ? (b / 1024).toFixed(1) + "KB" : (b / 1048576).toFixed(1) + "MB";
const fmtUptime = (s: number) => {
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
  return d > 0 ? d + "天" + h + "时" + m + "分" : h > 0 ? h + "时" + m + "分" : m + "分钟";
};

const TABS = [
  { k: "overview" as const, icon: Activity, label: "总览", color: "#6366f1" },
  { k: "processes" as const, icon: Cpu, label: "进程管理", color: "#f59e0b" },
  { k: "network" as const, icon: Wifi, label: "网络诊断", color: "#06b6d4" },
  { k: "files" as const, icon: FolderOpen, label: "文件浏览", color: "#22c55e" },
  { k: "diagnostics" as const, icon: Search, label: "诊断工具", color: "#a855f7" },
];

function MiniGauge({ pct, color, size = 48 }: { pct: number; color: string; size?: number }) {
  const r = (size - 5) / 2;
  const circ = 2 * Math.PI * r;
  const off = circ - (Math.min(pct, 100) / 100) * circ;
  return (
    <svg width={size} height={size} style={{ transform: "rotate(-90deg)" }}>
      <circle cx={size/2} cy={size/2} r={r} fill="none" stroke="var(--border-subtle)" strokeWidth={4} />
      <circle cx={size/2} cy={size/2} r={r} fill="none" stroke={color} strokeWidth={4}
        strokeDasharray={circ} strokeDashoffset={off} strokeLinecap="round"
        style={{ transition: "stroke-dashoffset 0.8s cubic-bezier(.34,1.56,.64,1)" }} />
    </svg>
  );
}

const SystemMonitor: React.FC = () => {
  const { error, confirm: confirmModal } = useAlert();
  const [tab, setTab] = useState<"overview" | "processes" | "network" | "files" | "diagnostics">("overview");
  const [data, setData] = useState<SystemMonitorData | null>(null);
  const [procs, setProcs] = useState<ProcessInfo[]>([]);
  const [nets, setNets] = useState<NetworkInterface[]>([]);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [fpath, setFpath] = useState("/");
  const [flist, setFlist] = useState<DirectoryListing | null>(null);
  const [floading, setFloading] = useState(false);
  const [pingTgt, setPingTgt] = useState("localhost");
  const [portHost, setPortHost] = useState("localhost");
  const [portNum, setPortNum] = useState(8080);
  const [dnsTgt, setDnsTgt] = useState("google.com");
  const [diagResults, setDiagResults] = useState<Array<{ type: string; label: string; result: unknown; time: number }>>([]);

  const loadAll = async () => {
    try {
      const [m, p, n] = await Promise.all([
        sdk.getSystemMonitorData().catch(() => null),
        sdk.listProcesses().catch(() => []),
        sdk.getNetworkInterfaces().catch(() => []),
      ]);
      setData(m); setProcs(p); setNets(n);
    } catch (err) {
      error("加载失败", `无法加载系统监控数据: ${err}`);
    } finally { setLoading(false); setRefreshing(false); }
  };

  const loadDir = async (p: string) => {
    setFloading(true);
    try { setFlist(await sdk.listDirectory(p)); setFpath(p); } catch (err) { error("读取失败", `无法读取目录: ${err}`); }
    finally { setFloading(false); }
  };

  const doPing = async () => {
    if (!pingTgt.trim()) return;
    const t = Date.now();
    try { const r = await sdk.ping(pingTgt, 4); setDiagResults(d => [{ type: "ping", label: "Ping " + pingTgt, result: r, time: Date.now() - t }, ...d]); }
    catch (e) { setDiagResults(d => [{ type: "ping", label: "Ping " + pingTgt, result: { error: String(e) }, time: Date.now() - t }, ...d]); }
  };

  const doPort = async () => {
    if (!portHost.trim()) return;
    const t = Date.now();
    try { const r = await sdk.checkPort(portHost, portNum); setDiagResults(d => [{ type: "port", label: "端口 " + portHost + ":" + portNum, result: r, time: Date.now() - t }, ...d]); }
    catch (e) { setDiagResults(d => [{ type: "port", label: "端口 " + portHost + ":" + portNum, result: { error: String(e) }, time: Date.now() - t }, ...d]); }
  };

  const doDns = async () => {
    if (!dnsTgt.trim()) return;
    const t = Date.now();
    try { const r = await sdk.dnsLookup(dnsTgt); setDiagResults(d => [{ type: "dns", label: "DNS " + dnsTgt, result: r, time: Date.now() - t }, ...d]); }
    catch (e) { setDiagResults(d => [{ type: "dns", label: "DNS " + dnsTgt, result: { error: String(e) }, time: Date.now() - t }, ...d]); }
  };

  const killProc = async (pid: number) => {
    const confirmed = await confirmModal({
      type: 'danger',
      title: '终止进程',
      message: `确定终止进程 PID=${pid} 吗？`,
    });
    if (!confirmed) return;
    try { await sdk.killProcess(pid); setProcs(procs.filter(p => p.pid !== pid)); } catch (err) { error("终止失败", `无法终止进程: ${err}`); }
  };

  useEffect(() => { loadAll(); }, []);
  useEffect(() => { if (!autoRefresh) return; const iv = setInterval(loadAll, 5000); return () => clearInterval(iv); }, [autoRefresh]);

  if (loading && !data) return (
    <div style={{ display: "flex", flexDirection: "column", gap: 20, alignItems: "center", justifyContent: "center", padding: "80px 20px" }}>
      <div className="loading-spinner" style={{ width: 40, height: 40 }} />
      <div style={{ color: "var(--text-secondary)", fontSize: 14 }}>正在加载系统监控数据...</div>
    </div>
  );

  return (
    <div className="page-container">
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
          <div style={{
            width: "44px", height: "44px", borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#f59e0b,#fbbf24)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(245,158,11,0.35), 0 0 0 1px rgba(255,255,255,0.08) inset",
          }}>
            <Monitor size={20} color="white" />
          </div>
          <div>
            <h1 style={{ display: "flex", alignItems: "center", gap: "10px" }}>
              系统监控
              <span style={{
                padding: "3px 10px", borderRadius: "var(--radius-full)", fontSize: "11px", fontWeight: 600,
                background: data ? "rgba(34,197,94,0.1)" : "rgba(245,158,11,0.1)",
                border: `1px solid ${data ? "rgba(34,197,94,0.2)" : "rgba(245,158,11,0.2)"}`,
                color: data ? "#22c55e" : "#f59e0b",
              }}>
                {data ? "已连接" : "模拟模式"}
              </span>
            </h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: 0 }}>
              实时资源监控 · 进程管理 · 网络诊断
            </p>
          </div>
        </div>
        <div style={{ display: "flex", gap: "8px", alignItems: "center" }}>
          <label style={{ display: "flex", alignItems: "center", gap: "6px", cursor: "pointer", fontSize: "12.5px", color: "var(--text-secondary)" }}>
            <input type="checkbox" checked={autoRefresh} onChange={e => setAutoRefresh(e.target.checked)} /> 自动刷新
          </label>
          <button className="btn btn-primary btn-sm" onClick={() => { setRefreshing(true); loadAll(); }} disabled={refreshing}>
            <RefreshCw size={14} className={refreshing ? "spin" : ""} /> 刷新
          </button>
        </div>
      </div>

      {/* Tab Bar */}
      <div style={{
        display: "flex", gap: "4px", marginBottom: "20px",
        background: "var(--bg-secondary)", padding: "4px", borderRadius: "var(--radius-lg)",
        border: "1px solid var(--border-subtle)",
      }}>
        {TABS.map(t => {
          const TabIcon = t.icon;
          return (
            <button key={t.k} onClick={() => setTab(t.k)} style={{
              padding: "8px 18px", border: "none", borderRadius: "var(--radius-md)",
              background: tab === t.k ? t.color : "transparent",
              color: tab === t.k ? "white" : "var(--text-secondary)",
              cursor: "pointer", fontWeight: 600, fontSize: "12.5px",
              transition: "all var(--transition-fast)",
              display: "flex", alignItems: "center", gap: "6px",
              boxShadow: tab === t.k ? `0 2px 8px ${t.color}40` : "none",
            }}>
              <TabIcon size={14} />{t.label}
            </button>
          );
        })}
      </div>

      {/* ═══ OVERVIEW ═══ */}
      {tab === "overview" && data && (
        <>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(220px, 1fr))", gap: "14px" }}>
            {[
              { icon: Cpu, label: "CPU 使用率", val: Math.round(data.cpu.usage_percent) + "%", sub: data.cpu.cores.length + " 核心", c: "#f59e0b", pct: data.cpu.usage_percent / 100 },
              { icon: HardDrive, label: "内存使用", val: Math.round(data.memory.used_gb) + "GB / " + Math.round(data.memory.total_gb) + "GB", sub: Math.round(data.memory.percent) + "%", c: "#6366f1", pct: data.memory.percent / 100 },
              { icon: Server, label: "磁盘使用", val: Math.round(data.disk.used_gb) + "GB / " + Math.round(data.disk.total_gb) + "GB", sub: Math.round(data.disk.percent) + "%", c: "#22c55e", pct: data.disk.percent / 100 },
              { icon: Clock, label: "运行时间", val: fmtUptime(data.uptime_seconds), sub: "", c: "#06b6d4", pct: 0 },
            ].map((card, i) => {
              const IconComp = card.icon;
              return (
                <div key={i} className="card card-elevated" style={{
                  animation: `staggerFadeIn 0.35s ease-out ${i * 70}ms both`,
                  position: "relative", overflow: "hidden",
                }}>
                  <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: `linear-gradient(90deg,${card.c}40,${card.c}10)` }} />
                  <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: "14px" }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
                      <div style={{
                        width: "40px", height: "40px", borderRadius: "var(--radius-md)",
                        background: `${card.c}12`, display: "flex", alignItems: "center", justifyContent: "center",
                      }}>
                        <IconComp size={19} color={card.c} />
                      </div>
                      <span style={{ fontSize: "13px", color: "var(--text-muted)", fontWeight: 500 }}>{card.label}</span>
                    </div>
                    {card.pct > 0 && <MiniGauge pct={card.pct * 100} color={card.c} size={44} />}
                  </div>
                  <div style={{ fontSize: "22px", fontWeight: 800, fontFamily: "var(--font-mono)", color: card.c, letterSpacing: "-0.02em" }}>{card.val}</div>
                  {card.sub && <div style={{ fontSize: "11.5px", color: "var(--text-muted)", marginTop: "4px" }}>{card.sub}</div>}
                  {card.pct > 0 && (
                    <div style={{ height: "4px", background: "var(--bg-tertiary)", borderRadius: "2px", marginTop: "12px", overflow: "hidden" }}>
                      <div style={{
                        width: Math.min(card.pct * 100, 100) + "%", height: "100%",
                        background: `linear-gradient(90deg,${card.c},${card.c}aa)`,
                        borderRadius: "2px", transition: "width 0.6s ease-out",
                      }} />
                    </div>
                  )}
                </div>
              );
            })}
          </div>

          <div className="grid-2" style={{ marginTop: "16px" }}>
            <div className="card card-elevated" style={{ position: "relative", overflow: "hidden" }}>
              <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#f59e0b40,#f59e0b10)" }} />
              <h3 className="card-title">
                <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "rgba(245,158,11,0.1)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                  <Cpu size={14} color="#f59e0b" />
                </div>
                CPU 核心详情
              </h3>
              <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(90px, 1fr))", gap: "8px" }}>
                {data.cpu.cores.map((c, i) => (
                  <div key={i} style={{
                    padding: "10px", borderRadius: "var(--radius-md)",
                    background: c.usage > 70 ? "rgba(239,68,68,0.06)" : c.usage > 30 ? "rgba(245,158,11,0.06)" : "rgba(34,197,94,0.04)",
                    textAlign: "center", border: `1px solid ${c.usage > 70 ? "rgba(239,68,68,0.12)" : c.usage > 30 ? "rgba(245,158,11,0.12)" : "var(--border-subtle)"}`,
                    transition: "all var(--transition-fast)",
                  }}>
                    <div style={{ fontSize: "16px", fontWeight: 800, color: c.usage > 70 ? "#ef4444" : c.usage > 30 ? "#f59e0b" : "#22c55e", fontFamily: "var(--font-mono)" }}>
                      {Math.round(c.usage)}%
                    </div>
                    <div style={{ fontSize: "10px", color: "var(--text-muted)", marginTop: "2px", fontWeight: 600 }}>Core #{i}</div>
                  </div>
                ))}
              </div>
            </div>

            <div className="card card-elevated" style={{ position: "relative", overflow: "hidden" }}>
              <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#6366f140,#6366f110)" }} />
              <h3 className="card-title">
                <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "var(--primary-light)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                  <HardDrive size={14} color="#6366f1" />
                </div>
                内存 & 磁盘
              </h3>
              <div style={{ display: "flex", flexDirection: "column", gap: "16px", padding: "8px 0" }}>
                {[
                  { l: "已使用", v: data.memory.used_gb, mx: data.memory.total_gb, c: "#6366f1" },
                  { l: "可用", v: data.memory.free_gb, mx: data.memory.total_gb, c: "#22c55e" },
                ].map(item => (
                  <div key={item.l}>
                    <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "6px" }}>
                      <span style={{ fontSize: "13px", fontWeight: 500 }}>{item.l}</span>
                      <span style={{ fontSize: "13px", fontWeight: 700, color: item.c, fontFamily: "var(--font-mono)" }}>
                        {item.v.toFixed(1)}GB / {item.mx.toFixed(1)}GB
                      </span>
                    </div>
                    <div style={{ height: "8px", background: "var(--bg-tertiary)", borderRadius: "4px", overflow: "hidden" }}>
                      <div style={{
                        width: (item.v / item.mx * 100) + "%", height: "100%",
                        background: `linear-gradient(90deg,${item.c},${item.c}aa)`,
                        borderRadius: "4px", transition: "width 0.6s ease-out",
                      }} />
                    </div>
                  </div>
                ))}
                <div style={{ marginTop: "12px", paddingTop: "12px", borderTop: "1px solid var(--border-subtle)" }}>
                  <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "6px" }}>
                    <span style={{ fontSize: "13px", fontWeight: 500 }}>磁盘</span>
                    <span style={{ fontSize: "13px", fontWeight: 700, color: "#22c55e", fontFamily: "var(--font-mono)" }}>
                      {data.disk.used_gb.toFixed(0)}GB / {data.disk.total_gb.toFixed(0)}GB ({Math.round(data.disk.percent)}%)
                    </span>
                  </div>
                  <div style={{ height: "8px", background: "var(--bg-tertiary)", borderRadius: "4px", overflow: "hidden" }}>
                    <div style={{
                      width: data.disk.percent + "%", height: "100%",
                      background: "linear-gradient(90deg,#22c55e,#4ade80)",
                      borderRadius: "4px", transition: "width 0.6s ease-out",
                    }} />
                  </div>
                </div>
              </div>
            </div>
          </div>

          {nets.length > 0 && (
            <div className="card card-elevated" style={{ marginTop: "16px", position: "relative", overflow: "hidden" }}>
              <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#06b6d440,#06b6d410)" }} />
              <h3 className="card-title">
                <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "var(--cyan-light)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                  <Wifi size={14} color="#06b6d4" />
                </div>
                网络接口
              </h3>
              <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(340px, 1fr))", gap: "12px" }}>
                {nets.map((iface, i) => (
                  <div key={i} style={{
                    padding: "16px", borderRadius: "var(--radius-md)",
                    border: `1px solid ${iface.is_up ? "rgba(34,197,94,0.2)" : "var(--border-subtle)"}`,
                    background: iface.is_up ? "rgba(34,197,94,0.03)" : "var(--bg-tertiary)",
                    animation: `staggerFadeIn 0.3s ease-out ${i * 60}ms both`,
                  }}>
                    <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: "10px" }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                        <Wifi size={18} color={iface.is_up ? "#22c55e" : "#94a3b8"} />
                        <strong>{iface.name}</strong>
                      </div>
                      <span className={`tag ${iface.is_up ? "status-running" : "status-stopped"}`} style={{ fontSize: "10.5px" }}>
                        {iface.is_up ? "在线" : "离线"}
                      </span>
                    </div>
                    <div style={{ display: "flex", flexDirection: "column", gap: "4px", fontSize: "12.5px", fontFamily: "var(--font-mono)", color: "var(--text-secondary)" }}>
                      <span><Globe size={11} style={{ marginRight: "5px", verticalAlign: "middle", opacity: 0.5 }} />IPv4: {iface.ipv4}</span>
                      <span style={{ display: "flex", gap: "16px", marginTop: "4px" }}>
                        <span><Upload size={11} style={{ verticalAlign: "middle", color: "#22c55e" }} /> ↑{formatBytes(iface.bytes_sent)}</span>
                        <span><Download size={11} style={{ verticalAlign: "middle", color: "#6366f1" }} /> ↓{formatBytes(iface.bytes_recv)}</span>
                      </span>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}
        </>
      )}

      {/* ═══ PROCESSES ═══ */}
      {tab === "processes" && (
        <div className="card card-elevated" style={{ position: "relative", overflow: "hidden" }}>
          <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#f59e0b40,#f59e0b10)" }} />
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "16px" }}>
            <h3 className="card-title">
              <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "rgba(245,158,11,0.1)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                <Cpu size={14} color="#f59e0b" />
              </div>
              进程列表
              <span style={{ fontSize: "12px", color: "var(--text-muted)", fontWeight: 400, marginLeft: "4px" }}>({procs.length})</span>
            </h3>
            <button className="btn btn-ghost btn-sm" onClick={() => sdk.listProcesses().then(setProcs).catch(() => {})}>
              <RefreshCw size={14} />
            </button>
          </div>
          <div style={{ overflowX: "auto" }}>
            <table style={{ width: "100%", borderCollapse: "collapse", fontSize: "13px" }}>
              <thead>
                <tr style={{ borderBottom: "2px solid var(--border-subtle)" }}>
                  {["PID", "进程名", "状态", "CPU%", "内存(MB)", "命令", "操作"].map(h => (
                    <th key={h} style={{
                      padding: "10px 12px", textAlign: "left", fontWeight: 700,
                      color: "var(--text-muted)", fontSize: "11px", textTransform: "uppercase",
                      letterSpacing: "0.04em", whiteSpace: "nowrap",
                    }}>{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {procs.length === 0 ? (
                  <tr><td colSpan={7} style={{ textAlign: "center", padding: 48, color: "var(--text-muted)" }}>暂无进程数据</td></tr>
                ) : procs.map((p, i) => (
                  <tr key={p.pid} style={{
                    borderBottom: "1px solid var(--border-subtle)",
                    animation: `staggerFadeIn 0.25s ease-out ${i * 30}ms both`,
                    transition: "background var(--transition-fast)",
                  }}
                    onMouseEnter={e => e.currentTarget.style.background = "var(--bg-tertiary)"}
                    onMouseLeave={e => e.currentTarget.style.background = ""}
                  >
                    <td style={{ padding: "10px 12px", fontFamily: "var(--font-mono)", color: "var(--primary-color)", whiteSpace: "nowrap", fontWeight: 600 }}>{p.pid}</td>
                    <td style={{ padding: "10px 12px", fontWeight: 500 }}>{p.name}</td>
                    <td style={{ padding: "10px 12px" }}>
                      <span className={`tag ${p.status === "running" ? "status-running" : ""}`} style={{ fontSize: "10px" }}>{p.status}</span>
                    </td>
                    <td style={{
                      padding: "10px 12px", fontFamily: "var(--font-mono)",
                      color: (p.cpu_percent || 0) > 50 ? "#ef4444" : (p.cpu_percent || 0) > 20 ? "#f59e0b" : "inherit",
                      fontWeight: (p.cpu_percent || 0) > 20 ? 700 : 400,
                    }}>{(p.cpu_percent || 0).toFixed(1)}</td>
                    <td style={{ padding: "10px 12px", fontFamily: "var(--font-mono)" }}>{(p.memory_mb || 0).toFixed(0)}</td>
                    <td style={{ padding: "10px 12px", maxWidth: 300, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap", color: "var(--text-muted)", fontSize: "12px" }}>{p.command}</td>
                    <td style={{ padding: "10px 12px" }}>
                      <button className="btn btn-danger btn-sm" onClick={() => killProc(p.pid)} title="终止进程" style={{ padding: "3px 8px", fontSize: "11px" }}>
                        <XCircle size={12} />
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      )}

      {/* ═══ NETWORK ═══ */}
      {tab === "network" && nets.length > 0 && (
        <div className="card card-elevated" style={{ position: "relative", overflow: "hidden" }}>
          <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#06b6d440,#06b6d410)" }} />
          <h3 className="card-title">
            <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "var(--cyan-light)", display: "flex", alignItems: "center", justifyContent: "center" }}>
              <Wifi size={14} color="#06b6d4" />
            </div>
            网络接口
          </h3>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(360px, 1fr))", gap: "12px" }}>
            {nets.map((iface, i) => (
              <div key={i} style={{
                padding: "16px", borderRadius: "var(--radius-md)",
                border: `1px solid ${iface.is_up ? "rgba(34,197,94,0.2)" : "var(--border-subtle)"}`,
                animation: `staggerFadeIn 0.3s ease-out ${i * 60}ms both`,
              }}>
                <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "12px" }}>
                  <Wifi size={22} color={iface.is_up ? "#22c55e" : "#94a3b8"} />
                  <strong style={{ fontSize: "15px" }}>{iface.name}</strong>
                  <span className={`tag ${iface.is_up ? "status-running" : "status-stopped"}`} style={{ marginLeft: "auto", fontSize: "10px" }}>
                    {iface.is_up ? "UP" : "DOWN"}
                  </span>
                </div>
                <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "8px", fontSize: "12.5px" }}>
                  <div style={{ padding: "8px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}>
                    <div style={{ color: "var(--text-muted)", fontSize: "11px", marginBottom: "2px" }}>IPv4</div>
                    <div style={{ fontFamily: "var(--font-mono)" }}>{iface.ipv4}</div>
                  </div>
                  <div style={{ padding: "8px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}>
                    <div style={{ color: "var(--text-muted)", fontSize: "11px", marginBottom: "2px" }}>MAC</div>
                    <div style={{ fontFamily: "var(--font-mono)" }}>{iface.mac}</div>
                  </div>
                  <div style={{ padding: "8px", background: "rgba(34,197,94,0.05)", borderRadius: "var(--radius-sm)" }}>
                    <div style={{ color: "#22c55e", fontSize: "11px", marginBottom: "2px" }}><Upload size={10} style={{ verticalAlign: "middle" }} /> 发送</div>
                    <div style={{ fontFamily: "var(--font-mono)" }}>{formatBytes(iface.bytes_sent)}</div>
                  </div>
                  <div style={{ padding: "8px", background: "rgba(99,102,241,0.05)", borderRadius: "var(--radius-sm)" }}>
                    <div style={{ color: "#6366f1", fontSize: "11px", marginBottom: "2px" }}><Download size={10} style={{ verticalAlign: "middle" }} /> 接收</div>
                    <div style={{ fontFamily: "var(--font-mono)" }}>{formatBytes(iface.bytes_recv)}</div>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* ═══ FILES ═══ */}
      {tab === "files" && (
        <div className="card card-elevated" style={{ position: "relative", overflow: "hidden" }}>
          <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#22c55e40,#22c55e10)" }} />
          <h3 className="card-title">
            <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "rgba(34,197,94,0.1)", display: "flex", alignItems: "center", justifyContent: "center" }}>
              <FolderOpen size={14} color="#22c55e" />
            </div>
            文件浏览器
          </h3>
          <div style={{ display: "flex", gap: "8px", marginBottom: "14px", alignItems: "center" }}>
            <button className="btn btn-ghost btn-sm" onClick={() => loadDir("/")} disabled={floading}><Server size={14} /></button>
            <input type="text" className="form-input" value={fpath} onChange={e => setFpath(e.target.value)}
              onKeyDown={e => e.key === "Enter" && loadDir(fpath)}
              style={{ flex: 1, fontFamily: "var(--font-mono)", fontSize: "13px" }} />
            <button className="btn btn-primary btn-sm" onClick={() => loadDir(fpath)} disabled={floading}>
              {floading ? <RefreshCw size={14} className="spin" /> : <FolderOpen size={14} />}
            </button>
          </div>
          <div style={{ display: "flex", gap: "6px", marginBottom: "14px", flexWrap: "wrap" }}>
            {["/", "/home", "/tmp", "/etc", "C:/"].map(p => (
              <button key={p} className="btn btn-ghost btn-sm" onClick={() => loadDir(p)}
                style={{ fontSize: "11.5px", fontFamily: "var(--font-mono)" }}>{p}</button>
            ))}
          </div>
          {flist ? (
            <div style={{ border: "1px solid var(--border-subtle)", borderRadius: "var(--radius-md)", overflow: "hidden" }}>
              <div style={{
                display: "flex", padding: "8px 14px", background: "var(--bg-tertiary)",
                borderBottom: "1px solid var(--border-subtle)",
                fontSize: "11px", fontWeight: 700, color: "var(--text-muted)",
                textTransform: "uppercase", letterSpacing: "0.04em",
              }}>
                <span style={{ flex: 2 }}>名称</span>
                <span style={{ flex: 1 }}>大小</span>
                <span style={{ flex: 1 }}>修改时间</span>
                <span style={{ width: 60 }}>权限</span>
              </div>
              {flist.files.length === 0 ? (
                <div style={{ textAlign: "center", padding: 32, color: "var(--text-muted)" }}>空目录</div>
              ) : flist.files.map((file, i) => (
                <div key={file.name} onClick={() => file.is_dir ? loadDir(file.path) : undefined} style={{
                  display: "flex", padding: "9px 14px", alignItems: "center",
                  borderBottom: "1px solid var(--border-subtle)",
                  cursor: file.is_dir ? "pointer" : "default",
                  transition: "background var(--transition-fast)",
                  animation: `staggerFadeIn 0.2s ease-out ${i * 25}ms both`,
                }}
                  onMouseEnter={e => { if (file.is_dir) e.currentTarget.style.background = "var(--bg-tertiary)"; }}
                  onMouseLeave={e => { e.currentTarget.style.background = ""; }}
                >
                  <span style={{ flex: 2, display: "flex", alignItems: "center", gap: "8px", fontWeight: file.is_dir ? 500 : 400, fontSize: "13px" }}>
                    {file.is_dir ? <FolderOpen size={15} color="#f59e0b" /> : <FileCode size={14} color="var(--text-muted)" />}
                    {file.name}
                    {file.is_dir && <ChevronRight size={12} style={{ marginLeft: "auto", color: "var(--text-muted)" }} />}
                  </span>
                  <span style={{ flex: 1, fontFamily: "var(--font-mono)", fontSize: "12px", color: "var(--text-secondary)" }}>
                    {file.is_dir ? "-" : formatBytes(file.size_bytes)}
                  </span>
                  <span style={{ flex: 1, fontSize: "12px", color: "var(--text-muted)" }}>
                    {new Date(file.modified_at).toLocaleString("zh-CN")}
                  </span>
                  <span style={{ width: 60, fontFamily: "var(--font-mono)", fontSize: "11px", color: "var(--text-muted)" }}>{file.permissions}</span>
                </div>
              ))}
              <div style={{
                padding: "8px 14px", background: "var(--bg-tertiary)",
                borderTop: "1px solid var(--border-subtle)",
                fontSize: "11.5px", color: "var(--text-muted)",
              }}>
                共 {flist.file_count} 个文件, {flist.dir_count} 个目录, 总计 {formatBytes(flist.total_size)}
              </div>
            </div>
          ) : (
            <div style={{ textAlign: "center", padding: 40, color: "var(--text-muted)" }}>
              <FolderOpen size={40} style={{ opacity: 0.3, marginBottom: 12 }} />
              <div>输入路径或点击快捷路径开始浏览文件系统</div>
            </div>
          )}
        </div>
      )}

      {/* ═══ DIAGNOSTICS ═══ */}
      {tab === "diagnostics" && (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(220px, 1fr))", gap: "12px" }}>
            {[
              { title: "Ping 检测", icon: Zap, color: "#06b6d4", tgt: pingTgt, setTgt: setPingTgt, action: doPing, placeholder: "主机地址" },
              { title: "端口检测", icon: Globe, color: "#a855f7", tgt: portHost, setTgt: setPortHost, action: doPort, placeholder: "主机" },
              { title: "DNS 解析", icon: Search, color: "#22c55e", tgt: dnsTgt, setTgt: setDnsTgt, action: doDns, placeholder: "域名" },
            ].map(diag => {
              const DiagIcon = diag.icon;
              return (
                <div key={diag.title} className="card card-elevated" style={{ position: "relative", overflow: "hidden" }}>
                  <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: `linear-gradient(90deg,${diag.color}40,${diag.color}10)` }} />
                  <h4 style={{ margin: "0 0 12px 0", fontSize: "14px", display: "flex", alignItems: "center", gap: "8px", fontWeight: 600 }}>
                    <div style={{ width: "26px", height: "26px", borderRadius: "var(--radius-sm)", background: `${diag.color}12`, display: "flex", alignItems: "center", justifyContent: "center" }}>
                      <DiagIcon size={14} color={diag.color} />
                    </div>
                    {diag.title}
                  </h4>
                  <div style={{ display: "flex", gap: "6px" }}>
                    <input type="text" className="form-input" placeholder={diag.placeholder}
                      value={diag.tgt} onChange={e => diag.setTgt(e.target.value)}
                      onKeyDown={e => e.key === "Enter" && diag.action()}
                      style={{ flex: 1, fontSize: "13px" }} />
                    {diag.title === "端口检测" && (
                      <input type="number" className="form-input" placeholder="端口"
                        value={portNum} onChange={e => setPortNum(Number(e.target.value))}
                        style={{ width: 75, fontSize: "13px" }} />
                    )}
                    <button className="btn btn-primary btn-sm" onClick={diag.action}
                      style={{ background: `linear-gradient(135deg,${diag.color},${diag.color}cc)` }}>
                      检测
                    </button>
                  </div>
                </div>
              );
            })}
          </div>
          {diagResults.length > 0 && (
            <div className="card card-elevated" style={{ position: "relative", overflow: "hidden" }}>
              <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#a855f740,#a855f710)" }} />
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                <h3 className="card-title">
                  <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "var(--purple-light)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                    <Activity size={14} color="#a855f7" />
                  </div>
                  诊断结果
                </h3>
                <button className="btn btn-ghost btn-sm" onClick={() => setDiagResults([])} style={{ color: "#ef4444" }}>
                  <Trash2 size={14} /> 清除
                </button>
              </div>
              <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
                {diagResults.map(function(r: any, i: number) {
                  var res = r.result as Record<string, unknown>;
                  var isOpen = (res as unknown as PortCheckResult)?.open ?? false;
                  var isError = !!res.error;
                  var tagBg = r.type === "ping" ? "rgba(6,182,212,0.08)" : r.type === "port" ? "rgba(168,85,247,0.08)" : "rgba(34,197,94,0.08)";
                  var tagClr = r.type === "ping" ? "#06b6d4" : r.type === "port" ? "#a855f7" : "#22c55e";
                  return (
                    <div key={String(r.time) + "-" + i} style={{
                      padding: "12px 16px", borderRadius: "var(--radius-md)",
                      background: isError ? "rgba(239,68,68,0.04)" : isOpen ? "rgba(34,197,94,0.04)" : "var(--bg-tertiary)",
                      border: `1px solid ${isError ? "rgba(239,68,68,0.15)" : isOpen ? "rgba(34,197,94,0.15)" : "var(--border-subtle)"}`,
                    }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "6px" }}>
                        <span className="tag" style={{ fontSize: "10px", background: tagBg, color: tagClr, fontWeight: 700 }}>{r.type.toUpperCase()}</span>
                        <strong style={{ fontSize: "13px" }}>{r.label}</strong>
                        <span style={{ marginLeft: "auto", fontSize: "11px", color: "var(--text-muted)", fontFamily: "var(--font-mono)" }}>{r.time}ms</span>
                      </div>
                      <pre style={{
                        margin: 0, fontFamily: "var(--font-mono)", fontSize: "12px",
                        color: isError ? "#ef4444" : "var(--text-secondary)",
                        whiteSpace: "pre-wrap", wordBreak: "break-word",
                      }}>{JSON.stringify(res, null, 2)}</pre>
                    </div>
                  );
                })}
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default SystemMonitor;
