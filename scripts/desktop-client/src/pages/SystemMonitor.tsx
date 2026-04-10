import React, { useState, useEffect } from "react";
import {
  Cpu,
  HardDrive,
  Wifi,
  Activity,
  Clock,
  Zap,
  Thermometer,
  Server,
  RefreshCw,
  Search,
  AlertTriangle,
  CheckCircle2,
  XCircle,
  ArrowUpRight,
  ArrowDownRight,
  Globe,
  FolderOpen,
  FileCode,
  Trash2,
  Terminal as TerminalIcon,
  Copy,
  Download,
  Upload,
  ChevronRight,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import type { SystemMonitorData, ProcessInfo, NetworkInterface, PortCheckResult, FileInfo, DirectoryListing } from "../services/agentos-sdk";

const SystemMonitor: React.FC = () => {
  const [activeTab, setActiveTab] = useState<"overview" | "processes" | "network" | "files" | "diagnostics">("overview");
  const [monitorData, setMonitorData] = useState<SystemMonitorData | null>(null);
  const [processes, setProcesses] = useState<ProcessInfo[]>([]);
  const [networkIfaces, setNetworkIfaces] = useState<NetworkInterface[]>([]);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const [autoRefresh, setAutoRefresh] = useState(true);

  const [fileBrowserPath, setFileBrowserPath] = useState("/");
  const [directoryListing, setDirectoryListing] = useState<DirectoryListing | null>(null);
  const [fileLoading, setFileLoading] = useState(false);

  const [pingTarget, setPingTarget] = useState("localhost");
  const [portHost, setPortHost] = useState("localhost");
  const [portNum, setPortNum] = useState(8080);
  const [dnsTarget, setDnsTarget] = useState("google.com");
  const [diagResults, setDiagResults] = useState<Array<{ type: string; label: string; result: unknown; time: number }>>([]);

  const loadAllData = async () => {
    setLoading(true);
    try {
      const [mData, procData, netData] = await Promise.all([
        sdk.getSystemMonitorData(),
        sdk.listProcesses().catch(() => []),
        sdk.getNetworkInterfaces().catch(() => []),
      ]);
      setMonitorData(mData || null);
      setProcesses(procData || []);
      setNetworkIfaces(netData || []);
    } catch (error) {
      console.error("Failed to load monitor data:", error);
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  };

  const loadDirectory = async (path: string) => {
    setFileLoading(true);
    try {
      const listing = await sdk.listDirectory(path);
      setDirectoryListing(listing || null);
      setFileBrowserPath(path);
    } catch (error) {
      alert(`无法读取目录: ${error}`);
    } finally {
      setFileLoading(false);
    }
  };

  const handlePing = async () => {
    if (!pingTarget.trim()) return;
    const start = Date.now();
    try {
      const result = await sdk.ping(pingTarget, 4);
      setDiagResults(prev => [{ type: "ping", label: `Ping ${pingTarget}`, result, time: Date.now() - start }, ...prev]);
    } catch (e) {
      setDiagResults(prev => [{ type: "ping", label: `Ping ${pingTarget}`, result: { error: String(e) }, time: Date.now() - start }, ...prev]);
    }
  };

  const handlePortCheck = async () => {
    if (!portHost.trim()) return;
    const start = Date.now();
    try {
      const result = await sdk.checkPort(portHost, portNum);
      setDiagResults(prev => [{ type: "port", label: `端口 ${portHost}:${portNum}`, result, time: Date.now() - start }, ...prev]);
    } catch (e) {
      setDiagResults(prev => [{ type: "port", label: `端口 ${portHost}:${portNum}`, result: { error: String(e) }, time: Date.now() - start }, ...prev]);
    }
  };

  const handleDnsLookup = async () => {
    if (!dnsTarget.trim()) return;
    const start = Date.now();
    try {
      const result = await sdk.dnsLookup(dnsTarget);
      setDiagResults(prev => [{ type: "dns", label: `DNS ${dnsTarget}`, result, time: Date.now() - start }, ...prev]);
    } catch (e) {
      setDiagResults(prev => [{ type: "dns", label: `DNS ${dnsTarget}`, result: { error: String(e) }, time: Date.now() - start }, ...prev]);
    }
  };

  const handleKillProcess = async (pid: number) => {
    if (!confirm(`确定要终止进程 PID=${pid} 吗？`)) return;
    try {
      await sdk.killProcess(pid);
      setProcesses(prev => prev.filter(p => p.pid !== pid));
    } catch (e) {
      alert(`终止失败: ${e}`);
    }
  };

  useEffect(() => { loadAllData(); }, []);
  useEffect(() => {
    if (!autoRefresh) return;
    const interval = setInterval(loadAllData, 5000);
    return () => clearInterval(interval);
  }, [autoRefresh]);

  const formatBytes = (bytes: number) => bytes < 1024 ? `${bytes}B` : bytes < 1048576 ? `${(bytes/1024).toFixed(1)}KB` : bytes < 1073741824 ? `${(bytes/1048576).toFixed(1)}MB` : `${(bytes/1073741824).toFixed(2)}GB`;
  const formatUptime = (secs: number) => {
    const d = Math.floor(secs / 86400), h = Math.floor((secs % 86400) / 3600), m = Math.floor((secs % 3600) / 60);
    return d > 0 ? `${d}天${h}时${m}分` : h > 0 ? `${h}时${m}分` : `${m}分钟`;
  };

  if (loading && !monitorData) {
    return (
      <div style={{ display: "flex", flexDirection: "column", gap: "20px", alignItems: "center", justifyContent: "center", padding: "80px 20px" }}>
        <div className="loading-spinner" style={{ width: 40, height: 40 }} />
        <div style={{ color: "var(--text-secondary)", fontSize: "14px" }}>正在加载系统监控数据...</div>
      </div>
    );
  }

  const data = monitorData;

  return (
    <div className="page-container">
      {/* Header */}
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
          <h1>系统监控</h1>
          <span className={`tag ${data ? "status-running" : ""}`} style={{ fontSize: "11.5px" }}>
            {data ? "已连接后端" : "模拟模式"}
          </span>
        </div>
        <div style={{ display: "flex", gap: "8px", alignItems: "center" }}>
          <label style={{ display: "flex", alignItems: "center", gap: "5px", cursor: "pointer", fontSize: "13px", color: "var(--text-secondary)" }}>
            <input type="checkbox" checked={autoRefresh} onChange={(e) => setAutoRefresh(e.target.checked)} />
            自动刷新
          </label>
          <button className="btn btn-primary btn-sm" onClick={() => { setRefreshing(true); loadAllData(); }} disabled={refreshing}>
            <RefreshCw size={14} className={refreshing ? "spin" : ""} /> 刷新
          </button>
        </div>
      </div>

      {/* Tabs */}
      <div style={{
        display: "flex", background: "var(--bg-tertiary)",
        borderRadius: "var(--radius-md)", padding: "3px", border: "1px solid var(--border-subtle)", width: "fit-content",
      }}>
        {[
          { key: "overview" as const, icon: Activity, label: "总览" },
          { key: "processes" as const, icon: Cpu, label: "进程管理" },
          { key: "network" as const, icon: Wifi, label: "网络诊断" },
          { key: "files" as const, icon: FolderOpen, label: "文件浏览" },
          { key: "diagnostics" as const, icon: Search, label: "诊断工具" },
        ].map(tab => (
          <button key={tab.key} onClick={() => setActiveTab(tab.key)} style={{
            padding: "8px 18px", border: "none", borderRadius: "var(--radius-sm)",
            background: activeTab === tab.key ? "var(--primary-color)" : "transparent",
            color: activeTab === tab.key ? "white" : "var(--text-secondary)",
            cursor: "pointer", fontWeight: 500, fontSize: "13px",
            transition: "all var(--transition-fast)", display: "flex", alignItems: "center", gap: "6px",
          }}>
            <tab.icon size={14} />{tab.label}
          </button>
        ))}
      </div>

      {/* ===== OVERVIEW TAB ===== */}
      {activeTab === "overview" && data && (
        <>
          {/* Core Metrics */}
          <div style={{
            display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(220px, 1fr))",
            gap: "14px",
          }}>
            {[
              { icon: Cpu, label: "CPU 使用率", value: `${Math.round(data.cpu.usagePercent)}%`, sub: `${data.cpu.cores.length} 核心`, color: "#f59e0b", pct: data.cpu.usagePercent / 100 },
              { icon: HardDrive, label: "内存使用", value: `${Math.round(data.memory.usedGb)}GB / ${Math.round(data.memory.totalGb)}GB`, sub: `${Math.round(data.memory.percent)}%`, color: "#6366f1", pct: data.memory.percent / 100 },
              { icon: Server, label: "磁盘使用", value: `${Math.round(data.disk.usedGb)}GB / ${Math.round(data.disk.totalGb)}GB`, sub: `${Math.round(data.disk.percent)}%`, color: "#22c55e", pct: data.disk.percent / 100 },
              { icon: Clock, label: "系统运行时间", value: formatUptime(data.uptimeSeconds), sub: "", color: "#06b6d4", pct: 0 },
            ].map((card, i) => {
              const IconComp = card.icon;
              return (
                <div key={i} className="card card-elevated" style={{ animation: `staggerFadeIn 0.35s ease-out ${i * 70}ms both` }}>
                  <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "12px" }}>
                    <div style={{ width: "38px", height: "38px", borderRadius: "var(--radius-md)", background: `${card.color}12`, display: "flex", alignItems: "center", justifyContent: "center" }}>
                      <IconComp size={19} color={card.color} />
                    </div>
                    <span style={{ fontSize: "13px", color: "var(--text-muted)" }}>{card.label}</span>
                  </div>
                  <div style={{ fontSize: "20px", fontWeight: 700, fontFamily: "'JetBrains Mono', monospace", color: card.color }}>{card.value}</div>
                  {card.sub && <div style={{ fontSize: "11.5px", color: "var(--text-muted)", marginTop: "3px" }}>{card.sub}</div>}
                  {card.pct > 0 && (
                    <div style={{ height: "4px", background: "var(--bg-tertiary)", borderRadius: "2px", marginTop: "10px", overflow: "hidden" }}>
                      <div style={{ width: `${Math.min(card.pct * 100, 100)}%`, height: "100%", background: card.color, borderRadius: "2px", transition: "width 0.6s ease-out" }} />
                    </div>
                  )}
                </div>
              );
            })}
          </div>

          {/* CPU Cores + Memory Detail */}
          <div className="grid-2">
            <div className="card card-elevated">
              <h3 className="card-title"><Cpu size={18} /> CPU 核心详情</h3>
              <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(90px, 1fr))", gap: "8px" }}>
                {data.cpu.cores.map((core, i) => (
                  <div key={i} style={{
                    padding: "10px", borderRadius: "var(--radius-sm)",
                    background: core.usage > 70 ? "rgba(239,68,68,0.06)" : core.usage > 30 ? "rgba(245,158,11,0.06)" : "rgba(34,197,94,0.04)",
                    textAlign: "center", border: "1px solid var(--border-subtle)",
                  }}>
                    <div style={{ fontSize: "16px", fontWeight: 700, color: core.usage > 70 ? "#ef4444" : core.usage > 30 ? "#f59e0b" : "#22c55e", fontFamily: "'JetBrains Mono', monospace" }}>
                      {Math.round(core.usage)}%
                    </div>
                    <div style={{ fontSize: "10px", color: "var(--text-muted)", marginTop: "2px" }}>Core #{i}</div>
                  </div>
                ))}
              </div>
            </div>

            <div className="card card-elevated">
              <h3 className="card-title"><HardDrive size={18} /> 内存分配</h3>
              <div style={{ display: "flex", flexDirection: "column", gap: "16px", padding: "8px 0" }}>
                {[
                  { label: "已使用", value: data.memory.usedGb, max: data.memory.totalGb, color: "#6366f1" },
                  { label: "可用", value: data.memory.freeGb, max: data.memory.totalGb, color: "#22c55e" },
                ].map(item => (
                  <div key={item.label}>
                    <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "6px" }}>
                      <span style={{ fontSize: "13px" }}>{item.label}</span>
                      <span style={{ fontSize: "13px", fontWeight: 600, color: item.color, fontFamily: "'JetBrains Mono', monospace" }}>
                        {item.value.toFixed(1)}GB / {item.max.toFixed(1)}GB
                      </span>
                    </div>
                    <div style={{ height: "10px", background: "var(--bg-tertiary)", borderRadius: "5px", overflow: "hidden" }}>
                      <div style={{ width: `${(item.value / item.max) * 100}%`, height: "100%", background: item.color, borderRadius: "5px", transition: "width 0.6s ease-out" }} />
                    </div>
                  </div>
                ))}
              </div>

              {/* Disk */}
              <div style={{ marginTop: "16px", paddingTop: "16px", borderTop: "1px solid var(--border-subtle)" }}>
                <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "6px" }}>
                  <span style={{ fontSize: "13px" }}>磁盘</span>
                  <span style={{ fontSize: "13px", fontWeight: 600, color: "#22c55e", fontFamily: "'JetBrains Mono', monospace" }}>
                    {data.disk.usedGb.toFixed(0)}GB / {data.disk.totalGb.toFixed(0)}GB ({Math.round(data.disk.percent)}%)
                  </span>
                </div>
                <div style={{ height: "10px", background: "var(--bg-tertiary)", borderRadius: "5px", overflow: "hidden" }}>
                  <div style={{ width: `${data.disk.percent}%`, height: "100%", background: "linear-gradient(90deg,#22c55e,#4ade80)", borderRadius: "5px", transition: "width 0.6s ease-out" }} />
                </div>
              </div>
            </div>
          </div>

          {/* Network Interfaces */}
          {networkIfaces.length > 0 && (
            <div className="card card-elevated">
              <h3 className="card-title"><Wifi size={18} /> 网络接口</h3>
              <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(340px, 1fr))", gap: "12px" }}>
                {networkIfaces.map((iface, i) => (
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
                    <div style={{ display: "flex", flexDirection: "column", gap: "4px", fontSize: "12.5px", fontFamily: "'JetBrains Mono', monospace", color: "var(--text-secondary)" }}>
                      <span><Globe size={11} style={{ marginRight: "5px", verticalAlign: "middle", opacity: 0.5 }} />IPv4: {iface.ipv4}</span>
                      {iface.ipv6 && <span><Globe size={11} style={{ marginRight: "5px", verticalAlign: "middle", opacity: 0.5 }} />IPv6: {iface.ipv6}</span>}
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

      {/* ===== PROCESSES TAB ===== */}
      {activeTab === "processes" && (
        <div className="card card-elevated">
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "16px" }}>
            <h3 className="card-title"><Cpu size={18} /> 进程列表 ({processes.length})</h3>
            <button className="btn btn-sm btn-ghost" onClick={() => sdk.listProcesses().then(setProcesses).catch(() => {})}><RefreshCw size={14} /></button>
          </div>
          <div style={{ overflowX: "auto" }}>
            <table style={{ width: "100%", borderCollapse: "collapse", fontSize: "13px" }}>
              <thead>
                <tr style={{ borderBottom: "2px solid var(--border-subtle)" }}>
                  {["PID", "进程名", "状态", "CPU%", "内存(MB)", "命令", "操作"].map(h => (
                    <th key={h} style={{ padding: "10px 12px", textAlign: "left", fontWeight: 600, color: "var(--text-muted)", fontSize: "11.5px", textTransform: "uppercase", whiteSpace: "nowrap" }}>{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {processes.length === 0 ? (
                  <tr><td colSpan={7} style={{ textAlign: "center", padding: "48px", color: "var(--text-muted)" }}>暂无进程数据</td></tr>
                ) : processes.map((p, i) => (
                  <tr key={p.pid} style={{ borderBottom: "1px solid var(--border-subtle)", animation: `staggerFadeIn 0.25s ease-out ${i * 30}ms both` }}>
                    <td style={{ padding: "10px 12px", fontFamily: "'JetBrains Mono', monospace", color: "var(--primary-color)", whiteSpace: "nowrap" }}>{p.pid}</td>
                    <td style={{ padding: "10px 12px", fontWeight: 500 }}>{p.name}</td>
                    <td style={{ padding: "10px 12px" }}>
                      <span className={`tag ${p.status === "running" ? "status-running" : p.status === "idle" ? "" : "status-stopped"}`} style={{ fontSize: "10px" }}>{p.status}</span>
                    </td>
                    <td style={{ padding: "10px 12px", fontFamily: "'JetBrains Mono', monospace", color: p.cpu_percent > 50 ? "#ef4444" : p.cpu_percent > 20 ? "#f59e0b" : "inherit" }}>{p.cpu_percent?.toFixed(1) || "-"}</td>
                    <td style={{ padding: "10px 12px", fontFamily: "'JetBrains Mono', monospace" }}>{p.memory_mb?.toFixed(0) || "-"}</td>
                    <td style={{ padding: "10px 12px", maxWidth: "300px", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap", color: "var(--text-muted)", fontSize: "12px" }}>{p.command}</td>
                    <td style={{ padding: "10px 12px" }}>
                      <button className="btn btn-danger btn-sm" onClick={() => handleKillProcess(p.pid)} title="终止进程" style={{ padding: "3px 8px", fontSize: "11px" }}>
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

      {/* ===== NETWORK TAB ===== */}
      {activeTab === "network" && (
        <>
          <div className="card card-elevated">
            <h3 className="card-title"><Wifi size={18} /> 网络接口</h3>
            {networkIfaces.length === 0 ? (
              <div style={{ textAlign: "center", padding: "32px", color: "var(--text-muted)" }}>暂无网络接口数据</div>
            ) : (
              <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(360px, 1fr))", gap: "12px" }}>
                {networkIfaces.map((iface, i) => (
                  <div key={i} style={{ padding: "16px", borderRadius: "var(--radius-md)", border: "1px solid var(--border-subtle)", animation: `staggerFadeIn 0.3s ease-out ${i * 60}ms both` }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "12px" }}>
                      <Wifi size={22} color={iface.is_up ? "#22c55e" : "#94a3b8"} />
                      <strong style={{ fontSize: "15px" }}>{iface.name}</strong>
                      <span className={`tag ${iface.is_up ? "status-running" : "status-stopped"}`} style={{ marginLeft: "auto", fontSize: "10px" }}>{iface.is_up ? "UP" : "DOWN"}</span>
                    </div>
                    <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "8px", fontSize: "12.5px" }}>
                      <div style={{ padding: "8px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}>
                        <div style={{ color: "var(--text-muted)", fontSize: "11px", marginBottom: "2px" }}>IPv4</div>
                        <div style={{ fontFamily: "'JetBrains Mono', monospace" }}>{iface.ipv4}</div>
                      </div>
                      <div style={{ padding: "8px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}>
                        <div style={{ color: "var(--text-muted)", fontSize: "11px", marginBottom: "2px" }}>MAC</div>
                        <div style={{ fontFamily: "'JetBrains Mono', monospace" }}>{iface.mac}</div>
                      </div>
                      <div style={{ padding: "8px", background: "rgba(34,197,94,0.05)", borderRadius: "var(--radius-sm)" }}>
                        <div style={{ color: "#22c55e", fontSize: "11px", marginBottom: "2px" }}><Upload size={10} style={{ verticalAlign: "middle" }} /> 发送</div>
                        <div style={{ fontFamily: "'JetBrains Mono', monospace" }}>{formatBytes(iface.bytes_sent)}</div>
                      </div>
                      <div style={{ padding: "8px", background: "rgba(99,102,241,0.05)", borderRadius: "var(--radius-sm)" }}>
                        <div style={{ color: "#6366f1", fontSize: "11px", marginBottom: "2px" }}><Download size={10} style={{ verticalAlign: "middle" }} /> 接收</div>
                        <div style={{ fontFamily: "'JetBrains Mono', monospace" }}>{formatBytes(iface.bytes_recv)}</div>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        </>
      )}

      {/* ===== FILES TAB ===== */}
      {activeTab === "files" && (
        <div className="card card-elevated">
          <h3 className="card-title"><FolderOpen size={18} /> 文件浏览器</h3>

          {/* Path bar */}
          <div style={{ display: "flex", gap: "8px", marginBottom: "14px", alignItems: "center" }}>
            <button className="btn btn-ghost btn-sm" onClick={() => loadDirectory("/")} disabled={fileLoading}>
              <Server size={14} />
            </button>
            <input
              type="text" className="form-input"
              value={fileBrowserPath}
              onChange={(e) => setFileBrowserPath(e.target.value)}
              onKeyDown={(e) => e.key === "Enter" && loadDirectory(fileBrowserPath)}
              style={{ flex: 1, fontFamily: "'JetBrains Mono', monospace", fontSize: "13px" }}
            />
            <button className="btn btn-primary btn-sm" onClick={() => loadDirectory(fileBrowserPath)} disabled={fileLoading}>
              {fileLoading ? <RefreshCw size={14} className="spin" /> : <FolderOpen size={14} />}
            </button>
          </div>

          {/* Quick paths */}
          <div style={{ display: "flex", gap: "6px", marginBottom: "14px", flexWrap: "wrap" }}>
            {[ "/", "/home", "/tmp", "/etc", "C:\\", "C:\\".map(p => (
              <button key={p} className="btn btn-ghost btn-sm" onClick={() => loadDirectory(p)} style={{ fontSize: "11.5px", fontFamily: "'JetBrains Mono', monospace" }}>
                {p}
              </button>
            )))}
          </div>

          {/* File list */}
          {directoryListing ? (
            <div style={{ border: "1px solid var(--border-subtle)", borderRadius: "var(--radius-md)", overflow: "hidden" }}>
              <div style={{ display: "flex", padding: "8px 14px", background: "var(--bg-tertiary)", borderBottom: "1px solid var(--border-subtle)", fontSize: "11.5px", fontWeight: 600, color: "var(--text-muted)" }}>
                <span style={{ flex: 2 }}>名称</span>
                <span style={{ flex: 1 }}>大小</span>
                <span style={{ flex: 1 }}>修改时间</span>
                <span style={{ width: "60px" }}>权限</span>
              </div>
              {directoryListing.files.length === 0 ? (
                <div style={{ textAlign: "center", padding: "32px", color: "var(--text-muted)" }}>空目录</div>
              ) : directoryListing.files.map((file, i) => (
                <div
                  key={file.name}
                  onClick={() => file.is_dir ? loadDirectory(file.path) : undefined}
                  style={{
                    display: "flex", padding: "9px 14px", alignItems: "center",
                    borderBottom: "1px solid var(--border-subtle)",
                    cursor: file.is_dir ? "pointer" : "default",
                    background: file.is_dir ? "transparent" : undefined,
                    transition: "background var(--transition-fast)",
                    animation: `staggerFadeIn 0.2s ease-out ${i * 25}ms both`,
                  }}
                  onMouseEnter={(e) => { if (file.is_dir) e.currentTarget.style.background = "var(--bg-tertiary)"; }}
                  onMouseLeave={(e) => { e.currentTarget.style.background = ""; }}
                >
                  <span style={{ flex: 2, display: "flex", alignItems: "center", gap: "8px", fontWeight: file.is_dir ? 500 : 400, fontSize: "13px" }}>
                    {file.is_dir ? <FolderOpen size={15} color="#f59e0b" /> : <FileCode size={14} color="var(--text-muted)" />}
                    {file.name}
                    {file.is_dir && <ChevronRight size={12} style={{ marginLeft: "auto", color: "var(--text-muted)" }} />}
                  </span>
                  <span style={{ flex: 1, fontFamily: "'JetBrains Mono', monospace", fontSize: "12px", color: "var(--text-secondary)" }}>
                    {file.is_dir ? "-" : formatBytes(file.size_bytes)}
                  </span>
                  <span style={{ flex: 1, fontSize: "12px", color: "var(--text-muted)" }}>
                    {new Date(file.modified_at).toLocaleString('zh-CN')}
                  </span>
                  <span style={{ width: "60px", fontFamily: "'JetBrains Mono', monospace", fontSize: "11px", color: "var(--text-muted)" }}>
                    {file.permissions}
                  </span>
                </div>
              ))}
              <div style={{ padding: "8px 14px", background: "var(--bg-tertiary)", borderTop: "1px solid var(--border-subtle)", fontSize: "11.5px", color: "var(--text-muted)" }}>
                共 {directoryListing.file_count} 个文件, {directoryListing.dir_count} 个目录, 总计 {formatBytes(directoryListing.total_size)}
              </div>
            </div>
          ) : (
            <div style={{ textAlign: "center", padding: "40px", color: "var(--text-muted)" }}>
              <FolderOpen size={40} style={{ opacity: 0.3, marginBottom: "12px" }} />
              <div>输入路径或点击快捷路径开始浏览文件系统</div>
            </div>
          )}
        </div>
      )}

      {/* ===== DIAGNOSTICS TAB ===== */}
      {activeTab === "diagnostics" && (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          {/* Tools */}
          <div className="grid-3">
            {/* Ping */}
            <div className="card card-elevated">
              <h4 style={{ margin: "0 0 12px 0", fontSize: "14px", display: "flex", alignItems: "center", gap: "8px" }}>
                <Zap size={16} color="#06b6d4" /> Ping 检测
              </h4>
              <div style={{ display: "flex", gap: "6px" }}>
                <input type="text" className="form-input" placeholder="主机地址" value={pingTarget} onChange={(e) => setPingTarget(e.target.value)} onKeyDown={(e) => e.key === "Enter" && handlePing()} style={{ flex: 1, fontSize: "13px" }} />
                <button className="btn btn-primary btn-sm" onClick={handlePing}>检测</button>
              </div>
            </div>

            {/* Port */}
            <div className="card card-elevated">
              <h4 style={{ margin: "0 0 12px 0", fontSize: "14px", display: "flex", alignItems: "center", gap: "8px" }}>
                <Globe size={16} color="#a855f7" /> 端口检测
              </h4>
              <div style={{ display: "flex", gap: "6px" }}>
                <input type="text" className="form-input" placeholder="主机" value={portHost} onChange={(e) => setPortHost(e.target.value)} style={{ flex: 1, fontSize: "13px" }} />
                <input type="number" className="form-input" placeholder="端口" value={portNum} onChange={(e) => setPortNum(Number(e.target.value))} style={{ width: "75px", fontSize: "13px" }} />
                <button className="btn btn-primary btn-sm" onClick={handlePortCheck}>检测</button>
              </div>
            </div>

            {/* DNS */}
            <div className="card card-elevated">
              <h4 style={{ margin: "0 0 12px 0", fontSize: "14px", display: "flex", alignItems: "center", gap: "8px" }}>
                <Search size={16} color="#22c55e" /> DNS 解析
              </h4>
              <div style={{ display: "flex", gap: "6px" }}>
                <input type="text" className="form-input" placeholder="域名" value={dnsTarget} onChange={(e) => setDnsTarget(e.target.value)} onKeyDown={(e) => e.key === "Enter" && handleDnsLookup()} style={{ flex: 1, fontSize: "13px" }} />
                <button className="btn btn-primary btn-sm" onClick={handleDnsLookup}>查询</button>
              </div>
            </div>
          </div>

          {/* Results */}
          {diagResults.length > 0 && (
            <div className="card card-elevated">
              <h3 className="card-title">诊断结果历史 ({diagResults.length})</h3>
              <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
                {diagResults.map((r, i) => {
                  const res = r.result as Record<string, unknown>;
                  const isOpen = (res as PortCheckResult)?.open ?? false;
                  const isError = !!res.error;
                  return (
                    <div key={`${r.time}-${i}`} style={{
                      padding: "12px 16px", borderRadius: "var(--radius-md"),
                      background: isError ? "rgba(239,68,68,0.04)" : isOpen ? "rgba(34,197,94,0.04)" : "var(--bg-tertiary)",
                      border: `1px solid ${isError ? "rgba(239,68,68,0.15)" : isOpen ? "rgba(34,197,94,0.15)" : "var(--border-subtle)"}`,
                      animation: `staggerFadeIn 0.25s ease-out ${i * 50}ms both`,
                    }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "6px" }}>
                        <span className={`tag ${r.type === "ping" ? "" : r.type === "port" ? "" : ""}`} style={{
                          fontSize: "10px", background:
                            r.type === "ping" ? "rgba(6,182,212,0.08)" :
                            r.type === "port" ? "rgba(168,85,247,0.08)" :
                            "rgba(34,197,94,0.08)",
                          color: r.type === "ping" ? "#06b6d4" : r.type === "port" ? "#a855f7" : "#22c55e",
                        }}>
                          {r.type.toUpperCase()}
                        </span>
                        <strong style={{ fontSize: "13px" }}>{r.label}</strong>
                        <span style={{ marginLeft: "auto", fontSize: "11px", color: "var(--text-muted)" }}>{r.time}ms</span>
                      </div>
                      <pre style={{
                        margin: 0, fontFamily: "'JetBrains Mono', monospace", fontSize: "12px",
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