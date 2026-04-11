import React, { useState, useEffect } from "react";
import {
  ClipboardList,
  Plus,
  Search,
  Filter,
  Clock,
  CheckCircle2,
  XCircle,
  AlertTriangle,
  Loader2,
  Play,
  Square,
  RotateCcw,
  Trash2,
  ExternalLink,
  ArrowUpRight,
  Zap,
  FileCode,
  Brain,
  Shield,
  TrendingUp,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import type { TaskInfo } from "../services/agentos-sdk";
import { useI18n } from "../i18n";

const taskTypeConfig: Record<string, { icon: typeof Zap; color: string; gradient: string; bgLight: string; label: string }> = {
  codegen: { icon: FileCode, color: "#6366f1", gradient: "linear-gradient(135deg, #6366f1, #818cf8)", bgLight: "rgba(99,102,241,0.08)", label: "代码生成" },
  research: { icon: Brain, color: "#22c55e", gradient: "linear-gradient(135deg, #22c55e, #4ade80)", bgLight: "rgba(34,197,94,0.08)", label: "研究分析" },
  system: { icon: Shield, color: "#f59e0b", gradient: "linear-gradient(135deg, #f59e0b, #fbbf24)", bgLight: "rgba(245,158,11,0.08)", label: "系统任务" },
};

const statusConfig: Record<string, { color: string; bg: string; label: string; dotColor: string; gradient?: string }> = {
  running: { color: "#3b82f6", bg: "rgba(59,130,246,0.08)", label: "运行中", dotColor: "#3b82f6", gradient: "linear-gradient(135deg, #3b82f6, #60a5fa)" },
  completed: { color: "#22c55e", bg: "rgba(34,197,94,0.08)", label: "已完成", dotColor: "#22c55e", gradient: "linear-gradient(135deg, #22c55e, #4ade80)" },
  failed: { color: "#ef4444", bg: "rgba(239,68,68,0.08)", label: "失败", dotColor: "#ef4444" },
  pending: { color: "#f59e0b", bg: "rgba(245,158,11,0.08)", label: "等待中", dotColor: "#f59e0b" },
  cancelled: { color: "#94a3b8", bg: "rgba(148,163,184,0.08)", label: "已取消", dotColor: "#94a3b8" },
};

const TaskProgressRing: React.FC<{ progress: number; size?: number; strokeWidth?: number; color: string; showLabel?: boolean }> = ({
  progress, size = 40, strokeWidth = 3.5, color, showLabel = true
}) => {
  const radius = (size - strokeWidth) / 2;
  const circumference = radius * 2 * Math.PI;
  const offset = circumference - (Math.min(progress, 100) / 100) * circumference;

  return (
    <div style={{ position: 'relative', width: size, height: size, flexShrink: 0 }}>
      <svg width={size} height={size} style={{ transform: 'rotate(-90deg)' }}>
        <circle cx={size / 2} cy={size / 2} r={radius} fill="none" stroke="var(--border-subtle)" strokeWidth={strokeWidth} />
        <circle
          cx={size / 2} cy={size / 2} r={radius}
          fill="none" stroke={color} strokeWidth={strokeWidth}
          strokeDasharray={circumference} strokeDashoffset={offset}
          strokeLinecap="round"
          style={{ transition: 'stroke-dashoffset 0.8s cubic-bezier(0.4, 0, 0.2, 1)' }}
        />
      </svg>
      {showLabel && (
        <div style={{
          position: 'absolute', inset: 0, display: 'flex', alignItems: 'center', justifyContent: 'center',
        }}>
          <span style={{ fontSize: size > 36 ? "11px" : "9px", fontWeight: 700, color: "var(--text-primary)" }}>
            {Math.round(progress)}%
          </span>
        </div>
      )}
    </div>
  );
};

const Tasks: React.FC = () => {
  const { t } = useI18n();
  const [activeTab, setActiveTab] = useState<"submit" | "history">("submit");
  const [tasks, setTasks] = useState<TaskInfo[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState("");
  const [actionLoading, setActionLoading] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const [taskName, setTaskName] = useState("");
  const [taskType, setTaskType] = useState("codegen");
  const [taskParams, setTaskParams] = useState("");

  useEffect(() => {
    loadTasks();
  }, []);

  const loadTasks = async () => {
    setLoading(true);
    try {
      const data = await sdk.listTasks();
      setTasks(data || []);
    } catch (error) {
      console.error("Failed to load tasks:", error);
    } finally {
      setLoading(false);
    }
  };

  const handleAction = async (taskId: string, action: string) => {
    setActionLoading(taskId + action);
    try {
      if (action === "stop") await sdk.stopTask(taskId);
      else if (action === "restart") await sdk.restartTask(taskId);
      else if (action === "delete") {
        if (!confirm(t.tasks.confirmDelete)) return;
        await sdk.deleteTask(taskId);
      }
      await loadTasks();
    } catch (error) {
      alert(`${t.tasks.actionFailed}: ${error}`);
    } finally {
      setActionLoading(null);
    }
  };

  const handleSubmitTask = async () => {
    if (!taskName.trim()) return;
    setSubmitting(true);
    try {
      let params: Record<string, unknown> = {};
      try { params = taskParams ? JSON.parse(taskParams) : {}; } catch { params = { raw: taskParams }; }
      const agentId = tasks.length > 0 && tasks[0].agent_id ? tasks[0].agent_id : "default";
      const newTask = await sdk.submitTask(agentId, taskName, params, "normal");
      setTasks(prev => [newTask, ...prev]);
      setTaskName("");
      setTaskParams("");
      setActiveTab("history");
    } catch (error) {
      alert(`任务提交失败: ${error}`);
    } finally {
      setSubmitting(false);
    }
  };

  const filteredTasks = tasks.filter(
    (task) =>
      (task.name || "").toLowerCase().includes(searchTerm.toLowerCase()) ||
      (task.type || "").toLowerCase().includes(searchTerm.toLowerCase()) ||
      task.status.toLowerCase().includes(searchTerm.toLowerCase())
  );

  const stats = {
    total: tasks.length,
    running: tasks.filter((t) => t.status === "running").length,
    completed: tasks.filter((t) => t.status === "completed").length,
    failed: tasks.filter((t) => t.status === "failed").length,
  };

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
          <div style={{
            width: "44px", height: "44px", borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#6366f1,#818cf8)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(99,102,241,0.35), 0 0 0 1px rgba(255,255,255,0.08) inset",
          }}>
            <ClipboardList size={20} color="white" />
          </div>
          <div>
            <h1>{t.tasks.title}</h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: 0 }}>
              {t.tasks.subtitle}
            </p>
          </div>
        </div>
      </div>

      {/* Stats Bar */}
      <div className="card card-elevated" style={{ marginBottom: "20px" }}>
        <div style={{
          display: "flex", gap: "16px",
          justifyContent: "space-between",
          alignItems: "center",
          flexWrap: "wrap",
        }}>
          {/* Tab Switch */}
          <div style={{
            display: "flex", background: "var(--bg-tertiary)",
            borderRadius: "var(--radius-md)", padding: "3px",
            border: "1px solid var(--border-subtle)",
          }}>
            {[
              { key: "submit" as const, icon: Plus, label: t.tasks.submitTask },
              { key: "history" as const, icon: Clock, label: t.tasks.taskHistory },
            ].map(tab => (
              <button
                key={tab.key}
                onClick={() => setActiveTab(tab.key)}
                style={{
                  padding: "7px 20px", border: "none", borderRadius: "var(--radius-sm)",
                  background: activeTab === tab.key ? "var(--primary-color)" : "transparent",
                  color: activeTab === tab.key ? "white" : "var(--text-secondary)",
                  cursor: "pointer", fontWeight: 500, fontSize: "13px",
                  transition: "all var(--transition-fast)",
                  display: "flex", alignItems: "center", gap: "6px",
                }}
              >
                <tab.icon size={14} />
                {tab.label}
                {tab.key === "history" && (
                  <span style={{
                    fontSize: "11px", background: activeTab === tab.key ? "rgba(255,255,255,0.25)" : "var(--bg-primary)",
                    padding: "1px 7px", borderRadius: "10px", fontWeight: 600,
                  }}>{stats.total}</span>
                )}
              </button>
            ))}
          </div>

          {/* Quick Stats */}
          <div style={{ display: "flex", gap: "12px" }}>
            {[
              { label: "全部", value: stats.total, color: "var(--text-secondary)" },
              { label: "运行中", value: stats.running, color: "#3b82f6" },
              { label: "已完成", value: stats.completed, color: "#22c55e" },
              { label: "失败", value: stats.failed, color: "#ef4444" },
            ].map(stat => (
              <div key={stat.label} style={{
                display: "flex", alignItems: "center", gap: "5px",
                fontSize: "12.5px", padding: "4px 12px", borderRadius: "20px",
                background: `${stat.color}10`, border: `1px solid ${stat.color}20`,
              }}>
                <span style={{ fontWeight: 700, color: stat.color }}>{stat.value}</span>
                <span style={{ color: stat.color }}>{stat.label}</span>
              </div>
            ))}
          </div>
        </div>
      </div>

      {activeTab === "submit" ? (
        /* Submit Task Tab */
        <div className="card card-elevated">
          <h3 className="card-title">
            <Plus size={18} />
            提交新任务
          </h3>

          <div style={{ maxWidth: "600px", display: "flex", flexDirection: "column", gap: "16px" }}>
            <div className="form-group">
              <label className="form-label">{t.tasks.taskName}</label>
              <input
                type="text"
                className="form-input"
                placeholder="例如：代码审查、数据分析、系统诊断..."
                value={taskName}
                onChange={(e) => setTaskName(e.target.value)}
                onKeyDown={(e) => e.key === "Enter" && handleSubmitTask()}
              />
              <p className="form-help">{t.tasks.taskNameHelp}</p>
            </div>

            <div className="form-group">
              <label className="form-label">{t.tasks.taskType}</label>
              <div style={{
                display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: "10px",
              }}>
                {Object.entries(taskTypeConfig).map(([key, cfg]) => {
                  const IconComp = cfg.icon;
                  return (
                    <div
                      key={key}
                      onClick={() => setTaskType(key)}
                      style={{
                        padding: "14px", borderRadius: "var(--radius-md)",
                        border: `2px solid ${taskType === key ? cfg.color : "var(--border-subtle)"}`,
                        cursor: "pointer", transition: "all var(--transition-fast)",
                        textAlign: "center",
                        background: taskType === key ? cfg.bgLight : "var(--bg-tertiary)",
                      }}
                    >
                      <IconComp size={24} color={cfg.color} style={{ marginBottom: "6px" }} />
                      <div style={{ fontSize: "13px", fontWeight: 600 }}>{cfg.label}</div>
                      <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>{key}</div>
                    </div>
                  );
                })}
              </div>
            </div>

            <div className="form-group">
              <label className="form-label">{t.tasks.parameters}</label>
              <textarea
                className="textarea-field"
                rows={5}
                placeholder='{"input": "...", "config": {...}}'
                value={taskParams}
                onChange={(e) => setTaskParams(e.target.value)}
                style={{ fontFamily: "'JetBrains Mono', monospace", fontSize: "13px" }}
              />
              <p className="form-help">{t.tasks.parametersHelp}</p>
            </div>

            <button
              className="btn btn-primary btn-lg"
              onClick={handleSubmitTask}
              disabled={!taskName.trim() || submitting}
            >
              {submitting ? <Loader2 size={16} className="spin" /> : <Play size={16} />}
              {t.tasks.submitTask}
            </button>
          </div>
        </div>
      ) : (
        /* History Tab */
        <>
          {/* Search & Filter */}
          <div className="card card-elevated" style={{ marginBottom: "16px" }}>
            <div style={{ display: "flex", gap: "10px", alignItems: "center" }}>
              <div style={{ position: "relative", flex: 1, maxWidth: "360px" }}>
                <Search size={15} style={{
                  position: "absolute", left: "12px", top: "50%",
                  transform: "translateY(-50%)", color: "var(--text-muted)"
                }} />
                <input
                  type="text"
                  className="form-input"
                  placeholder={`${t.tasks.searchTasks}...`}
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  style={{ paddingLeft: "38px" }}
                />
              </div>
              <button className="btn btn-secondary" onClick={loadTasks}>
                <RotateCcw size={15} />
              </button>
            </div>
          </div>

          {/* Task List */}
          {loading ? (
            <div style={{ textAlign: "center", padding: "48px" }}>
              <div className="loading-spinner" />
            </div>
          ) : filteredTasks.length === 0 ? (
            <div className="empty-state">
              <ClipboardList size={56} style={{ opacity: 0.25 }} />
              <div className="empty-state-text">{t.tasks.noTasks}</div>
              <div className="empty-state-hint">{t.tasks.noTasksHint}</div>
              <button className="btn btn-primary mt-8" onClick={() => setActiveTab("submit")}>
                <Plus size={16} /> {t.tasks.submitTask}
              </button>
            </div>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: "10px" }}>
              {filteredTasks.map((task, idx) => {
                const typeCfg = taskTypeConfig[task.type as keyof typeof taskTypeConfig] || { icon: Zap, color: "#94a3b8", gradient: "", bgLight: "", label: task.type || "unknown" };
                const stCfg = statusConfig[task.status] || statusConfig.pending;
                const TypeIcon = typeCfg.icon;

                return (
                  <div
                    key={task.id}
                    className="card-hover-lift"
                    style={{
                      padding: "18px 20px",
                      borderRadius: "var(--radius-lg)",
                      border: "1px solid var(--border-subtle)",
                      position: "relative",
                      overflow: "hidden",
                      animation: `staggerFadeIn 0.35s ease-out ${idx * 50}ms both`,
                      display: "flex",
                      alignItems: "center",
                      gap: "18px",
                    }}
                  >
                    {/* Progress Ring */}
                    <TaskProgressRing
                      progress={task.progress}
                      color={stCfg.color}
                      size={48}
                      strokeWidth={4}
                    />

                    {/* Content */}
                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "4px" }}>
                        <span style={{ fontWeight: 600, fontSize: "15px" }}>{task.name}</span>
                        <span className="tag" style={{
                          background: stCfg.bg, color: stCfg.color,
                          fontSize: "11px", fontWeight: 500, padding: "2px 8px",
                        }}>
                          {stCfg.label}
                        </span>
                        <span className="tag" style={{
                          background: typeCfg.bgLight, color: typeCfg.color,
                          fontSize: "11px", fontWeight: 400, padding: "2px 8px",
                        }}>
                          <TypeIcon size={10} style={{ display: "inline", marginRight: "3px" }} />
                          {typeCfg.label}
                        </span>
                      </div>

                      {/* Progress bar */}
                      <div style={{ display: "flex", alignItems: "center", gap: "10px", marginTop: "8px" }}>
                        <div style={{
                          flex: 1, height: "4px", background: "var(--bg-tertiary)",
                          borderRadius: "2px", overflow: "hidden",
                        }}>
                          <div style={{
                            width: `${task.progress}%`, height: "100%",
                            background: task.status === "failed"
                              ? `repeating-linear-gradient(90deg, ${stCfg.color}, ${stCfg.color} 4px, transparent 4px, transparent 8px)`
                              : stCfg.gradient || stCfg.color,
                            borderRadius: "2px",
                            transition: "width 0.6s cubic-bezier(0.4, 0, 0.2, 1)",
                          }} />
                        </div>
                        <span style={{
                          fontSize: "12px", color: "var(--text-muted)",
                          fontFamily: "'JetBrains Mono', monospace", whiteSpace: "nowrap",
                        }}>
                          {Math.round(task.progress)}%
                        </span>
                      </div>

                      {/* Meta info */}
                      <div style={{ display: "flex", gap: "16px", marginTop: "8px", fontSize: "11.5px", color: "var(--text-muted)" }}>
                        <span><Clock size={11} style={{ display: "inline", marginRight: "3px", verticalAlign: "middle" }} />{new Date(task.created_at).toLocaleString('zh-CN')}</span>
                        <span>ID: {task.id.slice(0, 8)}</span>
                      </div>
                    </div>

                    {/* Actions */}
                    <div style={{ display: "flex", gap: "6px", flexShrink: 0 }}>
                      {task.status === "running" && (
                        <button
                          className="btn btn-danger btn-sm"
                          onClick={() => handleAction(task.id, "stop")}
                          disabled={actionLoading !== null}
                          title={t.tasks.stopTask}
                        >
                          <Square size={13} />
                        </button>
                      )}
                      {(task.status === "completed" || task.status === "failed") && (
                        <button
                          className="btn btn-secondary btn-sm"
                          onClick={() => handleAction(task.id, "restart")}
                          disabled={actionLoading !== null}
                          title={t.tasks.restartTask}
                        >
                          <RotateCcw size={13} />
                        </button>
                      )}
                      <button
                        className="btn btn-ghost btn-sm"
                        onClick={() => handleAction(task.id, "delete")}
                        disabled={actionLoading !== null}
                        title={t.tasks.deleteTask}
                      >
                        <Trash2 size={13} />
                      </button>
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </>
      )}
    </div>
  );
};

export default Tasks;
