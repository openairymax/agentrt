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
  Container,
  Settings2,
} from "lucide-react";
import { invoke } from "../utils/tauriCompat";
import { useI18n } from "../i18n";

interface ServiceStatus {
  name: string;
  status: string;
  healthy: boolean;
  port?: number;
}

const Services: React.FC = () => {
  const { t } = useI18n();
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
      alert(`${t.services.failedToStart}: ${error}`);
    } finally {
      setActionLoading(null);
    }
  };

  const handleStopAll = async () => {
    if (!confirm(t.services.confirmStopAll)) return;

    setActionLoading("stop");
    try {
      await invoke("stop_services");
      setServices([]);
    } catch (error) {
      console.error("Failed to stop services:", error);
      alert(`${t.services.failedToStop}: ${error}`);
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
      alert(`${t.services.failedToRestart}: ${error}`);
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
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <h1>{t.services.title}</h1>
        <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
          {t.services.subtitle}
        </p>
      </div>

      {/* Control Panel */}
      <div className="card card-elevated" style={{ marginBottom: "24px" }}>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", flexWrap: "wrap", gap: "16px" }}>
          <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
            <select
              className="form-select"
              value={deployMode}
              onChange={(e) => setDeployMode(e.target.value as "dev" | "prod")}
              style={{ width: "auto", padding: "8px 14px" }}
            >
              <option value="dev">{t.services.devMode}</option>
              <option value="prod">{t.services.prodMode}</option>
            </select>

            <button
              className="btn btn-secondary"
              onClick={loadServiceStatus}
              disabled={loading}
            >
              <RefreshCw size={16} />
              {t.services.refresh}
            </button>
          </div>

          <div style={{ display: "flex", gap: "10px" }}>
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
              {t.services.startAll}
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
              {t.services.stopAll}
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
              {t.services.restartAll}
            </button>
          </div>
        </div>
      </div>

      {/* Services Table */}
      <div className="card card-elevated">
        <h3 className="card-title">
          <Container size={18} />
          {t.services.activeServices} ({services.length})
        </h3>

        {loading ? (
          <div style={{ textAlign: "center", padding: "48px" }}>
            <div className="loading-spinner" />
          </div>
        ) : services.length === 0 ? (
          <div className="empty-state">
            <div className="empty-state-icon">🐳</div>
            <div className="empty-state-text">{t.services.noRunningServices}</div>
            <div className="empty-state-hint">
              {t.services.noRunningServicesHint} ({deployMode === "dev" ? t.services.devMode : t.services.prodMode})
            </div>
          </div>
        ) : (
          <div style={{ overflowX: "auto" }}>
            <table style={{ width: "100%", borderCollapse: "collapse" }}>
              <thead>
                <tr style={{ borderBottom: "2px solid var(--border-color)" }}>
                  <th style={{
                    padding: "14px 16px",
                    textAlign: "left",
                    color: "var(--text-secondary)",
                    fontWeight: 600,
                    fontSize: "13px",
                    textTransform: "uppercase",
                    letterSpacing: "0.05em",
                  }}>
                    {t.services.serviceName}
                  </th>
                  <th style={{
                    padding: "14px 16px",
                    textAlign: "left",
                    color: "var(--text-secondary)",
                    fontWeight: 600,
                    fontSize: "13px",
                    textTransform: "uppercase",
                    letterSpacing: "0.05em",
                  }}>
                    {t.services.status}
                  </th>
                  <th style={{
                    padding: "14px 16px",
                    textAlign: "left",
                    color: "var(--text-secondary)",
                    fontWeight: 600,
                    fontSize: "13px",
                    textTransform: "uppercase",
                    letterSpacing: "0.05em",
                  }}>
                    {t.services.port}
                  </th>
                  <th style={{
                    padding: "14px 16px",
                    textAlign: "right",
                    color: "var(--text-secondary)",
                    fontWeight: 600,
                    fontSize: "13px",
                    textTransform: "uppercase",
                    letterSpacing: "0.05em",
                  }}>
                    {t.services.actions}
                  </th>
                </tr>
              </thead>
              <tbody>
                {services.map((service) => (
                  <tr
                    key={service.name}
                    style={{ borderBottom: "1px solid var(--border-subtle)" }}
                  >
                    <td style={{ padding: "16px" }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                        {service.healthy ? (
                          <CheckCircle2 size={18} color="#22c55e" />
                        ) : (
                          <XCircle size={18} color="#ef4444" />
                        )}
                        <span style={{ fontWeight: 500, fontSize: "14px" }}>{service.name}</span>
                      </div>
                    </td>
                    <td style={{ padding: "16px" }}>
                      <span className={`badge ${service.healthy ? "status-running" : "status-stopped"}`}>
                        {service.status.split("(")[0].trim()}
                      </span>
                    </td>
                    <td style={{ padding: "16px", color: "var(--text-muted)", fontSize: "14px" }}>
                      {service.port ? `:${service.port}` : "-"}
                    </td>
                    <td style={{ padding: "16px", textAlign: "right" }}>
                      {service.port && service.healthy && (
                        <button
                          className="btn btn-ghost btn-sm"
                          onClick={() => openServiceUrl(service.port!)}
                          title={`Open http://localhost:${service.port}`}
                        >
                          <ExternalLink size={14} />
                          {t.services.open}
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

      {/* Mode Info Cards */}
      <div className="grid-2" style={{ marginTop: "24px" }}>
        <div className="card card-elevated">
          <h3 className="card-title">
            <Settings2 size={18} />
            {t.services.devModeInfo}
          </h3>
          <ul style={{ color: "var(--text-secondary)", lineHeight: "2", paddingLeft: "20px", margin: 0 }}>
            <li>{t.services.devGatewayApi}: <code style={{
              background: "var(--bg-tertiary)",
              padding: "2px 8px",
              borderRadius: "4px",
              fontSize: "13px",
              fontFamily: "'JetBrains Mono', monospace",
            }}>http://localhost:18789</code></li>
            <li>{t.services.devKernelIpc}: <code style={{
              background: "var(--bg-tertiary)",
              padding: "2px 8px",
              borderRadius: "4px",
              fontSize: "13px",
              fontFamily: "'JetBrains Mono', monospace",
            }}>http://localhost:18080</code></li>
            <li>{t.services.devOpenLabUi}: <code style={{
              background: "var(--bg-tertiary)",
              padding: "2px 8px",
              borderRadius: "4px",
              fontSize: "13px",
              fontFamily: "'JetBrains Mono', monospace",
            }}>http://localhost:3000</code></li>
            <li>{t.services.devGrafana}: <code style={{
              background: "var(--bg-tertiary)",
              padding: "2px 8px",
              borderRadius: "4px",
              fontSize: "13px",
              fontFamily: "'JetBrains Mono', monospace",
            }}>http://localhost:3001</code></li>
          </ul>
        </div>

        <div className="card card-elevated">
          <h3 className="card-title">
            <Server size={18} />
            {t.services.prodModeInfo}
          </h3>
          <ul style={{ color: "var(--text-secondary)", lineHeight: "2", paddingLeft: "20px", margin: 0 }}>
            <li>Gateway: <code style={{
              background: "var(--bg-tertiary)",
              padding: "2px 8px",
              borderRadius: "4px",
              fontSize: "13px",
              fontFamily: "'JetBrains Mono', monospace",
            }}>:18789</code> ({t.services.prodHttpsProxy})</li>
            <li>{t.services.prodAutoScaling}</li>
            <li>{t.services.prodHealthChecks}</li>
            <li>{t.services.prodMonitoring}</li>
          </ul>
        </div>
      </div>
    </div>
  );
};

export default Services;
