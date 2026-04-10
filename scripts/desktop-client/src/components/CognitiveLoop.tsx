import React, { useState, useEffect } from "react";
import {
  Eye,
  Brain,
  Zap,
  RefreshCw,
  Play,
  Pause,
  RotateCcw,
  ArrowRight,
  CheckCircle2,
  AlertCircle,
  Clock,
  Target,
  Lightbulb,
  MessageSquare,
  Wrench,
  FileText,
  Sparkles,
} from "lucide-react";

type CyclePhase = "perception" | "reasoning" | "action" | "reflection" | "idle";

interface StepData {
  phase: CyclePhase;
  title: string;
  description: string;
  icon: typeof Eye;
  color: string;
  gradient: string;
  duration: number;
}

const PHASES: Record<CyclePhase, StepData> = {
  perception: {
    phase: "perception", title: "感知 (Perceive)",
    description: "接收用户输入、观察系统状态、读取环境信号",
    icon: Eye, color: "#06b6d4", gradient: "linear-gradient(135deg, #06b6d4, #22d3ee)",
    duration: 2000,
  },
  reasoning: {
    phase: "reasoning", title: "推理 (Reason)",
    description: "调用 LLM 进行语义理解、意图识别、方案规划",
    icon: Brain, color: "#6366f1", gradient: "linear-gradient(135deg, #6366f1, #818cf8)",
    duration: 3500,
  },
  action: {
    phase: "action", title: "行动 (Act)",
    description: "执行工具调用、系统操作、生成响应",
    icon: Zap, color: "#22c55e", gradient: "linear-gradient(135deg, #22c55e, #4ade80)",
    duration: 1500,
  },
  reflection: {
    phase: "reflection", title: "反思 (Reflect)",
    description: "评估结果质量、更新记忆、记录经验教训",
    icon: RefreshCw, color: "#f59e0b", gradient: "linear-gradient(135deg, #f59e0b, #fbbf24)",
    duration: 2000,
  },
  idle: { phase: "idle", title: "待机", description: "", icon: Clock, color: "#94a3b8", gradient: "", duration: 0 },
};

interface ThoughtStep {
  id: string;
  phase: CyclePhase;
  content: string;
  timestamp: Date;
  detail?: string;
}

interface ToolCall {
  id: string;
  name: string;
  status: "pending" | "running" | "success" | "error";
  input?: string;
  output?: string;
  duration?: number;
}

const MOCK_THOUGHTS: ThoughtStep[] = [
  { id: "t1", phase: "perception", content: "收到用户消息：「帮我启动所有服务」", timestamp: new Date(Date.now() - 9000), detail: "输入长度: 28 字符 | 语言: 中文" },
  { id: "t2", phase: "reasoning", content: "分析意图：用户请求启动 AgentOS 服务集群", timestamp: new Date(Date.now() - 7000), detail: "置信度: 94% | 意图分类: system_operation" },
  { id: "t3", phase: "reasoning", content: "检索记忆：上次启动服务的配置模式 (dev mode)", timestamp: new Date(Date.now() - 5500), detail: "召回相关性: 0.88 | 记忆ID: m2" },
  { id: "t4", phase: "reasoning", content: "制定执行计划：按依赖顺序依次启动 kernel → gateway → db → cache", timestamp: new Date(Date.now() - 4000), detail: "规划步骤: 4 | 预估时间: ~12s" },
  { id: "t5", phase: "action", content: "调用工具: start_services(mode='dev')", timestamp: new Date(Date.now() - 2500), detail: "Tauri invoke → Rust backend" },
  { id: "t6", phase: "action", content: "工具返回: 所有 4 个服务已成功启动", timestamp: new Date(Date.now() - 1000), detail: "耗时: 11.3s | 状态码: OK" },
  { id: "t7", phase: "reflection", content: "评估结果：任务完成，所有目标达成", timestamp: new Date(), detail: "质量评分: A+ | 新增记忆: 1 条" },
];

const MOCK_TOOLS: ToolCall[] = [
  { id: "tc1", name: "start_services", status: "success", input: "{ mode: 'dev' }", output: "{ running: ['kernel','gateway','postgres','redis'] }", duration: 11300 },
  { id: "tc2", name: "get_service_status", status: "success", input: "{}", output: "[{ name:'kernel', healthy:true, port:18080 }, ...]", duration: 420 },
  { id: "tc3", name: "memory_store", status: "success", input: "{ type:'fact', content:'...' }", output: "{ stored: true, id: 'm11' }", duration: 80 },
];

