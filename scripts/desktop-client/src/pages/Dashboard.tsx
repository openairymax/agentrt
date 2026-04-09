import React, { useState, useEffect } from "react";
import {
  Server,
  Cpu,
  HardDrive,
  Activity,
  Users,
  CheckCircle2,
  XCircle,
  AlertTriangle,
  TrendingUp,
  Clock,
} from "lucide-react";
import { invoke } from "@tauri-apps/api/core";

interface SystemInfo {
  os: string;
  os_version: string;
  architecture: string;
  cpu_cores: usize;
  total_memory_gb: f64;
  free_memory_gb: f64;
  hostname: string;
}

interface ServiceStatus {
  name: string;
  status: string;
  healthy: boolean;
  port?: number;
}

const Dashboard: React.FC = () => {
  const [systemInfo, setSystemInfo] = useState<SystemInfo | null>(null);
  const [services, setServices] = useState<ServiceStatus[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    loadDashboardData();
    const interval = setInterval(loadDashboardData, 30000); // Auto-refresh every 30s
    return () => clearInterval(interval);
  }, []);

  const loadDashboardData = async () => {
    try {
      const [sysInfo, svcStatus] = await Promise.all([
        invoke<SystemInfo>("get_system_info"),
        invoke<ServiceStatus[]>("get_service_status").catch(() => []),
      ]);
      setSystemInfo(sysInfo);
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

  if (loading) {
    return (
      <div style={{ display: "flex", justifyContent: "center", alignItems: "center", height: "400px" }}>
        <div className="loading-spinner" />
      </div>
    );
  }

  return (
    <div>
      <div className="grid-4" style={{ marginBottom: "24px" }}>
        {/* CPU Cores */}
        <div className="stat-card">
          <div className="stat-icon" style={{ background: "rgba(59, 130, 246, 0.15)" }}>
            <Cpu size={24} color="#3b82f6" />
          </div>
          <div className="stat-value">{systemInfo?.cpu_cores || 0}</div>
          <div className="stat-label">CPU Cores</div>
        </div>

        {/* Memory */}
        <div className="stat-card">
          <div className="stat-icon" style={{ background: "rgba(16, 185, 129, 0.15)" }}>
            <HardDrive size={24} color="#10b981" />
          </div>
          <div className="stat-value">
            {systemInfo ? `${systemInfo.total_memory_gb.toFixed(1)}GB` : "0GB"}
          </div>
          <div className="stat-label">
            Total Memory ({systemInfo ? `${systemInfo.free_memory_gb.toFixed(1)}GB free` : ""})
          </div>
        </div>

        {/* Services */}
        <div className="stat-card">
          <div className="stat-icon" style={{ background: "rgba(139, 92, 246, 0.15)" }}>
            <Server size={24} color="#8b5cf6" />
          </div>
          <div className="stat-value">{runningServices}/{totalServices}</div>
          <div className="stat-label">Services Running</div>
        </div>

        {/* Health */}
        <div className="stat-card">
          <div
            className="stat-icon"
            style={{
              background:
                healthPercentage >= 80
                  ? "rgba(16, 185, 129, 0.15)"
                  : healthPercentage >= 50
                  ? "rgba(245, 158, 11, 0.15)"
                  : "rgba(239, 68, 68, 0.15)",
            }}
          >
            <Activity
              size={24}
              color={
                healthPercentage >= 80
                  ? "#10b981"
                  : healthPercentage >= 50
                  ? "#f59e0b"
                  : "#ef4444"
              }
            />
          </div>
          <div className="stat-value">{healthPercentage}%</div>
          <div className="stat-label">System Health</div>
        </div>
      </div>

      <div className="grid-2">
        {/* System Information */}
        <div className="card">
          <h3 className="card-title">
            <Cpu size={20} />
            System Information
          </h3>
          <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
            <InfoRow label="Operating System" value={`${systemInfo?.os || "Unknown"} ${systemInfo?.os_version || ""}`} />
            <InfoRow label="Architecture" value={systemInfo?.architecture || "Unknown"} />
            <InfoRow label="Hostname" value={systemInfo?.hostname || "Unknown"} />
            <InfoRow label="Memory Usage" value={
              systemInfo
                ? `${((1 - systemInfo.free_memory_gb / systemInfo.total_memory_gb) * 100).toFixed(1)}%`
                : "N/A"
            } />
          </div>
        </div>

        {/* Service Status */}
        <div className="card">
          <h3 className="card-title">
            <Server size={20} />
            Service Status
          </h3>
          {services.length === 0 ? (
            <div className="empty-state">
              <div className="empty-state-icon">🐳</div>
              <div className="empty-state-text">No services detected</div>
              <div className="empty-state-hint">
                Start services from the Services page or use Deploy button
              </div>
            </div>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
              {services.map((service) => (
                <div
                  key={service.name}
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center",
                    padding: "12px",
                    background: "var(--bg-tertiary)",
                    borderRadius: "8px",
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                    {service.healthy ? (
                      <CheckCircle2 size={18} color="#10b981" />
                    ) : (
                      <XCircle size={18} color="#ef4444" />
                    )}
                    <span style={{ fontWeight: 500 }}>{service.name}</span>
                  </div>
                  <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>
                    {service.port && (
                      <span style={{ fontSize: "13px", color: "var(--text-muted)" }}>
                        :{service.port}
                      </span>
                    )}
                    <span
                      className={`status-badge ${
                        service.healthy ? "status-running" : "status-stopped"
                      }`}
                    >
                      {service.status.split(" ")[0]}
                    </span>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Quick Actions */}
      <div className="card">
        <h3 className="card-title">
          <TrendingUp size={20} />
          Quick Actions
        </h3>
        <div style={{ display: "flex", gap: "12px", flexWrap: "wrap" }}>
          <button className="btn btn-success" onClick={() => window.location.href = "/services"}>
            ▶ Start All Services
          </button>
          <button className="btn btn-danger" onClick={() => window.location.href = "/services">
            ⏹ Stop All Services
          </button>
          <button className="btn btn-secondary" onClick={() => window.location.href = "/terminal"}>
            <Terminal size={16} /> Open Terminal
          </button>
          <button className="btn btn-secondary" onClick={() => window.location.href = "/logs"}>
            <FileText size={16} /> View Logs
          </button>
        </div>
      </div>
    </div>
  );
};

function InfoRow({ label, value }: { label: string; value: string }) {
  return (
    <div
      style={{
        display: "flex",
        justifyContent: "space-between",
        padding: "8px 0",
        borderBottom: "1px solid var(--border-color)",
      }}
    >
      <span style={{ color: "var(--text-secondary)", fontSize: "14px" }}>{label}</span>
      <span style={{ fontWeight: 500, fontSize: "14px" }}>{value}</span>
    </div>
  );
}

export default Dashboard;
