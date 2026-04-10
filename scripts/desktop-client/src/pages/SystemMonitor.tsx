import React, { useState, useEffect } from "react";
import {
  Cpu, HardDrive, Wifi, Activity, Clock, Zap, Server, RefreshCw, Search,
  AlertTriangle, CheckCircle2, XCircle, Globe, FolderOpen, FileCode,
  Trash2, Terminal as TerminalIcon, Copy, Download, Upload, ChevronRight,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import type { SystemMonitorData, ProcessInfo, NetworkInterface, PortCheckResult, DirectoryListing } from "../services/agentos-sdk";

const formatBytes = (b: number) => b < 1024 ? b + "B" : b < 1048576 ? (b / 1024).toFixed(1) + "KB" : (b / 1048576).toFixed(1) + "MB";
const fmtUptime = (s: number) => { const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60); return d > 0 ? d + "天" + h + "时" + m + "分" : h > 0 ? h + "时" + m + "分" : m + "分钟"; };

const SystemMonitor: React.FC = () => {
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
      const [m, p, n] = await Promise.all([sdk.getSystemMonitorData().catch(() => null), sdk.listProcesses().catch(() => []), sdk.getNetworkInterfaces().catch(() => [])]);
      setData(m); setProcs(p); setNets(n);
    } catch (e) { console.error(e); }
    finally { setLoading(false); setRefreshing(false); }
  };

  const loadDir = async (p: string) => {
    setFloading(true);
    try { setFlist(await sdk.listDirectory(p)); setFpath(p); } catch (e) { alert("无法读取目录: " + e); }
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

  const killProc = async (pid: number) => { if (!confirm("确定终止进程 PID=" + pid + "?")) return; try { await sdk.killProcess(pid); setProcs(procs.filter(p => p.pid !== pid)); } catch (e) { alert("终止失败: " + e); } };

  useEffect(() => { loadAll(); }, []);
  useEffect(() => { if (!autoRefresh) return; const iv = setInterval(loadAll, 5000); return () => clearInterval(iv); }, [autoRefresh]);

  if (loading && !data) return <div style={{ display: "flex", flexDirection: "column", gap: 20, alignItems: "center", justifyContent: "center", padding: "80px 20px" }}><div className="loading-spinner" style={{ width: 40, height: 40 }} /><div style={{ color: "var(--text-secondary)", fontSize: 14 }}>正在加载系统监控数据...</div></div>;

  const tabs = [
    { k: "overview" as const, icon: Activity, label: "总览" },
    { k: "processes" as const, icon: Cpu, label: "进程管理" },
    { k: "network" as const, icon: Wifi, label: "网络诊断" },
    { k: "files" as const, icon: FolderOpen, label: "文件浏览" },
    { k: "diagnostics" as const, icon: Search, label: "诊断工具" },
  ];

  return (
    <div className="page-container">
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: 12 }}>
          <h1>系统监控</h1>
          <span className={data ? "tag status-running" : ""} style={{ fontSize: "11.5px" }}>{data ? "已连接后端" : "模拟模式"}</span>
        </div>
        <div style={{ display: "flex", gap: 8, alignItems: "center" }}>
          <label style={{ display: "flex", alignItems: "center", gap: 5, cursor: "pointer", fontSize: 13, color: "var(--text-secondary)" }}>
            <input type="checkbox" checked={autoRefresh} onChange={e => setAutoRefresh(e.target.checked)} /> 自动刷新
          </label>
          <button className="btn btn-primary btn-sm" onClick={() => { setRefreshing(true); loadAll(); }} disabled={refreshing}><RefreshCw size={14} className={refreshing ? "spin" : ""} /> 刷新</button>
        </div>
      </div>

      <div style={{ display: "flex", background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)", padding: 3, border: "1px solid var(--border-subtle)", width: "fit-content" }}>
        {tabs.map(t => (
          <button key={t.k} onClick={() => setTab(t.k)} style={{ padding: "8px 18px", border: "none", borderRadius: "var(--radius-sm)", background: tab === t.k ? "var(--primary-color)" : "transparent", color: tab === t.k ? "white" : "var(--text-secondary)", cursor: "pointer", fontWeight: 500, fontSize: 13, transition: "all var(--transition-fast)", display: "flex", alignItems: "center", gap: 6 }}>
            <t.icon size={14} />{t.label}
          </button>
        ))}
      </div>

      {/* OVERVIEW */}
      {tab === "overview" && data && (
        <>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(220px, 1fr))", gap: 14 }}>
            {[{ icon: Cpu, label: "CPU 使用率", val: Math.round(data.cpu.usage_percent) + "%", sub: data.cpu.cores.length + " 核心", c: "#f59e0b", pct: data.cpu.usage_percent / 100 },
              { icon: HardDrive, label: "内存使用", val: Math.round(data.memory.used_gb) + "GB / " + Math.round(data.memory.total_gb) + "GB", sub: Math.round(data.memory.percent) + "%", c: "#6366f1", pct: data.memory.percent / 100 },
              { icon: Server, label: "磁盘使用", val: Math.round(data.disk.used_gb) + "GB / " + Math.round(data.disk.total_gb) + "GB", sub: Math.round(data.disk.percent) + "%", c: "#22c55e", pct: data.disk.percent / 100 },
              { icon: Clock, label: "运行时间", val: fmtUptime(data.uptime_seconds), sub: "", c: "#06b6d4", pct: 0 },
            ].map((card, i) => {
              const IconComp = card.icon;
              return (<div key={i} className="card card-elevated" style={{ animation: "staggerFadeIn 0.35s ease-out " + i * 70 + "ms both" }}>
                <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 12 }}><div style={{ width: 38, height: 38, borderRadius: "var(--radius-md)", background: card.c + "12", display: "flex", alignItems: "center", justifyContent: "center" }}><IconComp size={19} color={card.c} /></div><span style={{ fontSize: 13, color: "var(--text-muted)" }}>{card.label}</span></div>
                <div style={{ fontSize: 20, fontWeight: 700, fontFamily: "'JetBrains Mono', monospace", color: card.c }}>{card.val}</div>
                {card.sub && <div style={{ fontSize: "11.5px", color: "var(--text-muted)", marginTop: 3 }}>{card.sub}</div>}
                {card.pct > 0 && <div style={{ height: 4, background: "var(--bg-tertiary)", borderRadius: 2, marginTop: 10, overflow: "hidden" }}><div style={{ width: Math.min(card.pct * 100, 100) + "%", height: "100%", background: card.c, borderRadius: 2, transition: "width 0.6s ease-out" }} /></div>}
              </div>);
            })}
          </div>

          <div className="grid-2">
            <div className="card card-elevated"><h3 className="card-title"><Cpu size={18} /> CPU 核心详情</h3>
              <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(90px, 1fr))", gap: 8 }}>
                {data.cpu.cores.map((c, i) => <div key={i} style={{ padding: 10, borderRadius: "var(--radius-sm)", background: c.usage > 70 ? "rgba(239,68,68,0.06)" : c.usage > 30 ? "rgba(245,158,11,0.06)" : "rgba(34,197,94,0.04)", textAlign: "center", border: "1px solid var(--border-subtle)" }}>
                  <div style={{ fontSize: 16, fontWeight: 700, color: c.usage > 70 ? "#ef4444" : c.usage > 30 ? "#f59e0b" : "#22c55e", fontFamily: "'JetBrains Mono', monospace" }}>{Math.round(c.usage)}%</div>
                  <div style={{ fontSize: 10, color: "var(--text-muted)", marginTop: 2 }}>Core #{i}</div>
                </div>)}
              </div>
            </div>

            <div className="card card-elevated"><h3 className="card-title"><HardDrive size={18} /> 内存 & 磁盘</h3>
              <div style={{ display: "flex", flexDirection: "column", gap: 16, padding: "8px 0" }}>
                {[
                  { l: "已使用", v: data.memory.used_gb, mx: data.memory.total_gb, c: "#6366f1" },
                  { l: "可用", v: data.memory.free_gb, mx: data.memory.total_gb, c: "#22c55e" },
                ].map(item => (<div key={item.l}>
                  <div style={{ display: "flex", justifyContent: "space-between", marginBottom: 6 }}><span style={{ fontSize: 13 }}>{item.l}</span><span style={{ fontSize: 13, fontWeight: 600, color: item.c, fontFamily: "'JetBrains Mono', monospace" }}>{item.v.toFixed(1)}GB / {item.mx.toFixed(1)}GB</span></div>
                  <div style={{ height: 10, background: "var(--bg-tertiary)", borderRadius: 5, overflow: "hidden" }}><div style={{ width: (item.v / item.mx * 100) + "%", height: "100%", background: item.c, borderRadius: 5, transition: "width 0.6s ease-out" }} /></div>
                </div>))}
                <div style={{ marginTop: 16, paddingTop: 16, borderTop: "1px solid var(--border-subtle)" }}>
                  <div style={{ display: "flex", justifyContent: "space-between", marginBottom: 6 }}><span style={{ fontSize: 13 }}>磁盘</span><span style={{ fontSize: 13, fontWeight: 600, color: "#22c55e", fontFamily: "'JetBrains Mono', monospace" }}>{data.disk.used_gb.toFixed(0)}GB / {data.disk.total_gb.toFixed(0)}GB ({Math.round(data.disk.percent)}%)</span></div>
                  <div style={{ height: 10, background: "var(--bg-tertiary)", borderRadius: 5, overflow: "hidden" }}><div style={{ width: data.disk.percent + "%", height: "100%", background: "linear-gradient(90deg,#22c55e,#4ade80)", borderRadius: 5, transition: "width 0.6s ease-out" }} /></div>
                </div>
              </div>
            </div>
          </div>

          {nets.length > 0 && <div className="card card-elevated"><h3 className="card-title"><Wifi size={18} /> 网络接口</h3>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(340px, 1fr))", gap: 12 }}>
              {nets.map((iface, i) => (<div key={i} style={{ padding: 16, borderRadius: "var(--radius-md)", border: "1px solid " + (iface.is_up ? "rgba(34,197,94,0.2)" : "var(--border-subtle)"), background: iface.is_up ? "rgba(34,197,94,0.03)" : "var(--bg-tertiary)", animation: "staggerFadeIn 0.3s ease-out " + i * 60 + "ms both" }}>
                <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 10 }}><div style={{ display: "flex", alignItems: "center", gap: 8 }}><Wifi size={18} color={iface.is_up ? "#22c55e" : "#94a3b8"} /><strong>{iface.name}</strong></div><span className={"tag " + (iface.is_up ? "status-running" : "status-stopped")} style={{ fontSize: "10.5px" }}>{iface.is_up ? "在线" : "离线"}</span></div>
                <div style={{ display: "flex", flexDirection: "column", gap: 4, fontSize: "12.5px", fontFamily: "'JetBrains Mono', monospace", color: "var(--text-secondary)" }}>
                  <span><Globe size={11} style={{ marginRight: 5, verticalAlign: "middle", opacity: 0.5 }} />IPv4: {iface.ipv4}</span>
                  <span style={{ display: "flex", gap: 16, marginTop: 4 }}><span><Upload size={11} style={{ verticalAlign: "middle", color: "#22c55e" }} /> ↑{formatBytes(iface.bytes_sent)}</span><span><Download size={11} style={{ verticalAlign: "middle", color: "#6366f1" }} /> ↓{formatBytes(iface.bytes_recv)}</span></span>
                </div>
              </div>))}
            </div>
          </div>}
        </>
      )}

      {/* PROCESSES */}
      {tab === "processes" && <div className="card card-elevated">
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 16 }}><h3 className="card-title"><Cpu size={18} /> 进程列表 ({procs.length})</h3><button className="btn btn-sm btn-ghost" onClick={() => sdk.listProcesses().then(setProcs).catch(() => {})}><RefreshCw size={14} /></button></div>
        <div style={{ overflowX: "auto" }}>
          <table style={{ width: "100%", borderCollapse: "collapse", fontSize: 13 }}>
            <thead><tr style={{ borderBottom: "2px solid var(--border-subtle)" }}>{["PID", "进程名", "状态", "CPU%", "内存(MB)", "命令", "操作"].map(h => <th key={h} style={{ padding: "10px 12px", textAlign: "left", fontWeight: 600, color: "var(--text-muted)", fontSize: "11.5px", textTransform: "uppercase", whiteSpace: "nowrap" }}>{h}</th>)}</tr></thead>
            <tbody>{procs.length === 0 ? <tr><td colSpan={7} style={{ textAlign: "center", padding: 48, color: "var(--text-muted)" }}>暂无进程数据</td></tr> : procs.map((p, i) => <tr key={p.pid} style={{ borderBottom: "1px solid var(--border-subtle)", animation: "staggerFadeIn 0.25s ease-out " + i * 30 + "ms both" }}>
              <td style={{ padding: "10px 12px", fontFamily: "'JetBrains Mono', monospace", color: "var(--primary-color)", whiteSpace: "nowrap" }}>{p.pid}</td>
              <td style={{ padding: "10px 12px", fontWeight: 500 }}>{p.name}</td>
              <td style={{ padding: "10px 12px" }}><span className={"tag " + (p.status === "running" ? "status-running" : "")} style={{ fontSize: 10 }}>{p.status}</span></td>
              <td style={{ padding: "10px 12px", fontFamily: "'JetBrains Mono', monospace", color: (p.cpu_percent || 0) > 50 ? "#ef4444" : (p.cpu_percent || 0) > 20 ? "#f59e0b" : "inherit" }}>{(p.cpu_percent || 0).toFixed(1)}</td>
              <td style={{ padding: "10px 12px", fontFamily: "'JetBrains Mono', monospace" }}>{(p.memory_mb || 0).toFixed(0)}</td>
              <td style={{ padding: "10px 12px", maxWidth: 300, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap", color: "var(--text-muted)", fontSize: 12 }}>{p.command}</td>
              <td style={{ padding: "10px 12px" }}><button className="btn btn-danger btn-sm" onClick={() => killProc(p.pid)} title="终止进程" style={{ padding: "3px 8px", fontSize: 11 }}><XCircle size={12} /></button></td>
            </tr>)}
            </tbody>
          </table>
        </div>
      </div>}

      {/* NETWORK */}
      {tab === "network" && nets.length > 0 && <div className="card card-elevated"><h3 className="card-title"><Wifi size={18} /> 网络接口</h3>
        <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(360px, 1fr))", gap: 12 }}>
          {nets.map((iface, i) => <div key={i} style={{ padding: 16, borderRadius: "var(--radius-md)", border: "1px solid var(--border-subtle)", animation: "staggerFadeIn 0.3s ease-out " + i * 60 + "ms both" }}>
            <div style={{ display: "flex", alignItems: "center", gap: 10, marginBottom: 12 }}><Wifi size={22} color={iface.is_up ? "#22c55e" : "#94a3b8"} /><strong style={{ fontSize: 15 }}>{iface.name}</strong><span className={"tag " + (iface.is_up ? "status-running" : "status-stopped")} style={{ marginLeft: "auto", fontSize: 10 }}>{iface.is_up ? "UP" : "DOWN"}</span></div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 8, fontSize: "12.5px" }}>
              <div style={{ padding: 8, background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}><div style={{ color: "var(--text-muted)", fontSize: 11, marginBottom: 2 }}>IPv4</div><div style={{ fontFamily: "'JetBrains Mono', monospace" }}>{iface.ipv4}</div></div>
              <div style={{ padding: 8, background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}><div style={{ color: "var(--text-muted)", fontSize: 11, marginBottom: 2 }}>MAC</div><div style={{ fontFamily: "'JetBrains Mono', monospace" }}>{iface.mac}</div></div>
              <div style={{ padding: 8, background: "rgba(34,197,94,0.05)", borderRadius: "var(--radius-sm)" }}><div style={{ color: "#22c55e", fontSize: 11, marginBottom: 2 }}><Upload size={10} style={{ verticalAlign: "middle" }} /> 发送</div><div style={{ fontFamily: "'JetBrains Mono', monospace" }}>{formatBytes(iface.bytes_sent)}</div></div>
              <div style={{ padding: 8, background: "rgba(99,102,241,0.05)", borderRadius: "var(--radius-sm)" }}><div style={{ color: "#6366f1", fontSize: 11, marginBottom: 2 }}><Download size={10} style={{ verticalAlign: "middle" }} /> 接收</div><div style={{ fontFamily: "'JetBrains Mono', monospace" }}>{formatBytes(iface.bytes_recv)}</div></div>
            </div>
          </div>)}
        </div>
      </div>}

      {/* FILES */}
      {tab === "files" && <div className="card card-elevated">
        <h3 className="card-title"><FolderOpen size={18} /> 文件浏览器</h3>
        <div style={{ display: "flex", gap: 8, marginBottom: 14, alignItems: "center" }}>
          <button className="btn btn-ghost btn-sm" onClick={() => loadDir("/")} disabled={floading}><Server size={14} /></button>
          <input type="text" className="form-input" value={fpath} onChange={e => setFpath(e.target.value)} onKeyDown={e => e.key === "Enter" && loadDir(fpath)} style={{ flex: 1, fontFamily: "'JetBrains Mono', monospace", fontSize: 13 }} />
          <button className="btn btn-primary btn-sm" onClick={() => loadDir(fpath)} disabled={floading}>{floading ? <RefreshCw size={14} className="spin" /> : <FolderOpen size={14} />}</button>
        </div>
        <div style={{ display: "flex", gap: 6, marginBottom: 14, flexWrap: "wrap" }}>{["/", "/home", "/tmp", "/etc", "C:/"].map(p => <button key={p} className="btn btn-ghost btn-sm" onClick={() => loadDir(p)} style={{ fontSize: "11.5px", fontFamily: "'JetBrains Mono', monospace" }}>{p}</button>)}</div>
        {flist ? <div style={{ border: "1px solid var(--border-subtle)", borderRadius: "var(--radius-md)", overflow: "hidden" }}>
          <div style={{ display: "flex", padding: "8px 14px", background: "var(--bg-tertiary)", borderBottom: "1px solid var(--border-subtle)", fontSize: "11.5px", fontWeight: 600, color: "var(--text-muted)" }}><span style={{ flex: 2 }}>名称</span><span style={{ flex: 1 }}>大小</span><span style={{ flex: 1 }}>修改时间</span><span style={{ width: 60 }}>权限</span></div>
          {flist.files.length === 0 ? <div style={{ textAlign: "center", padding: 32, color: "var(--text-muted)" }}>空目录</div> : flist.files.map((file, i) => <div key={file.name} onClick={() => file.is_dir ? loadDir(file.path) : undefined} style={{ display: "flex", padding: "9px 14px", alignItems: "center", borderBottom: "1px solid var(--border-subtle)", cursor: file.is_dir ? "pointer" : "default", transition: "background var(--transition-fast)", animation: "staggerFadeIn 0.2s ease-out " + i * 25 + "ms both" }}
            onMouseEnter={e => { if (file.is_dir) e.currentTarget.style.background = "var(--bg-tertiary)"; }} onMouseLeave={e => { e.currentTarget.style.background = ""; }}>
            <span style={{ flex: 2, display: "flex", alignItems: "center", gap: 8, fontWeight: file.is_dir ? 500 : 400, fontSize: 13 }}>{file.is_dir ? <FolderOpen size={15} color="#f59e0b" /> : <FileCode size={14} color="var(--text-muted)" />}{file.name}{file.is_dir && <ChevronRight size={12} style={{ marginLeft: "auto", color: "var(--text-muted)" }} />}</span>
            <span style={{ flex: 1, fontFamily: "'JetBrains Mono', monospace", fontSize: 12, color: "var(--text-secondary)" }}>{file.is_dir ? "-" : formatBytes(file.size_bytes)}</span>
            <span style={{ flex: 1, fontSize: 12, color: "var(--text-muted)" }}>{new Date(file.modified_at).toLocaleString("zh-CN")}</span>
            <span style={{ width: 60, fontFamily: "'JetBrains Mono', monospace", fontSize: 11, color: "var(--text-muted)" }}>{file.permissions}</span>
          </div>)}
          <div style={{ padding: "8px 14px", background: "var(--bg-tertiary)", borderTop: "1px solid var(--border-subtle)", fontSize: "11.5px", color: "var(--text-muted)" }}>共 {flist.file_count} 个文件, {flist.dir_count} 个目录, 总计 {formatBytes(flist.total_size)}</div>
        </div> : <div style={{ textAlign: "center", padding: 40, color: "var(--text-muted)" }}><FolderOpen size={40} style={{ opacity: 0.3, marginBottom: 12 }} /><div>输入路径或点击快捷路径开始浏览文件系统</div></div>}
      </div>}

      {/* DIAGNOSTICS */}
      {tab === "diagnostics" && <div style={{ display: "flex", flexDirection: "column", gap: 16 }}>
        <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(220px, 1fr))", gap: 12 }}>
          <div className="card card-elevated"><h4 style={{ margin: "0 0 12px 0", fontSize: 14, display: "flex", alignItems: "center", gap: 8 }}><Zap size={16} color="#06b6d4" /> Ping 检测</h4><div style={{ display: "flex", gap: 6 }}><input type="text" className="form-input" placeholder="主机地址" value={pingTgt} onChange={e => setPingTgt(e.target.value)} onKeyDown={e => e.key === "Enter" && doPing()} style={{ flex: 1, fontSize: 13 }} /><button className="btn btn-primary btn-sm" onClick={doPing}>检测</button></div></div>
          <div className="card card-elevated"><h4 style={{ margin: "0 0 12px 0", fontSize: 14, display: "flex", alignItems: "center", gap: 8 }}><Globe size={16} color="#a855f7" /> 端口检测</h4><div style={{ display: "flex", gap: 6 }}><input type="text" className="form-input" placeholder="主机" value={portHost} onChange={e => setPortHost(e.target.value)} style={{ flex: 1, fontSize: 13 }} /><input type="number" className="form-input" placeholder="端口" value={portNum} onChange={e => setPortNum(Number(e.target.value))} style={{ width: 75, fontSize: 13 }} /><button className="btn btn-primary btn-sm" onClick={doPort}>检测</button></div></div>
          <div className="card card-elevated"><h4 style={{ margin: "0 0 12px 0", fontSize: 14, display: "flex", alignItems: "center", gap: 8 }}><Search size={16} color="#22c55e" /> DNS 解析</h4><div style={{ display: "flex", gap: 6 }}><input type="text" className="form-input" placeholder="域名" value={dnsTgt} onChange={e => setDnsTgt(e.target.value)} onKeyDown={e => e.key === "Enter" && doDns()} style={{ flex: 1, fontSize: 13 }} /><button className="btn btn-primary btn-sm" onClick={doDns}>查询</button></div></div>
        </div>
        {diagResults.length > 0 && <div className="card card-elevated"><h3 className="card-title">诊断结果历史 ({diagResults.length})</h3>
          <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
            {diagResults.map(function(r: any, i: number) {
              var res = r.result as Record<string, unknown>;
              var isOpen = (res as unknown as PortCheckResult)?.open ?? false;
              var isError = !!res.error;
              var bg = isError ? "rgba(239,68,68,0.04)" : isOpen ? "rgba(34,197,94,0.04)" : "var(--bg-tertiary)";
              var bdr = isError ? "rgba(239,68,68,0.15)" : isOpen ? "rgba(34,197,94,0.15)" : "var(--border-subtle)";
              var tagBg = r.type === "ping" ? "rgba(6,182,212,0.08)" : r.type === "port" ? "rgba(168,85,247,0.08)" : "rgba(34,197,94,0.08)";
              var tagClr = r.type === "ping" ? "#06b6d4" : r.type === "port" ? "#a855f7" : "#22c55e";
              return (<div key={String(r.time) + "-" + i} style={{ padding: "12px 16px", borderRadius: "var(--radius-md)", background: bg, border: "1px solid " + bdr }}>
                <div style={{ display: "flex", alignItems: "center", gap: 8, marginBottom: 6 }}>
                  <span className="tag" style={{ fontSize: 10, background: tagBg, color: tagClr }}>{r.type.toUpperCase()}</span>
                  <strong style={{ fontSize: 13 }}>{r.label}</strong>
                  <span style={{ marginLeft: "auto", fontSize: 11, color: "var(--text-muted)" }}>{r.time}ms</span>
                </div>
                <pre style={{ margin: 0, fontFamily: "'JetBrains Mono', monospace", fontSize: 12, color: isError ? "#ef4444" : "var(--text-secondary)", whiteSpace: "pre-wrap", wordBreak: "break-word" }}>{JSON.stringify(res, null, 2)}</pre>
              </div>);
            })}
          </div>
        </div>}
      </div>}
    </div>
  );
};

export default SystemMonitor;