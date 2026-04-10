import React, { useState, useEffect } from "react";
import {
  Bot,
  Plus,
  Search,
  Filter,
  Cpu,
  Globe,
  Code2,
  Brain,
  Sparkles,
  Zap,
  Shield,
  ChevronRight,
  CheckCircle2,
  XCircle,
  Loader2,
  ExternalLink,
  Clock,
  Activity,
  Star,
} from "lucide-react";
import { invoke } from "../utils/tauriCompat";
import { useI18n } from "../i18n";

interface AgentInfo {
  name: string;
  type: string;
  status: string;
  description: string;
}

const agentTypeConfig: Record<string, { icon: typeof Bot; color: string; gradient: string; bgLight: string; label: string }> = {
  research: { icon: Brain, color: "#6366f1", gradient: "linear-gradient(135deg, #6366f1, #818cf8)", bgLight: "rgba(99,102,241,0.08)", label: "研究型" },
  coding: { icon: Code2, color: "#22c55e", gradient: "linear-gradient(135deg, #22c55e, #4ade80)", bgLight: "rgba(34,197,94,0.08)", label: "编码型" },
  assistant: { icon: Sparkles, color: "#a855f7", gradient: "linear-gradient(135deg, #a855f7, #c084fc)", bgLight: "rgba(168,85,247,0.08)", label: "助手型" },
  system: { icon: Shield, color: "#f59e0b", gradient: "linear-gradient(135deg, #f59e0b, #fbbf24)", bgLight: "rgba(245,158,11,0.08)", label: "系统型" },
};

