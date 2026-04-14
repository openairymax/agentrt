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
import { useAlert } from "../components/useAlert";
import { Button } from "../components/ui/Button";
import { Input } from "../components/ui/Input";
import { Card } from "../components/ui/Card";
import { PageLayout } from "../components/PageLayout";

const taskTypeConfig: Record<string, { icon: typeof Zap; color: string; gradient: string; bgLight: string; label: string }> = {
  codegen: { icon: FileCode, color: "var(--primary-color)", gradient: "var(--primary-gradient)", bgLight: "var(--primary-light)", label: "代码生成" },
  research: { icon: Brain, color: "var(--success-color)", gradient: "linear-gradient(135deg, var(--success-color), #4ade80)", bgLight: "var(--success-light)", label: "研究分析" },
  system: { icon: Shield, color: "var(--warning-color)", gradient: "linear-gradient(135deg, var(--warning-color), #fbbf24)", bgLight: "var(--warning-light)", label: "系统任务" },
};

const statusConfig: Record<string, { color: string; bg: string; label: string; dotColor: string; gradient?: string }> = {
  running: { color: "var(--primary-color)", bg: "var(--primary-light)", label: "运行中", dotColor: "var(--primary-color)", gradient: "var(--primary-gradient)" },
  completed: { color: "var(--success-color)", bg: "var(--success-light)", label: "已完成", dotColor: "var(--success-color)", gradient: "linear-gradient(135deg, var(--success-color), #4ade80)" },
  failed: { color: "var(--error-color)", bg: "var(--error-light)", label: "失败", dotColor: "var(--error-color)" },
  pending: { color: "var(--warning-color)", bg: "var(--warning-light)", label: "等待中", dotColor: "var(--warning-color)" },
  cancelled: { color: "var(--text-muted)", bg: "rgba(148,163,184,0.08)", label: "已取消", dotColor: "var(--text-muted)" },
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
  const { error, success, confirm: confirmModal } = useAlert();
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
    } catch (err) {
      error("加载失败", `无法加载任务列表: ${err}`);
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
        const confirmed = await confirmModal({
          type: 'danger',
          title: '删除任务',
          message: t.tasks.confirmDelete || '确定要删除此任务吗？此操作无法撤销。',
          confirmText: '删除',
          cancelText: '取消',
        });
        if (!confirmed) return;
        await sdk.deleteTask(taskId);
      }
      await loadTasks();
    } catch (err) {
      error("操作失败", `${t.tasks.actionFailed || '任务操作失败'}: ${err}`);
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
    } catch (err) {
      error("提交失败", `任务提交失败: ${err}`);
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
    <PageLayout
      title={t.tasks.title}
      subtitle={t.tasks.subtitle}
    >
      {/* Stats Bar */}
      <Card style={{ marginBottom: '20px' }}>
        <div style={{
          display: "flex", 
          gap: "20px",
          justifyContent: "space-between",
          alignItems: "center",
          flexWrap: "wrap",
          padding: '16px',
        }}>
          {/* Tab Switch */}
          <div style={{
            display: "flex", 
            background: "var(--bg-tertiary)",
            borderRadius: "8px", 
            padding: "4px",
            border: "1px solid var(--border-subtle)",
            boxShadow: 'var(--shadow-sm)',
          }}>
            {[
              { key: "submit" as const, icon: Plus, label: t.tasks.submitTask },
              { key: "history" as const, icon: Clock, label: t.tasks.taskHistory },
            ].map(tab => (
              <button
                key={tab.key}
                onClick={() => setActiveTab(tab.key)}
                style={{
                  padding: "10px 24px", 
                  border: "none", 
                  borderRadius: "6px",
                  background: activeTab === tab.key ? "var(--primary-color)" : "transparent",
                  color: activeTab === tab.key ? "white" : "var(--text-secondary)",
                  cursor: "pointer", 
                  fontWeight: 500, 
                  fontSize: "13px",
                  transition: "all 0.2s ease",
                  display: "flex", 
                  alignItems: "center", 
                  gap: "8px",
                }}
                onMouseEnter={(e) => {
                  if (activeTab !== tab.key) {
                    e.currentTarget.style.background = 'var(--bg-secondary)';
                  }
                }}
                onMouseLeave={(e) => {
                  if (activeTab !== tab.key) {
                    e.currentTarget.style.background = 'transparent';
                  }
                }}
              >
                <tab.icon size={14} />
                {tab.label}
                {tab.key === "history" && (
                  <span style={{
                    fontSize: "11px", 
                    background: activeTab === tab.key ? "rgba(255,255,255,0.25)" : "var(--bg-secondary)",
                    padding: "2px 8px", 
                    borderRadius: "12px", 
                    fontWeight: 600,
                  }}>
                    {stats.total}
                  </span>
                )}
              </button>
            ))}
          </div>

          {/* Quick Stats */}
          <div style={{ 
            display: "flex", 
            gap: "12px",
            flexWrap: 'wrap',
          }}>
            {[
              { label: "全部", value: stats.total, color: "var(--text-secondary)" },
              { label: "运行中", value: stats.running, color: "var(--primary-color)" },
              { label: "已完成", value: stats.completed, color: "var(--success-color)" },
              { label: "失败", value: stats.failed, color: "var(--error-color)" },
            ].map(stat => (
              <div 
                key={stat.label} 
                style={{
                  display: "flex", 
                  alignItems: "center", 
                  gap: "6px",
                  fontSize: "12px", 
                  padding: "6px 16px", 
                  borderRadius: "20px",
                  background: `${stat.color}10`, 
                  border: `1px solid ${stat.color}20`,
                  transition: 'all 0.2s ease',
                  boxShadow: 'var(--shadow-sm)',
                }}
                onMouseEnter={(e: React.MouseEvent<HTMLDivElement>) => {
                  e.currentTarget.style.transform = 'translateY(-2px)';
                  e.currentTarget.style.boxShadow = 'var(--shadow-md)';
                  e.currentTarget.style.background = `${stat.color}15`;
                }}
                onMouseLeave={(e: React.MouseEvent<HTMLDivElement>) => {
                  e.currentTarget.style.transform = 'translateY(0)';
                  e.currentTarget.style.boxShadow = 'var(--shadow-sm)';
                  e.currentTarget.style.background = `${stat.color}10`;
                }}
              >
                <span style={{ fontWeight: 600, color: stat.color }}>{stat.value}</span>
                <span style={{ color: stat.color }}>{stat.label}</span>
              </div>
            ))}
          </div>
        </div>
      </Card>

      {activeTab === "submit" ? (
        /* Submit Task Tab */
        <Card style={{ boxShadow: 'var(--shadow-sm)' }}>
          <div style={{ padding: '24px' }}>
            <h3 style={{ 
              margin: "0 0 24px 0", 
              fontSize: "18px", 
              fontWeight: 600, 
              display: "flex", 
              alignItems: "center", 
              gap: "10px",
              color: 'var(--text-primary)',
            }}>
              <div style={{
                width: "32px",
                height: "32px",
                borderRadius: "8px",
                background: "var(--primary-color)",
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
              }}>
                <Plus size={16} color="white" />
              </div>
              提交新任务
            </h3>

            <div style={{ maxWidth: "600px", display: "flex", flexDirection: "column", gap: "24px" }}>
              <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
                <label style={{ 
                  fontSize: "14px", 
                  fontWeight: 500,
                  color: 'var(--text-primary)',
                }}>
                  {t.tasks.taskName}
                </label>
                <Input
                  placeholder="例如：代码审查、数据分析、系统诊断..."
                  value={taskName}
                  onChange={(e) => setTaskName(e.target.value)}
                  onKeyDown={(e) => e.key === "Enter" && handleSubmitTask()}
                  style={{
                    transition: 'all 0.2s ease',
                  }}
                />
                <p style={{ 
                  fontSize: "12px", 
                  color: "var(--text-muted)", 
                  margin: 0,
                }}>
                  {t.tasks.taskNameHelp}
                </p>
              </div>

              <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
                <label style={{ 
                  fontSize: "14px", 
                  fontWeight: 500,
                  color: 'var(--text-primary)',
                }}>
                  {t.tasks.taskType}
                </label>
                <div style={{
                  display: "grid", 
                  gridTemplateColumns: "repeat(3, 1fr)", 
                  gap: "16px",
                }}>
                  {Object.entries(taskTypeConfig).map(([key, cfg]) => {
                    const IconComp = cfg.icon;
                    return (
                      <div
                        key={key}
                        onClick={() => setTaskType(key)}
                        style={{
                          padding: "20px", 
                          borderRadius: "8px",
                          border: `2px solid ${taskType === key ? cfg.color : "var(--border-subtle)"}`,
                          cursor: "pointer",
                          transition: "all 0.2s ease",
                          textAlign: "center",
                          background: taskType === key ? `${cfg.color}06` : "var(--bg-tertiary)",
                          boxShadow: 'var(--shadow-sm)',
                        }}
                        onMouseEnter={(e: React.MouseEvent<HTMLDivElement>) => {
                          if (taskType !== key) {
                            e.currentTarget.style.borderColor = 'var(--border-color)';
                            e.currentTarget.style.background = 'var(--bg-secondary)';
                            e.currentTarget.style.transform = 'translateY(-2px)';
                            e.currentTarget.style.boxShadow = 'var(--shadow-md)';
                          }
                        }}
                        onMouseLeave={(e: React.MouseEvent<HTMLDivElement>) => {
                          if (taskType !== key) {
                            e.currentTarget.style.borderColor = 'var(--border-subtle)';
                            e.currentTarget.style.background = 'var(--bg-tertiary)';
                            e.currentTarget.style.transform = 'translateY(0)';
                            e.currentTarget.style.boxShadow = 'var(--shadow-sm)';
                          }
                        }}
                      >
                        <div style={{
                          width: "48px",
                          height: "48px",
                          borderRadius: "12px",
                          background: `${cfg.color}15`,
                          display: "flex",
                          alignItems: "center",
                          justifyContent: "center",
                          margin: "0 auto 12px",
                        }}>
                          <IconComp size={24} color={cfg.color} />
                        </div>
                        <div style={{ 
                          fontSize: "14px", 
                          fontWeight: 600,
                          color: taskType === key ? cfg.color : 'var(--text-primary)',
                        }}>
                          {cfg.label}
                        </div>
                        <div style={{ 
                          fontSize: "11px", 
                          color: "var(--text-muted)",
                          marginTop: "4px",
                        }}>
                          {key}
                        </div>
                      </div>
                    );
                  })}
                </div>
              </div>

              <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
                <label style={{ 
                  fontSize: "14px", 
                  fontWeight: 500,
                  color: 'var(--text-primary)',
                }}>
                  {t.tasks.parameters}
                </label>
                <textarea
                  style={{
                    width: "100%",
                    padding: "16px",
                    borderRadius: "8px",
                    border: "1px solid var(--border-color)",
                    background: "var(--bg-secondary)",
                    color: "var(--text-primary)",
                    resize: "vertical",
                    minHeight: "140px",
                    fontFamily: "'JetBrains Mono', monospace",
                    fontSize: "13px",
                    transition: "all 0.2s ease",
                  }}
                  placeholder='{"input": "...", "config": {...}}'
                  value={taskParams}
                  onChange={(e) => setTaskParams(e.target.value)}
                />
                <p style={{ 
                  fontSize: "12px", 
                  color: "var(--text-muted)", 
                  margin: 0,
                }}>
                  {t.tasks.parametersHelp}
                </p>
              </div>

              <Button
                variant="primary"
                onClick={handleSubmitTask}
                disabled={!taskName.trim() || submitting}
                style={{
                  transition: 'all 0.2s ease',
                  boxShadow: 'var(--shadow-sm)',
                }}
              >
                {submitting ? <Loader2 size={16} className="spin" /> : <Play size={16} />}
                {t.tasks.submitTask}
              </Button>
            </div>
          </div>
        </Card>
      ) : (
        /* History Tab */
        <>
          {/* Search & Filter */}
          <Card style={{ marginBottom: "20px", boxShadow: 'var(--shadow-sm)' }}>
            <div style={{ 
              display: "flex", 
              gap: "12px", 
              alignItems: "center",
              padding: '16px',
            }}>
              <div style={{ position: "relative", flex: 1, maxWidth: "400px" }}>
                <Search size={16} style={{
                  position: "absolute", 
                  left: "12px", 
                  top: "50%",
                  transform: "translateY(-50%)", 
                  color: "var(--text-muted)"
                }} />
                <Input
                  placeholder={`${t.tasks.searchTasks}...`}
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  style={{ 
                    paddingLeft: "40px",
                    transition: 'all 0.2s ease',
                  }}
                />
              </div>
              <Button
                variant="secondary"
                onClick={loadTasks}
                style={{
                  transition: 'all 0.2s ease',
                }}
              >
                <RotateCcw size={16} />
                刷新
              </Button>
            </div>
          </Card>

          {/* Task List */}
          {loading ? (
            <div style={{ 
              textAlign: "center", 
              padding: "64px",
              background: 'var(--bg-secondary)',
              borderRadius: '8px',
              border: '1px solid var(--border-subtle)',
            }}>
              <div className="loading-spinner" />
              <div style={{ 
                marginTop: '16px', 
                fontSize: '14px', 
                color: 'var(--text-muted)'
              }}>
                加载任务列表...
              </div>
            </div>
          ) : filteredTasks.length === 0 ? (
            <Card style={{ 
              textAlign: "center", 
              padding: "64px",
              boxShadow: 'var(--shadow-sm)',
            }}>
              <div style={{
                width: "80px",
                height: "80px",
                borderRadius: "20px",
                background: "var(--bg-tertiary)",
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
                margin: "0 auto 24px",
              }}>
                <ClipboardList size={40} style={{ opacity: 0.2 }} />
              </div>
              <div style={{ 
                fontSize: "16px", 
                fontWeight: 500, 
                margin: "0 0 12px 0",
                color: 'var(--text-primary)',
              }}>
                {t.tasks.noTasks}
              </div>
              <div style={{ 
                fontSize: "14px", 
                color: "var(--text-muted)", 
                marginBottom: "32px",
                maxWidth: "320px",
                marginLeft: "auto",
                marginRight: "auto",
              }}>
                {t.tasks.noTasksHint}
              </div>
              <Button
                variant="primary"
                onClick={() => setActiveTab("submit")}
                style={{
                  transition: 'all 0.2s ease',
                  boxShadow: 'var(--shadow-sm)',
                }}
              >
                <Plus size={16} />
                {t.tasks.submitTask}
              </Button>
            </Card>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
              {filteredTasks.map((task) => {
                const typeCfg = taskTypeConfig[task.type as keyof typeof taskTypeConfig] || { icon: Zap, color: "var(--text-muted)", gradient: "", bgLight: "", label: task.type || "unknown" };
                const stCfg = statusConfig[task.status] || statusConfig.pending;
                const TypeIcon = typeCfg.icon;

                return (
                  <Card 
                    key={task.id}
                    style={{
                      transition: 'all 0.2s ease',
                      boxShadow: 'var(--shadow-sm)',
                      border: '1px solid var(--border-subtle)',
                      background: 'var(--bg-secondary)',
                    }}
                    onMouseEnter={(e: React.MouseEvent<HTMLDivElement>) => {
                      e.currentTarget.style.transform = 'translateY(-2px)';
                      e.currentTarget.style.boxShadow = 'var(--shadow-md)';
                      e.currentTarget.style.borderColor = 'var(--border-color)';
                    }}
                    onMouseLeave={(e: React.MouseEvent<HTMLDivElement>) => {
                      e.currentTarget.style.transform = 'translateY(0)';
                      e.currentTarget.style.boxShadow = 'var(--shadow-sm)';
                      e.currentTarget.style.borderColor = 'var(--border-subtle)';
                    }}
                  >
                    <div style={{
                      display: "flex",
                      alignItems: "center",
                      gap: "20px",
                      padding: '16px',
                    }}>
                      {/* Progress Ring */}
                      <TaskProgressRing
                        progress={task.progress}
                        color={stCfg.color}
                        size={52}
                        strokeWidth={3.5}
                      />

                      {/* Content */}
                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{ 
                          display: "flex", 
                          alignItems: "center", 
                          gap: "10px", 
                          marginBottom: "12px",
                          flexWrap: 'wrap',
                        }}>
                          <span style={{ 
                            fontWeight: 600, 
                            fontSize: "15px",
                            color: 'var(--text-primary)',
                          }}>
                            {task.name}
                          </span>
                          <span style={{
                            background: `${stCfg.color}15`, 
                            color: stCfg.color,
                            fontSize: "11px", 
                            fontWeight: 500, 
                            padding: "4px 12px",
                            borderRadius: "4px",
                          }}>
                            {stCfg.label}
                          </span>
                          <span style={{
                            background: `${typeCfg.color}15`, 
                            color: typeCfg.color,
                            fontSize: "11px", 
                            fontWeight: 500, 
                            padding: "4px 12px",
                            borderRadius: "4px",
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                          }}>
                            <TypeIcon size={12} />
                            {typeCfg.label}
                          </span>
                        </div>

                        {/* Progress bar */}
                        <div style={{ 
                          display: "flex", 
                          alignItems: "center", 
                          gap: "12px", 
                          marginBottom: "12px"
                        }}>
                          <div style={{
                            flex: 1, 
                            height: "6px", 
                            background: "var(--bg-tertiary)",
                            borderRadius: "3px", 
                            overflow: "hidden",
                          }}>
                            <div style={{
                              width: `${task.progress}%`, 
                              height: "100%",
                              background: task.status === "failed"
                                ? stCfg.color
                                : stCfg.color,
                              borderRadius: "3px",
                              transition: "width 0.6s ease",
                            }} />
                          </div>
                          <span style={{
                            fontSize: "12px", 
                            color: "var(--text-muted)",
                            fontFamily: "'JetBrains Mono', monospace", 
                            whiteSpace: "nowrap",
                          }}>
                            {Math.round(task.progress)}%
                          </span>
                        </div>

                        {/* Meta info */}
                        <div style={{ 
                          display: "flex", 
                          gap: "20px", 
                          fontSize: "12px", 
                          color: "var(--text-muted)",
                          flexWrap: 'wrap',
                        }}>
                          <span style={{ 
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                          }}>
                            <Clock size={12} />
                            {new Date(task.created_at).toLocaleString('zh-CN')}
                          </span>
                          <span>ID: {task.id.slice(0, 8)}</span>
                        </div>
                      </div>

                      {/* Actions */}
                      <div style={{ 
                        display: "flex", 
                        gap: "8px", 
                        flexShrink: 0,
                      }}>
                        {task.status === "running" && (
                          <Button
                            variant="danger"
                            size="sm"
                            onClick={() => handleAction(task.id, "stop")}
                            disabled={actionLoading !== null}
                            title={t.tasks.stopTask}
                            style={{
                              transition: 'all 0.2s ease',
                              borderRadius: '6px',
                            }}
                          >
                            <Square size={14} />
                          </Button>
                        )}
                        {(task.status === "completed" || task.status === "failed") && (
                          <Button
                            variant="secondary"
                            size="sm"
                            onClick={() => handleAction(task.id, "restart")}
                            disabled={actionLoading !== null}
                            title={t.tasks.restartTask}
                            style={{
                              transition: 'all 0.2s ease',
                              borderRadius: '6px',
                            }}
                          >
                            <RotateCcw size={14} />
                          </Button>
                        )}
                        <Button
                          variant="ghost"
                          size="sm"
                          onClick={() => handleAction(task.id, "delete")}
                          disabled={actionLoading !== null}
                          title={t.tasks.deleteTask}
                          style={{
                            transition: 'all 0.2s ease',
                            borderRadius: '6px',
                          }}
                        >
                          <Trash2 size={14} />
                        </Button>
                      </div>
                    </div>
                  </Card>
                );
              })}
            </div>
          )}
        </>
      )}
    </PageLayout>
  );
};

export default Tasks;
