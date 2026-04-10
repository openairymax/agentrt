import React, { useState, useEffect } from "react";
import {
  Database,
  Brain,
  Clock,
  Layers,
  Search,
  Plus,
  Trash2,
  ExternalLink,
  Zap,
  FileText,
  MessageSquare,
  Cpu,
  Shield,
  ChevronRight,
  ArrowUpRight,
  TrendingUp,
  Hash,
  Eye,
} from "lucide-react";

interface MemoryEntry {
  id: string;
  type: "conversation" | "fact" | "skill" | "preference" | "error" | "observation";
  content: string;
  timestamp: Date;
  source?: string;
  relevance: number;
  tokens: number;
  embedding?: number[];
}

interface ContextWindow {
  totalTokens: number;
  maxTokens: number;
  usedPercent: number;
  breakdown: { system: number; history: number; tools: number; output: number };
}

const MEMORY_TYPES = [
  { key: "conversation", icon: MessageSquare, color: "#6366f1", label: "对话记忆", desc: "多轮对话上下文" },
  { key: "fact", icon: Database, color: "#22c55e", label: "事实记忆", desc: "已确认的知识" },
  { key: "skill", icon: Zap, color: "#f59e0b", label: "技能记忆", desc: "习得的能力模式" },
  { key: "preference", icon: Shield, color: "#a855f7", label: "偏好记忆", desc: "用户习惯与偏好" },
  { key: "error", icon: FileText, color: "#ef4444", label: "错误记忆", desc: "失败经验教训" },
  { key: "observation", icon: Eye, color: "#06b6d4", label: "观察记忆", desc: "环境状态感知" },
];

const MOCK_MEMORIES: MemoryEntry[] = [
  { id: "m1", type: "conversation", content: "用户偏好使用中文进行交互，技术术语可保留英文", timestamp: new Date(Date.now() - 300000), source: "chat-003", relevance: 0.95, tokens: 18 },
  { id: "m2", type: "fact", content: "AgentOS 使用 Docker Compose 管理服务，开发模式端口 18789", timestamp: new Date(Date.now() - 600000), source: "system", relevance: 0.88, tokens: 16 },
  { id: "m3", type: "skill", content: "已学会：通过 invoke() 调用 Tauri 后端命令，支持异步返回", timestamp: new Date(Date.now() - 900000), source: "learning", relevance: 0.82, tokens: 14 },
  { id: "m4", type: "preference", content: "用户喜欢简洁的回复格式，避免冗长解释", timestamp: new Date(Date.now() - 1200000), source: "chat-001", relevance: 0.91, tokens: 12 },
  { id: "m5", type: "error", content: "上次启动 PostgreSQL 时因端口冲突失败，需要先检查端口占用", timestamp: new Date(Date.now() - 1800000), source: "service-start", relevance: 0.75, tokens: 20 },
  { id: "m6", type: "observation", content: "系统当前运行在 Windows 平台，x64 架构，16GB 内存", timestamp: new Date(Date.now() - 100000), source: "system-scan", relevance: 0.98, tokens: 15 },
  { id: "m7", type: "fact", content: "OpenAI GPT-4o 支持 128K 上下文窗口，输出最大 16K tokens", timestamp: new Date(Date.now() - 2400000), source: "config", relevance: 0.70, tokens: 15 },
  { id: "m8", type: "skill", content: "已掌握：解析 JSON 格式的任务参数并转换为 Tauri 命令调用", timestamp: new Date(Date.now() - 3600000), source: "learning", relevance: 0.78, tokens: 17 },
  { id: "m9", type: "conversation", content: "用户正在开发一个桌面客户端项目，使用 React + TypeScript + Vite", timestamp: new Date(Date.now() - 4200000), source: "chat-002", relevance: 0.93, tokens: 16 },
  { id: "m10", type: "preference", content: "用户倾向于浅色主题界面设计，注重视觉层次和动效", timestamp: new Date(Date.now() - 4800000), source: "ui-feedback", relevance: 0.85, tokens: 14 },
];

