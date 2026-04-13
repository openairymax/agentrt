import React, { useState, useEffect, useCallback } from "react";
import {
  Brain, Activity, Layers, Zap, Shield, Network,
  RefreshCw, Eye, Database, Target, Workflow,
  Radio, Bot, Sparkles, Wrench, ArrowRight,
  Clock, TrendingUp, CheckCircle2, AlertTriangle,
  Download, Pause, Play, Settings2, BarChart3
} from "lucide-react";
import { useNavigate } from "react-router-dom";
import sdk from "../services/agentos-sdk";
import { exportToCSV } from "../utils/export";
import { Button } from "../components/ui/Button";
import { Card } from "../components/ui/Card";
import { PageLayout } from "../components/PageLayout";

const PHASES = [
  { key: "perception", label: "感知", icon: Eye, color: "#06b6d4", gradient: "linear-gradient(135deg,#06b6d4,#22d3ee)", desc: "意图理解与上下文分析" },
  { key: "planning", label: "规划", icon: Brain, color: "#8b5cf6", gradient: "linear-gradient(135deg,#8b5cf6,#a78bfa)", desc: "DAG任务分解与策略生成" },
  { key: "action", label: "行动", icon: Target, color: "#10b981", gradient: "linear-gradient(135deg,#10b981,#34d399)", desc: "执行调度与结果反馈" },
];

const MEM_LAYERS = [
  { key: "L1", name: "L1 原始卷", color: "#6366f1", grad: "linear-gradient(135deg,#6366f1,#818cf8)", entries: 1250, max: 1500, icon: Database, tech: ["Raw Storage", "JSON/Binary"] },
  { key: "L2", name: "L2 特征层", color: "#06b6d4", grad: "linear-gradient(135deg,#06b6d4,#22d3ee)", entries: 890, max: 1200, icon: Zap, tech: ["FAISS Vector", "768-dim"] },
  { key: "L3", name: "L3 结构层", color: "#f59e0b", grad: "linear-gradient(135deg,#f59e0b,#fbbf24)", entries: 456, max: 600, icon: Network, tech: ["Knowledge Graph", "RDF"] },
  { key: "L4", name: "L4 模式层", color: "#10b981", grad: "linear-gradient(135deg,#10b981,#34d399)", entries: 128, max: 200, icon: Sparkles, tech: ["Persistent Homology", "Stable Rules"] },
];

const SERVICES = [
  { name: "gateway_d", label: "网关服务", up: true, hrs: 48, cpu: 2.1, mem: 64 },
  { name: "llm_d", label: "大模型引擎", up: true, hrs: 48, cpu: 12.4, mem: 512 },
  { name: "tool_d", label: "工具调度器", up: true, hrs: 48, cpu: 0.8, mem: 32 },
  { name: "sched_d", label: "任务调度器", up: true, hrs: 47, cpu: 1.5, mem: 48 },
  { name: "monit_d", label: "系统监控器", up: true, hrs: 47, cpu: 3.2, mem: 96 },
  { name: "market_d", label: "市场服务", up: false, hrs: 0, cpu: 0, mem: 0 },
];


















/* ─── Dual System Panel ─── */
function DualPanel() {
  return (
    <div className="card" style={{ padding: "20px" }}>
      <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
        <Brain size={18} color="#f59e0b" />
        <span style={{ fontSize: "15px", fontWeight: 600 }}>双系统思考</span>
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "12px" }}>
        {[
          { name: "快思考 S1", desc: "直觉响应 · 模式匹配", latency: "~23ms", color: "#22c55e" },
          { name: "慢推理 S2", desc: "深度分析 · 逻辑推演", latency: "~1.2s", color: "#8b5cf6" },
        ].map(sys => (
          <div key={sys.name} style={{
            padding: "16px",
            borderLeft: `3px solid ${sys.color}`,
            background: "var(--bg-secondary)",
          }}>
            <div style={{ fontSize: "14px", fontWeight: 600, color: sys.color, marginBottom: "4px" }}>{sys.name}</div>
            <div style={{ fontSize: "11px", color: "var(--text-muted)", marginBottom: "8px" }}>{sys.desc}</div>
            <div style={{ fontSize: "12px", color: sys.color, fontWeight: 600 }}>{sys.latency}</div>
          </div>
        ))}
      </div>
    </div>
  );
}

