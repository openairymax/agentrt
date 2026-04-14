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
import { useAlert } from "../components/useAlert";

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
  const { error, success, confirm: confirmModal } = useAlert();
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
    } catch (err) {
      error("操作失败", `无法完成请求: ${err}`);
    } finally {
      setActionLoading(null);
    }
  };

  const handleStartAll = () => doAction('start');
  const handleStopAll = async () => {
    const confirmed = await confirmModal({
      type: 'danger',
      title: '停止所有服务',
      message: t.services.confirmStopAll || '确定要停止所有正在运行的服务吗？此操作将中断所有活跃连接。',
      confirmText: '停止服务',
      cancelText: '取消',
    });
    if (confirmed) doAction('stop');
  };
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
        <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>
          <div style={{
            width: "48px", 
            height: "48px", 
            borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#3b82f6,#60a5fa)",
            display: "flex", 
            alignItems: "center", 
            justifyContent: "center",
            boxShadow: "0 4px 16px rgba(59,130,246,0.35), 0 0 0 1px rgba(255,255,255,0.08) inset",
            transition: "all 0.2s ease",
          }}>
            <Server size={24} color="white" />
          </div>
          <div>
            <h1 style={{ display: "flex", alignItems: "center", gap: "12px", margin: 0, fontSize: "20px" }}>
              {t.services.title}
              <span style={{
                padding: "4px 12px",
                borderRadius: "9999px",
                background: services.length > 0 ? "#22c55e15" : "#ef444415",
                color: services.length > 0 ? "#22c55e" : "#ef4444",
                fontSize: "12px",
                fontWeight: 500,
                border: `1px solid ${services.length > 0 ? "#22c55e20" : "#ef444420"}`,
              }}>
                {services.length} {t.services.activeServices.toLowerCase()}
              </span>
            </h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "14px", margin: "8px 0 0 0" }}>
              {t.services.subtitle}
            </p>
          </div>
        </div>
      </div>

      {/* Control Panel */}
      <div className="card card-elevated" style={{ marginBottom: "24px", boxShadow: 'var(--shadow-sm)' }}>
        <div style={{ padding: "20px" }}>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", flexWrap: "wrap", gap: "16px" }}>
            <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
              {/* Mode Toggle */}
              <div style={{
                display: "flex", 
                background: "var(--bg-tertiary)", 
                borderRadius: "var(--radius-md)",
                padding: "4px", 
                border: "1px solid var(--border-subtle)",
                transition: "all 0.2s ease",
              }}>
                {(['dev', 'prod'] as const).map((mode) => (
                  <button
                    key={mode}
                    onClick={() => setDeployMode(mode)}
                    style={{
                      padding: "8px 18px", 
                      border: "none", 
                      borderRadius: "var(--radius-sm)",
                      background: deployMode === mode ? "var(--primary-color)" : "transparent",
                      color: deployMode === mode ? "white" : "var(--text-secondary)",
                      cursor: "pointer", 
                      fontWeight: 500, 
                      fontSize: "13px",
                      transition: "all 0.2s ease", 
                      display: "flex", 
                      alignItems: "center", 
                      gap: "6px",
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
                style={{
                  transition: "all 0.2s ease",
                  borderRadius: "var(--radius-md)",
                }}
              >
                <RefreshCw size={16} className={loading ? "spin" : ""} />
                <span style={{ marginLeft: "6px" }}>{t.services.refresh}</span>
              </button>
            </div>

            <div style={{ display: "flex", gap: "12px" }}>
              <button
                className="btn btn-success"
                onClick={handleStartAll}
                disabled={actionLoading !== null}
                style={{
                  transition: "all 0.2s ease",
                  borderRadius: "var(--radius-md)",
                }}
              >
                {actionLoading === "start" ? <Loader2 size={16} className="spin" /> : <Play size={16} />}
                <span style={{ marginLeft: "6px" }}>{t.services.startAll}</span>
              </button>

              <button
                className="btn btn-danger"
                onClick={handleStopAll}
                disabled={actionLoading !== null || services.length === 0}
                style={{
                  transition: "all 0.2s ease",
                  borderRadius: "var(--radius-md)",
                }}
              >
                {actionLoading === "stop" ? <Loader2 size={16} className="spin" /> : <Square size={16} />}
                <span style={{ marginLeft: "6px" }}>{t.services.stopAll}</span>
              </button>

              <button
                className="btn btn-secondary"
                onClick={handleRestartAll}
                disabled={actionLoading !== null || services.length === 0}
                style={{
                  transition: "all 0.2s ease",
                  borderRadius: "var(--radius-md)",
                }}
              >
                {actionLoading === "restart" ? <Loader2 size={16} className="spin" /> : <RotateCcw size={16} />}
                <span style={{ marginLeft: "6px" }}>{t.services.restartAll}</span>
              </button>
            </div>
          </div>

          {/* Last Action Toast */}
          {lastAction && (
            <div style={{
              marginTop: "16px", 
              padding: "12px 16px", 
              borderRadius: "var(--radius-md)",
              background: lastAction.type === 'start' ? "rgba(34,197,94,0.08)" : lastAction.type === 'stop' ? "rgba(239,68,68,0.08)" : "rgba(245,158,11,0.08)",
              border: `1px solid ${lastAction.type === 'start' ? '#22c55e40' : lastAction.type === 'stop' ? '#ef444440' : '#f59e0b40'}`,
              display: "flex", 
              alignItems: "center", 
              gap: "10px", 
              fontSize: "13px",
              animation: "fadeIn 0.3s ease-out",
              transition: "all 0.2s ease",
            }}>
              <div style={{
                width: "24px",
                height: "24px",
                borderRadius: "6px",
                background: lastAction.type === 'start' ? "#22c55e15" : lastAction.type === 'stop' ? "#ef444415" : "#f59e0b15",
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
              }}>
                {lastAction.type === 'start' && <CheckCircle2 size={16} color="#22c55e" />}
                {lastAction.type === 'stop' && <XCircle size={16} color="#ef4444" />}
                {lastAction.type === 'restart' && <RotateCcw size={16} color="#f59e0b" />}
              </div>
              <span>
                {lastAction.type === 'start' ? t.services.startAll : lastAction.type === 'stop' ? t.services.stopAll : t.services.restartAll}
                {' '}· {lastAction.time.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit' })}
              </span>
            </div>
          )}
        </div>
      </div>

      {/* Services Grid (Card-based) */}
      <div className="card card-elevated" style={{ boxShadow: 'var(--shadow-sm)' }}>
        <div style={{ padding: "20px" }}>
          <h3 style={{ 
            display: "flex", 
            alignItems: "center", 
            gap: "12px", 
            margin: "0 0 20px 0", 
            fontSize: "16px", 
            fontWeight: 600,
            color: 'var(--text-primary)',
          }}>
            <Container size={18} />
            {t.services.activeServices}
            <span style={{ 
              marginLeft: "auto",
              padding: "4px 12px",
              borderRadius: "9999px",
              background: "var(--bg-secondary)",
              color: "var(--text-secondary)",
              fontSize: "12px",
              fontWeight: 500,
            }}>{services.length}</span>
          </h3>

          {loading ? (
            <div style={{ textAlign: "center", padding: "60px 20px" }}>
              <div className="loading-spinner" />
              <p style={{ color: "var(--text-muted)", marginTop: "16px", fontSize: "14px" }}>正在加载服务状态...</p>
            </div>
          ) : services.length === 0 ? (
            <div style={{
              textAlign: "center",
              padding: "60px 20px",
              background: "var(--bg-tertiary)",
              borderRadius: "var(--radius-lg)",
              border: "1px dashed var(--border-subtle)",
            }}>
              <div style={{ fontSize: "48px", marginBottom: "16px" }}>🐳</div>
              <div style={{ fontSize: "16px", fontWeight: 600, color: "var(--text-primary)", marginBottom: "8px" }}>{t.services.noRunningServices}</div>
              <div style={{ fontSize: "14px", color: "var(--text-muted)", marginBottom: "24px" }}>{t.services.noRunningServicesHint}</div>
              <button 
                className="btn btn-primary"
                onClick={handleStartAll}
                style={{
                  padding: "10px 24px",
                  borderRadius: "var(--radius-md)",
                  transition: "all 0.2s ease",
                }}
              >
                <Play size={16} />
                <span style={{ marginLeft: "8px" }}>{t.services.startAll}</span>
              </button>
            </div>
          ) : (
            <div style={{
              display: "grid",
              gridTemplateColumns: "repeat(auto-fill, minmax(340px, 1fr))",
              gap: "20px",
            }}>
              {services.map((service, idx) => {
                const meta = getServiceMeta(service.name);
                const IconComp = meta.icon;

                return (
                  <div
                    key={service.name}
                    style={{
                      padding: "20px",
                      borderRadius: "var(--radius-lg)",
                      border: "1px solid",
                      borderColor: service.healthy ? `${meta.color}25` : "var(--border-color)",
                      background: service.healthy ? `${meta.color}06` : "var(--bg-tertiary)",
                      position: "relative",
                      overflow: "hidden",
                      transition: "all 0.2s ease",
                      animation: `staggerFadeIn 0.35s ease-out ${idx * 60}ms both`,
                      boxShadow: 'var(--shadow-sm)',
                    }}
                    onMouseEnter={(e: React.MouseEvent<HTMLDivElement>) => {
                      e.currentTarget.style.transform = 'translateY(-2px)';
                      e.currentTarget.style.boxShadow = 'var(--shadow-md)';
                      e.currentTarget.style.borderColor = service.healthy ? meta.color : "var(--border-color)";
                    }}
                    onMouseLeave={(e: React.MouseEvent<HTMLDivElement>) => {
                      e.currentTarget.style.transform = 'translateY(0)';
                      e.currentTarget.style.boxShadow = 'var(--shadow-sm)';
                      e.currentTarget.style.borderColor = service.healthy ? `${meta.color}25` : "var(--border-color)";
                    }}
                  >
                    {/* Top accent line */}
                    <div style={{
                      position: "absolute", top: 0, left: 0, right: 0, height: "3px",
                      background: service.healthy ? meta.gradient : "var(--border-color)",
                    }} />

                    <div style={{ display: "flex", alignItems: "center", gap: "16px", marginBottom: "16px" }}>
                      <div style={{
                        width: "48px", 
                        height: "48px", 
                        borderRadius: "var(--radius-md)",
                        background: service.healthy ? meta.gradient : "var(--bg-primary)",
                        display: "flex", 
                        alignItems: "center", 
                        justifyContent: "center",
                        flexShrink: 0,
                        transition: "all 0.2s ease",
                      }}>
                        <IconComp size={24} color={service.healthy ? "white" : "var(--text-muted)"} />
                      </div>

                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{ 
                          fontWeight: 600, 
                          fontSize: "16px", 
                          whiteSpace: "nowrap", 
                          overflow: "hidden", 
                          textOverflow: "ellipsis",
                          color: 'var(--text-primary)',
                        }}>
                          {service.name}
                        </div>
                        <div style={{ display: "flex", alignItems: "center", gap: "8px", marginTop: "6px" }}>
                          {service.healthy ? (
                            <>
                              <span style={{
                                width: "8px", 
                                height: "8px", 
                                borderRadius: "50%",
                                background: "#22c55e", 
                                boxShadow: "0 0 6px rgba(34,197,94,0.5)",
                                animation: "statusPulse 2s ease-in-out infinite",
                              }} />
                              <span style={{ 
                                fontSize: "13px", 
                                color: "#22c55e", 
                                fontWeight: 500 
                              }}>{t.services.running}</span>
                            </>
                          ) : (
                            <>
                              <span style={{ 
                                width: "8px", 
                                height: "8px", 
                                borderRadius: "50%", 
                                background: "#ef4444" 
                              }} />
                              <span style={{ 
                                fontSize: "13px", 
                                color: "#ef4444", 
                                fontWeight: 500 
                              }}>{t.services.stopped}</span>
                            </>
                          )}
                        </div>
                      </div>

                      {service.port && service.healthy && (
                        <button
                          className="btn btn-ghost btn-sm"
                          onClick={() => openServiceUrl(service.port!)}
                          title={`http://localhost:${service.port}`}
                          style={{ 
                            flexShrink: 0,
                            transition: "all 0.2s ease",
                            borderRadius: "var(--radius-md)",
                          }}
                        >
                          <ExternalLink size={16} />
                          <span style={{ marginLeft: "6px" }}>{t.services.open}</span>
                        </button>
                      )}
                    </div>

                    {/* Service details row */}
                    <div style={{ 
                      display: "flex", 
                      justifyContent: "space-between", 
                      alignItems: "center", 
                      paddingTop: "16px", 
                      borderTop: "1px solid var(--border-subtle)",
                    }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>
                        {service.port && (
                          <span style={{
                            padding: "4px 12px",
                            borderRadius: "9999px",
                            background: "var(--bg-secondary)",
                            color: "var(--text-secondary)",
                            fontSize: "12px",
                            fontWeight: 500,
                          }}>:{service.port}</span>
                        )}
                        <span style={{ 
                          fontSize: "12px", 
                          color: "var(--text-muted)",
                          fontWeight: 500,
                        }}>
                          {deployMode === 'dev' ? t.services.devMode : t.services.prodMode}
                        </span>
                      </div>
                      <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                        {[...Array(3)].map((_, i) => (
                          <div key={i} style={{
                            width: "4px", 
                            height: "4px", 
                            borderRadius: "50%",
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
      </div>

      {/* Mode Info Cards - Enhanced */}
      <div style={{ 
        display: "grid", 
        gridTemplateColumns: "1fr 1fr", 
        gap: "20px", 
        marginTop: "24px",
      }}>
        <div className="card card-elevated" style={{
          borderLeft: "3px solid #6366f1",
          boxShadow: 'var(--shadow-sm)',
        }}>
          <div style={{ padding: "20px" }}>
            <h3 style={{ 
              display: "flex", 
              alignItems: "center", 
              gap: "12px", 
              margin: "0 0 16px 0", 
              fontSize: "16px", 
              fontWeight: 600,
              color: 'var(--text-primary)',
            }}>
              <Settings2 size={18} />
              {t.services.devModeInfo}
            </h3>
            <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
              {[
                { name: t.services.devGatewayApi, url: "http://localhost:18789", port: "18789" },
                { name: t.services.devKernelIpc, url: "http://localhost:18080", port: "18080" },
                { name: t.services.devOpenLabUi, url: "http://localhost:3000", port: "3000" },
                { name: t.services.devGrafana, url: "http://localhost:3001", port: "3001" },
              ].map((svc) => (
                <div 
                  key={svc.port} 
                  style={{
                    display: "flex", 
                    justifyContent: "space-between", 
                    alignItems: "center",
                    padding: "12px 16px", 
                    background: "var(--bg-secondary)", 
                    borderRadius: "var(--radius-md)",
                    cursor: "pointer", 
                    transition: "all 0.2s ease",
                    border: "1px solid transparent",
                    boxShadow: 'var(--shadow-sm)',
                  }}
                  onMouseEnter={(e: React.MouseEvent<HTMLDivElement>) => {
                    e.currentTarget.style.background = "var(--primary-light)";
                    e.currentTarget.style.borderColor = "var(--primary-color)";
                    e.currentTarget.style.transform = 'translateY(-2px)';
                    e.currentTarget.style.boxShadow = 'var(--shadow-md)';
                  }}
                  onMouseLeave={(e: React.MouseEvent<HTMLDivElement>) => {
                    e.currentTarget.style.background = "var(--bg-secondary)";
                    e.currentTarget.style.borderColor = "transparent";
                    e.currentTarget.style.transform = 'translateY(0)';
                    e.currentTarget.style.boxShadow = 'var(--shadow-sm)';
                  }}
                  onClick={() => sdk.openBrowser(svc.url)}
                >
                  <div>
                    <span style={{ fontSize: "14px", fontWeight: 500, color: 'var(--text-primary)' }}>{svc.name}</span>
                    <code style={{
                      background: "transparent", 
                      color: "var(--primary-color)",
                      fontSize: "12px", 
                      fontFamily: "'JetBrains Mono', monospace", 
                      marginLeft: "8px",
                    }}>:{svc.port}</code>
                  </div>
                  <ArrowUpRight size={16} style={{ color: "var(--text-muted)" }} />
                </div>
              ))}
            </div>
          </div>
        </div>

        <div className="card card-elevated" style={{
          borderLeft: "3px solid #22c55e",
          boxShadow: 'var(--shadow-sm)',
        }}>
          <div style={{ padding: "20px" }}>
            <h3 style={{ 
              display: "flex", 
              alignItems: "center", 
              gap: "12px", 
              margin: "0 0 16px 0", 
              fontSize: "16px", 
              fontWeight: 600,
              color: 'var(--text-primary)',
            }}>
              <Server size={18} />
              {t.services.prodModeInfo}
            </h3>
            <ul style={{ 
              color: "var(--text-secondary)", 
              lineHeight: "2.1", 
              paddingLeft: "20px", 
              margin: "0 0 16px 0", 
              fontSize: "14px",
            }}>
              <li><strong style={{ color: 'var(--text-primary)' }}>Gateway</strong>: <code style={{
                background: "var(--bg-tertiary)", 
                padding: "2px 8px", 
                borderRadius: "4px",
                fontSize: "12.5px", 
                fontFamily: "'JetBrains Mono', monospace",
                color: 'var(--text-primary)',
              }}>:18789</code> ({t.services.prodHttpsProxy})</li>
              <li><CheckCircle2 size={14} style={{ display: "inline", verticalAlign: "middle", marginRight: "6px", color: "#22c55e" }} />{t.services.prodAutoScaling}</li>
              <li><CheckCircle2 size={14} style={{ display: "inline", verticalAlign: "middle", marginRight: "6px", color: "#22c55e" }} />{t.services.prodHealthChecks}</li>
              <li><CheckCircle2 size={14} style={{ display: "inline", verticalAlign: "middle", marginRight: "6px", color: "#22c55e" }} />{t.services.prodMonitoring}</li>
            </ul>
            <div style={{
              padding: "14px 18px",
              borderRadius: "var(--radius-md)",
              background: "rgba(34,197,94,0.06)",
              border: "1px solid rgba(34,197,94,0.15)",
              display: "flex", 
              alignItems: "center", 
              gap: "10px", 
              fontSize: "14px", 
              color: "#22c55e",
              transition: "all 0.2s ease",
            }}>
              <Shield size={16} />
              生产模式已启用安全加固和自动扩缩容功能
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Services;