const CognitiveLoop: React.FC = () => {
  const [currentPhase, setCurrentPhase] = useState<CyclePhase>("idle");
  const [isRunning, setIsRunning] = useState(false);
  const [thoughts, setThoughts] = useState<ThoughtStep[]>(MOCK_THOUGHTS);
  const [tools, setTools] = useState<ToolCall[]>(MOCK_TOOLS);
  const [phaseProgress, setPhaseProgress] = useState(0);
  const [cycleCount, setCycleCount] = useState(0);
  const [activeThoughtIdx, setActiveThoughtIdx] = useState<number>(-1);

  const phases: CyclePhase[] = ["perception", "reasoning", "action", "reflection"];

  const runCycle = async () => {
    if (isRunning) return;
    setIsRunning(true);
    setCycleCount(c => c + 1);

    for (let i = 0; i < phases.length; i++) {
      const phase = phases[i];
      setCurrentPhase(phase);
      setPhaseProgress(0);

      const duration = PHASES[phase].duration;
      const steps = Math.ceil(duration / 200);

      for (let s = 0; s < steps; s++) {
        await new Promise(r => setTimeout(r, 200));
        setPhaseProgress(((s + 1) / steps) * 100);
        if (i === 1 && s === Math.floor(steps * 0.5)) setActiveThoughtIdx(2);
        if (i === 1 && s === Math.floor(steps * 0.85)) setActiveThoughtIdx(3);
        if (i === 2 && s === Math.floor(steps * 0.7)) setActiveThoughtIdx(5);
        if (i === 3 && s === Math.floor(steps * 0.5)) setActiveThoughtIdx(6);
      }
    }

    setCurrentPhase("idle");
    setIsRunning(false);
    setActiveThoughtIdx(-1);
    setPhaseProgress(0);
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "20px" }}>
      {/* Header */}
      <div className="card card-elevated">
        <div style={{
          display: "flex", justifyContent: "space-between", alignItems: "center",
          padding: "18px 20px",
        }}>
          <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
            <div style={{
              width: "48px", height: "48px", borderRadius: "var(--radius-lg)",
              background: "linear-gradient(135deg, #6366f1, #a78bfa)",
              display: "flex", alignItems: "center", justifyContent: "center",
              boxShadow: "0 4px 16px rgba(99,102,241,0.3)",
            }}>
              <Brain size="24" color="white" />
            </div>
            <div>
              <h3 style={{ margin: 0, fontSize: "18px" }}>认知循环引擎</h3>
              <p style={{ margin: "4px 0 0 0", fontSize: "13px", color: "var(--text-secondary)" }}>
                AgentOS 核心推理循环：感知 → 推理 → 行动 → 反思
                {cycleCount > 0 && <span style={{ marginLeft: "10px", color: "var(--primary-color)", fontWeight: 600 }}>· 已完成 {cycleCount} 轮</span>}
              </p>
            </div>
          </div>

          <div style={{ display: "flex", gap: "10px" }}>
            <button
              className={`btn ${isRunning ? "btn-danger" : "btn-primary"} btn-lg`}
              onClick={isRunning ? () => { setIsRunning(false); setCurrentPhase("idle"); } : runCycle}
            >
              {isRunning ? <><Pause size={16} /> 停止</> : <><Play size={16} /> 运行循环</>}
            </button>
            <button className="btn btn-secondary btn-lg" onClick={() => { setThoughts(MOCK_THOUGHTS); setTools(MOCK_TOOLS); setCycleCount(0); }}>
              <RotateCcw size={16} /> 重置
            </button>
          </div>
        </div>
      </div>

      {/* Circular Phase Diagram */}
      <div className="card card-elevated">
        <div style={{
          display: "flex", alignItems: "center", justifyContent: "center",
          padding: "40px 20px", position: "relative",
        }}>
          {/* Center */}
          <div style={{
            width: "120px", height: "120px", borderRadius: "50%",
            background: isRunning ? PHASES[currentPhase]?.gradient || "var(--bg-tertiary)" : "var(--bg-tertiary)",
            display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center",
            transition: "background 0.5s ease-out", boxShadow: `0 8px 32px ${(PHASES[currentPhase]?.color || "#94a3b8")}25`,
            zIndex: 2,
          }}>
            {isRunning ? (
              (() => {
                const PhaseIcon = PHASES[currentPhase]?.icon || Clock;
                return (
                  <>
                    <PhaseIcon size={32} color="white" />
                    <span style={{ color: "white", fontSize: "13px", fontWeight: 600, marginTop: "6px" }}>
                      {(PHASES[currentPhase]?.title.split(" ")[0]) || "待机"}
                    </span>
                    <span style={{ color: "rgba(255,255,255,0.75)", fontSize: "11px" }}>
                      {Math.round(phaseProgress)}%
                    </span>
                  </>
                );
              })()
            ) : (
              <>
                <Sparkles size={32} color="var(--text-muted)" />
                <span style={{ color: "var(--text-muted)", fontSize: "13px", marginTop: "6px" }}>待机中</span>
              </>
            )}
          </div>

          {/* Orbiting Phases */}
          {phases.map((phase, idx) => {
            const data = PHASES[phase];
            const angle = (idx * 90 - 90) * (Math.PI / 180);
            const radius = 160;
            const x = Math.cos(angle) * radius;
            const y = Math.sin(angle) * radius;
            const isActive = currentPhase === phase && isRunning;

            return (
              <div
                key={phase}
                style={{
                  position: "absolute",
                  left: `calc(50% + ${x}px)`,
                  top: `calc(50% + ${y}px)`,
                  transform: "translate(-50%, -50%)",
                  transition: "all 0.5s cubic-bezier(0.34, 1.56, 0.64, 1)",
                  transformOrigin: `${-x}px ${-y}px`,
                  zIndex: isActive ? 3 : 1,
                }}
              >
                <div style={{
                  width: isActive ? "72px" : "60px",
                  height: isActive ? "72px" : "60px",
                  borderRadius: "50%",
                  background: isActive ? data.gradient : "var(--bg-secondary)",
                  border: `2px solid ${isActive ? data.color : "var(--border-subtle)"}`,
                  display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center",
                  boxShadow: isActive ? `0 4px 20px ${data.color}40` : "none",
                  transition: "all 0.35s ease-out",
                  cursor: "pointer",
                }}>
                  <data.icon size={isActive ? 22 : 16} color={isActive ? "white" : data.color} />
                  <span style={{
                    fontSize: isActive ? "11px" : "9px",
                    color: isActive ? "rgba(255,255,255,0.9)" : "var(--text-muted)",
                    fontWeight: isActive ? 600 : 400,
                    marginTop: "2px",
                  }}>{data.title.split(" ")[0]}</span>
                </div>
                {isActive && (
                  <div style={{
                    position: "absolute", bottom: "-28px", left: "50%", transform: "translateX(-50%)",
                    whiteSpace: "nowrap", fontSize: "12px", fontWeight: 600, color: data.color,
                  }}>
                    <ArrowRight size={14} style={{ display: "inline", verticalAlign: "middle", marginRight: "4px" }} />
                    {data.title.split(" ")[1]}
                  </div>
                )}
              </div>
            );
          })}
        </div>
      </div>

      {/* Two Column Layout: Thoughts + Tools */}
      <div className="grid-2">
        {/* Thought Chain */}
        <div className="card card-elevated">
          <h3 className="card-title"><Lightbulb size={18} /> 思维链追踪</h3>
          <div style={{ display: "flex", flexDirection: "column", gap: "6px", maxHeight: "420px", overflowY: "auto" }}>
            {thoughts.map((thought, idx) => {
              const data = PHASES[thought.phase];
              const IconComp = data.icon;
              const isActive = activeThoughtIdx === idx && isRunning;
              return (
                <div
                  key={thought.id}
                  onClick={() => setActiveThoughtIdx(isActive ? -1 : idx)}
                  style={{
                    display: "flex", gap: "12px", padding: "12px 14px",
                    borderRadius: "var(--radius-md)", cursor: "pointer",
                    background: isActive ? `${data.color}15` : "transparent",
                    borderLeft: `3px solid ${isActive ? data.color : data.color}40`,
                    transition: "all var(--transition-fast)",
                    animation: !isRunning ? `staggerFadeIn 0.3s ease-out ${idx * 60}ms both` : undefined,
                  }}
                >
                  <div style={{
                    width: "30px", height: "30px", borderRadius: "var(--radius-sm)",
                    background: isActive ? data.gradient : `${data.color}15`,
                    display: "flex", alignItems: "center", justifyContent: "center",
                    flexShrink: 0,
                  }}>
                    <IconComp size={14} color={isActive ? "white" : data.color} />
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{ fontSize: "13px", fontWeight: 500, lineHeight: 1.4 }}>{thought.content}</div>
                    {(isActive || thought.detail) && (
                      <div style={{
                        fontSize: "11.5px", color: "var(--text-muted)", marginTop: "4px",
                        fontFamily: "'JetBrains Mono', monospace",
                      }}>{thought.detail}</div>
                    )}
                  </div>
                  <div style={{
                    fontSize: "10px", color: "var(--text-muted)", flexShrink: 0,
                    whiteSpace: "nowrap",
                  }}>
                    {thought.timestamp.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit', second: '2-digit' })}
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {/* Tool Calls */}
        <div className="card card-elevated">
          <h3 className="card-title"><Wrench size={18} /> 工具调用链</h3>
          <div style={{ display: "flex", flexDirection: "column", gap: "10px" }}>
            {tools.map((tool, idx) => (
              <div key={tool.id} style={{
                padding: "14px", borderRadius: "var(--radius-md)",
                background: "var(--bg-tertiary)", border: "1px solid var(--border-subtle)",
                animation: `staggerFadeIn 0.35s ease-out ${idx * 120}ms both`,
              }}>
                <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "8px" }}>
                  <div style={{
                    width: "32px", height: "32px", borderRadius: "var(--radius-sm)",
                    background: tool.status === "success" ? "linear-gradient(135deg,#22c55e,#4ade80)"
                      : tool.status === "error" ? "linear-gradient(135deg,#ef4444,#f87171)"
                      : tool.status === "running" ? "linear-gradient(135deg,#3b82f6,#60a5fa)"
                      : "var(--bg-primary)",
                    display: "flex", alignItems: "center", justifyContent: "center",
                    flexShrink: 0,
                  }}>
                    {tool.status === "success" ? <CheckCircle2 size="16" color="white" />
                      : tool.status === "error" ? <AlertCircle size="16" color="white" />
                      : <FileText size="16" color={tool.status === "running" ? "white" : "var(--text-muted)"} />}
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                      <span style={{ fontWeight: 600, fontSize: "13.5px", fontFamily: "'JetBrains Mono', monospace" }}>{tool.name}</span>
                      <span className={`tag ${
                        tool.status === "success" ? "status-running"
                          : tool.status === "error" ? "status-stopped"
                          : ""
                      }`} style={{
                        fontSize: "10.5px", padding: "2px 8px",
                        background: tool.status === "success" ? "rgba(34,197,94,0.1)" : tool.status === "error" ? "rgba(239,68,68,0.1)" : "rgba(59,130,246,0.1)",
                        color: tool.status === "success" ? "#22c55e" : tool.status === "error" ? "#ef4444" : "#3b82f6",
                      }}>
                        {tool.status === "success" ? "完成" : tool.status === "error" ? "失败" : "运行中"}
                      </span>
                    </div>
                    {tool.duration && (
                      <span style={{ fontSize: "11px", color: "var(--text-muted)" }}>
                        {(tool.duration / 1000).toFixed(1)}s
                      </span>
                    )}
                  </div>
                </div>

                {tool.input && (
                  <div style={{ fontSize: "11.5px", color: "var(--text-secondary)", fontFamily: "'JetBrains Mono', monospace", wordBreak: "break-all" }}>
                    <span style={{ color: "var(--text-muted)" }}>→ </span>{tool.input}
                  </div>
                )}
                {tool.output && (
                  <div style={{ fontSize: "11.5px", color: "var(--text-secondary)", fontFamily: "'JetBrains Mono', monospace", wordBreak: "break-all" }}>
                    <span style={{ color: "var(--text-muted)" }}>← </span>{tool.output}
                  </div>
                )}
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};

export default CognitiveLoop;
