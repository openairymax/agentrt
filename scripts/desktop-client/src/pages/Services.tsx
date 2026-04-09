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
    <div>
      <div
        className="card"
        style={{ marginBottom: "20px", display: "flex", justifyContent: "space-between", alignItems: "center" }}
      >
        <div>
          <h3 className="card-title" style={{ marginBottom: 0 }}>
            <Server size={20} />
            {t.services.title}
          </h3>
          <p style={{ color: "var(--text-secondary)", fontSize: "14px", marginTop: "4px" }}>
            {t.services.subtitle}
          </p>
        </div>

        <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
          <select
            className="input-field"
            value={deployMode}
            onChange={(e) => setDeployMode(e.target.value as "dev" | "prod")}
            style={{ width: "auto", padding: "8px 12px" }}
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
      </div>

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
            {t.services.startAll} ({deployMode})
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

      <div className="card">
        <h3 className="card-title">
          {t.services.activeServices} ({services.length})
        </h3>

        {loading ? (
          <div style={{ textAlign: "center", padding: "40px" }}>
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
                  <th
                    style={{
                      padding: "12px",
                      textAlign: "left",
                      color: "var(--text-secondary)",
                      fontWeight: 600,
                    }}
                  >
                    {t.services.serviceName}
                  </th>
                  <th
                    style={{
                      padding: "12px",
                      textAlign: "left",
                      color: "var(--text-secondary)",
                      fontWeight: 600,
                    }}
                  >
                    {t.services.status}
                  </th>
                  <th
                    style={{
                      padding: "12px",
                      textAlign: "left",
                      color: "var(--text-secondary)",
                      fontWeight: 600,
                    }}
                  >
                    {t.services.port}
                  </th>
                  <th
                    style={{
                      padding: "12px",
                      textAlign: "right",
                      color: "var(--text-secondary)",
                      fontWeight: 600,
                    }}
                  >
                    {t.services.actions}
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

      <div className="grid-2">
        <div className="card">
          <h3 className="card-title">{t.services.devModeInfo}</h3>
          <ul style={{ color: "var(--text-secondary)", lineHeight: "1.8", paddingLeft: "20px" }}>
            <li>{t.services.devGatewayApi}: <code>http://localhost:18789</code></li>
            <li>{t.services.devKernelIpc}: <code>http://localhost:18080</code></li>
            <li>{t.services.devOpenLabUi}: <code>http://localhost:3000</code></li>
            <li>{t.services.devGrafana}: <code>http://localhost:3001</code></li>
          </ul>
        </div>

        <div className="card">
          <h3 className="card-title">{t.services.prodModeInfo}</h3>
          <ul style={{ color: "var(--text-secondary)", lineHeight: "1.8", paddingLeft: "20px" }}>
            <li>Gateway: <code>:18789</code> ({t.services.prodHttpsProxy})</li>
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
