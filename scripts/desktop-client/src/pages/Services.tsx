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
  Zap,
  Globe,
  Database,
  Shield,
  Activity,
  ArrowUpRight,
  Cpu,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import type { ServiceStatus } from "../services/agentos-sdk";
import { useI18n } from "../i18n";

const serviceIcons: Record<string, { icon: typeof Server; color: string; gradient: string }> = {
  kernel: { icon: Cpu, color: "#6366f1", gradient: "linear-gradient(135deg, #6366f1, #818cf8)" },
  gateway: { icon: Globe, color: "#22c55e", gradient: "linear-gradient(135deg, #22c55e, #4ade80)" },
  postgres: { icon: Database, color: "#3b82f6", gradient: "linear-gradient(135deg, #3b82f6, #60a5fa)" },
  redis: { icon: Database, color: "#ef4444", gradient: "linear-gradient(135deg, #ef4444, #f87171)" },
  openlab: { icon: Activity, color: "#a855f7", gradient: "linear-gradient(135deg, #a855f7, #c084fc)" },
  grafana: { icon: Shield, color: "#f59e0b", gradient: "linear-gradient(135deg, #f59e0b, #fbbf24)" },
};

const Services: React.FC = () => {
  const { t } = useI18n();
  const [services, setServices] = useState<ServiceStatus[]>([]);
  const [loading, setLoading] = useState(true);
  const [actionLoading, setActionLoading] = useState<string | null>(null);
  const [deployMode, setDeployMode] = useState<"dev" | "prod">("dev");
  const [lastAction, setLastAction] = useState<{ type: string; time: Date } | null>(null);

  useEffect(() => {
    loadServiceStatus();
  }, []);

  const loadServiceStatus = async () => {
    setLoading(true);
    try {
      const status = await sdk.getServiceStatus();
      setServices(status);
    } catch (error) {
      console.error("Failed to load service status:", error);
    } finally {
      setLoading(false);
    }
  };

  const doAction = async (action: string) => {
    setActionLoading(action);
    try {
      if (action === 'start') await sdk.startServices(deployMode);
      else if (action === 'stop') await sdk.stopServices();
      else if (action === 'restart') await sdk.restartServices(deployMode);
      await loadServiceStatus();
      setLastAction({ type: action, time: new Date() });
    } catch (error) {
      alert(`${t.services.failedToStart || `Failed to start: ${error}`}`);
    } finally {
      setActionLoading(null);
    }
  };

  const handleStartAll = () => doAction('start');
  const handleStopAll = () => { if (confirm(t.services.confirmStopAll)) doAction('stop'); };
  const handleRestartAll = () => doAction('restart');

  const openServiceUrl = (port: number) => {
    sdk.openBrowser(`http://localhost:${port}`);
  };

  const getServiceMeta = (name: string) => {
    const lower = name.toLowerCase();
    for (const [key, val] of Object.entries(serviceIcons)) {
      if (lower.includes(key)) return val;
    }
    return { icon: Server, color: "#94a3b8", gradient: "linear-gradient(135deg, #94a3b8, #cbd5e1)" };
  };

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
          <h1>{t.services.title}</h1>
          <span className={`badge ${services.length > 0 ? 'status-running' : 'status-stopped'}`}>
            {services.length} {t.services.activeServices.toLowerCase()}
          </span>
        </div>
        <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
          {t.services.subtitle}
        </p>
      </div>

      {/* Control Panel */}
      <div className="card card-elevated" style={{ marginBottom: "24px" }}>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", flexWrap: "wrap", gap: "16px" }}>
          <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
            {/* Mode Toggle */}
            <div style={{
              display: "flex", background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)",
              padding: "3px", border: "1px solid var(--border-subtle)",
            }}>
              {(['dev', 'prod'] as const).map((mode) => (
                <button
                  key={mode}
                  onClick={() => setDeployMode(mode)}
                  style={{
                    padding: "7px 16px", border: "none", borderRadius: "var(--radius-sm)",
                    background: deployMode === mode ? "var(--primary-color)" : "transparent",
                    color: deployMode === mode ? "white" : "var(--text-secondary)",
                    cursor: "pointer", fontWeight: 500, fontSize: "13px",
                    transition: "all var(--transition-fast)", display: "flex", alignItems: "center", gap: "5px",
                  }}
                >
                  {mode === 'dev' ? <Zap size={14} /> : <Shield size={14} />}
                  {mode === 'dev' ? t.services.devMode : t.services.prodMode}
                </button>
              ))}
            </div>

            <button
              className="btn btn-secondary"
              onClick={loadServiceStatus}
              disabled={loading}
            >
              <RefreshCw size={16} className={loading ? "spin" : ""} />
              {t.services.refresh}
            </button>
          </div>

          <div style={{ display: "flex", gap: "10px" }}>
            <button
              className="btn btn-success"
              onClick={handleStartAll}
              disabled={actionLoading !== null}
            >
              {actionLoading === "start" ? <Loader2 size={16} className="spin" /> : <Play size={16} />}
              {t.services.startAll}
            </button>

            <button
              className="btn btn-danger"
              onClick={handleStopAll}
              disabled={actionLoading !== null || services.length === 0}
            >
              {actionLoading === "stop" ? <Loader2 size={16} className="spin" /> : <Square size={16} />}
              {t.services.stopAll}
            </button>

            <button
              className="btn btn-secondary"
              onClick={handleRestartAll}
              disabled={actionLoading !== null || services.length === 0}
            >
              {actionLoading === "restart" ? <Loader2 size={16} className="spin" /> : <RotateCcw size={16} />}
              {t.services.restartAll}
            </button>
          </div>
        </div>

        {/* Last Action Toast */}
        {lastAction && (
          <div style={{
            marginTop: "12px", padding: "10px 14px", borderRadius: "var(--radius-md)",
            background: lastAction.type === 'start' ? "rgba(34,197,94,0.08)" : lastAction.type === 'stop' ? "rgba(239,68,68,0.08)" : "rgba(245,158,11,0.08)",
            border: `1px solid ${lastAction.type === 'start' ? '#22c55e40' : lastAction.type === 'stop' ? '#ef444440' : '#f59e0b40'}`,
            display: "flex", alignItems: "center", gap: "8px", fontSize: "13px",
            animation: "fadeIn 0.3s ease-out",
          }}>
            {lastAction.type === 'start' && <CheckCircle2 size={14} color="#22c55e" />}
            {lastAction.type === 'stop' && <XCircle size={14} color="#ef4444" />}
            {lastAction.type === 'restart' && <RotateCcw size={14} color="#f59e0b" />}
            <span>
              {lastAction.type === 'start' ? t.services.startAll : lastAction.type === 'stop' ? t.services.stopAll : t.services.restartAll}
              {' '}· {lastAction.time.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit' })}
            </span>
          </div>
        )}
      </div>

      {/* Services Grid (Card-based) */}
      <div className="card card-elevated">
        <h3 className="card-title">
          <Container size={18} />
          {t.services.activeServices}
          <span className="tag" style={{ marginLeft: "auto" }}>{services.length}</span>
        </h3>

        {loading ? (
          <div style={{ textAlign: "center", padding: "48px" }}>
            <div className="loading-spinner" />
          </div>
        ) : services.length === 0 ? (
          <div className="empty-state">
            <div className="empty-state-icon">🐳</div>
            <div className="empty-state-text">{t.services.noRunningServices}</div>
            <div className="empty-state-hint">{t.services.noRunningServicesHint}</div>
            <button className="btn btn-primary mt-8" onClick={handleStartAll}>
              <Play size={16} /> {t.services.startAll}
            </button>
          </div>
        ) : (
          <div style={{
            display: "grid",
            gridTemplateColumns: "repeat(auto-fill, minmax(320px, 1fr))",
            gap: "14px",
          }}>
            {services.map((service, idx) => {
              const meta = getServiceMeta(service.name);
              const IconComp = meta.icon;

              return (
                <div
                  key={service.name}
                  className="card-hover-lift"
                  style={{
                    padding: "18px",
                    borderRadius: "var(--radius-lg)",
                    border: "1px solid",
                    borderColor: service.healthy ? `${meta.color}25` : "var(--border-color)",
                    background: service.healthy ? `${meta.color}06` : "var(--bg-tertiary)",
                    position: "relative",
                    overflow: "hidden",
                    animation: `staggerFadeIn 0.35s ease-out ${idx * 60}ms both`,
                  }}
                >
                  {/* Top accent line */}
                  <div style={{
                    position: "absolute", top: 0, left: 0, right: 0, height: "3px",
                    background: service.healthy ? meta.gradient : "var(--border-color)",
                  }} />

                  <div style={{ display: "flex", alignItems: "center", gap: "14px", marginBottom: "14px" }}>
                    <div style={{
                      width: "42px", height: "42px", borderRadius: "var(--radius-md)",
                      background: service.healthy ? meta.gradient : "var(--bg-primary)",
                      display: "flex", alignItems: "center", justifyContent: "center",
                      flexShrink: 0,
                    }}>
                      <IconComp size={20} color={service.healthy ? "white" : "var(--text-muted)"} />
                    </div>

                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{ fontWeight: 600, fontSize: "15px", whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
                        {service.name}
                      </div>
                      <div style={{ display: "flex", alignItems: "center", gap: "8px", marginTop: "4px" }}>
                        {service.healthy ? (
                          <>
                            <span className="status-dot-running" style={{
                              width: "8px", height: "8px", borderRadius: "50%",
                              background: "#22c55e", boxShadow: "0 0 6px rgba(34,197,94,0.5)",
                              animation: "statusPulse 2s ease-in-out infinite",
                            }} />
                            <span style={{ fontSize: "12.5px", color: "#22c55e", fontWeight: 500 }}>{t.services.running}</span>
                          </>
                        ) : (
                          <>
                            <span style={{ width: "8px", height: "8px", borderRadius: "50%", background: "#ef4444" }} />
                            <span style={{ fontSize: "12.5px", color: "#ef4444", fontWeight: 500 }}>{t.services.stopped}</span>
                          </>
                        )}
                      </div>
                    </div>

                    {service.port && service.healthy && (
                      <button
                        className="btn btn-ghost btn-sm"
                        onClick={() => openServiceUrl(service.port!)}
                        title={`http://localhost:${service.port}`}
                        style={{ flexShrink: 0 }}
                      >
                        <ExternalLink size={14} />
                        <span style={{ marginLeft: "4px" }}>{t.services.open}</span>
                      </button>
                    )}
                  </div>

                  {/* Service details row */}
                  <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", paddingTop: "12px", borderTop: "1px solid var(--border-subtle)" }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                      {service.port && (
                        <span className="tag">:{service.port}</span>
                      )}
                      <span style={{ fontSize: "11.5px", color: "var(--text-muted)" }}>
                        {deployMode === 'dev' ? t.services.devMode : t.services.prodMode}
                      </span>
                    </div>
                    <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                      {[...Array(3)].map((_, i) => (
                        <div key={i} style={{
                          width: "4px", height: "4px", borderRadius: "50%",
                          background: i === 0 ? meta.color : "var(--border-color)",
                          opacity: 1 - i * 0.3,
                        }} />
                      ))}
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* Mode Info Cards - Enhanced */}
      <div className="grid-2" style={{ marginTop: "24px" }}>
        <div className="card card-elevated" style={{
          borderLeft: "3px solid #6366f1",
        }}>
          <h3 className="card-title">
            <Settings2 size={18} />
            {t.services.devModeInfo}
          </h3>
          <div style={{ display: "flex", flexDirection: "column", gap: "10px" }}>
            {[
              { name: t.services.devGatewayApi, url: "http://localhost:18789", port: "18789" },
              { name: t.services.devKernelIpc, url: "http://localhost:18080", port: "18080" },
              { name: t.services.devOpenLabUi, url: "http://localhost:3000", port: "3000" },
              { name: t.services.devGrafana, url: "http://localhost:3001", port: "3001" },
            ].map((svc) => (
              <div key={svc.port} style={{
                display: "flex", justifyContent: "space-between", alignItems: "center",
                padding: "10px 14px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)",
                cursor: "pointer", transition: "all var(--transition-fast)",
              }} onMouseEnter={(e) => { e.currentTarget.style.background = "var(--primary-light)"; e.currentTarget.style.borderColor = "var(--primary-color)"; }}
              onMouseLeave={(e) => { e.currentTarget.style.background = "var(--bg-tertiary)"; e.currentTarget.style.borderColor = ""; }}
              onClick={() => sdk.openBrowser(svc.url)}
              >
                <div>
                  <span style={{ fontSize: "13.5px", fontWeight: 500 }}>{svc.name}</span>
                  <code style={{
                    background: "transparent", color: "var(--primary-color)",
                    fontSize: "12px", fontFamily: "'JetBrains Mono', monospace", marginLeft: "8px",
                  }}>:{svc.port}</code>
                </div>
                <ArrowUpRight size={14} style={{ color: "var(--text-muted)" }} />
              </div>
            ))}
          </div>
        </div>

        <div className="card card-elevated" style={{
          borderLeft: "3px solid #22c55e",
        }}>
          <h3 className="card-title">
            <Server size={18} />
            {t.services.prodModeInfo}
          </h3>
          <ul style={{ color: "var(--text-secondary)", lineHeight: "2.1", paddingLeft: "20px", margin: 0, fontSize: "13.5px" }}>
            <li><strong>Gateway</strong>: <code style={{
              background: "var(--bg-tertiary)", padding: "2px 8px", borderRadius: "4px",
              fontSize: "12.5px", fontFamily: "'JetBrains Mono', monospace",
            }}>:18789</code> ({t.services.prodHttpsProxy})</li>
            <li><CheckCircle2 size={13} style={{ display: "inline", verticalAlign: "middle", marginRight: "4px", color: "#22c55e" }} />{t.services.prodAutoScaling}</li>
            <li><CheckCircle2 size={13} style={{ display: "inline", verticalAlign: "middle", marginRight: "4px", color: "#22c55e" }} />{t.services.prodHealthChecks}</li>
            <li><CheckCircle2 size={13} style={{ display: "inline", verticalAlign: "middle", marginRight: "4px", color: "#22c55e" }} />{t.services.prodMonitoring}</li>
          </ul>
          <div style={{
            marginTop: "16px",
            padding: "12px 16px",
            borderRadius: "var(--radius-md)",
            background: "rgba(34,197,94,0.06)",
            border: "1px solid rgba(34,197,94,0.15)",
            display: "flex", alignItems: "center", gap: "8px", fontSize: "13px", color: "#22c55e",
          }}>
            <Shield size={15} />
            生产模式已启用安全加固和自动扩缩容功能
          </div>
        </div>
      </div>
    </div>
  );
};

export default Services;