const Agents: React.FC = () => {
  const { t } = useI18n();
  const [agents, setAgents] = useState<AgentInfo[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState("");
  const [selectedAgent, setSelectedAgent] = useState<AgentInfo | null>(null);
  const [showRegisterModal, setShowRegisterModal] = useState(false);

  useEffect(() => {
    loadAgents();
  }, []);

  const loadAgents = async () => {
    setLoading(true);
    try {
      const data = await invoke<AgentInfo[]>("list_agents");
      setAgents(data || []);
    } catch (error) {
      console.error("Failed to load agents:", error);
    } finally {
      setLoading(false);
    }
  };

  const handleRegister = async (name: string, type: string) => {
    try {
      await invoke("register_agent", { agent_name: name, agent_type: type });
      setShowRegisterModal(false);
      loadAgents();
    } catch (error) {
      alert(`${t.agents.registerFailed}: ${error}`);
    }
  };

  const filteredAgents = agents.filter(
    (agent) =>
      agent.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      agent.type.toLowerCase().includes(searchTerm.toLowerCase()) ||
      (agent.description && agent.description.toLowerCase().includes(searchTerm.toLowerCase()))
  );

  const getAgentConfig = (type: string) => {
    return agentTypeConfig[type] || { icon: Bot, color: "#94a3b8", gradient: "linear-gradient(135deg, #94a3b8, #cbd5e1)", bgLight: "rgba(148,163,184,0.08)", label: type };
  };

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
          <h1>{t.agents.title}</h1>
          <span className={`badge ${agents.length > 0 ? 'status-running' : 'status-stopped'}`}>
            {agents.length} {t.agents.activeAgents.toLowerCase()}
          </span>
        </div>
        <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
          {t.agents.subtitle}
        </p>
      </div>

      {/* Toolbar */}
      <div className="card card-elevated" style={{ marginBottom: "20px" }}>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", flexWrap: "wrap", gap: "12px" }}>
          <div style={{ display: "flex", alignItems: "center", gap: "10px", flex: 1, minWidth: "200px" }}>
            <div style={{ position: "relative", flex: 1, maxWidth: "360px" }}>
              <Search size={15} style={{
                position: "absolute", left: "12px", top: "50%",
                transform: "translateY(-50%)", color: "var(--text-muted)"
              }} />
              <input
                type="text"
                className="form-input"
                placeholder={`${t.agents.searchAgents}...`}
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                style={{ paddingLeft: "38px" }}
              />
            </div>
          </div>
          <button
            className="btn btn-primary"
            onClick={() => setShowRegisterModal(true)}
          >
            <Plus size={16} />
            {t.agents.registerNew}
          </button>
        </div>
      </div>

      {/* Agent Type Stats Bar */}
      <div style={{
        display: "flex", gap: "10px", marginBottom: "20px",
        overflowX: "auto", paddingBottom: "4px",
      }}>
        {Object.entries(agentTypeConfig).map(([typeKey, config]) => {
          const count = agents.filter(a => a.type === typeKey).length;
          const IconComp = config.icon;
          return (
            <div key={typeKey} style={{
              padding: "10px 16px", borderRadius: "var(--radius-md)",
              background: config.bgLight, border: `1px solid ${config.color}20`,
              display: "flex", alignItems: "center", gap: "10px",
              minWidth: "140px", cursor: "pointer",
              transition: "all var(--transition-fast)",
            }}
            onMouseEnter={(e) => { e.currentTarget.style.transform = "translateY(-2px)"; e.currentTarget.style.boxShadow = `0 4px 12px ${config.color}15`; }}
            onMouseLeave={(e) => { e.currentTarget.style.transform = ""; e.currentTarget.style.boxShadow = ""; }}
            >
              <div style={{
                width: "32px", height: "32px", borderRadius: "var(--radius-sm)",
                background: config.gradient, display: "flex", alignItems: "center", justifyContent: "center",
                flexShrink: 0,
              }}>
                <IconComp size={16} color="white" />
              </div>
              <div>
                <div style={{ fontSize: "12px", color: "var(--text-muted)" }}>{config.label}</div>
                <div style={{ fontSize: "17px", fontWeight: 700, color: config.color, lineHeight: 1.2 }}>{count}</div>
              </div>
            </div>
          );
        })}
      </div>

      {/* Main Content Grid */}
      <div style={{ display: "grid", gridTemplateColumns: selectedAgent ? "1fr 340px" : "1fr", gap: "20px", transition: "grid-template-columns 0.35s ease-out" }}>
        {/* Agent Cards Grid */}
        <div>
          {loading ? (
            <div style={{ textAlign: "center", padding: "48px" }}>
              <div className="loading-spinner" />
            </div>
          ) : filteredAgents.length === 0 ? (
            <div className="empty-state">
              <Bot size={56} style={{ opacity: 0.25 }} />
              <div className="empty-state-text">{t.agents.noAgentsRegistered}</div>
              <div className="empty-state-hint">{t.agents.noAgentsHint}</div>
              <button className="btn btn-primary mt-8" onClick={() => setShowRegisterModal(true)}>
                <Plus size={16} /> {t.agents.registerNew}
              </button>
            </div>
          ) : (
            <div style={{
              display: "grid",
              gridTemplateColumns: "repeat(auto-fill, minmax(300px, 1fr))",
              gap: "14px",
            }}>
              {filteredAgents.map((agent, idx) => {
                const config = getAgentConfig(agent.type);
                const IconComp = config.icon;
                const isSelected = selectedAgent?.name === agent.name;

                return (
                  <div
                    key={agent.name}
                    onClick={() => setSelectedAgent(isSelected ? null : agent)}
                    className="card-hover-lift"
                    style={{
                      padding: "20px",
                      borderRadius: "var(--radius-lg)",
                      border: `2px solid`,
                      borderColor: isSelected ? config.color : "var(--border-subtle)",
                      background: isSelected ? `${config.color}06` : "var(--bg-secondary)",
                      position: "relative",
                      overflow: "hidden",
                      cursor: "pointer",
                      animation: `staggerFadeIn 0.35s ease-out ${idx * 60}ms both`,
                      transition: "all var(--transition-fast)",
                    }}
                  >
                    {/* Top gradient accent */}
                    <div style={{
                      position: "absolute", top: 0, left: 0, right: 0, height: "3px",
                      background: config.gradient,
                      opacity: isSelected ? 1 : 0.5,
                    }} />

                    <div style={{ display: "flex", alignItems: "flex-start", gap: "14px" }}>
                      <div style={{
                        width: "46px", height: "46px", borderRadius: "var(--radius-md)",
                        background: config.gradient,
                        display: "flex", alignItems: "center", justifyContent: "center",
                        flexShrink: 0, boxShadow: `0 4px 12px ${config.color}30`,
                      }}>
                        <IconComp size={22} color="white" />
                      </div>

                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "4px" }}>
                          <span style={{ fontWeight: 600, fontSize: "15.5px" }}>{agent.name}</span>
                          {isSelected && <ChevronRight size={14} style={{ color: config.color }} />}
                        </div>

                        <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "6px" }}>
                          <span className="tag" style={{
                            background: config.bgLight,
                            color: config.color,
                            fontWeight: 500,
                            fontSize: "11.5px",
                            padding: "2px 8px",
                          }}>
                            {config.label}
                          </span>
                        </div>

                        {agent.description && (
                          <p style={{
                            fontSize: "13px", color: "var(--text-secondary)",
                            lineHeight: 1.5, margin: 0, display: "-webkit-box",
                            WebkitLineClamp: 2, WebkitBoxOrient: "vertical", overflow: "hidden",
                          }}>
                            {agent.description}
                          </p>
                        )}
                      </div>
                    </div>

                    {/* Bottom status bar */}
                    <div style={{
                      marginTop: "14px", paddingTop: "12px",
                      borderTop: "1px solid var(--border-subtle)",
                      display: "flex", justifyContent: "space-between", alignItems: "center",
                    }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "6px" }}>
                        {agent.status === "active" ? (
                          <>
                            <span style={{
                              width: "8px", height: "8px", borderRadius: "50%",
                              background: "#22c55e", boxShadow: "0 0 6px rgba(34,197,94,0.5)",
                              animation: "statusPulse 2s ease-in-out infinite",
                            }} />
                            <span style={{ fontSize: "12px", color: "#22c55e", fontWeight: 500 }}>运行中</span>
                          </>
                        ) : (
                          <>
                            <span style={{ width: "8px", height: "8px", borderRadius: "50%", background: "#94a3b8" }} />
                            <span style={{ fontSize: "12px", color: "var(--text-muted)" }}>空闲</span>
                          </>
                        )}
                      </div>
                      <Activity size={13} style={{ color: "var(--text-muted)" }} />
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </div>

        {/* Detail Panel - Slides in from right */}
        {selectedAgent && (
          <div className="card card-elevated" style={{
            height: "fit-content",
            position: "sticky",
            top: "88px",
            animation: "slideInRight 0.3s ease-out",
          }}>
            {(() => {
              const config = getAgentConfig(selectedAgent.type);
              const IconComp = config.icon;

              return (
                <>
                  <div style={{
                    padding: "20px", borderBottom: "1px solid var(--border-subtle)",
                    background: config.bgLight, borderRadius: "var(--radius-lg) var(--radius-lg) 0 0",
                  }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                      <div style={{
                        width: "52px", height: "52px", borderRadius: "var(--radius-md)",
                        background: config.gradient,
                        display: "flex", alignItems: "center", justifyContent: "center",
                        boxShadow: `0 4px 16px ${config.color}30`,
                      }}>
                        <IconComp size="26" color="white" />
                      </div>
                      <div>
                        <h3 style={{ margin: 0, fontSize: "17px" }}>{selectedAgent.name}</h3>
                        <span className="tag" style={{ background: config.bgLight, color: config.color, fontSize: "11.5px" }}>
                          {config.label}
                        </span>
                      </div>
                    </div>
                  </div>

                  <div style={{ padding: "20px", display: "flex", flexDirection: "column", gap: "16px" }}>
                    <DetailRow icon={<Star size={14} />} label="类型" value={selectedAgent.type} />
                    <DetailRow icon={<Activity size={14} />} label="状态" value={
                      <span style={{ color: selectedAgent.status === "active" ? "#22c55e" : "var(--text-muted)" }}>
                        {selectedAgent.status === "active" ? "● 运行中" : "○ 空闲"}
                      </span>
                    } />
                    <DetailRow icon={<Clock size={14} />} label="描述" value={selectedAgent.description || "—"} />

                    <div style={{ marginTop: "8px", display: "flex", flexDirection: "column", gap: "8px" }}>
                      <button className="btn btn-primary btn-block">
                        <Zap size={15} /> 启动智能体
                      </button>
                      <button className="btn btn-ghost btn-block">
                        <ExternalLink size={15} /> 查看详情
                      </button>
                    </div>
                  </div>
                </>
              );
            })()}
          </div>
        )}
      </div>

      {/* Register Modal */}
      {showRegisterModal && (
        <RegisterModal
          onClose={() => setShowRegisterModal(false)}
          onRegister={handleRegister}
        />
      )}
    </div>
  );
};

function DetailRow({ icon, label, value }: { icon: React.ReactNode; label: string; value: React.ReactNode }) {
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
      <div style={{ display: "flex", alignItems: "center", gap: "6px", fontSize: "11.5px", color: "var(--text-muted)", textTransform: "uppercase", letterSpacing: "0.05em" }}>
        {icon}
        {label}
      </div>
      <div style={{ fontSize: "14px", fontWeight: 500, paddingLeft: "20px" }}>{value}</div>
    </div>
  );
}

function RegisterModal({ onClose, onRegister }: { onClose: () => void; onRegister: (name: string, type: string) => void }) {
  const { t } = useI18n();
  const [step, setStep] = useState<"info" | "confirm">("info");
  const [agentName, setAgentName] = useState("");
  const [agentType, setAgentType] = useState("research");
  const [submitting, setSubmitting] = useState(false);

  const handleSubmit = async () => {
    if (!agentName.trim()) return;
    if (step === "info") { setStep("confirm"); return; }
    setSubmitting(true);
    try { await onRegister(agentName.trim(), agentType); } finally { setSubmitting(false); }
  };

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal-content" style={{ maxWidth: "520px" }} onClick={(e) => e.stopPropagation()}>
        <div className="modal-header">
          <h2 className="modal-title">{t.agents.registerNew}</h2>
          <button className="modal-close-btn" onClick={onClose}>×</button>
        </div>

        {step === "info" ? (
          <div style={{ padding: "28px", display: "flex", flexDirection: "column", gap: "20px" }}>
            <div style={{ textAlign: "center", marginBottom: "8px" }}>
              <div style={{
                width: "64px", height: "64px", borderRadius: "var(--radius-lg)",
                background: "linear-gradient(135deg, #6366f1, #a78bfa)",
                display: "flex", alignItems: "center", justifyContent: "center",
                margin: "0 auto 12px", boxShadow: "0 8px 24px rgba(99,102,241,0.3)",
              }}>
                <Plus size={28} color="white" />
              </div>
              <p style={{ color: "var(--text-secondary)", fontSize: "14px", margin: 0 }}>
                创建一个新的 AI 智能体，赋予它独特的能力和角色
              </p>
            </div>

            <div className="form-group">
              <label className="form-label">{t.agents.agentName}</label>
              <input
                type="text"
                className="form-input"
                value={agentName}
                onChange={(e) => setAgentName(e.target.value)}
                placeholder="例如：Research Assistant、Code Reviewer..."
                onKeyDown={(e) => e.key === "Enter" && handleSubmit()}
                autoFocus
              />
              <p className="form-help">{t.agents.agentNameHelp}</p>
            </div>

            <div className="form-group">
              <label className="form-label">{t.agents.agentType}</label>
              <div style={{
                display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: "10px",
              }}>
                {Object.entries(agentTypeConfig).map(([key, cfg]) => {
                  const IconComp = cfg.icon;
                  const isActive = agentType === key;
                  return (
                    <div
                      key={key}
                      onClick={() => setAgentType(key)}
                      style={{
                        padding: "14px", borderRadius: "var(--radius-md)",
                        border: `2px solid`, borderColor: isActive ? cfg.color : "var(--border-subtle)",
                        background: isActive ? cfg.bgLight : "var(--bg-tertiary)",
                        cursor: "pointer", transition: "all var(--transition-fast)",
                        display: "flex", alignItems: "center", gap: "10px",
                      }}
                    >
                      <div style={{
                        width: "32px", height: "32px", borderRadius: "var(--radius-sm)",
                        background: isActive ? cfg.gradient : "var(--bg-primary)",
                        display: "flex", alignItems: "center", justifyContent: "center",
                        flexShrink: 0,
                      }}>
                        <IconComp size={15} color={isActive ? "white" : "var(--text-muted)"} />
                      </div>
                      <div>
                        <div style={{ fontSize: "13px", fontWeight: 600, color: isActive ? cfg.color : "var(--text-primary)" }}>{cfg.label}</div>
                        <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>{key}</div>
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>

            <div style={{ display: "flex", gap: "10px", marginTop: "4px" }}>
              <button className="btn btn-secondary btn-lg" onClick={onClose} style={{ flex: 1 }}>
                取消
              </button>
              <button
                className="btn btn-primary btn-lg"
                onClick={handleSubmit}
                disabled={!agentName.trim() || submitting}
                style={{ flex: 1 }}
              >
                下一步 <ChevronRight size={16} />
              </button>
            </div>
          </div>
        ) : (
          <div style={{ padding: "28px", display: "flex", flexDirection: "column", gap: "20px", alignItems: "center", textAlign: "center" }}>
            <CheckCircle2 size={48} color="#6366f1" />
            <div>
              <h3 style={{ margin: "0 0 8px 0" }}>确认注册</h3>
              <p style={{ color: "var(--text-secondary)", fontSize: "14px", margin: 0 }}>
                即将注册以下智能体：
              </p>
            </div>

            <div style={{
              width: "100%", padding: "16px", borderRadius: "var(--radius-md)",
              background: "var(--bg-tertiary)", border: "1px solid var(--border-subtle)",
              textAlign: "left",
            }}>
              <div style={{ display: "grid", gridTemplateColumns: "100px 1fr", gap: "10px", fontSize: "14px" }}>
                <span style={{ color: "var(--text-muted)" }}>名称</span>
                <strong>{agentName}</strong>
                <span style={{ color: "var(--text-muted)" }}>类型</span>
                <span><span className="tag" style={{ background: agentTypeConfig[agentType]?.bgLight, color: agentTypeConfig[agentType]?.color }}>{agentTypeConfig[agentType]?.label}</span></span>
              </div>
            </div>

            <div style={{ display: "flex", gap: "10px", width: "100%" }}>
              <button className="btn btn-secondary btn-lg" onClick={() => setStep("info")} style={{ flex: 1 }}>
                返回修改
              </button>
              <button
                className="btn btn-success btn-lg"
                onClick={handleSubmit}
                disabled={submitting}
                style={{ flex: 1 }}
              >
                {submitting ? <Loader2 size={16} className="spin" /> : <CheckCircle2 size={16} />}
                确认注册
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

export default Agents;
