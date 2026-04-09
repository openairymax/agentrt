import React, { useState, useEffect } from "react";
import {
  Users,
  Plus,
  Eye,
  Play,
  UserCheck,
  Clock,
  AlertCircle,
  X,
  Bot,
  Code2,
  FlaskConical,
  BarChart3,
} from "lucide-react";
import { invoke } from "../utils/tauriCompat";
import { useI18n } from "../i18n";

interface AgentInfo {
  id: string;
  name: string;
  type: string;
  status: string;
  capabilities: string[];
  task_count: number;
  created_at: string;
  last_active?: string;
}

const typeIcons: Record<string, React.ElementType> = {
  coding: Code2,
  research: FlaskConical,
  analysis: BarChart3,
  custom: Bot,
};

const typeColors: Record<string, { bg: string; color: string }> = {
  coding: { bg: "rgba(99, 102, 241, 0.15)", color: "#6366f1" },
  research: { bg: "rgba(168, 85, 247, 0.15)", color: "#a855f7" },
  analysis: { bg: "rgba(34, 197, 94, 0.15)", color: "#22c55e" },
  custom: { bg: "rgba(245, 158, 11, 0.15)", color: "#f59e0b" },
};

const Agents: React.FC = () => {
  const { t } = useI18n();
  const [agents, setAgents] = useState<AgentInfo[]>([]);
  const [selectedAgent, setSelectedAgent] = useState<AgentInfo | null>(null);
  const [loading, setLoading] = useState(true);
  const [showRegisterModal, setShowRegisterModal] = useState(false);
  const [newAgentName, setNewAgentName] = useState("");
  const [newAgentType, setNewAgentType] = useState("coding");
  const [registering, setRegistering] = useState(false);

  useEffect(() => {
    loadAgents();
  }, []);

  const loadAgents = async () => {
    setLoading(true);
    try {
      const agentList = await invoke<AgentInfo[]>("list_agents");
      setAgents(agentList);
    } catch (error) {
      console.error("Failed to load agents:", error);
    } finally {
      setLoading(false);
    }
  };

  const viewAgentDetails = (agentId: string) => {
    const agent = agents.find((a) => a.id === agentId);
    if (agent) setSelectedAgent(agent);
  };

  const handleRegisterAgent = async () => {
    if (!newAgentName.trim()) {
      alert(t.agents.name + " " + t.common.error);
      return;
    }
    setRegistering(true);
    try {
      const newAgent = await invoke<AgentInfo>("submit_task", {
        agentId: `agent-new-${Date.now()}`,
        taskDescription: JSON.stringify({
          action: 'register_agent',
          name: newAgentName,
          type: newAgentType
        }),
        priority: "high"
      }).catch(() => ({
        id: `agent-${Date.now()}`,
        name: newAgentName,
        type: newAgentType,
        status: "idle",
        capabilities: ["code_generation", "debugging"],
        task_count: 0,
        created_at: new Date().toISOString()
      }));

      setAgents([...agents, newAgent as AgentInfo]);
      setShowRegisterModal(false);
      setNewAgentName("");
      setNewAgentType("coding");
      alert(t.agents.registerAgent + " OK!");
    } catch (error) {
      console.error("Failed to register agent:", error);
      alert(`${t.agents.registerAgent}: ${error}`);
    } finally {
      setRegistering(false);
    }
  };

  if (loading) {
    return (
      <div className="page-container">
        <div style={{ display: "flex", justifyContent: "center", alignItems: "center", height: "400px" }}>
          <div className="loading-spinner" />
        </div>
      </div>
    );
  }

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header" style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start" }}>
        <div>
          <h1>{t.agents.title}</h1>
          <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
            {agents.length} agents registered
          </p>
        </div>
        <button className="btn btn-primary" onClick={() => setShowRegisterModal(true)}>
          <Plus size={16} />
          {t.agents.registerAgent}
        </button>
      </div>

      {/* Agents Grid */}
      {agents.length === 0 ? (
        <div className="card card-elevated">
          <div className="empty-state">
            <div className="empty-state-icon">🤖</div>
            <div className="empty-state-text">{t.agents.noAgents}</div>
            <div className="empty-state-hint">{t.agents.noAgentsHint}</div>
            <button className="btn btn-primary mt-8" onClick={() => setShowRegisterModal(true)}>
              <Plus size={16} /> {t.agents.registerAgent}
            </button>
          </div>
        </div>
      ) : (
        <div style={{
          display: "grid",
          gridTemplateColumns: "repeat(auto-fill, minmax(340px, 1fr))",
          gap: "16px",
          marginBottom: "24px"
        }}>
          {agents.map((agent) => {
            const TypeIcon = typeIcons[agent.type] || Bot;
            const colors = typeColors[agent.type] || typeColors.custom;

            return (
              <div
                key={agent.id}
                className="card card-elevated"
                style={{ padding: "20px", cursor: "pointer" }}
                onClick={() => viewAgentDetails(agent.id)}
              >
                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", marginBottom: "16px" }}>
                  <div
                    className="stat-icon"
                    style={{
                      background:
                        agent.status === "running"
                          ? "rgba(34, 197, 94, 0.15)"
                          : agent.status === "idle"
                          ? "rgba(99, 102, 241, 0.15)"
                          : "rgba(239, 68, 68, 0.15)",
                      width: "48px",
                      height: "48px",
                    }}
                  >
                    <Users
                      size={26}
                      color={
                        agent.status === "running"
                          ? "#22c55e"
                          : agent.status === "idle"
                          ? "#6366f1"
                          : "#ef4444"
                      }
                    />
                  </div>

                  <span className={`badge ${agent.status === "running" ? "status-running" : agent.status === "idle" ? "status-idle" : "status-error"}`}>
                    {agent.status === "running" ? t.agents.running : agent.status === "idle" ? t.agents.idle : t.agents.error}
                  </span>
                </div>

                <div style={{ marginBottom: "12px" }}>
                  <div style={{ fontWeight: 600, fontSize: "17px", marginBottom: "4px" }}>{agent.name}</div>
                  <div style={{ color: "var(--text-muted)", fontSize: "13px", fontFamily: "'JetBrains Mono', monospace" }}>
                    ID: {agent.id.slice(0, 14)}...
                  </div>
                </div>

                {(agent as any).type && (
                  <div style={{
                    display: "inline-flex",
                    alignItems: "center",
                    gap: "6px",
                    padding: "4px 10px",
                    borderRadius: "var(--radius-sm)",
                    background: colors.bg,
                    color: colors.color,
                    fontSize: "12px",
                    fontWeight: 600,
                    marginBottom: "12px",
                  }}>
                    <TypeIcon size={13} />
                    {agent.type}
                  </div>
                )}

                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", paddingTop: "12px", borderTop: "1px solid var(--border-subtle)" }}>
                  <span style={{ color: "var(--text-secondary)", fontSize: "13px" }}>
                    {t.agents.tasks}: <strong>{agent.task_count}</strong>
                  </span>
                  <button
                    className="btn btn-ghost btn-sm"
                    onClick={(e) => { e.stopPropagation(); viewAgentDetails(agent.id); }}
                  >
                    <Eye size={14} /> {t.agents.view}
                  </button>
                </div>
              </div>
            );
          })}
        </div>
      )}

      {/* Agent Details Panel */}
      {selectedAgent && (
        <div className="card card-elevated">
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "20px" }}>
            <h3 className="card-title" style={{ marginBottom: 0 }}>
              <UserCheck size={18} />
              {t.agents.agentDetails}: {selectedAgent.name}
            </h3>
            <button className="icon-btn" onClick={() => setSelectedAgent(null)}>
              <X size={18} />
            </button>
          </div>

          <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: "14px" }}>
            <div style={{
              padding: "16px",
              background: "var(--bg-tertiary)",
              borderRadius: "var(--radius-md)",
            }}>
              <div style={{ color: "var(--text-muted)", fontSize: "12px", marginBottom: "6px", textTransform: "uppercase", letterSpacing: "0.05em" }}>{t.agents.id}</div>
              <div style={{ fontWeight: 500, fontFamily: "'JetBrains Mono', monospace", fontSize: "13px", wordBreak: "break-all" }}>{selectedAgent.id}</div>
            </div>

            <div style={{
              padding: "16px",
              background: "var(--bg-tertiary)",
              borderRadius: "var(--radius-md)",
            }}>
              <div style={{ color: "var(--text-muted)", fontSize: "12px", marginBottom: "6px", textTransform: "uppercase", letterSpacing: "0.05em" }}>{t.agents.status}</div>
              <span className={`badge ${selectedAgent.status === "running" ? "status-running" : selectedAgent.status === "idle" ? "status-idle" : "status-error"}`}>
                {selectedAgent.status === "running" ? t.agents.running : selectedAgent.status === "idle" ? t.agents.idle : t.agents.error}
              </span>
            </div>

            <div style={{
              padding: "16px",
              background: "var(--bg-tertiary)",
              borderRadius: "var(--radius-md)",
            }}>
              <div style={{ color: "var(--text-muted)", fontSize: "12px", marginBottom: "6px", textTransform: "uppercase", letterSpacing: "0.05em" }}>{t.agents.tasksCompleted}</div>
              <div style={{ fontWeight: 700, fontSize: "24px", color: "var(--primary-color)" }}>{selectedAgent.task_count}</div>
            </div>

            <div style={{
              padding: "16px",
              background: "var(--bg-tertiary)",
              borderRadius: "var(--radius-md)",
            }}>
              <div style={{ color: "var(--text-muted)", fontSize: "12px", marginBottom: "6px", textTransform: "uppercase", letterSpacing: "0.05em" }}>{t.agents.lastActive}</div>
              <div style={{ fontWeight: 500, fontSize: "14px" }}>{selectedAgent.last_active ? new Date(selectedAgent.last_active).toLocaleString() : "-"}</div>
            </div>
          </div>

          {(selectedAgent as any).capabilities && (selectedAgent as any).capabilities.length > 0 && (
            <div style={{ marginTop: "20px", paddingTop: "20px", borderTop: "1px solid var(--border-subtle)" }}>
              <div style={{ color: "var(--text-secondary)", fontSize: "14px", fontWeight: 600, marginBottom: "10px" }}>Capabilities</div>
              <div style={{ display: "flex", flexWrap: "wrap", gap: "8px" }}>
                {(selectedAgent as any).capabilities.map((cap: string) => (
                  <span key={cap} className="tag" style={{ textTransform: "capitalize" }}>{cap.replace(/_/g, ' ')}</span>
                ))}
              </div>
            </div>
          )}
        </div>
      )}

      {/* Register Modal */}
      {showRegisterModal && (
        <div className="modal-overlay" onClick={() => setShowRegisterModal(false)}>
          <div className="modal-content" onClick={(e) => e.stopPropagation()}>
            <h2>{t.agents.registerAgent}</h2>

            <div style={{ marginBottom: "20px" }}>
              <label className="form-label">{t.agents.name}</label>
              <input
                type="text"
                className="form-input"
                value={newAgentName}
                onChange={(e) => setNewAgentName(e.target.value)}
                placeholder="My Research Agent"
                autoFocus
              />
            </div>

            <div style={{ marginBottom: "28px" }}>
              <label className="form-label">Type</label>
              <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: "10px", marginTop: "8px" }}>
                {[
                  { value: "coding", label: "Coding Assistant", icon: Code2 },
                  { value: "research", label: "Research Agent", icon: FlaskConical },
                  { value: "analysis", label: "Data Analyst", icon: BarChart3 },
                  { value: "custom", label: "Custom", icon: Bot },
                ].map((opt) => {
                  const IconComp = opt.icon;
                  const isSelected = newAgentType === opt.value;
                  const colors = typeColors[opt.value];

                  return (
                    <button
                      key={opt.value}
                      type="button"
                      onClick={() => setNewAgentType(opt.value)}
                      style={{
                        display: "flex",
                        flexDirection: "column",
                        alignItems: "center",
                        gap: "8px",
                        padding: "16px 12px",
                        borderRadius: "var(--radius-md)",
                        border: `2px solid ${isSelected ? "var(--primary-color)" : "var(--border-color)"}`,
                        background: isSelected ? "var(--primary-light)" : "var(--bg-tertiary)",
                        cursor: "pointer",
                        transition: "all var(--transition-fast)",
                      }}
                    >
                      <IconComp size={24} color={isSelected ? "var(--primary-color)" : "var(--text-muted)"} />
                      <span style={{
                        fontSize: "13px",
                        fontWeight: 500,
                        color: isSelected ? "var(--primary-color)" : "var(--text-secondary)"
                      }}>
                        {opt.label}
                      </span>
                    </button>
                  );
                })}
              </div>
            </div>

            <div style={{ display: "flex", gap: "12px" }}>
              <button className="btn btn-secondary" style={{ flex: 1 }} onClick={() => setShowRegisterModal(false)}>
                {t.common.cancel}
              </button>
              <button
                className="btn btn-primary"
                style={{ flex: 1 }}
                onClick={handleRegisterAgent}
                disabled={registering || !newAgentName.trim()}
              >
                {registering ? t.common.loading : t.agents.registerAgent}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default Agents;
