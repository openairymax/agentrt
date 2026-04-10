import React, { useState, useEffect, useRef } from "react";
import {
  FileText,
  RefreshCw,
  Download,
  Filter,
  Search,
  AlertCircle,
  Info,
  Trash2,
  Pause,
  Play,
  Clock,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import { useI18n } from "../i18n";

const SERVICES = [
  { value: "", label: "All Services" },
  { value: "kernel", label: "Kernel" },
  { value: "gateway", label: "Gateway" },
  { value: "postgres", label: "PostgreSQL" },
  { value: "redis", label: "Redis" },
  { value: "openlab", label: "OpenLab" },
  { value: "prometheus", label: "Prometheus" },
  { value: "grafana", label: "Grafana" },
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
    const interval = setInterval(loadLogs, autoRefresh ? 5000 : 0);
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
    } catch (error) {
      console.error("Download failed:", error);
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

  const getLineColor = (line: string) => {
    const lower = line.toLowerCase();
    if (lower.includes("error") || lower.includes("fatal")) return { color: "#ef4444", weight: "600" };
    if (lower.includes("warn")) return { color: "#f59e0b", weight: "500" };
    if (lower.includes("info")) return { color: "#6366f1", weight: "400" };
    if (lower.includes("debug")) return { color: "#22c55e", weight: "400" };
    return { color: "var(--text-secondary)", weight: "400" };
  };

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <h1>{t.logs.systemLogsViewer}</h1>
        <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
          Real-time system logs with syntax highlighting
        </p>
      </div>

      {/* Control Panel */}
      <div className="card card-elevated" style={{ marginBottom: "20px" }}>
        <div style={{ display: "flex", gap: "12px", flexWrap: "wrap", alignItems: "end" }}>
          {/* Service Filter */}
          <div style={{ minWidth: "160px" }}>
            <label className="form-label">{t.logs.serviceFilter}</label>
            <select className="form-select" value={selectedService} onChange={(e) => setSelectedService(e.target.value)}>
              {SERVICES.map(s => <option key={s.value || 'all'} value={s.value}>{s.label}</option>)}
            </select>
          </div>

          {/* Line Count */}
          <div style={{ width: "130px" }}>
            <label className="form-label">
              <Clock size={13} style={{ display: "inline", marginRight: "4px", verticalAlign: "middle" }} />
              {t.logs.showLastLines}
            </label>
            <select className="form-select" value={tailCount} onChange={(e) => setTailCount(Number(e.target.value))}>
              {[50, 100, 250, 500, 1000].map(n => <option key={n} value={n}>{n}</option>)}
            </select>
          </div>

          {/* Log Level */}
          <div style={{ minWidth: "140px" }}>
            <label className="form-label">Log Level</label>
            <select className="form-select" value={logLevel} onChange={(e) => setLogLevel(e.target.value)}>
              {LOG_LEVELS.map(l => (
                <option key={l.value} value={l.value}>{l.label}</option>
              ))}
            </select>
          </div>

          {/* Search */}
          <div style={{ flex: 1, minWidth: "200px" }}>
            <label className="form-label">{t.logs.searchLogs}</label>
            <div style={{ position: "relative" }}>
              <Search size={15} style={{ position: "absolute", left: "12px", top: "50%", transform: "translateY(-50%)", color: "var(--text-muted)" }} />
              <input type="text" className="form-input" placeholder={t.logs.filterLogs} value={searchTerm} onChange={(e) => setSearchTerm(e.target.value)} style={{ paddingLeft: "36px" }} />
            </div>
          </div>

          {/* Action Buttons */}
          <div style={{ display: "flex", gap: "8px" }}>
            <button className="btn btn-secondary" onClick={loadLogs} disabled={loading}>
              <RefreshCw size={16} />{t.logs.refresh}
            </button>
            <button className={`btn ${autoRefresh ? "btn-primary" : "btn-secondary"}`} onClick={() => setAutoRefresh(!autoRefresh)}>
              {autoRefresh ? <><Pause size={16} />Pause</> : <><Play size={16} />Auto</>}
            </button>
            <button className="btn btn-secondary" onClick={handleDownloadLogs}>
              <Download size={16} />{t.logs.export}
            </button>
            <button className="btn btn-danger" onClick={() => { if (confirm(t.logs.clearConfirm)) setLogs(""); }} disabled={!logs}>
              <Trash2 size={16} />
            </button>
          </div>
        </div>
      </div>

      {/* Log Stats Bar */}
      <div style={{
        display: "flex", justifyContent: "space-between", alignItems: "center",
        padding: "10px 16px", background: "var(--bg-secondary)", borderRadius: "var(--radius-md)",
        fontSize: "12.5px", color: "var(--text-muted)", marginBottom: "12px", border: "1px solid var(--border-subtle)"
      }}>
        <span>Showing <strong>{filteredLogs.split("\n").length}</strong> lines</span>
        <div style={{ display: "flex", gap: "16px" }}>
          {LOG_LEVELS.slice(1).map(lvl => (
              <span key={lvl.value}><span style={{ color: lvl.color }}>●</span> {lvl.label}</span>
          ))}
          <span>{autoRefresh ? "● Auto-refresh ON (5s)" : "○ Auto-refresh OFF"}</span>
        </div>
      </div>

      {/* Log Viewer */}
      <div
        ref={logContainerRef}
        style={{
          background: "var(--bg-primary)", border: "1px solid var(--border-color)",
          borderRadius: "var(--radius-lg)", overflow: "auto", height: "calc(100vh - 380px)",
          minHeight: "420px", fontFamily: "'JetBrains Mono', 'Fira Code', monospace",
          fontSize: "12.5px", lineHeight: "1.7", padding: "16px"
        }}
      >
        {loading ? (
          <div style={{ textAlign: "center", padding: "80px 0", color: "var(--text-muted)" }}>
            <div className="loading-spinner" style={{ margin: "0 auto 16px" }} />
            <p>{t.logs.loadingLogs}</p>
          </div>
        ) : !filteredLogs ? (
          <div className="empty-state">
            <Info size={48} style={{ opacity: 0.3 }} />
            <div className="empty-state-text">{t.logs.noLogsAvailable}</div>
            <div className="empty-state-hint">{t.logs.startServicesForLogs}</div>
          </div>
        ) : (
          <pre style={{ margin: 0, whiteSpace: "pre-wrap", wordBreak: "break-all" }}>
            {filteredLogs.split("\n").map((line, i) => {
              const { color, weight } = getLineColor(line);
              return (
                <div
                  key={i}
                  style={{
                    color, fontWeight: weight, padding: "1px 6px", borderRadius: "3px",
                    transition: "background var(--transition-fast)"
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.background = "rgba(255,255,255,0.04)"}
                  onMouseLeave={(e) => e.currentTarget.style.background = "transparent"}
                >{line || "\u00A0"}</div>
              );
            })}
          </pre>
        )}
      </div>
    </div>
  );
};

export default Logs;
