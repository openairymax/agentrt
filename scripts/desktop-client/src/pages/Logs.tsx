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
} from "lucide-react";
import { invoke } from "@tauri-apps/api/core";

const Logs: React.FC = () => {
  const [logs, setLogs] = useState("");
  const [selectedService, setSelectedService] = useState<string>("");
  const [tailCount, setTailCount] = useState(100);
  const [loading, setLoading] = useState(false);
  const [autoRefresh, setAutoRefresh] = useState(false);
  const [searchTerm, setSearchTerm] = useState("");
  const logContainerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    loadLogs();
  }, []);

  useEffect(() => {
    if (autoRefresh) {
      const interval = setInterval(loadLogs, 5000);
      return () => clearInterval(interval);
    }
  }, [autoRefresh, selectedService, tailCount]);

  const loadLogs = async () => {
    setLoading(true);
    try {
      const logContent = await invoke<string>("get_logs", {
        service: selectedService || null,
        tail: tailCount,
      });
      setLogs(logContent);
    } catch (error) {
      console.error("Failed to load logs:", error);
      setLogs(`Error loading logs: ${error}`);
    } finally {
      setLoading(false);
    }
  };

  const handleDownloadLogs = async () => {
    try {
      await invoke("get_logs", {
        service: selectedService || null,
        tail: 1000,
      }).then((content) => {
        const blob = new Blob([content as string], { type: "text/plain" });
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = `agentos-logs-${new Date().toISOString().slice(0, 19).replace(/:/g, "-")}.log`;
        a.click();
        URL.revokeObjectURL(url);
      });
    } catch (error) {
      console.error("Failed to download logs:", error);
    }
  };

  const clearLogs = () => {
    if (confirm("Are you sure you want to clear the displayed logs?")) {
      setLogs("");
    }
  };

  const filteredLogs = searchTerm
    ? logs
        .split("\n")
        .filter((line) => line.toLowerCase().includes(searchTerm.toLowerCase()))
        .join("\n")
    : logs;

  const logLines = filteredLogs.split("\n").length;

  useEffect(() => {
    if (logContainerRef.current && autoRefresh) {
      logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
    }
  }, [filteredLogs, autoRefresh]);

  return (
    <div>
      {/* Header */}
      <div className="card" style={{ marginBottom: "20px" }}>
        <h3 className="card-title" style={{ marginBottom: 0 }}>
          <FileText size={20} />
          System Logs Viewer
        </h3>
      </div>

      {/* Controls */}
      <div className="card" style={{ marginBottom: "20px" }}>
        <div
          style={{
            display: "flex",
            gap: "12px",
            alignItems: "center",
            flexWrap: "wrap",
          }}
        >
          {/* Service Filter */}
          <div style={{ flex: 1, minWidth: "200px" }}>
            <label style={{ fontSize: "12px", color: "var(--text-muted)", display: "block", marginBottom: "4px" }}>
              Service Filter
            </label>
            <select
              className="input-field"
              value={selectedService}
              onChange={(e) => setSelectedService(e.target.value)}
            >
              <option value="">All Services</option>
              <option value="kernel">Kernel</option>
              <option value="gateway">Gateway</option>
              <option value="postgres">PostgreSQL</option>
              <option value="redis">Redis</option>
              <option value="openlab">OpenLab</option>
              <option value="prometheus">Prometheus</option>
              <option value="grafana">Grafana</option>
            </select>
          </div>

          {/* Tail Count */}
          <div style={{ width: "150px" }}>
            <label style={{ fontSize: "12px", color: "var(--text-muted)", display: "block", marginBottom: "4px" }}>
              Show Last N Lines
            </label>
            <select
              className="input-field"
              value={tailCount}
              onChange={(e) => setTailCount(Number(e.target.value))}
            >
              <option value={50}>Last 50 lines</option>
              <option value={100}>Last 100 lines</option>
              <option value={250}>Last 250 lines</option>
              <option value={500}>Last 500 lines</option>
              <option value={1000}>Last 1000 lines</option>
            </select>
          </div>

          {/* Search */}
          <div style={{ flex: 1, minWidth: "200px" }}>
            <label style={{ fontSize: "12px", color: "var(--text-muted)", display: "block", marginBottom: "4px" }}>
              Search Logs
            </label>
            <div style={{ position: "relative" }}>
              <Search
                size={16}
                style={{
                  position: "absolute",
                  left: "12px",
                  top: "50%",
                  transform: "translateY(-50%)",
                  color: "var(--text-muted)",
                }}
              />
              <input
                type="text"
                className="input-field"
                placeholder="Filter logs..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                style={{ paddingLeft: "36px" }}
              />
            </div>
          </div>

          {/* Action Buttons */}
          <div style={{ display: "flex", gap: "8px", alignItems: "end" }}>
            <button className="btn btn-secondary" onClick={loadLogs} disabled={loading}>
              <RefreshCw size={16} />
              Refresh
            </button>

            <button
              className={`btn ${autoRefresh ? "btn-primary" : "btn-secondary"}`}
              onClick={() => setAutoRefresh(!autoRefresh)}
            >
              {autoRefresh ? "⏸ Auto-refresh ON" : "▶ Auto-refresh OFF"}
            </button>

            <button className="btn btn-secondary" onClick={handleDownloadLogs}>
              <Download size={16} />
              Export
            </button>

            <button className="btn btn-danger" onClick={clearLogs} disabled={!logs}>
              <Trash2 size={16} />
              Clear
            </button>
          </div>
        </div>
      </div>

      {/* Log Stats */}
      <div
        style={{
          display: "flex",
          justifyContent: "space-between",
          alignItems: "center",
          padding: "8px 0",
          marginBottom: "12px",
          fontSize: "13px",
          color: "var(--text-muted)",
        }}
      >
        <span>
          Showing {logLines} lines{searchTerm && ` (filtered by "${searchTerm}")`}
        </span>
        <span>{selectedService ? `Service: ${selectedService}` : "All services"}</span>
      </div>

      {/* Log Viewer */}
      <div
        ref={logContainerRef}
        style={{
          background: "#0d1117",
          border: "1px solid var(--border-color)",
          borderRadius: "10px",
          overflow: "auto",
          height: "calc(100vh - 380px)",
          minHeight: "400px",
          fontFamily: "'Fira Code', 'Cascadia Code', monospace",
          fontSize: "13px",
          lineHeight: "1.6",
          padding: "16px",
        }}
      >
        {loading ? (
          <div style={{ textAlign: "center", padding: "60px", color: "#94a3b8" }}>
            <div className="loading-spinner" />
            <p style={{ marginTop: "16px" }}>Loading logs...</p>
          </div>
        ) : !filteredLogs ? (
          <div style={{ textAlign: "center", padding: "60px", color: "#94a3b8" }}>
            <Info size={48} style={{ opacity: 0.5 }} />
            <p style={{ marginTop: "16px" }}>No logs available</p>
            <p style={{ fontSize: "13px", marginTop: "8px" }}>
              Start services to generate logs
            </p>
          </div>
        ) : (
          <pre
            style={{
              margin: 0,
              whiteSpace: "pre-wrap",
              wordBreak: "break-all",
              color: "#c9d1d9",
            }}
          >
            {filteredLogs.split("\n").map((line, index) => {
              let color = "#c9d1d9";
              let fontWeight = "normal";

              if (line.toLowerCase().includes("error") || line.toLowerCase().includes("fatal")) {
                color = "#f85149";
                fontWeight = "600";
              } else if (line.toLowerCase().includes("warn")) {
                color = "#d29922";
              } else if (line.toLowerCase().includes("info")) {
                color="#58a6ff";
              }

              return (
                <div
                  key={index}
                  style={{
                    color,
                    fontWeight,
                    padding: "2px 4px",
                    borderRadius: "2px",
                    transition: "background 0.15s ease",
                  }}
                  onMouseEnter={(e) =>
                    (e.currentTarget.style.background = "rgba(255,255,255,0.05)")
                  }
                  onMouseLeave={(e) =>
                    (e.currentTarget.style.background = "transparent")
                  }
                >
                  {line || "\u00A0"}
                </div>
              );
            })}
          </pre>
        )}
      </div>

      {/* Footer */}
      <div
        style={{
          marginTop: "12px",
          padding: "12px 16px",
          background: "var(--bg-secondary)",
          borderRadius: "8px",
          fontSize: "12px",
          color: "var(--text-muted)",
          display: "flex",
          justifyContent: "space-between",
          alignItems: "center",
        }}
      >
        <div style={{ display: "flex", gap: "20px" }}>
          <span><span style={{ color: "#f85149" }}>●</span> Error</span>
          <span><span style={{ color: "#d29922" }}>●</span> Warning</span>
          <span><span style={{ color: "#58a6ff" }}>●</span> Info</span>
        </div>
        <span>
          Auto-refresh: {autoRefresh ? "ON (5s)" : "OFF"}
        </span>
      </div>
    </div>
  );
};

export default Logs;
