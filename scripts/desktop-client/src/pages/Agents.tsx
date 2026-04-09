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
import { invoke } from "../utils/tauriCompat";

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

  if (loading) {
    return (
      <div style={{ display: "flex", justifyContent: "center", alignItems: "center", height: "400px" }}>
        <div className="loading-spinner" />
      </div>
    );
  }

  return (
    <div>
      <div className="card">
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "20px" }}>
          <h3 className="card-title" style={{ marginBottom: 0 }}>
            <Users size={20} />
            AI Agents
          </h3>
          <button className="btn btn-primary">
            <Plus size={16} />
            Register Agent
          </button>
        </div>

        {agents.length === 0 ? (
          <div className="empty-state">
            <div className="empty-state-icon">🤖</div>
            <div className="empty-state-text">No agents registered</div>
            <div className="empty-state-hint">
              Register an AI agent to get started
            </div>
          </div>
        ) : (
          <div style={{ display: "grid", gap: "16px" }}>
            {agents.map((agent) => (
              <div
                key={agent.id}
                className="card"
                style={{
                  padding: "16px",
                  display: "flex",
                  justifyContent: "space-between",
                  alignItems: "center",
                  cursor: "pointer",
                }}
                onClick={() => viewAgentDetails(agent.id)}
              >
                <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>
                  <div
                    className="stat-icon"
                    style={{
                      background:
                        agent.status === "running"
                          ? "rgba(16, 185, 129, 0.15)"
                          : agent.status === "idle"
                          ? "rgba(59, 130, 246, 0.15)"
                          : "rgba(239, 68, 68, 0.15)",
                    }}
                  >
                    <Users
                      size={24}
                      color={
                        agent.status === "running"
                          ? "#10b981"
                          : agent.status === "idle"
                          ? "#3b82f6"
                          : "#ef4444"
                      }
                    />
                  </div>
                  <div>
                    <div style={{ fontWeight: 600, fontSize: "16px" }}>{agent.name}</div>
                    <div style={{ color: "var(--text-secondary)", fontSize: "14px" }}>
                      ID: {agent.id} • Tasks: {agent.task_count}
                    </div>
                  </div>
                </div>
                <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>
                  <span
                    className={`status-badge ${
                      agent.status === "running"
                        ? "status-running"
                        : agent.status === "idle"
                        ? "status-idle"
                        : "status-error"
                    }`}
                  >
                    {agent.status}
                  </span>
                  <button className="btn btn-secondary" style={{ padding: "6px 12px" }}>
                    <Eye size={14} />
                    View
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {selectedAgent && (
        <div className="card">
          <h3 className="card-title">
            <UserCheck size={20} />
            Agent Details: {selectedAgent.name}
          </h3>
          <div style={{ display: "grid", gap: "12px" }}>
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <span style={{ color: "var(--text-secondary)" }}>ID:</span>
              <span style={{ fontWeight: 500 }}>{selectedAgent.id}</span>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <span style={{ color: "var(--text-secondary)" }}>Status:</span>
              <span className={`status-badge ${
                selectedAgent.status === "running"
                  ? "status-running"
                  : selectedAgent.status === "idle"
                  ? "status-idle"
                  : "status-error"
              }`}>
                {selectedAgent.status}
              </span>
            </div>
            <div style={{ display: "flex", justifyContent: "space-between" }}>
              <span style={{ color: "var(--text-secondary)" }}>Tasks Completed:</span>
              <span style={{ fontWeight: 500 }}>{selectedAgent.task_count}</span>
            </div>
            {selectedAgent.last_active && (
              <div style={{ display: "flex", justifyContent: "space-between" }}>
                <span style={{ color: "var(--text-secondary)" }}>Last Active:</span>
                <span style={{ fontWeight: 500 }}>
                  {new Date(selectedAgent.last_active).toLocaleString()}
                </span>
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  );
};

export default Agents;