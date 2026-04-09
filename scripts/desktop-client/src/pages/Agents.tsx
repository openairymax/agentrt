import React, { useState, useEffect } from "react";
import {
  Users,
  Plus,
  Eye,
  Play,
  UserCheck,
  Clock,
  AlertCircle,
} from "lucide-react";
import { invoke } from "@tauri-apps/api/core";

interface AgentInfo {
  id: string;
  name: string;
  status: string;
  task_count: number;
  last_active?: string;
}

const Agents: React.FC = () => {
  const [agents, setAgents] = useState<AgentInfo[]>([]);
  const [selectedAgent, setSelectedAgent] = useState<AgentInfo | null>(null);
  const [loading, setLoading] = useState(true);

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

  const viewAgentDetails = async (agentId: string) => {
    try {
      const details = await invoke<AgentInfo>("get_agent_details", { agentId });
      setSelectedAgent(details);
    } catch (error) {
      console.error("Failed to get agent details:", error);
    }
  };

  return (
    <div>
      <div className="card" style={{ marginBottom: "20px", display: "flex", justifyContent: "space-between", alignItems: "center" }}>
        <h3 className="card-title" style={{ marginBottom: 0 }}>
          <Users size={20} />
          Agent Management
        </h3>
        <button className="btn btn-primary">
          <Plus size={16} />
          Create Agent
        </button>
      </div>

      <div className="grid-2">
        {/* Agent List */}
        <div className="card">
          <h3 className="card-title">Registered Agents ({agents.length})</h3>

          {loading ? (
            <div style={{ textAlign: "center", padding: "40px" }}>
              <div className="loading-spinner" />
            </div>
          ) : agents.length === 0 ? (
            <div className="empty-state">
              <div className="empty-state-icon">🤖</div>
              <div className="empty-state-text">No agents registered</div>
              <div className="empty-state-hint">
                Create your first AI agent to get started
              </div>
            </div>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
              {agents.map((agent) => (
                <div
                  key={agent.id}
                  onClick={() => viewAgentDetails(agent.id)}
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center",
                    padding: "16px",
                    background:
                      selectedAgent?.id === agent.id
                        ? "rgba(59, 130, 246, 0.1)"
                        : "var(--bg-tertiary)",
                    borderRadius: "10px",
                    cursor: "pointer",
                    border: selectedAgent?.id === agent.id
                      ? "1px solid var(--primary-color)"
                      : "1px solid transparent",
                    transition: "all 0.2s ease",
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                    <UserCheck size={24} color={
                      agent.status === "running" ? "#10b981" :
                      agent.status === "idle" ? "#f59e0b" :
                      "#ef4444"
                    } />
                    <div>
                      <div style={{ fontWeight: 600 }}>{agent.name}</div>
                      <div style={{ fontSize: "12px", color: "var(--text-muted)" }}>
                        ID: {agent.id}
                      </div>
                    </div>
                  </div>

                  <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>
                    <span
                      className={`status-badge ${
                        agent.status === "running"
                          ? "status-running"
                          : agent.status === "idle"
                          ? "status-warning"
                          : "status-stopped"
                      }`}
                    >
                      {agent.status}
                    </span>
                    <div style={{ textAlign: "right" }}>
                      <div style={{ fontSize: "13px", color: "var(--text-secondary)" }}>
                        {agent.task_count} tasks
                      </div>
                      {agent.last_active && (
                        <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>
                          <Clock size={11} style={{ display: "inline", marginRight: 4 }} />
                          Active just now
                        </div>
                      )}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Agent Details Panel */}
        <div className="card">
          <h3 className="card-title">
            <Eye size={20} />
            Agent Details
          </h3>

          {!selectedAgent ? (
            <div className="empty-state">
              <div className="empty-state-icon">👆</div>
              <div className="empty-state-text">Select an agent to view details</div>
              <div className="empty-state-hint">
                Click on an agent from the list to see more information
              </div>
            </div>
          ) : (
            <div>
              <div style={{ marginBottom: "24px" }}>
                <div style={{ fontSize: "24px", fontWeight: 700, marginBottom: "4px" }}>
                  {selectedAgent.name}
                </div>
                <div style={{ color: "var(--text-muted)", fontSize: "14px" }}>
                  {selectedAgent.id}
                </div>
              </div>

              <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
                <DetailRow label="Status" value={selectedAgent.status} />
                <DetailRow label="Active Tasks" value={`${selectedAgent.task_count}`} />
                <DetailRow
                  label="Last Active"
                  value={selectedAgent.last_active ? "Just now" : "Never"}
                />

                <div style={{ marginTop: "12px", paddingTop: "16px", borderTop: "1px solid var(--border-color)" }}>
                  <button
                    className="btn btn-success"
                    style={{ width: "100%" }}
                    onClick={() => window.location.href = `/tasks?agent=${selectedAgent.id}`}
                  >
                    <Play size={16} />
                    Submit Task to this Agent
                  </button>
                </div>

                <div style={{ marginTop: "8px" }}>
                  <button className="btn btn-secondary" style={{ width: "100%" }}>
                    <Eye size={16} />
                    View Full Configuration
                  </button>
                </div>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Quick Stats */}
      <div className="grid-3" style={{ marginTop: "20px" }}>
        <div className="stat-card">
          <div className="stat-value">{agents.length}</div>
          <div className="stat-label">Total Agents</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{agents.filter(a => a.status === "running").length}</div>
          <div className="stat-label">Running</div>
        </div>
        <div className="stat-card">
          <div className="stat-value">{agents.reduce((sum, a) => sum + a.task_count, 0)}</div>
          <div className="stat-label">Total Tasks Completed</div>
        </div>
      </div>
    </div>
  );
};

function DetailRow({ label, value }: { label: string; value: string }) {
  return (
    <div
      style={{
        display: "flex",
        justifyContent: "space-between",
        padding: "8px 0",
        borderBottom: "1px solid var(--border-color)",
      }}
    >
      <span style={{ color: "var(--text-secondary)", fontSize: "14px" }}>{label}</span>
      <span style={{ fontWeight: 500, fontSize: "14px" }}>{value}</span>
    </div>
  );
}

export default Agents;