const TokenRing: React.FC<{ value: number; max: number; size?: number; label?: string; color?: string }> = ({
  value, max, size = 60, label, color = "#6366f1"
}) => {
  const pct = Math.min((value / max) * 100, 100);
  const radius = (size - 6) / 2;
  const circ = radius * 2 * Math.PI;
  const offset = circ - (pct / 100) * circ;
  return (
    <div style={{ position: 'relative', width: size, height: size, flexShrink: 0 }}>
      <svg width={size} height={size} style={{ transform: 'rotate(-90deg)' }}>
        <circle cx={size/2} cy={size/2} r={radius} fill="none" stroke="var(--border-subtle)" strokeWidth="4" />
        <circle cx={size/2} cy={size/2} r={radius} fill="none" stroke={color} strokeWidth="4"
          strokeDasharray={circ} strokeDashoffset={offset} strokeLinecap="round"
          style={{ transition: 'stroke-dashoffset 0.8s ease-out' }} />
      </svg>
      <div style={{ position: 'absolute', inset: 0, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
        <span style={{ fontSize: size > 50 ? "14px" : "11px", fontWeight: 700 }}>{value}</span>
        {label && <span style={{ fontSize: "9px", color: "var(--text-muted)" }}>{label}</span>}
      </div>
    </div>
  );
};

const MemorySystem: React.FC = () => {
  const [memories, setMemories] = useState<MemoryEntry[]>(MOCK_MEMORIES);
  const [filterType, setFilterType] = useState<string>("all");
  const [searchQuery, setSearchQuery] = useState("");
  const [activeTab, setActiveTab] = useState<"overview" | "entries" | "context">("overview");
  const [contextWindow, setContextWindow] = useState<ContextWindow>({
    totalTokens: 3842,
    maxTokens: 128000,
    usedPercent: 3.0,
    breakdown: { system: 256, history: 3200, tools: 128, output: 258 },
  });

  useEffect(() => {
    const interval = setInterval(() => {
      setContextWindow(prev => {
        const delta = Math.floor((Math.random() - 0.45) * 60);
        const newTotal = Math.max(800, Math.min(prev.maxTokens * 0.85, prev.totalTokens + delta));
        return {
          ...prev,
          totalTokens: newTotal,
          usedPercent: (newTotal / prev.maxTokens) * 100,
          breakdown: {
            system: 256,
            history: Math.floor(newTotal * 0.82),
            tools: 128,
            output: Math.floor(newTotal * 0.07),
          },
        };
      });
    }, 4000);
    return () => clearInterval(interval);
  }, []);

  const filteredMemories = memories.filter(m => {
    if (filterType !== "all" && m.type !== filterType) return false;
    if (searchQuery && !m.content.toLowerCase().includes(searchQuery.toLowerCase())) return false;
    return true;
  });

  const typeStats = Object.entries(
    memories.reduce((acc, m) => { acc[m.type] = (acc[m.type] || 0) + 1; return acc; }, {} as Record<string, number>)
  ).sort((a, b) => b[1] - a[1]);

  const totalTokens = memories.reduce((s, m) => s + m.tokens, 0);

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "20px" }}>
      {/* Header */}
      <div className="card card-elevated">
        <div style={{
          display: "flex", justifyContent: "space-between", alignItems: "center",
          padding: "20px",
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
              <h3 style={{ margin: 0, fontSize: "18px" }}>认知记忆系统</h3>
              <p style={{ margin: "4px 0 0 0", fontSize: "13px", color: "var(--text-secondary)" }}>
                AgentOS 核心能力：短期/长期记忆 + 向量检索 + 上下文管理
              </p>
            </div>
          </div>

          {/* Quick Stats */}
          <div style={{ display: "flex", gap: "24px" }}>
            <div style={{ textAlign: "center" }}>
              <TokenRing value={memories.length} max={50} size={56} label="条目" color="#6366f1" />
            </div>
            <div style={{ textAlign: "center" }}>
              <TokenRing value={totalTokens} max={5000} size={56} label="tokens" color="#22c55e" />
            </div>
            <div style={{ textAlign: "center" }}>
              <TokenRing
                value={Math.round(contextWindow.usedPercent)}
                max={100}
                size={56}
                label="上下文"
                color={contextWindow.usedPercent > 70 ? "#ef4444" : contextWindow.usedPercent > 40 ? "#f59e0b" : "#22c55e"}
              />
            </div>
          </div>
        </div>
      </div>

      {/* Tabs */}
      <div style={{
        display: "flex", background: "var(--bg-tertiary)",
        borderRadius: "var(--radius-md)", padding: "3px", border: "1px solid var(--border-subtle)",
        width: "fit-content",
      }}>
        {[
          { key: "overview" as const, icon: Brain, label: "总览" },
          { key: "entries" as const, icon: Database, label: "记忆条目" },
          { key: "context" as const, icon: Layers, label: "上下文窗口" },
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

      {activeTab === "overview" && (
        <>
          {/* Memory Type Distribution */}
          <div className="card card-elevated">
            <h3 className="card-title"><Database size={18} /> 记忆类型分布</h3>
            <div style={{
              display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: "12px",
            }}>
              {MEMORY_TYPES.map(mt => {
                const IconComp = mt.icon;
                const count = memories.filter(m => m.type === mt.key).length;
                return (
                  <div key={mt.key} style={{
                    padding: "16px", borderRadius: "var(--radius-md)",
                    background: `${mt.color}08`, border: `1px solid ${mt.color}20`,
                    cursor: "pointer", transition: "all var(--transition-fast)",
                  }}
                  onMouseEnter={(e) => { e.currentTarget.style.transform = "translateY(-2px)"; e.currentTarget.style.boxShadow = `0 4px 12px ${mt.color}15`; }}
                  onMouseLeave={(e) => { e.currentTarget.style.transform = ""; e.currentTarget.style.boxShadow = ""; }}
                  onClick={() => { setActiveTab("entries"); setFilterType(mt.key); }}
                  >
                    <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "10px" }}>
                      <div style={{
                        width: "36px", height: "36px", borderRadius: "var(--radius-sm)",
                        background: mt.color, display: "flex", alignItems: "center", justifyContent: "center",
                        flexShrink: 0,
                      }}>
                        <IconComp size="17" color="white" />
                      </div>
                      <div>
                        <div style={{ fontSize: "20px", fontWeight: 700, color: mt.color }}>{count}</div>
                        <div style={{ fontSize: "12px", color: "var(--text-muted)" }}>{mt.label}</div>
                      </div>
                    </div>
                    <div style={{ fontSize: "11.5px", color: "var(--text-secondary)" }}>{mt.desc}</div>
                  </div>
                );
              })}
            </div>
          </div>

          {/* Memory Flow Diagram */}
          <div className="card card-elevated">
            <h3 className="card-title"><TrendingUp size={18} /> 记忆流转架构</h3>
            <div style={{
              display: "flex", alignItems: "center", justifyContent: "space-between",
              padding: "30px 20px", position: "relative",
            }}>
              {[{ label: "感知输入", icon: Eye, color: "#06b6d4", sub: "观察/对话/事件" },
                { label: "工作记忆", icon: Brain, color: "#6366f1", sub: "短期缓存/推理" },
                { label: "长期存储", icon: Database, color: "#22c55e", sub: "向量索引/持久化" },
                { label: "检索召回", icon: Search, color: "#f59e0b", sub: "语义相似度匹配" },
              ].map((node, i) => (
                <React.Fragment key={node.label}>
                  <div style={{
                    width: "140px", textAlign: "center",
                    animation: `staggerFadeIn 0.4s ease-out ${i * 150}ms both`,
                  }}>
                    <div style={{
                      width: "52px", height: "52px", borderRadius: "var(--radius-lg)",
                      background: node.color, display: "flex", alignItems: "center", justifyContent: "center",
                      margin: "0 auto 8px", boxShadow: `0 4px 14px ${node.color}35`,
                    }}>
                      <node.icon size="24" color="white" />
                    </div>
                    <div style={{ fontWeight: 600, fontSize: "13px" }}>{node.label}</div>
                    <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>{node.sub}</div>
                  </div>
                  {i < 3 && (
                    <ChevronRight size={20} style={{ color: "var(--text-muted)", flexShrink: 0 }} />
                  )}
                </React.Fragment>
              ))}
            </div>
          </div>
        </>
      )}

      {activeTab === "entries" && (
        <div className="card card-elevated">
          <div style={{ display: "flex", gap: "10px", marginBottom: "16px", flexWrap: "wrap", alignItems: "end" }}>
            <div style={{ position: "relative", flex: 1, minWidth: "200px" }}>
              <Search size={14} style={{ position: "absolute", left: "12px", top: "50%", transform: "translateY(-50%)", color: "var(--text-muted)" }} />
              <input
                type="text"
                className="form-input"
                placeholder="搜索记忆内容..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                style={{ paddingLeft: "36px" }}
              />
            </div>
            <select className="form-select" value={filterType} onChange={(e) => setFilterType(e.target.value)} style={{ width: "160px" }}>
              <option value="all">全部类型</option>
              {MEMORY_TYPES.map(mt => <option key={mt.key} value={mt.key}>{mt.label}</option>)}
            </select>
          </div>

          <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
            {filteredMemories.map((mem, idx) => {
              const mt = MEMORY_TYPES.find(m => m.key === mem.type);
              const IconComp = mt?.icon || Database;
              return (
                <div
                  key={mem.id}
                  className="card-hover-lift"
                  style={{
                    padding: "14px 18px", borderRadius: "var(--radius-md)",
                    borderLeft: `3px solid ${mt?.color || "#94a3b8"}`,
                    background: `${(mt?.color || "#94a3b8")}06`,
                    display: "flex", alignItems: "center", gap: "14px",
                    animation: `staggerFadeIn 0.3s ease-out ${idx * 40}ms both`,
                  }}
                >
                  <div style={{
                    width: "34px", height: "34px", borderRadius: "var(--radius-sm)",
                    background: mt?.color, display: "flex", alignItems: "center", justifyContent: "center",
                    flexShrink: 0,
                  }}>
                    <IconComp size="15" color="white" />
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{ fontSize: "13.5px", lineHeight: 1.5, color: "var(--text-primary)" }}>{mem.content}</div>
                    <div style={{ display: "flex", gap: "12px", marginTop: "6px", fontSize: "11px", color: "var(--text-muted)" }}>
                      <span><Hash size={10} style={{ display: "inline", marginRight: "3px" }} />{mem.id}</span>
                      <span>{mem.source}</span>
                      <span>{mem.timestamp.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: "2-digit" })}</span>
                      <span style={{ marginLeft: "auto" }}>{mem.tokens} tokens</span>
                    </div>
                  </div>
                  <div style={{
                    width: "44px", height: "6px", borderRadius: "3px", background: "var(--bg-primary)",
                    overflow: "hidden", flexShrink: 0,
                  }}>
                    <div style={{ width: `${mem.relevance * 100}%`, height: "100%", background: mt?.color, borderRadius: "3px" }} />
                  </div>
                </div>
              );
            })}
            {filteredMemories.length === 0 && (
              <div style={{ textAlign: "center", padding: "48px", color: "var(--text-muted)" }}>
                <Database size={40} style={{ opacity: 0.3, marginBottom: "12px" }} />
                <div>没有匹配的记忆条目</div>
              </div>
            )}
          </div>
        </div>
      )}

      {activeTab === "context" && (
        <>
          <div className="grid-2">
            <div className="card card-elevated">
              <h3 className="card-title"><Layers size={18} /> 上下文窗口状态</h3>
              <div style={{ display: "flex", flexDirection: "column", gap: "20px", alignItems: "center", padding: "20px 0" }}>
                <TokenRing
                  value={Math.round(contextWindow.totalTokens)}
                  max={contextWindow.maxTokens}
                  size={140}
                  label={`/${(contextWindow.maxTokens / 1000).toFixed(0)}K`}
                  color={contextWindow.usedPercent > 70 ? "#ef4444" : contextWindow.usedPercent > 40 ? "#f59e0b" : "#22c55e"}
                />
                <div style={{ textAlign: "center" }}>
                  <div style={{ fontSize: "28px", fontWeight: 700 }}>{contextWindow.usedPercent.toFixed(1)}%</div>
                  <div style={{ fontSize: "13px", color: "var(--text-muted)" }}>上下文占用率</div>
                </div>
              </div>
            </div>

            <div className="card card-elevated">
              <h3 className="card-title"><Cpu size={18} /> Token 分配明细</h3>
              <div style={{ display: "flex", flexDirection: "column", gap: "14px", padding: "8px 0" }}>
                {[
                  { label: "系统提示", value: contextWindow.breakdown.system, color: "#a855f7", max: 2048 },
                  { label: "对话历史", value: contextWindow.breakdown.history, color: "#6366f1", max: 96000 },
                  { label: "工具定义", value: contextWindow.breakdown.tools, color: "#f59e0b", max: 4096 },
                  { label: "输出预留", value: contextWindow.breakdown.output, color: "#22c55e", max: 16384 },
                ].map(item => (
                  <div key={item.label}>
                    <div style={{ display: "flex", justifyContent: "space-between", marginBottom: "6px" }}>
                      <span style={{ fontSize: "13px", fontWeight: 500 }}>{item.label}</span>
                      <span style={{ fontSize: "12.5px", fontFamily: "'JetBrains Mono', monospace", color: item.color }}>
                        {item.value.toLocaleString()} / {(item.max / 1000).toFixed(0)}K
                      </span>
                    </div>
                    <div style={{
                      height: "8px", background: "var(--bg-tertiary)", borderRadius: "4px", overflow: "hidden",
                    }}>
                      <div style={{
                        width: `${(item.value / item.max) * 100}%`, height: "100%",
                        background: item.color, borderRadius: "4px",
                        transition: "width 0.6s cubic-bezier(0.4, 0, 0.2, 1)",
                      }} />
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>

          {/* Context Window Info */}
          <div className="card card-elevated">
            <h3 className="card-title"><Shield size={18} /> 上下文管理策略</h3>
            <div style={{
              display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(280px, 1fr))", gap: "14px",
            }}>
              {[
                { title: "滑动窗口", desc: "保持最近 N 轮对话，自动裁剪超长历史", icon: Clock, color: "#6366f1" },
                { title: "摘要压缩", desc: "对早期对话生成摘要，释放 token 空间", icon: FileText, color: "#22c55e" },
                { title: "优先级遗忘", desc: "按相关性评分淘汰低价值记忆条目", icon: TrendingUp, color: "#f59e0b" },
                { title: "向量召回", desc: "基于语义相似度从长期记忆中检索相关内容", icon: Search, color: "#a855f7" },
              ].map(item => (
                <div key={item.title} style={{
                  padding: "16px", borderRadius: "var(--radius-md)",
                  background: `${item.color}08`, border: `1px solid ${item.color}20`,
                }}>
                  <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "8px" }}>
                    <item.icon size={18} color={item.color} />
                    <span style={{ fontWeight: 600, fontSize: "14px" }}>{item.title}</span>
                  </div>
                  <p style={{ fontSize: "12.5px", color: "var(--text-secondary)", margin: 0, lineHeight: 1.5 }}>{item.desc}</p>
                </div>
              ))}
            </div>
          </div>
        </>
      )}
    </div>
  );
};

export default MemorySystem;
