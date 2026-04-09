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
  Zap,
  ListTodo,
} from "lucide-react";
import { invoke } from "../utils/tauriCompat";
import { useI18n } from "../i18n";

interface TaskInfo {
  id: string;
  agent_id: string;
  status: string;
  progress: number;
  created_at: string;
  updated_at?: string;
}

const Tasks: React.FC = () => {
  const { t } = useI18n();
  const [taskDescription, setTaskDescription] = useState("");
  const [selectedAgent, setSelectedAgent] = useState("agent-001");
  const [priority, setPriority] = useState("normal");
  const [tasks, setTasks] = useState<TaskInfo[]>([]);
  const [submitting, setSubmitting] = useState(false);
  const [activeTab, setActiveTab] = useState<"submit" | "history">("submit");

  const handleSubmitTask = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!taskDescription.trim()) {
      alert(t.tasks.enterDescription);
      return;
    }

    if (!selectedAgent) {
      alert(t.tasks.selectAgentFirst);
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
      alert(`${t.tasks.failedToSubmit}: ${error}`);
    } finally {
      setSubmitting(false);
    }
  };

  const handleCancelTask = async (taskId: string) => {
    if (!confirm(t.tasks.cancelConfirm)) return;

    try {
      await invoke("cancel_task", { taskId });
      setTasks(tasks.map((tk) =>
        tk.id === taskId ? { ...tk, status: "cancelled" } : tk
      ));
    } catch (error) {
      console.error("Failed to cancel task:", error);
      alert(`${t.tasks.failedToCancel}: ${error}`);
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case "completed":
        return <CheckCircle2 size={16} color="#22c55e" />;
      case "running":
        return <Loader2 size={16} color="#6366f1" className="spin" />;
      case "failed":
        return <XCircle size={16} color="#ef4444" />;
      case "cancelled":
        return <XCircle size={16} color="#f59e0b" />;
      default:
        return <Clock size={16} color="#9ca3af" />;
    }
  };

  const getStatusLabel = (status: string) => {
    switch (status) {
      case "completed": return t.tasks.completed;
      case "running": return t.tasks.running;
      case "failed": return t.tasks.failed;
      case "cancelled": return t.tasks.cancelled;
      case "pending": return t.tasks.pending;
      default: return status;
    }
  };

  const getStatusBadgeClass = (status: string) => {
    switch (status) {
      case "completed": return "status-running";
      case "running": return "status-warning";
      case "failed": return "status-error";
      case "cancelled": return "status-stopped";
      default: return "status-idle";
    }
  };

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <h1>{t.tasks.title}</h1>
        <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
          Submit and manage AI agent tasks
        </p>
      </div>

      {/* Tab Navigation */}
      <div style={{
        display: "flex",
        gap: "4px",
        marginBottom: "24px",
        background: "var(--bg-secondary)",
        padding: "4px",
        borderRadius: "var(--radius-lg)",
        width: "fit-content",
        border: "1px solid var(--border-subtle)",
      }}>
        <button
          onClick={() => setActiveTab("submit")}
          style={{
            padding: "10px 24px",
            border: "none",
            borderRadius: "var(--radius-md)",
            background: activeTab === "submit" ? "var(--primary-color)" : "transparent",
            color: activeTab === "submit" ? "white" : "var(--text-secondary)",
            cursor: "pointer",
            fontWeight: 500,
            transition: "all var(--transition-fast)",
            display: "flex",
            alignItems: "center",
            gap: "8px",
            fontSize: "13.5px",
          }}
        >
          <Plus size={16} />
          {t.tasks.submitTask}
        </button>
        <button
          onClick={() => setActiveTab("history")}
          style={{
            padding: "10px 24px",
            border: "none",
            borderRadius: "var(--radius-md)",
            background: activeTab === "history" ? "var(--primary-color)" : "transparent",
            color: activeTab === "history" ? "white" : "var(--text-secondary)",
            cursor: "pointer",
            fontWeight: 500,
            transition: "all var(--transition-fast)",
            display: "flex",
            alignItems: "center",
            gap: "8px",
            fontSize: "13.5px",
          }}
        >
          <Clock size={16} />
          {t.tasks.taskHistory} ({tasks.length})
        </button>
      </div>

      {activeTab === "submit" ? (
        <div className="grid-2">
          {/* Submit Form */}
          <div className="card card-elevated">
            <h3 className="card-title">
              <Zap size={18} />
              {t.tasks.newTask}
            </h3>

            <form onSubmit={handleSubmitTask}>
              <div className="form-group">
                <label className="form-label">{t.tasks.selectAgent} *</label>
                <select
                  className="form-select"
                  value={selectedAgent}
                  onChange={(e) => setSelectedAgent(e.target.value)}
                  required
                >
                  <option value="">{t.tasks.chooseAgent}</option>
                  <option value="agent-001">Research Assistant</option>
                  <option value="agent-002">Code Reviewer</option>
                  <option value="agent-003">Data Analyst</option>
                </select>
              </div>

              <div className="form-group">
                <label className="form-label">{t.tasks.priorityLevel}</label>
                <select
                  className="form-select"
                  value={priority}
                  onChange={(e) => setPriority(e.target.value)}
                >
                  <option value="low">{t.tasks.low}</option>
                  <option value="normal">{t.tasks.normal}</option>
                  <option value="high">{t.tasks.high}</option>
                  <option value="urgent">{t.tasks.urgent}</option>
                </select>
              </div>

              <div className="form-group">
                <label className="form-label">{t.tasks.taskDescription} *</label>
                <textarea
                  className="textarea-field"
                  value={taskDescription}
                  onChange={(e) => setTaskDescription(e.target.value)}
                  placeholder={t.tasks.descriptionPlaceholder}
                  required
                  rows={5}
                />
              </div>

              <button
                type="submit"
                className="btn btn-primary btn-lg"
                disabled={submitting || !selectedAgent || !taskDescription.trim()}
                style={{ width: "100%" }}
              >
                {submitting ? (
                  <>
                    <Loader2 size={16} className="spin" />
                    {t.tasks.submitting}
                  </>
                ) : (
                  <>
                    <Send size={16} />
                    {t.tasks.submitTask}
                  </>
                )}
              </button>
            </form>
          </div>

          {/* Guidelines */}
          <div className="card card-elevated">
            <h3 className="card-title">
              <AlertCircle size={18} />
              {t.tasks.guidelines}
            </h3>
            <div style={{ color: "var(--text-secondary)", lineHeight: "1.9", fontSize: "14px" }}>
              <p style={{ marginBottom: "14px", fontWeight: 600, color: "var(--text-primary)" }}>
                {t.tasks.bestPractices}
              </p>
              <ul style={{ paddingLeft: "20px", marginBottom: "16px" }}>
                <li style={{ marginBottom: "6px" }}>{t.tasks.beSpecific}</li>
                <li style={{ marginBottom: "6px" }}>{t.tasks.includeFormat}</li>
                <li style={{ marginBottom: "6px" }}>{t.tasks.setPriority}</li>
                <li>{t.tasks.provideContext}</li>
              </ul>

              <p style={{ marginBottom: "14px", fontWeight: 600, color: "var(--text-primary)" }}>
                {t.tasks.exampleTasks}
              </p>
              <ul style={{ paddingLeft: "20px" }}>
                <li style={{ marginBottom: "6px" }}>"Review PR #123 for code quality issues"</li>
                <li style={{ marginBottom: "6px" }}>"Generate unit tests for auth module"</li>
                <li style={{ marginBottom: "6px" }}>"Analyze Q4 performance metrics"</li>
                <li>"Research latest trends in microservices architecture"</li>
              </ul>
            </div>
          </div>
        </div>
      ) : (
        <div className="card card-elevated">
          <h3 className="card-title">
            <ListTodo size={18} />
            {t.tasks.recentTasks} ({tasks.length})
          </h3>

          {tasks.length === 0 ? (
            <div className="empty-state">
              <div className="empty-state-icon">📋</div>
              <div className="empty-state-text">{t.tasks.noTasksYet}</div>
              <div className="empty-state-hint">
                {t.tasks.switchToSubmit}
              </div>
              <button className="btn btn-primary mt-8" onClick={() => setActiveTab("submit")}>
                <Plus size={16} /> {t.tasks.submitTask}
              </button>
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
                    padding: "18px 20px",
                    background: "var(--bg-tertiary)",
                    borderRadius: "var(--radius-lg)",
                    borderLeft: `4px solid ${
                      task.status === "completed"
                        ? "#22c55e"
                        : task.status === "running"
                        ? "#6366f1"
                        : task.status === "failed"
                        ? "#ef4444"
                        : "#9ca3af"
                    }`,
                    transition: "all var(--transition-fast)",
                  }}
                >
                  <div style={{ flex: 1 }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "12px", marginBottom: "8px" }}>
                      {getStatusIcon(task.status)}
                      <span style={{ fontWeight: 600, fontSize: "15px" }}>
                        Task #{task.id.substring(0, 8)}...
                      </span>
                      <span className={`badge ${getStatusBadgeClass(task.status)}`}>
                        {getStatusLabel(task.status)}
                      </span>
                    </div>

                    <div style={{ fontSize: "13px", color: "var(--text-muted)", marginLeft: "28px" }}>
                      {t.tasks.agent}: {task.agent_id} • {t.tasks.created}: {new Date(task.created_at).toLocaleString()}
                    </div>

                    {task.status === "running" && (
                      <div style={{ marginTop: "10px", marginLeft: "28px" }}>
                        <div className="progress-bar" style={{ maxWidth: "320px" }}>
                          <div
                            className="progress-bar-fill"
                            style={{ width: `${task.progress}%` }}
                          />
                        </div>
                        <span style={{ fontSize: "12px", color: "var(--text-muted)", marginTop: "6px", display: "inline-block" }}>
                          {Math.round(task.progress)}% {t.tasks.complete}
                        </span>
                      </div>
                    )}
                  </div>

                  {(task.status === "pending" || task.status === "running") && (
                    <button
                      className="btn btn-danger btn-sm"
                      style={{ marginLeft: "16px" }}
                      onClick={() => handleCancelTask(task.id)}
                    >
                      <XCircle size={14} />
                      {t.tasks.cancel}
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
