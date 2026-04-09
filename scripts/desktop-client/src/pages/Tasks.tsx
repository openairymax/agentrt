import React, { useState } from "react";
import {
  ClipboardList,
  Plus,
  Send,
  XCircle,
  Clock,
  CheckCircle2,
  Loader2,
  AlertCircle,
} from "lucide-react";
import { invoke } from "@tauri-apps/api/core";

interface TaskInfo {
  id: string;
  agent_id: string;
  status: string;
  progress: number;
  created_at: string;
  updated_at?: string;
}

const Tasks: React.FC = () => {
  const [taskDescription, setTaskDescription] = useState("");
  const [selectedAgent, setSelectedAgent] = useState("");
  const [priority, setPriority] = useState("normal");
  const [tasks, setTasks] = useState<TaskInfo[]>([]);
  const [submitting, setSubmitting] = useState(false);
  const [activeTab, setActiveTab] = useState<"submit" | "history">("submit");

  const handleSubmitTask = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!taskDescription.trim()) {
      alert("Please enter a task description");
      return;
    }

    if (!selectedAgent) {
      alert("Please select an agent");
      return;
    }

    setSubmitting(true);
    try {
      const newTask = await invoke<TaskInfo>("submit_task", {
        agentId: selectedAgent,
        taskDescription: taskDescription.trim(),
        priority,
      });

      setTasks([newTask, ...tasks]);
      setTaskDescription("");
      setActiveTab("history");
    } catch (error) {
      console.error("Failed to submit task:", error);
      alert(`Failed to submit task: ${error}`);
    } finally {
      setSubmitting(false);
    }
  };

  const handleCancelTask = async (taskId: string) => {
    if (!confirm("Are you sure you want to cancel this task?")) return;

    try {
      await invoke("cancel_task", { taskId });
      setTasks(tasks.map((t) =>
        t.id === taskId ? { ...t, status: "cancelled" } : t
      ));
    } catch (error) {
      console.error("Failed to cancel task:", error);
      alert(`Failed to cancel task: ${error}`);
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case "completed":
        return <CheckCircle2 size={16} color="#10b981" />;
      case "running":
        return <Loader2 size={16} color="#3b82f6" className="spin" />;
      case "failed":
        return <XCircle size={16} color="#ef4444" />;
      case "cancelled":
        return <XCircle size={16} color="#f59e0b" />;
      default:
        return <Clock size={16} color="#94a3b8" />;
    }
  };

  return (
    <div>
      {/* Header */}
      <div className="card" style={{ marginBottom: "20px" }}>
        <h3 className="card-title" style={{ marginBottom: 0 }}>
          <ClipboardList size={20} />
          Task Management
        </h3>
      </div>

      {/* Tabs */}
      <div
        style={{
          display: "flex",
          gap: "4px",
          marginBottom: "20px",
          background: "var(--bg-secondary)",
          padding: "4px",
          borderRadius: "10px",
          width: "fit-content",
        }}
      >
        <button
          onClick={() => setActiveTab("submit")}
          style={{
            padding: "10px 24px",
            border: "none",
            borderRadius: "8px",
            background: activeTab === "submit" ? "var(--primary-color)" : "transparent",
            color: activeTab === "submit" ? "white" : "var(--text-secondary)",
            cursor: "pointer",
            fontWeight: 500,
            transition: "all 0.2s ease",
          }}
        >
          <Plus size={16} style={{ display: "inline", marginRight: 6 }} />
          Submit Task
        </button>
        <button
          onClick={() => setActiveTab("history")}
          style={{
            padding: "10px 24px",
            border: "none",
            borderRadius: "8px",
            background: activeTab === "history" ? "var(--primary-color)" : "transparent",
            color: activeTab === "history" ? "white" : "var(--text-secondary)",
            cursor: "pointer",
            fontWeight: 500,
            transition: "all 0.2s ease",
          }}
        >
          <Clock size={16} style={{ display: "inline", marginRight: 6 }} />
          Task History ({tasks.length})
        </button>
      </div>

      {activeTab === "submit" ? (
        /* Submit Task Form */
        <div className="grid-2">
          <div className="card">
            <h3 className="card-title">
              <Send size={20} />
              New Task
            </h3>

            <form onSubmit={handleSubmitTask}>
              <div style={{ marginBottom: "20px" }}>
                <label
                  style={{
                    display: "block",
                    marginBottom: "8px",
                    fontSize: "14px",
                    fontWeight: 500,
                    color: "var(--text-secondary)",
                  }}
                >
                  Select Agent *
                </label>
                <select
                  className="input-field"
                  value={selectedAgent}
                  onChange={(e) => setSelectedAgent(e.target.value)}
                  required
                >
                  <option value="">Choose an agent...</option>
                  <option value="agent-001">Research Assistant</option>
                  <option value="agent-002">Code Reviewer</option>
                  <option value="agent-003">Data Analyst</option>
                </select>
              </div>

              <div style={{ marginBottom: "20px" }}>
                <label
                  style={{
                    display: "block",
                    marginBottom: "8px",
                    fontSize: "14px",
                    fontWeight: 500,
                    color: "var(--text-secondary)",
                  }}
                >
                  Priority Level
                </label>
                <select
                  className="input-field"
                  value={priority}
                  onChange={(e) => setPriority(e.target.value)}
                >
                  <option value="low">Low</option>
                  <option value="normal">Normal (Default)</option>
                  <option value="high">High</option>
                  <option value="urgent">Urgent</option>
                </select>
              </div>

              <div style={{ marginBottom: "20px" }}>
                <label
                  style={{
                    display: "block",
                    marginBottom: "8px",
                    fontSize: "14px",
                    fontWeight: 500,
                    color: "var(--text-secondary)",
                  }}
                >
                  Task Description *
                </label>
                <textarea
                  className="textarea-field"
                  value={taskDescription}
                  onChange={(e) => setTaskDescription(e.target.value)}
                  placeholder="Describe what you want the agent to do...&#10;&#10;Example: Analyze the latest sales data and generate a summary report with key insights and recommendations."
                  required
                  rows={6}
                />
              </div>

              <button
                type="submit"
                className="btn btn-primary"
                disabled={submitting || !selectedAgent || !taskDescription.trim()}
                style={{ width: "100%" }}
              >
                {submitting ? (
                  <>
                    <Loader2 size={16} className="spin" />
                    Submitting...
                  </>
                ) : (
                  <>
                    <Send size={16} />
                    Submit Task
                  </>
                )}
              </button>
            </form>
          </div>

          <div className="card">
            <h3 className="card-title">
              <AlertCircle size={20} />
              Guidelines
            </h3>
            <div style={{ color: "var(--text-secondary)", lineHeight: "1.8", fontSize: "14px" }}>
              <p style={{ marginBottom: "12px" }}>
                <strong>Best Practices:</strong>
              </p>
              <ul style={{ paddingLeft: "20px", marginBottom: "12px" }}>
                <li>Be specific and clear in your task descriptions</li>
                <li>Include expected output format if needed</li>
                <li>Set appropriate priority levels</li>
                <li>Provide context and constraints</li>
              </ul>

              <p style={{ marginBottom: "12px" }}>
                <strong>Example Tasks:</strong>
              </p>
              <ul style={{ paddingLeft: "20px" }}>
                <li>"Review PR #123 for code quality issues"</li>
                <li>"Generate unit tests for auth module"</li>
                <li>"Analyze Q4 performance metrics"</li>
                <li>"Research latest trends in microservices architecture"</li>
              </ul>
            </div>
          </div>
        </div>
      ) : (
        /* Task History */
        <div className="card">
          <h3 className="card-title">
            <Clock size={20} />
            Recent Tasks ({tasks.length})
          </h3>

          {tasks.length === 0 ? (
            <div className="empty-state">
              <div className="empty-state-icon">📋</div>
              <div className="empty-state-text">No tasks submitted yet</div>
              <div className="empty-state-hint">
                Switch to "Submit Task" tab to create a new task
              </div>
            </div>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
              {tasks.map((task) => (
                <div
                  key={task.id}
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center",
                    padding: "16px",
                    background: "var(--bg-tertiary)",
                    borderRadius: "10px",
                    borderLeft: `4px solid ${
                      task.status === "completed"
                        ? "#10b981"
                        : task.status === "running"
                        ? "#3b82f6"
                        : task.status === "failed"
                        ? "#ef4444"
                        : "#94a3b8"
                    }`,
                  }}
                >
                  <div style={{ flex: 1 }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "6px" }}>
                      {getStatusIcon(task.status)}
                      <span style={{ fontWeight: 600, fontSize: "15px" }}>
                        Task #{task.id.substring(0, 8)}...
                      </span>
                      <span
                        className={`status-badge ${
                          task.status === "completed"
                            ? "status-running"
                            : task.status === "running"
                            ? "status-warning"
                            : "status-stopped"
                        }`}
                      >
                        {task.status}
                      </span>
                    </div>

                    <div style={{ fontSize: "13px", color: "var(--text-muted)", marginLeft: "26px" }}>
                      Agent: {task.agent_id} • Created: {new Date(task.created_at).toLocaleString()}
                    </div>

                    {task.status === "running" && (
                      <div style={{ marginTop: "8px", marginLeft: "26px" }}>
                        <div
                          style={{
                            height: "6px",
                            background: "var(--bg-primary)",
                            borderRadius: "3px",
                            overflow: "hidden",
                            maxWidth: "300px",
                          }}
                        >
                          <div
                            style={{
                              width: `${task.progress}%`,
                              height: "100%",
                              background: "linear-gradient(90deg, #3b82f6, #8b5cf6)",
                              borderRadius: "3px",
                              transition: "width 0.3s ease",
                            }}
                          />
                        </div>
                        <span style={{ fontSize: "12px", color: "var(--text-muted)", marginTop: "4px", display: "inline-block" }}>
                          {Math.round(task.progress)}% complete
                        </span>
                      </div>
                    )}
                  </div>

                  {(task.status === "pending" || task.status === "running") && (
                    <button
                      className="btn btn-danger"
                      style={{ padding: "6px 12px", fontSize: "13px", marginLeft: "16px" }}
                      onClick={() => handleCancelTask(task.id)}
                    >
                      <XCircle size={14} />
                      Cancel
                    </button>
                  )}
                </div>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default Tasks;
