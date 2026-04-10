import React, { useState } from "react";
import {
  Activity,
  Brain,
  Database,
  Zap,
  Cpu,
  Globe,
  Shield,
  Settings2,
  Terminal,
  Eye,
  MessageSquare,
  Wrench,
  Layers,
  TrendingUp,
  Clock,
  Plus,
  FileText,
  Search,
  RefreshCw,
  CheckCircle2,
  Hash,
} from "lucide-react";
import MemorySystem from "../components/MemorySystem";
import CognitiveLoop from "../components/CognitiveLoop";

const AGENT_TOOLS = [
  { name: "start_services", desc: "启动/停止/重启服务集群", category: "system", icon: Zap, color: "#6366f1", calls: 12 },
  { name: "get_service_status", desc: "查询服务运行状态", category: "system", icon: Activity, color: "#22c55e", calls: 45 },
  { name: "list_agents", desc: "列出已注册智能体", category: "agent", icon: Brain, color: "#a855f7", calls: 23 },
  { name: "register_agent", desc: "注册新智能体", category: "agent", icon: Plus, color: "#f59e0b", calls: 3 },
  { name: "submit_task", desc: "提交异步任务", category: "task", icon: FileText, color: "#06b6d4", calls: 18 },
  { name: "get_system_info", desc: "获取系统硬件信息", category: "system", icon: Cpu, color: "#ef4444", calls: 67 },
  { name: "memory_store", desc: "存储记忆条目到向量库", category: "memory", icon: Database, color: "#ec4899", calls: 31 },
  { name: "memory_search", desc: "语义搜索长期记忆", category: "memory", icon: Search, color: "#8b5cf6", calls: 52 },
  { name: "open_browser", desc: "打开外部浏览器", category: "io", icon: Globe, color: "#14b8a6", calls: 8 },
  { name: "read_file", desc: "读取本地文件内容", category: "io", icon: FileText, color: "#f97316", calls: 15 },
];

const RUNTIME_METRICS = [
  { label: "认知循环", value: 847, unit: "次", icon: RefreshCw, color: "#6366f1" },
  { label: "工具调用", value: 274, unit: "次", icon: Wrench, color: "#22c55e" },
  { label: "记忆存储", value: 156, unit: "条", icon: Database, color: "#a855f7" },
  { label: "平均延迟", value: 1.24, unit: "s", icon: Clock, color: "#f59e0b" },
  { label: "成功率", value: 98.5, unit: "%", icon: CheckCircle2, color: "#22c55e" },
  { label: "Token 消耗", value: 42.3, unit: "K", icon: Hash, color: "#06b6d4" },
];