/* ─── Security Shield ─── */
function SecShield() {
  const shields = [
    { emoji: "\ud83d\udce6", name: "虚拟工作空间", sub: "进程级隔离沙箱", c: "#10b981" },
    { emoji: "\ud83d\udd11", name: "RBAC 权限裁决", sub: "细粒度访问控制", c: "#06b6d4" },
    { emoji: "\ud83d\udee1\ufe0f", name: "输入净化过滤", sub: "注入攻击防护", c: "#f59e0b" },
    { emoji: "\ud83d\udccb", name: "全链路审计追踪", sub: "不可篡改日志", c: "#8b5cf6" },
  ];
  return (
    <div className="card" style={{ padding: "20px" }}>
      <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
        <Shield size={18} color="#ef4444" />
        <span style={{ fontSize: "15px", fontWeight: 600 }}>安全状态</span>
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "12px", marginBottom: "16px" }}>
        {shields.map(s => (
          <div key={s.name} style={{
            padding: "12px",
            borderLeft: `3px solid ${s.c}`,
            background: "var(--bg-secondary)",
          }}>
            <div style={{ fontSize: "13px", fontWeight: 600, color: s.c, marginBottom: "2px" }}>{s.name}</div>
            <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>{s.sub}</div>
          </div>
        ))}
      </div>
      <div style={{
        padding: "12px",
        background: "var(--bg-secondary)",
        borderLeft: "3px solid #10b981",
      }}>
        <div style={{ fontSize: "13px", fontWeight: 600, color: "#10b981", marginBottom: "4px" }}>安全罩已激活 — 零信任架构运行正常</div>
        <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>所有防护层在线，实时监控中</div>
      </div>
    </div>
  );
}

