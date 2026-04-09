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
  const [selectedAgent, setSelectedAgent] = useState("");
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

  return (
    <div>
      <div className="card" style={{ marginBottom: "20px" }}>
        <h3 className="card-title" style={{ marginBottom: 0 }}>
          <ClipboardList size={20} />
          {t.tasks.title}
        </h3>
      </div>

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
          {t.tasks.submitTask}
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
          {t.tasks.taskHistory} ({tasks.length})
        </button>
      </div>

      {activeTab === "submit" ? (
        <div className="grid-2">
          <div className="card">
            <h3 className="card-title">
              <Send size={20} />
              {t.tasks.newTask}
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
                  {t.tasks.selectAgent} *
                </label>
                <select
                  className="input-field"
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
                  {t.tasks.priorityLevel}
                </label>
                <select
                  className="input-field"
                  value={priority}
                  onChange={(e) => setPriority(e.target.value)}
                >
                  <option value="low">{t.tasks.low}</option>
                  <option value="normal">{t.tasks.normal}</option>
                  <option value="high">{t.tasks.high}</option>
                  <option value="urgent">{t.tasks.urgent}</option>
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
                  {t.tasks.taskDescription} *
                </label>
                <textarea
                  className="textarea-field"
                  value={taskDescription}
                  onChange={(e) => setTaskDescription(e.target.value)}
                  placeholder={t.tasks.descriptionPlaceholder}
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

          <div className="card">
            <h3 className="card-title">
              <AlertCircle size={20} />
              {t.tasks.guidelines}
            </h3>
            <div style={{ color: "var(--text-secondary)", lineHeight: "1.8", fontSize: "14px" }}>
              <p style={{ marginBottom: "12px" }}>
                <strong>{t.tasks.bestPractices}</strong>
              </p>
              <ul style={{ paddingLeft: "20px", marginBottom: "12px" }}>
                <li>{t.tasks.beSpecific}</li>
                <li>{t.tasks.includeFormat}</li>
                <li>{t.tasks.setPriority}</li>
                <li>{t.tasks.provideContext}</li>
              </ul>

              <p style={{ marginBottom: "12px" }}>
                <strong>{t.tasks.exampleTasks}</strong>
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
        <div className="card">
          <h3 className="card-title">
            <Clock size={20} />
            {t.tasks.recentTasks} ({tasks.length})
          </h3>

          {tasks.length === 0 ? (
            <div className="empty-state">
              <div className="empty-state-icon">📋</div>
              <div className="empty-state-text">{t.tasks.noTasksYet}</div>
              <div className="empty-state-hint">
                {t.tasks.switchToSubmit}
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
                        {getStatusLabel(task.status)}
                      </span>
                    </div>

                    <div style={{ fontSize: "13px", color: "var(--text-muted)", marginLeft: "26px" }}>
                      {t.tasks.agent}: {task.agent_id} • {t.tasks.created}: {new Date(task.created_at).toLocaleString()}
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
                          {Math.round(task.progress)}% {t.tasks.complete}
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