const AgentRuntime: React.FC = () => {
  const [activeTab, setActiveTab] = useState<"overview" | "memory" | "cognitive" | "tools">("overview");

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
          <h1>AgentOS 运行时</h1>
          <span className="badge status-running">运行中</span>
        </div>
        <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
          智能体核心能力总览：认知循环 · 记忆卷载 · 工具调度 · 状态监控
        </p>
      </div>

      {/* Runtime Metrics Bar */}
      <div className="card card-elevated" style={{ marginBottom: "20px" }}>
        <div style={{
          display: "grid",
          gridTemplateColumns: "repeat(6, 1fr)",
          gap: "16px",
        }}>
          {RUNTIME_METRICS.map((metric) => {
            const IconComp = metric.icon;
            return (
              <div key={metric.label} style={{
                textAlign: "center", padding: "16px 10px",
                borderRadius: "var(--radius-md)",
                background: `${metric.color}08`, border: `1px solid ${metric.color}15`,
                transition: "all var(--transition-fast)",
              }}
              onMouseEnter={(e) => { e.currentTarget.style.transform = "translateY(-2px)"; e.currentTarget.style.boxShadow = `0 4px 12px ${metric.color}15`; }}
              onMouseLeave={(e) => { e.currentTarget.style.transform = ""; e.currentTarget.style.boxShadow = ""; }}
              >
                <IconComp size={18} style={{ color: metric.color, marginBottom: "6px" }} />
                <div style={{ fontSize: "22px", fontWeight: 700, color: metric.color, lineHeight: 1.2 }}>
                  {typeof metric.value === 'number' && metric.unit === '%' ? metric.value.toFixed(1)
                    : typeof metric.value === 'number' && metric.value < 100 ? metric.value.toFixed(metric.value % 1 === 0 ? 0 : 2)
                    : metric.value}
                  <span style={{ fontSize: "11px", fontWeight: 400, color: "var(--text-muted)" }}> {metric.unit}</span>
                </div>
                <div style={{ fontSize: "11.5px", color: "var(--text-muted)", marginTop: "2px" }}>{metric.label}</div>
              </div>
            );
          })}
        </div>
      </div>

      {/* Tab Navigation */}
      <div style={{
        display: "flex", background: "var(--bg-tertiary)",
        borderRadius: "var(--radius-md)", padding: "3px", border: "1px solid var(--border-subtle)",
        marginBottom: "20px", width: "fit-content",
      }}>
        {[
          { key: "overview" as const, icon: Activity, label: "总览" },
          { key: "memory" as const, icon: Database, label: "记忆系统" },
          { key: "cognitive" as const, icon: Brain, label: "认知循环" },
          { key: "tools" as const, icon: Wrench, label: "工具集" },
        ].map(tab => (
          <button
            key={tab.key}
            onClick={() => setActiveTab(tab.key)}
            style={{
              padding: "8px 20px", border: "none", borderRadius: "var(--radius-sm)",
              background: activeTab === tab.key ? "var(--primary-color)" : "transparent",
              color: activeTab === tab.key ? "white" : "var(--text-secondary)",
              cursor: "pointer", fontWeight: 500, fontSize: "13px",
              transition: "all var(--transition-fast)",
              display: "flex", alignItems: "center", gap: "6px",
            }}
          >
            <tab.icon size={14} />{tab.label}
          </button>
        ))}
      </div>

      {/* Tab Content */}
      {activeTab === "overview" && (
        <>
          {/* Architecture Overview */}
          <div className="card card-elevated" style={{ marginBottom: "20px" }}>
            <h3 className="card-title"><Layers size={18} /> AgentOS 核心架构</h3>
            <div style={{
              display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: "16px",
              marginTop: "12px",
            }}>
              {[
                { title: "感知层 (Perception)", items: ["用户输入解析", "系统状态监听", "环境信号采集"], icon: Eye, color: "#06b6d4" },
                { title: "推理层 (Reasoning)", items: ["LLM 意图识别", "方案规划生成", "上下文检索"], icon: Brain, color: "#6366f1" },
                { title: "行动层 (Action)", items: ["工具调度执行", "API 调用管理", "结果聚合"], icon: Zap, color: "#22c55e" },
                { title: "反思层 (Reflection)", items: ["质量评估反馈", "经验知识沉淀", "策略自我优化"], icon: RefreshCw, color: "#f59e0b" },
              ].map(layer => (
                <div key={layer.title} style={{
                  padding: "18px", borderRadius: "var(--radius-md)",
                  background: `${layer.color}06`, border: `1px solid ${layer.color}20`,
                  borderTop: `3px solid ${layer.color}`,
                }}>
                  <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "12px" }}>
                    <layer.icon size={18} color={layer.color} />
                    <span style={{ fontWeight: 600, fontSize: "13px" }}>{layer.title}</span>
                  </div>
                  <ul style={{ margin: 0, paddingLeft: "18px", fontSize: "12.5px", color: "var(--text-secondary)", lineHeight: "2" }}>
                    {layer.items.map(item => <li key={item}>{item}</li>)}
                  </ul>
                </div>
              ))}
            </div>
          </div>

          {/* Quick Access Cards */}
          <div className="grid-2">
            <div className="card card-elevated" style={{
              cursor: "pointer", borderLeft: "3px solid #6366f1",
            }} onClick={() => setActiveTab("cognitive")}>
              <h3 className="card-title"><Brain size={18} /> 认知引擎</h3>
              <p style={{ fontSize: "13px", color: "var(--text-secondary)", lineHeight: 1.6, margin: 0 }}>
                完整的感知→推理→行动→反思循环，可视化展示 AI 思维过程。
                支持实时追踪思维链、工具调用链和执行状态。
              </p>
              <button className="btn btn-primary btn-sm" style={{ marginTop: "12px" }}>
                启动认知循环演示 →
              </button>
            </div>

            <div className="card card-elevated" style={{
              cursor: "pointer", borderLeft: "3px solid #a855f7",
            }} onClick={() => setActiveTab("memory")}>
              <h3 className="card-title"><Database size={18} /> 记忆系统</h3>
              <p style={{ fontSize: "13px", color: "var(--text-secondary)", lineHeight: 1.6, margin: 0 }}>
                多层次记忆架构：工作记忆、长期记忆、向量索引。
                支持语义搜索、相关性评分、上下文窗口自动管理。
              </p>
              <button className="btn btn-secondary btn-sm" style={{ marginTop: "12px" }}>
                查看记忆详情 →
              </button>
            </div>
          </div>
        </>
      )}

      {activeTab === "memory" && <MemorySystem />}
      {activeTab === "cognitive" && <CognitiveLoop />}

      {activeTab === "tools" && (
        <div className="card card-elevated">
          <h3 className="card-title"><Wrench size={18} /> 可用工具集</h3>
          <div style={{
            display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(300px, 1fr))", gap: "12px",
          }}>
            {AGENT_TOOLS.map((tool, idx) => {
              const IconComp = tool.icon;
              return (
                <div key={tool.name}
                  className="card-hover-lift"
                  style={{
                    padding: "16px", borderRadius: "var(--radius-md)",
                    border: "1px solid var(--border-subtle)",
                    animation: `staggerFadeIn 0.35s ease-out ${idx * 50}ms both`,
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "8px" }}>
                    <div style={{
                      width: "34px", height: "34px", borderRadius: "var(--radius-sm)",
                      background: tool.color, display: "flex", alignItems: "center", justifyContent: "center",
                      flexShrink: 0,
                    }}>
                      <IconComp size="16" color="white" />
                    </div>
                    <div style={{ flex: 1, minWidth: 0 }}>
                      <code style={{ fontSize: "13px", fontWeight: 600 }}>{tool.name}</code>
                      <span className="tag" style={{ marginLeft: "8px", fontSize: "10px" }}>{tool.category}</span>
                    </div>
                    <span style={{
                      fontSize: "12px", fontWeight: 600, color: tool.color,
                      fontFamily: "'JetBrains Mono', monospace",
                    }}>{tool.calls}</span>
                  </div>
                  <p style={{ fontSize: "12px", color: "var(--text-secondary)", margin: 0 }}>{tool.desc}</p>
                </div>
              );
            })}
          </div>
        </div>
      )}
    </div>
  );
};

export default AgentRuntime;
