import React, { useState, useEffect } from "react";
import {
  Cpu,
  Brain,
  Database,
  Wrench,
  Zap,
  Activity,
  Clock,
  TrendingUp,
  CheckCircle2,
  ArrowUpRight,
  Layers,
  RefreshCw,
  Sparkles,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import type { AgentRuntimeMetrics, ToolDefinition } from "../services/agentos-sdk";
import MemorySystem from "../components/MemorySystem";
import CognitiveLoop from "../components/CognitiveLoop";

const AgentRuntime: React.FC = () => {
  const [activeTab, setActiveTab] = useState<"overview" | "memory" | "cognitive" | "tools">("overview");
  const [metrics, setMetrics] = useState<AgentRuntimeMetrics | null>(null);
  const [availableTools, setAvailableTools] = useState<Array<{name: string; category: string; description: string}>>([]);
  const [loadingMetrics, setLoadingMetrics] = useState(true);
  const [loadingTools, setLoadingTools] = useState(true);

  useEffect(() => {
    const loadAll = async () => {
      try {
        const [mData, tData] = await Promise.all([
          sdk.getRuntimeMetrics(),
          sdk.listAvailableTools(),
        ]);
        setMetrics(mData || null);
        setAvailableTools((tData || []).map((t: any) => ({
          name: t.function?.name || t.name,
          description: t.function?.description || t.description,
          category: (t.function?.parameters as any)?.category || t.category || "general",
        })));
      } catch (error) {
        console.error("Failed to load runtime data:", error);
      } finally {
        setLoadingMetrics(false);
        setLoadingTools(false);
      }
    };
    loadAll();
    const interval = setInterval(() => {
      sdk.getRuntimeMetrics().then(d => d && setMetrics(d)).catch(() => {});
    }, 5000);
    return () => clearInterval(interval);
  }, []);

  const displayMetrics = metrics || {
    cycle_count: 0,
    tool_call_count: 0,
    memory_entries_count: 0,
    avg_latency_ms: 0,
    success_rate: 100.0,
    total_tokens_consumed: 0,
  };

  const METRIC_CARDS = [
    { key: "cycle_count", icon: Cpu, label: "认知循环", value: displayMetrics.cycle_count, unit: "", color: "#6366f1", bgLight: "rgba(99,102,241,0.08)" },
    { key: "tool_call_count", icon: Wrench, label: "工具调用", value: displayMetrics.tool_call_count, unit: "", color: "#22c55e", bgLight: "rgba(34,197,94,0.08)" },
    { key: "memory_entries_count", icon: Database, label: "记忆条目", value: displayMetrics.memory_entries_count, unit: "", color: "#f59e0b", bgLight: "rgba(245,158,11,0.08)" },
    { key: "avg_latency_ms", icon: Clock, label: "平均延迟", value: (displayMetrics.avg_latency_ms / 1000).toFixed(2), unit: "s", color: "#06b6d4", bgLight: "rgba(6,182,212,0.08)" },
    { key: "success_rate", icon: CheckCircle2, label: "成功率", value: displayMetrics.success_rate.toFixed(1), unit: "%", color: "#a855f7", bgLight: "rgba(168,85,247,0.08)" },
    { key: "total_tokens_consumed", icon: Zap, label: "Token消耗", value: (displayMetrics.total_tokens_consumed / 1000).toFixed(1), unit: "K", color: "#ef4444", bgLight: "rgba(239,68,68,0.08)" },
  ];

  const ARCHITECTURE_CARDS = [
    { title: "感知层 (Perception)", desc: "多模态输入处理、环境状态监控、事件流订阅", icon: Activity, color: "#06b6d4", gradient: "linear-gradient(135deg,#06b6d4,#22d3ee)", features: ["文本/图像输入解析", "系统事件监听", "传感器数据采集"] },
    { title: "推理层 (Reasoning)", desc: "LLM 驱动的语义理解、意图识别、方案规划", icon: Brain, color: "#6366f1", gradient: "linear-gradient(135deg,#6366f1,#818cf8)", features: ["上下文理解与生成", "思维链推理追踪", "多步决策规划"] },
    { title: "行动层 (Action)", desc: "工具调度执行、API调用、文件操作、进程管理", icon: Zap, color: "#22c55e", gradient: "linear-gradient(135deg,#22c55e,#4ade80)", features: ["工具注册与调用", "并行任务编排", "结果聚合返回"] },
    { title: "反思层 (Reflection)", desc: "执行结果评估、记忆更新、经验学习优化", icon: TrendingUp, color: "#f59e0b", gradient: "linear-gradient(135deg,#f59e0b,#fbbf24)", features: ["结果质量评估", "长期记忆写入", "策略自我优化"] },
  ];

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "20px" }}>
      <div className="card card-elevated">
        <div style={{ padding: "22px 26px", borderBottom: "1px solid var(--border-subtle)" }}>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
            <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
              <div style={{
                width: "48px", height: "48px", borderRadius: "var(--radius-lg)",
                background: "linear-gradient(135deg, #6366f1, #a78bfa)",
                display: "flex", alignItems: "center", justifyContent: "center",
                boxShadow: "0 4px 16px rgba(99,102,241,0.3)",
              }}>
                <Cpu size="24" color="white" />
              </div>
              <div>
                <h3 style={{ margin: 0, fontSize: "18px" }}>AgentOS 运行时</h3>
                <p style={{ margin: "4px 0 0 0", fontSize: "13px", color: "var(--text-secondary)" }}>
                  核心运行时引擎：实时指标 · 记忆管理 · 认知循环 · 工具调度
                  {metrics && <span style={{ marginLeft: "10px", color: "var(--primary-color)", fontWeight: 600 }}>· 已连接后端</span>}
                </p>
              </div>
            </div>
            <button className="btn btn-ghost" onClick={() => window.location.reload()}>
              <RefreshCw size={14} /> 刷新
            </button>
          </div>
        </div>

        {/* Metrics Bar */}
        <div style={{
          display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(150px, 1fr))",
          gap: "12px", padding: "18px 22px",
        }}>
          {METRIC_CARDS.map(mc => {
            const IconComp = mc.icon;
            return (
              <div key={mc.key} style={{
                padding: "14px 16px", borderRadius: "var(--radius-md)",
                background: mc.bgLight, border: `1px solid ${mc.color}15`,
                transition: "all var(--transition-fast)",
              }}
              onMouseEnter={(e) => { e.currentTarget.style.transform = "translateY(-2px)"; e.currentTarget.style.boxShadow = `0 8px 24px ${mc.color}10`; }}
              onMouseLeave={(e) => { e.currentTarget.style.transform = ""; e.currentTarget.style.boxShadow = ""; }}
              >
                <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "8px" }}>
                  <IconComp size={16} color={mc.color} />
                  <span style={{ fontSize: "11.5px", color: "var(--text-muted)", fontWeight: 500 }}>{mc.label}</span>
                </div>
                <div style={{ fontSize: "22px", fontWeight: 700, fontFamily: "'JetBrains Mono', monospace", color: mc.color }}>
                  {loadingMetrics ? "-" : mc.value}
                  <span style={{ fontSize: "13px", fontWeight: 400, color: "var(--text-muted)", marginLeft: "3px" }}>{mc.unit}</span>
                </div>
              </div>
            );
          })}
        </div>

        {/* Tab Switcher */}
        <div style={{
          display: "flex", gap: "4px", padding: "0 22px 16px",
          borderBottom: "1px solid var(--border-subtle)",
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
                padding: "9px 18px", border: "none", borderRadius: "var(--radius-sm)",
                background: activeTab === tab.key ? `${"#6366f1"}10` : "transparent",
                color: activeTab === tab.key ? "var(--primary-color)" : "var(--text-secondary)",
                cursor: "pointer", fontWeight: 500, fontSize: "13px",
                transition: "all var(--transition-fast)", display: "flex", alignItems: "center", gap: "6px",
                borderBottom: activeTab === tab.key ? `2px solid #6366f1` : "2px solid transparent",
              }}
            >
              <tab.icon size={14} />{tab.label}
            </button>
          ))}
        </div>
      </div>

      {activeTab === "overview" && (
        <>
          <div className="grid-2">
            {ARCHITECTURE_CARDS.map((ac, idx) => {
              const IconComp = ac.icon;
              return (
                <div key={ac.title} className="card card-elevated" style={{
                  animation: `staggerFadeIn 0.45s ease-out ${idx * 120}ms both`,
                  cursor: "pointer",
                }} onClick={() => {
                  if (idx === 1) setActiveTab("cognitive");
                  else if (idx === 2) setActiveTab("tools");
                }}>
                  <div style={{ display: "flex", alignItems: "center", gap: "12px", marginBottom: "14px" }}>
                    <div style={{
                      width: "42px", height: "42px", borderRadius: "var(--radius-md)",
                      background: ac.gradient, display: "flex", alignItems: "center", justifyContent: "center",
                      boxShadow: `0 4px 14px ${ac.color}25`, flexShrink: 0,
                    }}>
                      <IconComp size="21" color="white" />
                    </div>
                    <div>
                      <h4 style={{ margin: 0, fontSize: "15px", fontWeight: 600 }}>{ac.title}</h4>
                      <p style={{ margin: "2px 0 0 0", fontSize: "12px", color: "var(--text-muted)" }}>{ac.desc}</p>
                    </div>
                  </div>
                  <div style={{ display: "flex", flexWrap: "wrap", gap: "6px" }}>
                    {ac.features.map(f => (
                      <span key={f} style={{
                        padding: "3px 10px", borderRadius: "var(--radius-full)",
                        background: `${ac.color}08`, color: ac.color, fontSize: "11.5px", fontWeight: 500,
                      }}>{f}</span>
                    ))}
                  </div>
                  <div style={{ marginTop: "12px", textAlign: "right" }}>
                    <ArrowUpRight size={14} color="var(--primary-color)" />
                  </div>
                </div>
              );
            })}
          </div>

          <div className="card card-elevated">
            <h3 className="card-title"><Sparkles size={18} /> 快速访问</h3>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(200px, 1fr))", gap: "10px" }}>
              {[
                { label: "查看记忆条目", icon: Database, action: () => setActiveTab("memory"), color: "#6366f1" },
                { label: "运行认知循环", icon: Brain, action: () => setActiveTab("cognitive"), color: "#22c55e" },
                { label: "浏览工具列表", icon: Wrench, action: () => setActiveTab("tools"), color: "#f59e0b" },
                { label: "刷新运行指标", icon: RefreshCw, action: () => window.location.reload(), color: "#06b6d4" },
              ].map(item => {
                const IconComp = item.icon;
                return (
                  <button key={item.label} onClick={item.action} style={{
                    display: "flex", alignItems: "center", gap: "10px", padding: "12px 16px",
                    borderRadius: "var(--radius-md)", border: "1px solid var(--border-subtle)",
                    background: `${item.color}06`, cursor: "pointer",
                    transition: "all var(--transition-fast)", textAlign: "left",
                  }}
                  onMouseEnter={(e) => { e.currentTarget.style.borderColor = item.color; e.currentTarget.style.background = `${item.color}10`; }}
                  onMouseLeave={(e) => { e.currentTarget.style.borderColor = ""; e.currentTarget.style.background = `${item.color}06`; }}
                  >
                    <IconComp size={17} color={item.color} />
                    <span style={{ fontSize: "13.5px", fontWeight: 500 }}>{item.label}</span>
                  </button>
                );
              })}
            </div>
          </div>
        </>
      )}

      {activeTab === "memory" && <MemorySystem />}
      {activeTab === "cognitive" && <CognitiveLoop />}
      {activeTab === "tools" && (
        <div className="card card-elevated">
          <h3 className="card-title"><Wrench size={18} /> 可用工具列表 ({loadingTools ? "..." : availableTools.length})</h3>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(320px, 1fr))", gap: "12px" }}>
            {loadingTools ? (
              Array.from({ length: 6 }).map((_, i) => (
                <div key={i} style={{ padding: "18px", borderRadius: "var(--radius-md)", background: "var(--bg-tertiary)" }}>
                  <div style={{ height: "16px", width: "60%", borderRadius: "4px", background: "var(--border-subtle)", marginBottom: "10px" }} />
                  <div style={{ height: "12px", width: "90%", borderRadius: "4px", background: "var(--border-subtle)", marginBottom: "6px" }} />
                  <div style={{ height: "12px", width: "40%", borderRadius: "4px", background: "var(--border-subtle)" }} />
                </div>
              ))
            ) : availableTools.length === 0 ? (
              <div style={{ gridColumn: "1/-1", textAlign: "center", padding: "48px", color: "var(--text-muted)" }}>
                <Wrench size={40} style={{ opacity: 0.3, marginBottom: "12px" }} />
                <div>暂无可用工具，请检查后端服务是否正常运行</div>
              </div>
            ) : availableTools.map((tool, idx) => (
              <div key={tool.name + idx} style={{
                padding: "16px 18px", borderRadius: "var(--radius-md)",
                border: "1px solid var(--border-subtle)", transition: "all var(--transition-fast)",
                animation: `staggerFadeIn 0.35s ease-out ${idx * 60}ms both`,
              }}
              onMouseEnter={(e) => { e.currentTarget.style.transform = "translateY(-2px)"; e.currentTarget.style.borderColor = "var(--primary-color)30"; e.currentTarget.style.boxShadow = "0 4px 16px rgba(99,102,241,0.08)"; }}
              onMouseLeave={(e) => { e.currentTarget.style.transform = ""; e.currentTarget.style.borderColor = ""; e.currentTarget.style.boxShadow = ""; }}
              >
                <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "8px" }}>
                  <div style={{
                    width: "34px", height: "34px", borderRadius: "var(--radius-sm)",
                    background: "linear-gradient(135deg, #6366f1, #a78bfa)",
                    display: "flex", alignItems: "center", justifyContent: "center",
                    flexShrink: 0,
                  }}>
                    <Wrench size="16" color="white" />
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{ fontWeight: 600, fontSize: "14px", fontFamily: "'JetBrains Mono', monospace" }}>{tool.name}</div>
                    <div style={{ fontSize: "11px", color: "var(--text-muted)", marginTop: "2px" }}>{tool.category}</div>
                  </div>
                  <span style={{
                    padding: "2px 8px", borderRadius: "var(--radius-full)",
                    background: "rgba(34,197,94,0.08)", color: "#22c55e",
                    fontSize: "11px", fontWeight: 500,
                  }}>可用</span>
                </div>
                <p style={{ margin: 0, fontSize: "12.5px", color: "var(--text-secondary)", lineHeight: 1.5 }}>{tool.description}</p>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};

export default AgentRuntime;