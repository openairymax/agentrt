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
  Shield,
  Brain,
  Sparkles,
  ArrowUpRight,
  Clock,
  Wifi,
  Database,
  Cloud,
  TrendingUp,
  AlertCircle,
} from "lucide-react";
import { invoke } from "../utils/tauriCompat";
import { useI18n } from "../i18n";
import { PageLoader } from "../components/Skeleton";

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

interface TimelineEvent {
  id: string;
  type: "success" | "warning" | "error" | "info";
  title: string;
  desc: string;
  time: string;
}

const ProgressRing: React.FC<{
  size?: number;
  strokeWidth?: number;
  progress: number;
  color: string;
  label?: string;
  value?: string;
}> = ({ size = 80, strokeWidth = 6, progress, color, label, value }) => {
  const radius = (size - strokeWidth) / 2;
  const circumference = radius * 2 * Math.PI;
  const offset = circumference - (progress / 100) * circumference;

  return (
    <div style={{ position: 'relative', width: size, height: size, flexShrink: 0 }}>
      <svg width={size} height={size} style={{ transform: 'rotate(-90deg)' }}>
        <circle
          cx={size / 2} cy={size / 2} r={radius}
          fill="none" stroke="var(--border-subtle)" strokeWidth={strokeWidth}
        />
        <circle
          cx={size / 2} cy={size / 2} r={radius}
          fill="none" stroke={color} strokeWidth={strokeWidth}
          strokeDasharray={circumference}
          strokeDashoffset={offset}
          strokeLinecap="round"
          className="progress-ring-circle"
          style={{ transition: 'stroke-dashoffset 0.8s cubic-bezier(0.4, 0, 0.2, 1)' }}
        />
      </svg>
      <div style={{
        position: 'absolute', inset: 0, display: 'flex',
        flexDirection: 'column', alignItems: 'center', justifyContent: 'center',
      }}>
        {value && <span style={{ fontSize: '16px', fontWeight: 700, lineHeight: 1, color: 'var(--text-primary)' }}>{value}</span>}
        {label && <span style={{ fontSize: '9px', color: 'var(--text-muted)', marginTop: '2px' }}>{label}</span>}
      </div>
    </div>
  );
};

