import React, { useState, useEffect } from "react";
import {
  Server,
  Play,
  Square,
  RotateCcw,
  RefreshCw,
  CheckCircle2,
  XCircle,
  Loader2,
  ExternalLink,
} from "lucide-react";
import { invoke } from "@tauri-apps/api/core";

interface ServiceStatus {
  name: string;
  status: string;
  healthy: boolean;
  port?: number;
}

const Services: React.FC = () => {
  const [services, setServices] = useState<ServiceStatus[]>([]);
  const [loading, setLoading] = useState(true);
  const [actionLoading, setActionLoading] = useState<string | null>(null);
  const [deployMode, setDeployMode] = useState<"dev" | "prod">("dev");

  useEffect(() => {
    loadServiceStatus();
  }, []);

  const loadServiceStatus = async () => {
    setLoading(true);
    try {
      const status = await invoke<ServiceStatus[]>("get_service_status");
      setServices(status);
    } catch (error) {
      console.error("Failed to load service status:", error);
    } finally {
      setLoading(false);
    }
  };

  const handleStartAll = async () => {
    setActionLoading("start");
    try {
      await invoke("start_services", { mode: deployMode });
      await loadServiceStatus();
    } catch (error) {
      console.error("Failed to start services:", error);
      alert(`Failed to start services: ${error}`);
    } finally {
      setActionLoading(null);
    }
  };

  const handleStopAll = async () => {
    if (!confirm("Are you sure you want to stop all services?")) return;

    setActionLoading("stop");
    try {
      await invoke("stop_services");
      setServices([]);
    } catch (error) {
      console.error("Failed to stop services:", error);
      alert(`Failed to stop services: ${error}`);
    } finally {
      setActionLoading(null);
    }
  };

  const handleRestartAll = async () => {
    setActionLoading("restart");
    try {
      await invoke("restart_services", { mode: deployMode });
      await loadServiceStatus();
    } catch (error) {
      console.error("Failed to restart services:", error);
      alert(`Failed to restart services: ${error}`);
    } finally {
      setActionLoading(null);
    }
  };

  const openServiceUrl = (port: number) => {
    if (port === 18789 || port === 18080 || port === 3000 || port === 3001) {
      invoke("open_browser", { url: `http://localhost:${port}` });
    }
  };

  return (
    <div>
      {/* Header Actions */}
      <div
        className="card"
        style={{ marginBottom: "20px", display: "flex", justifyContent: "space-between", alignItems: "center" }}
      >
        <div>
          <h3 className="card-title" style={{ marginBottom: 0 }}>
            <Server size={20} />
            Docker Services Management
          </h3>
          <p style={{ color: "var(--text-secondary)", fontSize: "14px", marginTop: "4px" }}>
            Manage your AgentOS containerized services
          </p>
        </div>

        <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
          <select
            className="input-field"
            value={deployMode}
            onChange={(e) => setDeployMode(e.target.value as "dev" | "prod")}
            style={{ width: "auto", padding: "8px 12px" }}
          >
            <option value="dev">Development</option>
            <option value="prod">Production</option>
          </select>

          <button
            className="btn btn-secondary"
            onClick={loadServiceStatus}
            disabled={loading}
          >
            <RefreshCw size={16} />
            Refresh
          </button>
        </div>
      </div>

      {/* Control Buttons */}
      <div className="card" style={{ marginBottom: "20px" }}>
        <div style={{ display: "flex", gap: "12px", flexWrap: "wrap" }}>
          <button
            className="btn btn-success"
            onClick={handleStartAll}
            disabled={actionLoading !== null}
          >
            {actionLoading === "start" ? (
              <Loader2 size={16} className="spin" />
            ) : (
              <Play size={16} />
            )}
            Start All ({deployMode})
          </button>

          <button
            className="btn btn-danger"
            onClick={handleStopAll}
            disabled={actionLoading !== null || services.length === 0}
          >
            {actionLoading === "stop" ? (
              <Loader2 size={16} className="spin" />
            ) : (
              <Square size={16} />
            )}
            Stop All
          </button>

          <button
            className="btn btn-secondary"
            onClick={handleRestartAll}
            disabled={actionLoading !== null || services.length === 0}
          >
            {actionLoading === "restart" ? (
              <Loader2 size={16} className="spin" />
            ) : (
              <RotateCcw size={16} />
            )}
            Restart All
          </button>
        </div>
      </div>

      {/* Service List */}
      <div className="card">
        <h3 className="card-title">
          Active Services ({services.length})
        </h3>

        {loading ? (
          <div style={{ textAlign: "center", padding: "40px" }}>
            <div className="loading-spinner" />
          </div>
        ) : services.length === 0 ? (
          <div className="empty-state">
            <div className="empty-state-icon">🐳</div>
            <div className="empty-state-text">No running services</div>
            <div className="empty-state-hint">
              Click "Start All" to launch AgentOS services in {deployMode} mode
            </div>
          </div>
        ) : (
          <div style={{ overflowX: "auto" }}>
            <table style={{ width: "100%", borderCollapse: "collapse" }}>
              <thead>
                <tr style={{ borderBottom: "2px solid var(--border-color)" }}>
                  <th
                    style={{
                      padding: "12px",
                      textAlign: "left",
                      color: "var(--text-secondary)",
                      fontWeight: 600,
                    }}
                  >
                    Service Name
                  </th>
                  <th
                    style={{
                      padding: "12px",
                      textAlign: "left",
                      color: "var(--text-secondary)",
                      fontWeight: 600,
                    }}
                  >
                    Status
                  </th>
                  <th
                    style={{
                      padding: "12px",
                      textAlign: "left",
                      color: "var(--text-secondary)",
                      fontWeight: 600,
                    }}
                  >
                    Port
                  </th>
                  <th
                    style={{
                      padding: "12px",
                      textAlign: "right",
                      color: "var(--text-secondary)",
                      fontWeight: 600,
                    }}
                  >
                    Actions
                  </th>
                </tr>
              </thead>
              <tbody>
                {services.map((service) => (
                  <tr
                    key={service.name}
                    style={{ borderBottom: "1px solid var(--border-color)" }}
                  >
                    <td style={{ padding: "16px 12px" }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
                        {service.healthy ? (
                          <CheckCircle2 size={18} color="#10b981" />
                        ) : (
                          <XCircle size={18} color="#ef4444" />
                        )}
                        <span style={{ fontWeight: 500 }}>{service.name}</span>
                      </div>
                    </td>
                    <td style={{ padding: "16px 12px" }}>
                      <span
                        className={`status-badge ${
                          service.healthy ? "status-running" : "status-stopped"
                        }`}
                      >
                        {service.status.split("(")[0].trim()}
                      </span>
                    </td>
                    <td style={{ padding: "16px 12px", color: "var(--text-muted)" }}>
                      {service.port ? `:${service.port}` : "-"}
                    </td>
                    <td style={{ padding: "16px 12px", textAlign: "right" }}>
                      {service.port && service.healthy && (
                        <button
                          className="btn btn-secondary"
                          style={{ padding: "6px 12px", fontSize: "13px" }}
                          onClick={() => openServiceUrl(service.port!)}
                          title={`Open http://localhost:${service.port}`}
                        >
                          <ExternalLink size={14} />
                          Open
                        </button>
                      )}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>

      {/* Service Info */}
      <div className="grid-2">
        <div className="card">
          <h3 className="card-title">Development Mode</h3>
          <ul style={{ color: "var(--text-secondary)", lineHeight: "1.8", paddingLeft: "20px" }}>
            <li>Gateway API: <code>http://localhost:18789</code></li>
            <li>Kernel IPC: <code>http://localhost:18080</code></li>
            <li>OpenLab UI: <code>http://localhost:3000</code></li>
            <li>Grafana: <code>http://localhost:3001</code></li>
          </ul>
        </div>

        <div className="card">
          <h3 className="card-title">Production Mode</h3>
          <ul style={{ color: "var(--text-secondary)", lineHeight: "1.8", paddingLeft: "20px" }}>
            <li>Gateway: <code>:18789</code> (HTTPS via reverse proxy)</li>
            <li>Auto-scaling enabled</li>
            <li>Health checks active</li>
            <li>Monitoring & logging integrated</li>
          </ul>
        </div>
      </div>
    </div>
  );
};

export default Services;