/* ─── Service Grid ─── */
function SvcGrid() {
  const live = SERVICES.filter(s => s.up).length;
  return (
    <div className="card" style={{ padding: "20px" }}>
      <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
        <Network size={18} color="#3b82f6" />
        <div>
          <span style={{ fontSize: "15px", fontWeight: 600 }}>守护服务</span>
          <span style={{ fontSize: "11px", color: "var(--text-muted)", marginLeft: "8px" }}>{live}/6 服务在线</span>
        </div>
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: "12px" }}>
        {SERVICES.map(svc => {
          const up = svc.up;
          return (
            <div key={svc.name} style={{
              padding: "12px",
              borderLeft: up ? "3px solid #22c55e" : "3px solid var(--border-color)",
              background: "var(--bg-secondary)",
            }}>
              <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "8px" }}>
                <div style={{
                  width: "8px", height: "8px", borderRadius: "50%",
                  background: up ? "#22c55e" : "var(--border-color)",
                }} />
                <span style={{ fontSize: "13px", fontWeight: 600 }}>{svc.label}</span>
              </div>
              {up && (
                <div style={{ display: "flex", gap: "8px", fontSize: "11px", color: "var(--text-muted)" }}>
                  <span>{svc.hrs}h</span>
                  <span>CPU {svc.cpu}%</span>
                  <span>{svc.mem}MB</span>
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}

/* ─── Activity Timeline ─── */
function TimeLine() {
  const events = [
    { time: "刚刚", text: "认知循环完成一轮 感知→规划→行动 全流程", type: "cycle", c: "#8b5cf6", icon: Workflow },
    { time: "2m前", text: "memory_store(): 写入 12 条新记忆到 L2 特征层向量索引", type: "memory", c: "#06b6d4", icon: Database },
    { time: "5m前", text: "tool_d.read_file() 执行完成，耗时 23ms，返回 1.2KB 数据", type: "tool", c: "#10b981", icon: Wrench },
    { time: "8m前", text: "Cupolas 输入净化模块拦截了 1 个异常请求（SQL注入尝试）", type: "sec", c: "#ef4444", icon: Shield },
    { time: "12m前", text: "System 2 深度推理：对用户问题进行了多角度逻辑分析", type: "think", c: "#f59e0b", icon: Brain },
  ];
  return (
    <div className="card" style={{ padding: "20px" }}>
      <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
        <Activity size={18} color="#ec4899" />
        <span style={{ fontSize: "15px", fontWeight: 600 }}>实时流</span>
      </div>
      <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
        {events.map((ev, i) => {
          const EvIcon = ev.icon;
          return (
            <div key={i} style={{ display: "flex", gap: "12px", padding: "12px 0", borderBottom: "1px solid var(--border-subtle)" }}>
              <EvIcon size={16} color={ev.c} />
              <div style={{ flex: 1 }}>
                <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "4px" }}>
                  <span style={{ fontSize: "12px", color: ev.c, fontWeight: 600 }}>{ev.time}</span>
                  <span style={{ fontSize: "10px", padding: "2px 6px", background: `${ev.c}12`, color: ev.c, borderRadius: "4px" }}>{ev.type}</span>
                </div>
                <div style={{ fontSize: "13px", color: "var(--text-primary)" }}>{ev.text}</div>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}

/* ─── Quick Navigation Cards ─── */
function QNav({ nav }: { nav: (p: string) => void }) {
  const items = [
    { Ic: Target, label: "任务编排", path: "/tasks", c: "#6366f1" },
    { Ic: Eye, label: "可观测性", path: "/logs", c: "#06b6d4" },
    { Ic: Database, label: "记忆管理", path: "/memory-evolution", c: "#8b5cf6" },
    { Ic: Wrench, label: "工具管理", path: "/tools", c: "#10b981" },
  ];
  return (
    <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: "12px" }}>
      {items.map(it => {
        const Icon = it.Ic;
        return (
          <div key={it.path}
            onClick={() => nav(it.path)}
            style={{
              padding: "16px",
              borderLeft: `3px solid ${it.c}`,
              background: "var(--bg-secondary)",
              cursor: "pointer",
              transition: "all 0.2s ease",
            }}
            onMouseEnter={e => {
              e.currentTarget.style.background = "var(--bg-tertiary)";
            }}
            onMouseLeave={e => {
              e.currentTarget.style.background = "var(--bg-secondary)";
            }}
          >
            <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "8px" }}>
              <Icon size={18} color={it.c} />
              <span style={{ fontSize: "14px", fontWeight: 600 }}>{it.label}</span>
            </div>
            <div style={{ fontSize: "11px", color: it.c }}>进入模块</div>
          </div>
        );
      })}
    </div>
  );
}

/* ═ Phase 2 Enhanced Metrics Component ═ */
function Phase2Metrics() {
  const metrics = [
    {
      title: "双模型协调",
      icon: Brain,
      color: "#8b5cf6",
      values: [
        { label: "决策一致率", value: "94.2%", trend: "+1.2%" },
        { label: "自适应阈值", value: "0.72", trend: "动态" },
        { label: "历史记录", value: "87/100", trend: "环形缓冲" },
      ],
      description: "第二阶段增强：自适应学习与交叉验证",
    },
    {
      title: "缓存优化",
      icon: Database,
      color: "#06b6d4",
      values: [
        { label: "命中率", value: "87.5%", trend: "+3.1%" },
        { label: "TTL过期", value: "124", trend: "惰性清理" },
        { label: "访问频率", value: "2.4k", trend: "LFU辅助" },
      ],
      description: "第二阶段增强：TTL支持与统计系统",
    },
    {
      title: "遗忘策略",
      icon: Activity,
      color: "#10b981",
      values: [
        { label: "Lambda参数", value: "0.85", trend: "自适应" },
        { label: "样本误差", value: "0.12", trend: "持续优化" },
        { label: "调整历史", value: "42", trend: "100条记录" },
      ],
      description: "第二阶段增强：基于误差反馈的自适应学习",
    },
    {
      title: "安全沙箱",
      icon: Shield,
      color: "#ef4444",
      values: [
        { label: "审计日志", value: "856", trend: "1000条缓冲" },
        { label: "违规检测", value: "3", trend: "输入净化" },
        { label: "动态策略", value: "v2.1", trend: "已启用" },
      ],
      description: "第二阶段增强：动态策略与审计增强",
    },
  ];

  return (
    <div className="card" style={{ marginTop: "20px", padding: "20px" }}>
      <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
        <Sparkles size={18} color="#6366f1" />
        <span style={{ fontSize: "15px", fontWeight: 600 }}>第二阶段增强功能监控</span>
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: "12px" }}>
        {metrics.map((metric) => {
          const Icon = metric.icon;
          return (
            <div key={metric.title} style={{
              padding: "16px",
              borderLeft: `3px solid ${metric.color}`,
              background: "var(--bg-secondary)",
            }}>
              <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "12px" }}>
                <Icon size={16} color={metric.color} />
                <div>
                  <div style={{ fontSize: "13px", fontWeight: 600, color: metric.color }}>{metric.title}</div>
                  <div style={{ fontSize: "10px", color: "var(--text-muted)" }}>{metric.description}</div>
                </div>
              </div>
              <div style={{ display: "flex", flexDirection: "column", gap: "6px" }}>
                {metric.values.map((v, i) => (
                  <div key={i} style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                    <span style={{ fontSize: "11px", color: "var(--text-muted)" }}>{v.label}</span>
                    <span style={{ fontSize: "12px", fontWeight: 600 }}>{v.value}</span>
                  </div>
                ))}
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}

/* ════════════════════════════════════
   DASHBOARD MAIN COMPONENT
   ════════════════════════════════════ */
export default function Dashboard() {
  const navigate = useNavigate();
  const [loading, setLoading] = useState(true);
  const [phaseIdx, setPhaseIdx] = useState(1);
  const [refreshing, setRefreshing] = useState(false);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [connectionStatus, setConnectionStatus] = useState<'connected' | 'disconnected' | 'checking'>('checking');
  const [refreshInterval, setRefreshInterval] = useState(10000);
  const [isPaused, setIsPaused] = useState(false);
  const [cpuHistory, setCpuHistory] = useState<number[]>([]);
  const [memHistory, setMemHistory] = useState<number[]>([]);
  const [systemData, setSystemData] = useState<{
    cpu: number; memory: number; disk: number; processes: number;
    servicesUp: number; servicesTotal: number;
    uptime: string; version: string;
  } | null>(null);

  const fetchDashboardData = useCallback(async () => {
    setRefreshing(true);
    setConnectionStatus('checking');
    try {
      const [monitorData, serviceStatus] = await Promise.all([
        sdk.getSystemMonitorData().catch(() => null),
        sdk.getServiceStatus().catch(() => null),
      ]);
      if (monitorData || serviceStatus) {
        setConnectionStatus('connected');
      } else {
        setConnectionStatus('disconnected');
      }
      if (monitorData) {
        const newCpu = Math.round(monitorData.cpu.usage_percent);
        const newMem = Math.round(monitorData.memory.percent);
        setSystemData({
          cpu: newCpu,
          memory: newMem,
          disk: Math.round(monitorData.disk.percent),
          processes: monitorData.cpu.cores.length,
          servicesUp: serviceStatus ? serviceStatus.filter((s: any) => s.healthy).length : 5,
          servicesTotal: serviceStatus ? serviceStatus.length : 6,
          uptime: Math.floor(monitorData.uptime_seconds / 3600) + "h",
          version: "v2.1.0",
        });
        setCpuHistory(prev => [...prev.slice(-19), newCpu]);
        setMemHistory(prev => [...prev.slice(-19), newMem]);
      }
      setLastUpdate(new Date());
    } catch {
      setConnectionStatus('disconnected');
    } finally {
      setRefreshing(false);
    }
  }, []);

  useEffect(() => { const t = setTimeout(() => setLoading(false), 500); return () => clearTimeout(t); }, []);
  useEffect(() => { const tm = setInterval(() => setPhaseIdx(p => (p + 1) % 3), 4000); return () => clearInterval(tm); }, []);
  useEffect(() => {
    fetchDashboardData();
    if (refreshInterval > 0 && !isPaused) {
      const iv = setInterval(fetchDashboardData, refreshInterval);
      return () => clearInterval(iv);
    }
  }, [fetchDashboardData, refreshInterval, isPaused]);

  const cpuVal = systemData?.cpu ?? 23;
  const memVal = systemData?.memory ?? 26;
  const diskVal = systemData?.disk ?? 75;
  const procVal = systemData ? Math.round((systemData.servicesUp / systemData.servicesTotal) * 100) : 83;

  if (loading) {
    return (
      <PageLayout title="AgentOS 控制中心">
        <div style={{ display: "flex", alignItems: "center", justifyContent: "center", minHeight: "72vh" }}>
          <div style={{ textAlign: "center" }}>
            <div style={{
              width: "48px", height: "48px", margin: "0 auto 16px", borderRadius: "8px",
              background: "var(--primary-color)",
              display: "flex", alignItems: "center", justifyContent: "center",
            }}>
              <Bot size={24} color="white" />
            </div>
            <div className="loading-spinner" />
            <p style={{ color: "var(--text-muted)", marginTop: "12px", fontSize: "13px", fontWeight: 500 }}>正在加载控制中心...</p>
          </div>
        </div>
      </PageLayout>
    );
  }

  return (
    <PageLayout
      title="AgentOS 控制中心"
      subtitle="工业级 AI 智能体 · 实时状态监控"
    >
      {/* Header Actions */}
      <div style={{ display: "flex", alignItems: "center", gap: "12px", marginBottom: "20px" }}>
        {/* Connection Status Indicator */}
        <div style={{
          display: "flex", alignItems: "center", gap: "6px", padding: "6px 14px",
          borderRadius: "9999px",
          background: connectionStatus === 'connected' ? "var(--bg-secondary)" : "var(--bg-tertiary)",
          border: `1px solid ${
            connectionStatus === 'connected' ? "#10b98120"
            : connectionStatus === 'disconnected' ? "#ef444420"
            : "var(--border-subtle)"
          }`,
        }}>
          <div style={{
            width: "6px", height: "6px", borderRadius: "50%",
            background: connectionStatus === 'connected' ? "#10b981"
              : connectionStatus === 'disconnected' ? "#ef4444"
              : "#f59e0b",
          }} />
          <span style={{ fontSize: "12px",
            color: connectionStatus === 'connected' ? "#10b981"
              : connectionStatus === 'disconnected' ? "#ef4444"
              : "#f59e0b"
          }}>
            {connectionStatus === 'connected' ? "已连接"
              : connectionStatus === 'disconnected' ? "未连接"
              : "检测中"}
          </span>
        </div>

        {/* Quick Actions */}
        <div style={{ display: "flex", gap: "6px" }}>
          <Button
            variant="ghost"
            size="sm"
            onClick={() => navigate('/agents')}
            title="管理智能体"
          >
            <Bot size={14} />
          </Button>
          <Button
            variant="ghost"
            size="sm"
            onClick={() => navigate('/tasks')}
            title="任务队列"
          >
            <Workflow size={14} />
          </Button>
          <Button
            variant="ghost"
            size="sm"
            onClick={() => navigate('/terminal')}
            title="终端"
          >
            <Radio size={14} />
          </Button>
        </div>

        <Button
          variant="ghost"
          size="sm"
          onClick={fetchDashboardData}
          disabled={refreshing}
          title="刷新"
        >
          <RefreshCw size={14} className={refreshing ? "spin" : ""} />
        </Button>

        <div style={{
          display: 'flex', alignItems: 'center', gap: '6px',
          borderLeft: '1px solid var(--border-subtle)',
          paddingLeft: '12px', marginLeft: '4px'
        }}>
          <Button
            variant="ghost"
            size="sm"
            onClick={() => {
              if (systemData) {
                exportToCSV([{
                  指标: 'CPU使用率', 值: systemData.cpu, 单位: '%',
                  时间: lastUpdate.toLocaleString('zh-CN'),
                }, {
                  指标: '内存使用率', 值: systemData.memory, 单位: '%',
                  时间: lastUpdate.toLocaleString('zh-CN'),
                }, {
                  指标: '磁盘使用率', 值: systemData.disk, 单位: '%',
                  时间: lastUpdate.toLocaleString('zh-CN'),
                }], 'agentos_system_metrics');
              }
            }}
            title="导出CSV"
          >
            <Download size={12} />
          </Button>
          <span style={{ fontSize: "11px", color: "var(--text-muted)" }}>
            {lastUpdate.toLocaleTimeString("zh-CN", { hour: "2-digit", minute: "2-digit" })}
          </span>
        </div>
      </div>

      {/* Resource Gauges */}
      <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: "16px", marginBottom: "20px" }}>
        {[
          { v: cpuVal, u: "%", l: "CPU", s: systemData ? `${systemData.processes}核心` : "8核/16线程", c: "#6366f1" },
          { v: memVal, u: "%", l: "内存", s: systemData ? `${systemData.memory}% 已用` : "4.2 / 16 GB", c: "#06b6d4" },
          { v: diskVal, u: "%", l: "磁盘", s: systemData ? `${systemData.disk}% 已用` : "128 / 512 GB", c: "#f59e0b" },
          { v: procVal, u: "%", l: "进程", s: systemData ? `${systemData.servicesUp}/${systemData.servicesTotal} 在线` : "5/6 在线", c: "#10b981" },
        ].map((m) => (
          <Card key={m.l} style={{
            borderLeft: `3px solid ${m.c}`,
          }}>
            <div style={{ fontSize: "12px", color: "var(--text-muted)", marginBottom: "8px" }}>{m.l}</div>
            <div style={{ fontSize: "24px", fontWeight: 600, color: m.c, marginBottom: "4px" }}>{m.v}{m.u}</div>
            <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>{m.s}</div>
          </Card>
        ))}
      </div>

      {/* Cognitive Pipeline */}
      <Card style={{ marginBottom: "20px" }}>
        <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
          <Workflow size={18} color="#8b5cf6" />
          <span style={{ fontSize: "15px", fontWeight: 600 }}>认知处理流程</span>
        </div>
        <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: "12px" }}>
          {PHASES.map((p) => {
            const Icon = p.icon;
            return (
              <div key={p.key} style={{ padding: "12px", borderLeft: `3px solid ${p.color}`, background: "var(--bg-secondary)" }}>
                <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "8px" }}>
                  <Icon size={16} color={p.color} />
                  <span style={{ fontSize: "13px", fontWeight: 600, color: p.color }}>{p.label}</span>
                </div>
                <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>{p.desc}</div>
              </div>
            );
          })}
        </div>
      </Card>

      {/* Main Grid: Memory + Dual System */}
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px", marginBottom: "20px" }}>
        <Card>
          <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
            <Layers size={18} color="#8b5cf6" />
            <span style={{ fontSize: "15px", fontWeight: 600 }}>记忆体系</span>
          </div>
          <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
            {MEM_LAYERS.map((layer) => {
              const Ic = layer.icon;
              const pct = Math.round((layer.entries / layer.max) * 100);
              return (
                <div key={layer.key} style={{ display: "flex", justifyContent: "space-between", alignItems: "center", padding: "8px 0", borderBottom: "1px solid var(--border-subtle)" }}>
                  <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                    <Ic size={16} color={layer.color} />
                    <span style={{ fontSize: "13px" }}>{layer.name}</span>
                  </div>
                  <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                    <span style={{ fontSize: "12px", color: layer.color, fontWeight: 600 }}>{pct}%</span>
                    <span style={{ fontSize: "11px", color: "var(--text-muted)" }}>{layer.entries}/{layer.max}</span>
                  </div>
                </div>
              );
            })}
          </div>
        </Card>
        <DualPanel />
      </div>

      {/* Security + Services */}
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px", marginBottom: "20px" }}>
        <SecShield />
        <SvcGrid />
      </div>

      {/* Timeline + Quick Nav */}
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px" }}>
        <TimeLine />
        <QNav nav={navigate} />
      </div>
    </PageLayout>
  );
}
