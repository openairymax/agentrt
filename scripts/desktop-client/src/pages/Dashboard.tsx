import React, { useState, useEffect } from "react";
import { useNavigate } from "react-router-dom";
import {
  Server,
  Cpu,
  HardDrive,
  Activity,
  CheckCircle2,
  XCircle,
  Terminal,
  FileText,
  Play,
  Square,
  RotateCcw,
  Zap,
  TrendingUp,
  Shield,
} from "lucide-react";
import { invoke } from "../utils/tauriCompat";
import { useI18n } from "../i18n";

interface SystemInfo {
  os: string;
  os_version: string;
  architecture: string;
  cpu_cores: number;
  total_memory_gb: number;
  free_memory_gb: number;
  hostname: string;
}

interface ServiceStatus {
  name: string;
  status: string;
  healthy: boolean;
  port?: number;
}

const Dashboard: React.FC = () => {
  const { t } = useI18n();
  const navigate = useNavigate();
  const [systemInfo, setSystemInfo] = useState<SystemInfo | null>(null);
  const [services, setServices] = useState<ServiceStatus[]>([]);
  const [loading, setLoading] = useState(true);
  const [actionLoading, setActionLoading] = useState<string | null>(null);

  useEffect(() => {
    loadDashboardData();
    const interval = setInterval(loadDashboardData, 30000);
    return () => clearInterval(interval);
  }, []);

  const loadDashboardData = async () => {
    try {
      const sysInfo = await invoke<SystemInfo>("get_system_info");
      setSystemInfo(sysInfo);
      const svcStatus = await invoke<ServiceStatus[]>("get_service_status").catch(() => []);
      setServices(svcStatus);
    } catch (error) {
      console.error("Failed to load dashboard data:", error);
    } finally {
      setLoading(false);
    }
  };

  const runningServices = services.filter((s) => s.healthy).length;
  const totalServices = services.length;
  const healthPercentage =
    totalServices > 0 ? Math.round((runningServices / totalServices) * 100) : 0;

  const handleQuickAction = async (action: string) => {
    switch (action) {
      case 'start':
        setActionLoading('start');
        try {
          await invoke("start_services", { mode: "dev" });
          await loadDashboardData();
        } catch (e) {
          alert(`${t.services.failedToStart}: ${e}`);
        }
        setActionLoading(null);
        break;
      case 'stop':
        if (!confirm(t.services.confirmStopAll)) return;
        setActionLoading('stop');
        try {
          await invoke("stop_services");
          setServices([]);
        } catch (e) {
          alert(`${t.services.failedToStop}: ${e}`);
        }
        setActionLoading(null);
        break;
      case 'restart':
        setActionLoading('restart');
        try {
          await invoke("restart_services", { mode: "dev" });
          await loadDashboardData();
        } catch (e) {
          alert(`${t.services.failedToRestart}: ${e}`);
        }
        setActionLoading(null);
        break;
      case 'terminal':
        navigate('/terminal');
        break;
      case 'logs':
        navigate('/logs');
        break;
    }
  };

  if (loading) {
    return (
      <div className="page-container">
        <div style={{ display: "flex", justifyContent: "center", alignItems: "center", height: "400px" }}>
          <div className="loading-spinner" />
        </div>
      </div>
    );
  }

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <h1>{t.dashboard.title}</h1>
        <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
          {t.dashboard.systemInfo} &middot; Real-time monitoring
        </p>
      </div>

      {/* Stats Grid */}
      <div className="grid-4" style={{ marginBottom: "24px" }}>
        <div className="stat-card">
          <div className="stat-icon" style={{ background: "rgba(99, 102, 241, 0.15)" }}>
            <Cpu size={24} color="#6366f1" />
          </div>
          <div className="stat-value">{systemInfo?.cpu_cores || 0}</div>
          <div className="stat-label">{t.dashboard.cpuCores}</div>
        </div>

        <div className="stat-card">
          <div className="stat-icon" style={{ background: "rgba(34, 197, 94, 0.15)" }}>
            <HardDrive size={24} color="#22c55e" />
          </div>
          <div className="stat-value">
            {systemInfo ? `${systemInfo.total_memory_gb.toFixed(1)}GB` : "0GB"}
          </div>
          <div className="stat-label">
            {t.dashboard.totalMemory} ({systemInfo ? `${systemInfo.free_memory_gb.toFixed(1)}GB ${t.dashboard.freeMemory}` : ""})
          </div>
        </div>

        <div className="stat-card">
          <div className="stat-icon" style={{ background: "rgba(168, 85, 247, 0.15)" }}>
            <Server size={24} color="#a855f7" />
          </div>
          <div className="stat-value">{runningServices}/{totalServices}</div>
          <div className="stat-label">{t.dashboard.servicesRunning}</div>
        </div>

        <div className="stat-card">
          <div
            className="stat-icon"
            style={{
              background:
                healthPercentage >= 80
                  ? "rgba(34, 197, 94, 0.15)"
                  : healthPercentage >= 50
                  ? "rgba(245, 158, 11, 0.15)"
                  : "rgba(239, 68, 68, 0.15)",
            }}
          >
            <Activity
              size={24}
              color={
                healthPercentage >= 80
                  ? "#22c55e"
                  : healthPercentage >= 50
                  ? "#f59e0b"
                  : "#ef4444"
              }
            />
          </div>
          <div className="stat-value">{healthPercentage}%</div>
          <div className="stat-label">{t.dashboard.systemHealth}</div>
        </div>
      </div>

      {/* Main Content Grid */}
      <div className="grid-2" style={{ marginBottom: "24px" }}>
        {/* System Info Card */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Cpu size={18} />
            {t.dashboard.systemInfo}
          </h3>
          <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
            <InfoRow label={t.dashboard.os} value={`${systemInfo?.os || "-"} ${systemInfo?.os_version || ""}`} />
            <InfoRow label={t.dashboard.architecture} value={systemInfo?.architecture || "-"} />
            <InfoRow label={t.dashboard.hostname} value={systemInfo?.hostname || "-"} />
            <InfoRow label={t.dashboard.memoryUsage} value={
              systemInfo && systemInfo.total_memory_gb > 0
                ? `${((1 - systemInfo.free_memory_gb / systemInfo.total_memory_gb) * 100).toFixed(1)}%`
                : "-"
            } />
          </div>
        </div>

        {/* Service Health Card */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Shield size={18} />
            {t.dashboard.serviceHealth}
          </h3>
          {services.length === 0 ? (
            <div className="empty-state">
              <div className="empty-state-icon">🐳</div>
              <div className="empty-state-text">{t.dashboard.noServices}</div>
              <div className="empty-state-hint">{t.dashboard.noServicesHint}</div>
            </div>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: "10px" }}>
              {services.map((service) => (
                <div
                  key={service.name}
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center",
                    padding: "14px 16px",
                    background: "var(--bg-tertiary)",
                    borderRadius: "var(--radius-md)",
                    transition: "all var(--transition-fast)",
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                    {service.healthy ? (
                      <CheckCircle2 size={18} color="#22c55e" />
                    ) : (
                      <XCircle size={18} color="#ef4444" />
                    )}
                    <span style={{ fontWeight: 500, fontSize: "14px" }}>{service.name}</span>
                  </div>
                  <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>
                    {service.port && (
                      <span className="tag">:{service.port}</span>
                    )}
                    <span className={`badge ${service.healthy ? "status-running" : "status-stopped"}`}>
                      {service.status.split(" ")[0]}
                    </span>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Quick Actions Card */}
      <div className="card card-elevated">
        <h3 className="card-title">
          <Zap size={18} />
          {t.dashboard.quickActions}
        </h3>
        <div style={{ display: "flex", gap: "12px", flexWrap: "wrap" }}>
          <button
            className="btn btn-success"
            onClick={() => handleQuickAction('start')}
            disabled={actionLoading !== null}
          >
            {actionLoading === 'start' ? <RotateCcw size={16} className="spin" /> : <Play size={16} />}
            {t.dashboard.startServices}
          </button>
          <button
            className="btn btn-danger"
            onClick={() => handleQuickAction('stop')}
            disabled={actionLoading !== null}
          >
            {actionLoading === 'stop' ? <RotateCcw size={16} className="spin" /> : <Square size={16} />}
            {t.dashboard.stopServices}
          </button>
          <button className="btn btn-secondary" onClick={() => handleQuickAction('restart')} disabled={actionLoading !== null}>
            <RotateCcw size={16} /> {t.dashboard.restartServices}
          </button>
          <div style={{ flex: 1 }} />
          <button className="btn btn-ghost" onClick={() => handleQuickAction('terminal')}>
            <Terminal size={16} /> {t.dashboard.openTerminal}
          </button>
          <button className="btn btn-ghost" onClick={() => handleQuickAction('logs')}>
            <FileText size={16} /> {t.dashboard.viewLogs}
          </button>
        </div>
      </div>
    </div>
  );
};

function InfoRow({ label, value }: { label: string; value: string }) {
  return (
    <div style={{
      display: "flex",
      justifyContent: "space-between",
      padding: "10px 0",
      borderBottom: "1px solid var(--border-subtle)",
      fontSize: "13.5px"
    }}>
      <span style={{ color: "var(--text-secondary)" }}>{label}</span>
      <span style={{ fontWeight: 500 }}>{value}</span>
    </div>
  );
}

export default Dashboard;