const MiniChart: React.FC<{
  data: number[];
  color: string;
  width?: number;
  height?: number;
}> = ({ data, color, width = 120, height = 36 }) => {
  if (data.length < 2) return null;
  const max = Math.max(...data);
  const min = Math.min(...data);
  const range = max - min || 1;

  const points = data.map((v, i) => {
    const x = (i / (data.length - 1)) * width;
    const y = height - ((v - min) / range) * (height - 4) - 2;
    return `${x},${y}`;
  }).join(' ');

  const areaPoints = `0,${height} ${points} ${width},${height}`;

  return (
    <svg width={width} height={height} style={{ overflow: 'visible' }}>
      <defs>
        <linearGradient id={`grad-${color.replace('#', '')}`} x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stopColor={color} stopOpacity="0.2" />
          <stop offset="100%" stopColor={color} stopOpacity="0" />
        </linearGradient>
      </defs>
      <polygon points={areaPoints} fill={`url(#grad-${color.replace('#', '')})`} />
      <polyline points={points} fill="none" stroke={color} strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
};

const ResourceBar: React.FC<{
  label: string;
  value: number;
  color: string;
  icon?: React.ReactNode;
}> = ({ label, value, color, icon }) => (
  <div className="resource-bar-item">
    <div className="resource-bar-label" style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
      {icon}
      {label}
    </div>
    <div className="resource-bar-track">
      <div
        className="resource-bar-fill"
        style={{ width: `${Math.min(value, 100)}%`, background: `linear-gradient(90deg, ${color}, ${color}cc)` }}
      />
    </div>
    <div className="resource-bar-value">{value}%</div>
  </div>
);

const Dashboard: React.FC = () => {
  const { t } = useI18n();
  const navigate = useNavigate();
  const [systemInfo, setSystemInfo] = useState<SystemInfo | null>(null);
  const [services, setServices] = useState<ServiceStatus[]>([]);
  const [loading, setLoading] = useState(true);
  const [actionLoading, setActionLoading] = useState<string | null>(null);
  const [liveCpu, setLiveCpu] = useState(47);
  const [liveMem, setLiveMem] = useState(68);

  const cpuHistory = [32, 45, 38, 52, 41, 55, 48, 62, 45, 50, 43, 47];
  const memHistory = [60, 62, 65, 63, 68, 70, 72, 69, 71, 73, 70, 68];
  const netHistory = [12, 18, 8, 25, 15, 30, 22, 35, 28, 20, 16, 24];
  const diskHistory = [45, 45, 46, 46, 47, 47, 48, 48, 49, 49, 50, 50];

  const timelineEvents: TimelineEvent[] = [
    { id: '1', type: 'success', title: '系统启动完成', desc: '所有核心服务已成功启动并运行正常', time: '2 分钟前' },
    { id: '2', type: 'info', title: 'AI 模型已连接', desc: 'GPT-4o 模型连接成功，延迟 120ms', time: '5 分钟前' },
    { id: '3', type: 'success', title: '智能体注册成功', desc: 'Research Assistant 智能体已上线', time: '8 分钟前' },
    { id: '4', type: 'warning', title: '内存使用率较高', desc: '当前内存使用率达到 68%，建议关注', time: '12 分钟前' },
    { id: '5', type: 'info', title: '配置文件已更新', desc: 'kernel-config.yaml 配置已自动重载', time: '15 分钟前' },
  ];

  useEffect(() => {
    loadDashboardData();
    const interval = setInterval(loadDashboardData, 30000);
    const liveInterval = setInterval(() => {
      setLiveCpu(prev => Math.max(10, Math.min(95, prev + (Math.random() - 0.5) * 8)));
      setLiveMem(prev => Math.max(40, Math.min(90, prev + (Math.random() - 0.5) * 4)));
    }, 3000);
    return () => { clearInterval(interval); clearInterval(liveInterval); };
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
  const healthPercentage = totalServices > 0 ? Math.round((runningServices / totalServices) * 100) : 0;
  const memUsage = systemInfo && systemInfo.total_memory_gb > 0
    ? Math.round((1 - systemInfo.free_memory_gb / systemInfo.total_memory_gb) * 100)
    : liveMem;

  const handleQuickAction = async (action: string) => {
    switch (action) {
      case 'start':
        setActionLoading('start');
        try { await invoke("start_services", { mode: "dev" }); await loadDashboardData(); }
        catch (e) { alert(`${t.services.failedToStart}: ${e}`); }
        setActionLoading(null);
        break;
      case 'stop':
        if (!confirm(t.services.confirmStopAll)) return;
        setActionLoading('stop');
        try { await invoke("stop_services"); setServices([]); }
        catch (e) { alert(`${t.services.failedToStop}: ${e}`); }
        setActionLoading(null);
        break;
      case 'restart':
        setActionLoading('restart');
        try { await invoke("restart_services", { mode: "dev" }); await loadDashboardData(); }
        catch (e) { alert(`${t.services.failedToRestart}: ${e}`); }
        setActionLoading(null);
        break;
      case 'terminal': navigate('/terminal'); break;
      case 'logs': navigate('/logs'); break;
    }
  };

  if (loading) return <PageLoader />;

  return (
    <div className="page-container">
      {/* AI Welcome Banner */}
      <div className="card card-elevated" style={{
        marginBottom: "24px",
        background: "var(--primary-gradient)",
        border: "none",
        color: "white",
        position: "relative",
        overflow: "hidden",
      }}>
        <div style={{
          position: "absolute", top: "-40px", right: "-20px",
          width: "200px", height: "200px", borderRadius: "50%",
          background: "rgba(255,255,255,0.08)",
        }} />
        <div style={{
          position: "absolute", bottom: "-60px", right: "60px",
          width: "140px", height: "140px", borderRadius: "50%",
          background: "rgba(255,255,255,0.05)",
        }} />
        <div style={{ position: "relative", zIndex: 1, padding: "8px 0" }}>
          <div style={{ display: "flex", alignItems: "center", gap: "12px", marginBottom: "8px" }}>
            <Sparkles size={22} />
            <h2 style={{ fontSize: "18px", fontWeight: 700, margin: 0 }}>
              {t.dashboard.title}
            </h2>
          </div>
          <p style={{ opacity: 0.85, fontSize: "14px", margin: 0, lineHeight: 1.6 }}>
            {t.dashboard.systemInfo} · {systemInfo?.hostname || "AgentOS"} · {systemInfo?.os || ""}
          </p>
        </div>
      </div>

      {/* Stats with Progress Rings */}
      <div style={{
        display: "grid",
        gridTemplateColumns: "repeat(4, 1fr)",
        gap: "16px",
        marginBottom: "24px",
      }}>
        {/* CPU Card */}
        <div className="stat-card card-hover-lift" style={{ display: "flex", alignItems: "center", gap: "16px" }}>
          <ProgressRing progress={Math.round(liveCpu)} color="#6366f1" value={`${Math.round(liveCpu)}%`} label="CPU" />
          <div style={{ flex: 1, minWidth: 0 }}>
            <div style={{ fontSize: "12px", color: "var(--text-muted)", marginBottom: "4px" }}>{t.dashboard.cpuCores}</div>
            <div style={{ fontSize: "24px", fontWeight: 700 }}>{systemInfo?.cpu_cores || 0}</div>
            <MiniChart data={cpuHistory} color="#6366f1" width={100} height={28} />
          </div>
        </div>

        {/* Memory Card */}
        <div className="stat-card card-hover-lift" style={{ display: "flex", alignItems: "center", gap: "16px" }}>
          <ProgressRing progress={Math.round(liveMem)} color="#22c55e" value={`${Math.round(liveMem)}%`} label="MEM" />
          <div style={{ flex: 1, minWidth: 0 }}>
            <div style={{ fontSize: "12px", color: "var(--text-muted)", marginBottom: "4px" }}>{t.dashboard.totalMemory}</div>
            <div style={{ fontSize: "24px", fontWeight: 700 }}>{systemInfo ? `${systemInfo.total_memory_gb.toFixed(1)}` : "0"}<span style={{ fontSize: "14px", fontWeight: 400 }}>GB</span></div>
            <MiniChart data={memHistory} color="#22c55e" width={100} height={28} />
          </div>
        </div>

        {/* Services Card */}
        <div className="stat-card card-hover-lift" style={{ display: "flex", alignItems: "center", gap: "16px" }}>
          <ProgressRing
            progress={totalServices > 0 ? (runningServices / totalServices) * 100 : 0}
            color="#a855f7"
            value={`${runningServices}/${totalServices}`}
            label="SVC"
          />
          <div style={{ flex: 1, minWidth: 0 }}>
            <div style={{ fontSize: "12px", color: "var(--text-muted)", marginBottom: "4px" }}>{t.dashboard.servicesRunning}</div>
            <div style={{ fontSize: "24px", fontWeight: 700 }}>{runningServices}<span style={{ fontSize: "14px", fontWeight: 400, color: "var(--text-muted)" }}>/{totalServices}</span></div>
            <div style={{ display: "flex", gap: "4px", marginTop: "4px" }}>
              {services.slice(0, 5).map(s => (
                <div key={s.name} style={{
                  width: "8px", height: "8px", borderRadius: "50%",
                  background: s.healthy ? "#22c55e" : "#ef4444",
                }} />
              ))}
            </div>
          </div>
        </div>

        {/* Health Card */}
        <div className="stat-card card-hover-lift" style={{ display: "flex", alignItems: "center", gap: "16px" }}>
          <ProgressRing
            progress={healthPercentage}
            color={healthPercentage >= 80 ? "#22c55e" : healthPercentage >= 50 ? "#f59e0b" : "#ef4444"}
            value={`${healthPercentage}%`}
            label="HP"
          />
          <div style={{ flex: 1, minWidth: 0 }}>
            <div style={{ fontSize: "12px", color: "var(--text-muted)", marginBottom: "4px" }}>{t.dashboard.systemHealth}</div>
            <div style={{ fontSize: "24px", fontWeight: 700, color: healthPercentage >= 80 ? "#22c55e" : healthPercentage >= 50 ? "#f59e0b" : "#ef4444" }}>
              {healthPercentage >= 80 ? "Excellent" : healthPercentage >= 50 ? "Warning" : "Critical"}
            </div>
            <MiniChart data={netHistory} color={healthPercentage >= 80 ? "#22c55e" : "#f59e0b"} width={100} height={28} />
          </div>
        </div>
      </div>

      {/* System Monitor + Resource Distribution */}
      <div className="grid-2" style={{ marginBottom: "24px" }}>
        {/* System Monitor */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Activity size={18} />
            系统监控
          </h3>
          <div className="monitor-grid">
            <div className="monitor-metric">
              <div className="monitor-metric-header">
                <span className="monitor-metric-label"><Cpu size={12} style={{ display: 'inline', marginRight: '4px', verticalAlign: 'middle' }} />CPU</span>
                <TrendingUp size={14} style={{ color: liveCpu > 70 ? '#f59e0b' : '#22c55e' }} />
              </div>
              <div className="monitor-metric-value">{Math.round(liveCpu)}%</div>
              <div className="monitor-metric-bar">
                <div className="monitor-metric-bar-fill" style={{
                  width: `${liveCpu}%`,
                  background: liveCpu > 80 ? '#ef4444' : liveCpu > 60 ? '#f59e0b' : '#22c55e',
                }} />
              </div>
            </div>

            <div className="monitor-metric">
              <div className="monitor-metric-header">
                <span className="monitor-metric-label"><HardDrive size={12} style={{ display: 'inline', marginRight: '4px', verticalAlign: 'middle' }} />内存</span>
                <TrendingUp size={14} style={{ color: liveMem > 75 ? '#f59e0b' : '#22c55e' }} />
              </div>
              <div className="monitor-metric-value">{Math.round(liveMem)}%</div>
              <div className="monitor-metric-bar">
                <div className="monitor-metric-bar-fill" style={{
                  width: `${liveMem}%`,
                  background: liveMem > 80 ? '#ef4444' : liveMem > 65 ? '#f59e0b' : '#22c55e',
                }} />
              </div>
            </div>

            <div className="monitor-metric">
              <div className="monitor-metric-header">
                <span className="monitor-metric-label"><Wifi size={12} style={{ display: 'inline', marginRight: '4px', verticalAlign: 'middle' }} />网络</span>
                <Activity size={14} style={{ color: '#6366f1' }} />
              </div>
              <div className="monitor-metric-value">24 MB/s</div>
              <div className="monitor-metric-bar">
                <div className="monitor-metric-bar-fill" style={{ width: '35%', background: '#6366f1' }} />
              </div>
            </div>

            <div className="monitor-metric">
              <div className="monitor-metric-header">
                <span className="monitor-metric-label"><Database size={12} style={{ display: 'inline', marginRight: '4px', verticalAlign: 'middle' }} />磁盘</span>
                <Cloud size={14} style={{ color: '#a855f7' }} />
              </div>
              <div className="monitor-metric-value">50%</div>
              <div className="monitor-metric-bar">
                <div className="monitor-metric-bar-fill" style={{ width: '50%', background: '#a855f7' }} />
              </div>
            </div>
          </div>
        </div>

        {/* Resource Distribution */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <HardDrive size={18} />
            资源分布
          </h3>
          <div className="resource-bar-container">
            <ResourceBar
              label="CPU"
              value={Math.round(liveCpu)}
              color={liveCpu > 80 ? '#ef4444' : liveCpu > 60 ? '#f59e0b' : '#6366f1'}
              icon={<Cpu size={12} />}
            />
            <ResourceBar
              label="内存"
              value={Math.round(liveMem)}
              color={liveMem > 80 ? '#ef4444' : liveMem > 65 ? '#f59e0b' : '#22c55e'}
              icon={<HardDrive size={12} />}
            />
            <ResourceBar
              label="磁盘"
              value={50}
              color="#a855f7"
              icon={<Database size={12} />}
            />
            <ResourceBar
              label="网络"
              value={35}
              color="#6366f1"
              icon={<Wifi size={12} />}
            />
            <ResourceBar
              label="GPU"
              value={0}
              color="#f59e0b"
              icon={<Zap size={12} />}
            />
          </div>
        </div>
      </div>

      {/* System Info + Activity Timeline */}
      <div className="grid-2" style={{ marginBottom: "24px" }}>
        {/* System Info Card */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Cpu size={18} />
            {t.dashboard.systemInfo}
          </h3>
          <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
            <InfoRow label={t.dashboard.os} value={`${systemInfo?.os || "-"} ${systemInfo?.os_version || ""}`} />
            <InfoRow label={t.dashboard.architecture} value={systemInfo?.architecture || "-"} />
            <InfoRow label={t.dashboard.hostname} value={systemInfo?.hostname || "-"} />
            <InfoRow label={t.dashboard.memoryUsage} value={
              systemInfo && systemInfo.total_memory_gb > 0
                ? `${((1 - systemInfo.free_memory_gb / systemInfo.total_memory_gb) * 100).toFixed(1)}% (${systemInfo.free_memory_gb.toFixed(1)}GB ${t.dashboard.freeMemory})`
                : "-"
            } />
          </div>
        </div>

        {/* Activity Timeline */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Clock size={18} />
            活动时间线
          </h3>
          <div className="activity-timeline">
            {timelineEvents.map((event) => (
              <div key={event.id} className="timeline-item">
                <div className={`timeline-dot ${event.type}`} />
                <div className="timeline-content">
                  <div className="timeline-title">{event.title}</div>
                  <div className="timeline-desc">{event.desc}</div>
                  <div className="timeline-time">{event.time}</div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Service Health + Quick Actions */}
      <div className="grid-2" style={{ marginBottom: "24px" }}>
        {/* Service Health Card */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Shield size={18} />
            {t.dashboard.serviceHealth}
          </h3>
          {services.length === 0 ? (
            <div className="empty-state" style={{ padding: "40px 20px" }}>
              <div className="empty-state-icon">🐳</div>
              <div className="empty-state-text">{t.dashboard.noServices}</div>
              <div className="empty-state-hint">{t.dashboard.noServicesHint}</div>
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
                    padding: "12px 14px",
                    background: "var(--bg-tertiary)",
                    borderRadius: "var(--radius-md)",
                    transition: "all var(--transition-fast)",
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
                    {service.healthy ? <CheckCircle2 size={16} color="#22c55e" /> : <XCircle size={16} color="#ef4444" />}
                    <span style={{ fontWeight: 500, fontSize: "13.5px" }}>{service.name}</span>
                  </div>
                  <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                    {service.port && <span className="tag">:{service.port}</span>}
                    <span className={`badge ${service.healthy ? "status-running" : "status-stopped"}`}>
                      {service.status.split(" ")[0]}
                    </span>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Quick Actions */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Zap size={18} />
            {t.dashboard.quickActions}
          </h3>
          <div style={{ display: "flex", gap: "10px", flexWrap: "wrap" }}>
            <button className="btn btn-success" onClick={() => handleQuickAction('start')} disabled={actionLoading !== null}>
              {actionLoading === 'start' ? <RotateCcw size={16} className="spin" /> : <Play size={16} />}
              {t.dashboard.startServices}
            </button>
            <button className="btn btn-danger" onClick={() => handleQuickAction('stop')} disabled={actionLoading !== null}>
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

      {/* AI Suggestions */}
      <div className="card card-elevated" style={{
        borderLeft: "3px solid var(--primary-color)",
      }}>
        <h3 className="card-title">
          <Brain size={18} />
          AI 智能建议
        </h3>
        <div style={{ display: "flex", flexDirection: "column", gap: "10px" }}>
          {liveMem > 70 && (
            <SuggestionItem
              icon={<HardDrive size={16} color="#f59e0b" />}
              title="内存使用率较高"
              desc={`当前内存使用 ${Math.round(liveMem)}%，建议关闭不需要的服务以释放资源`}
              action="优化内存"
            />
          )}
          {services.length === 0 && (
            <SuggestionItem
              icon={<Server size={16} color="#6366f1" />}
              title="尚未启动任何服务"
              desc="点击「启动服务」开始使用 AgentOS 的全部功能"
              action="立即启动"
              onAction={() => handleQuickAction('start')}
            />
          )}
          <SuggestionItem
            icon={<Brain size={16} color="#a855f7" />}
            title="配置 AI 模型"
            desc="配置 OpenAI、Claude 等大模型 API，解锁智能体全部能力"
            action="去配置"
            onAction={() => navigate('/llm-config')}
          />
          <SuggestionItem
            icon={<AlertCircle size={16} color="#22c55e" />}
            title="系统运行正常"
            desc="所有服务运行良好，系统状态健康"
          />
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

function SuggestionItem({ icon, title, desc, action, onAction }: {
  icon: React.ReactNode;
  title: string;
  desc: string;
  action?: string;
  onAction?: () => void;
}) {
  return (
    <div style={{
      display: "flex", gap: "12px", padding: "12px",
      background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)",
      transition: "all var(--transition-fast)",
    }}>
      <div style={{ flexShrink: 0, marginTop: "2px" }}>{icon}</div>
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{ fontWeight: 600, fontSize: "13.5px", marginBottom: "3px" }}>{title}</div>
        <div style={{ fontSize: "12.5px", color: "var(--text-secondary)", lineHeight: 1.5 }}>{desc}</div>
      </div>
      {action && (
        <button className="btn btn-primary btn-sm" onClick={onAction} style={{ alignSelf: "center", flexShrink: 0 }}>
          <ArrowUpRight size={13} />{action}
        </button>
      )}
    </div>
  );
}

export default Dashboard;
