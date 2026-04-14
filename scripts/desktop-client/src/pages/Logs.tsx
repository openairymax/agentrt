import React, { useState, useEffect, useRef } from "react";
import {
  FileText,
  RefreshCw,
  Download,
  Search,
  AlertCircle,
  Info,
  Trash2,
  Pause,
  Play,
  Clock,
  Terminal,
  ArrowDown,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import { useI18n } from "../i18n";
import { useAlert } from "../components/useAlert";

const SERVICES = [
  { value: "", label: "All Services", color: "#9ca3af" },
  { value: "kernel", label: "Kernel", color: "#8b5cf6" },
  { value: "gateway", label: "Gateway", color: "#22c55e" },
  { value: "postgres", label: "PostgreSQL", color: "#3b82f6" },
  { value: "redis", label: "Redis", color: "#ef4444" },
  { value: "openlab", label: "OpenLab", color: "#a855f7" },
  { value: "prometheus", label: "Prometheus", color: "#f59e0b" },
  { value: "grafana", label: "Grafana", color: "#06b6d4" },
];

const LOG_LEVELS = [
  { value: "", label: "All Levels", color: "#9ca3af" },
  { value: "error", label: "Error", color: "#ef4444" },
  { value: "warn", label: "Warning", color: "#f59e0b" },
  { value: "info", label: "Info", color: "#6366f1" },
  { value: "debug", label: "Debug", color: "#22c55e" },
];

const Logs: React.FC = () => {
  const { t } = useI18n();
  const { error, confirm: confirmModal } = useAlert();
  const [logs, setLogs] = useState("");
  const [selectedService, setSelectedService] = useState("");
  const [tailCount, setTailCount] = useState(100);
  const [loading, setLoading] = useState(false);
  const [autoRefresh, setAutoRefresh] = useState(false);
  const [searchTerm, setSearchTerm] = useState("");
  const [logLevel, setLogLevel] = useState("");
  const logContainerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    loadLogs();
    if (!autoRefresh) return;
    const interval = setInterval(loadLogs, 5000);
    return () => clearInterval(interval);
  }, [autoRefresh, selectedService, tailCount]);

  useEffect(() => {
    if (logContainerRef.current && autoRefresh) {
      logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
    }
  }, [logs, autoRefresh]);

  const loadLogs = async () => {
    setLoading(true);
    try {
      const content = await sdk.getLogs(selectedService || undefined, tailCount);
      setLogs(content);
    } catch (error) {
      setLogs(`${t.common.error}: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const handleDownloadLogs = async () => {
    try {
      const content = await sdk.getLogs(selectedService || undefined, 1000);
      const blob = new Blob([content], { type: "text/plain" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = `agentos-logs-${new Date().toISOString().slice(0, 19).replace(/:/g, "-")}.log`;
      a.click();
      URL.revokeObjectURL(url);
    } catch (err) {
      error("下载失败", `无法下载日志文件: ${err}`);
    }
  };

  const filteredLogs = (() => {
    let result = logs;
    if (logLevel) {
      result = result.split("\n").filter(line => line.toLowerCase().includes(logLevel.toLowerCase())).join("\n");
    }
    if (searchTerm) {
      result = result.split("\n").filter(line => line.toLowerCase().includes(searchTerm.toLowerCase())).join("\n");
    }
    return result;
  })();

  const lines = filteredLogs.split("\n");

  const getLogLevelColor = (text: string) => {
    const lower = text.toLowerCase();
    if (lower.includes("error") || lower.includes("fatal")) return { color: "var(--error-color)", level: "error" };
    if (lower.includes("warn")) return { color: "var(--warning-color)", level: "warn" };
    if (lower.includes("info")) return { color: "var(--primary-color)", level: "info" };
    if (lower.includes("debug")) return { color: "var(--success-color)", level: "debug" };
    return { color: "var(--text-secondary)", level: "info" };
  };

  const levelCounts = (() => {
    const counts: Record<string, number> = { error: 0, warn: 0, info: 0, debug: 0, total: 0 };
    logs.split("\n").forEach(line => {
      const lower = line.toLowerCase();
      counts.total++;
      if (lower.includes("error") || lower.includes("fatal")) counts.error++;
      else if (lower.includes("warn")) counts.warn++;
      else if (lower.includes("info")) counts.info++;
      else if (lower.includes("debug")) counts.debug++;
    });
    return counts;
  })();

  const highlightSearch = (text: string, term: string): React.ReactNode => {
    if (!term) return text;
    const lower = text.toLowerCase();
    const idx = lower.indexOf(term.toLowerCase());
    if (idx === -1) return text;
    return (
      <>
        {text.substring(0, idx)}
        <mark style={{ background: "var(--warning-light)", color: "var(--warning-color)", borderRadius: "2px", padding: "0 2px" }}>{text.substring(idx, idx + term.length)}</mark>
        {highlightSearch(text.substring(idx + term.length), term)}
      </>
    );
  };

  const scrollToBottom = () => {
    if (logContainerRef.current) {
      logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
    }
  };

  return (
    <div className="page-container">
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
          <div style={{
            width: "44px", height: "44px", borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#06b6d4,#22d3ee)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(6,182,212,0.35), 0 0 0 1px rgba(255,255,255,0.08) inset",
          }}>
            <FileText size={20} color="white" />
          </div>
          <div>
            <h1>{t.logs.systemLogsViewer}</h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: 0 }}>
              实时系统日志 · 语法高亮 · 多维度筛选
            </p>
          </div>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
          {autoRefresh && (
            <div style={{
              display: "flex", alignItems: "center", gap: "6px",
              padding: "5px 14px", borderRadius: "var(--radius-full)",
              background: "rgba(34,197,94,0.1)", border: "1px solid rgba(34,197,94,0.2)",
              fontSize: "12px", fontWeight: 600, color: "#22c55e",
            }}>
              <div style={{ width: "6px", height: "6px", borderRadius: "50%", background: "#22c55e", animation: "statusPulse 2s infinite" }} />
              自动刷新 5s
            </div>
          )}
        </div>
      </div>

      {/* Stats Bar */}
      <div style={{
        display: "grid", gridTemplateColumns: "repeat(5, 1fr)", gap: "10px",
        marginBottom: "16px",
      }}>
        {[
          { label: "总行数", value: levelCounts.total, color: "var(--text-primary)", bg: "var(--bg-secondary)" },
          { label: "Error", value: levelCounts.error, color: "var(--error-color)", bg: "var(--error-light)" },
          { label: "Warning", value: levelCounts.warn, color: "var(--warning-color)", bg: "var(--warning-light)" },
          { label: "Info", value: levelCounts.info, color: "var(--primary-color)", bg: "var(--primary-light)" },
          { label: "Debug", value: levelCounts.debug, color: "var(--success-color)", bg: "var(--success-light)" },
        ].map(stat => (
          <div key={stat.label} style={{
            padding: "12px 16px", borderRadius: "var(--radius-md)",
            background: stat.bg, border: "1px solid var(--border-subtle)",
            display: "flex", alignItems: "center", justifyContent: "space-between",
          }}>
            <span style={{ fontSize: "11.5px", color: "var(--text-muted)", fontWeight: 600, textTransform: "uppercase", letterSpacing: "0.03em" }}>{stat.label}</span>
            <span style={{ fontSize: "18px", fontWeight: 800, color: stat.color, fontFamily: "var(--font-mono)" }}>{stat.value}</span>
          </div>
        ))}
      </div>

      {/* Control Panel */}
      <div className="card card-elevated" style={{ marginBottom: "16px", padding: "14px 18px" }}>
        <div style={{ display: "flex", gap: "10px", flexWrap: "wrap", alignItems: "center" }}>
          <div style={{ minWidth: "150px" }}>
            <select className="form-select" value={selectedService} onChange={(e) => setSelectedService(e.target.value)} style={{ fontSize: "12.5px", padding: "8px 12px" }}>
              {SERVICES.map(s => <option key={s.value || 'all'} value={s.value}>{s.label}</option>)}
            </select>
          </div>
          <div style={{ width: "110px" }}>
            <select className="form-select" value={tailCount} onChange={(e) => setTailCount(Number(e.target.value))} style={{ fontSize: "12.5px", padding: "8px 12px" }}>
              {[50, 100, 250, 500, 1000].map(n => <option key={n} value={n}>{n} 行</option>)}
            </select>
          </div>
          <div style={{ minWidth: "130px" }}>
            <select className="form-select" value={logLevel} onChange={(e) => setLogLevel(e.target.value)} style={{ fontSize: "12.5px", padding: "8px 12px" }}>
              {LOG_LEVELS.map(l => <option key={l.value} value={l.value}>{l.label}</option>)}
            </select>
          </div>
          <div style={{ flex: 1, minWidth: "180px", position: "relative" }}>
            <Search size={14} style={{ position: "absolute", left: "12px", top: "50%", transform: "translateY(-50%)", color: "var(--text-muted)" }} />
            <input type="text" className="form-input" placeholder="搜索日志内容..." value={searchTerm} onChange={(e) => setSearchTerm(e.target.value)} style={{ paddingLeft: "34px", fontSize: "12.5px", padding: "8px 12px 8px 34px" }} />
          </div>
          <div style={{ display: "flex", gap: "4px" }}>
            <button className="btn btn-ghost btn-sm" onClick={loadLogs} disabled={loading} title="Refresh">
              <RefreshCw size={14} />
            </button>
            <button className={`btn btn-sm ${autoRefresh ? "btn-primary" : "btn-ghost"}`} onClick={() => setAutoRefresh(!autoRefresh)} title={autoRefresh ? "Pause" : "Auto Refresh"}>
              {autoRefresh ? <Pause size={14} /> : <Play size={14} />}
            </button>
            <button className="btn btn-ghost btn-sm" onClick={handleDownloadLogs} title="Download">
              <Download size={14} />
            </button>
            <button className="btn btn-ghost btn-sm" onClick={async () => {
              const confirmed = await confirmModal({
                type: 'danger',
                title: '清除日志',
                message: t.logs.clearConfirm,
              });
              if (confirmed) setLogs("");
            }} disabled={!logs} title="Clear" style={{ color: logs ? "var(--error-color)" : undefined }}>
              <Trash2 size={14} />
            </button>
          </div>
        </div>
      </div>

      {/* Log Viewer */}
      <div style={{ position: "relative" }}>
        <div
          ref={logContainerRef}
          style={{
            background: "#0a0a0f", border: "1px solid var(--border-color)",
            borderRadius: "var(--radius-lg)", overflow: "auto", height: "calc(100vh - 380px)",
            minHeight: "400px", fontFamily: "var(--font-mono)",
            fontSize: "12px", lineHeight: "1.65",
          }}
        >
          {/* Terminal-style title bar */}
          <div style={{
            position: "sticky", top: 0, zIndex: 2,
            background: "#111118", borderBottom: "1px solid var(--border-subtle)",
            padding: "8px 16px", display: "flex", alignItems: "center", gap: "8px",
          }}>
            <div style={{ width: "10px", height: "10px", borderRadius: "50%", background: "var(--error-color)" }} />
            <div style={{ width: "10px", height: "10px", borderRadius: "50%", background: "var(--warning-color)" }} />
            <div style={{ width: "10px", height: "10px", borderRadius: "50%", background: "var(--success-color)" }} />
            <span style={{ marginLeft: "8px", fontSize: "11.5px", color: "var(--text-muted)", fontFamily: "var(--font-mono)" }}>
              {selectedService ? SERVICES.find(s => s.value === selectedService)?.label : "All Services"} — {lines.length} 行
            </span>
          </div>

          {loading ? (
            <div style={{ display: "flex", justifyContent: "center", alignItems: "center", height: "200px", flexDirection: "column", gap: "12px" }}>
              <div className="loading-spinner" />
              <span style={{ fontSize: "12px", color: "var(--text-muted)" }}>{t.logs.loadingLogs}</span>
            </div>
          ) : !filteredLogs ? (
            <div style={{ display: "flex", justifyContent: "center", alignItems: "center", height: "200px", flexDirection: "column", gap: "12px" }}>
              <Terminal size={32} style={{ color: "var(--text-muted)", opacity: 0.3 }} />
              <span style={{ fontSize: "13px", color: "var(--text-muted)" }}>{t.logs.noLogsAvailable}</span>
              <span style={{ fontSize: "11.5px", color: "var(--text-muted)", opacity: 0.6 }}>{t.logs.startServicesForLogs}</span>
            </div>
          ) : (
            <div style={{ display: "flex" }}>
              {/* Line Numbers */}
              <div style={{
                padding: "12px 10px 12px 16px", textAlign: "right", userSelect: "none",
                color: "var(--text-muted)", opacity: 0.3, fontSize: "11px",
                borderRight: "1px solid rgba(255,255,255,0.04)",
                position: "sticky", left: 0, background: "#0a0a0f", zIndex: 1,
              }}>
                {lines.map((_, i) => (
                  <div key={i} style={{ height: "19.8px" }}>{i + 1}</div>
                ))}
              </div>
              {/* Log Content */}
              <div style={{ flex: 1, padding: "12px 16px", minWidth: 0 }}>
                {lines.map((line, i) => {
                  const info = getLogLevelColor(line);
                  return (
                    <div
                      key={i}
                      style={{
                        color: info.color, fontWeight: info.level === "error" ? 600 : info.level === "warn" ? 500 : 400,
                        padding: "0 6px", borderRadius: "3px", whiteSpace: "pre-wrap", wordBreak: "break-all",
                        transition: "background var(--transition-fast)",
                        minHeight: "19.8px",
                      }}
                      onMouseEnter={(e) => e.currentTarget.style.background = "rgba(255,255,255,0.03)"}
                      onMouseLeave={(e) => e.currentTarget.style.background = "transparent"}
                    >
                      {searchTerm ? highlightSearch(line || "\u00A0", searchTerm) : (line || "\u00A0")}
                    </div>
                  );
                })}
              </div>
            </div>
          )}
        </div>

        {/* Scroll to bottom button */}
        <button
          onClick={scrollToBottom}
          style={{
            position: "absolute", bottom: "14px", right: "14px",
            width: "32px", height: "32px", borderRadius: "50%",
            background: "var(--bg-elevated)", border: "1px solid var(--border-color)",
            display: "flex", alignItems: "center", justifyContent: "center",
            cursor: "pointer", boxShadow: "var(--shadow-md)",
            transition: "all var(--transition-fast)",
          }}
          onMouseEnter={e => { e.currentTarget.style.background = "var(--primary-color)"; e.currentTarget.style.borderColor = "var(--primary-color)"; }}
          onMouseLeave={e => { e.currentTarget.style.background = "var(--bg-elevated)"; e.currentTarget.style.borderColor = "var(--border-color)"; }}
        >
          <ArrowDown size={14} color="white" />
        </button>
      </div>
    </div>
  );
};

export default Logs;
