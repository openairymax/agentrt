import React, { useState, useEffect, useMemo } from "react";
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
  Play,
  Square,
  ArrowUpDown,
  Download,
  CheckSquare,
} from "lucide-react";
import { exportToCSV } from "../utils/export";
import sdk from "../services/agentos-sdk";
import type { AgentInfo } from "../services/agentos-sdk";
import { useI18n } from "../i18n";
import { useNavigate } from "react-router-dom";
import { useAlert } from "../components/useAlert";

const agentTypeConfig: Record<string, { icon: typeof Bot; color: string; gradient: string; bgLight: string; label: string }> = {
  research: { icon: Brain, color: "#6366f1", gradient: "linear-gradient(135deg, #6366f1, #818cf8)", bgLight: "rgba(99,102,241,0.08)", label: "研究型" },
  coding: { icon: Code2, color: "#22c55e", gradient: "linear-gradient(135deg, #22c55e, #4ade80)", bgLight: "rgba(34,197,94,0.08)", label: "编码型" },
  assistant: { icon: Sparkles, color: "#a855f7", gradient: "linear-gradient(135deg, #a855f7, #c084fc)", bgLight: "rgba(168,85,247,0.08)", label: "助手型" },
  system: { icon: Shield, color: "#f59e0b", gradient: "linear-gradient(135deg, #f59e0b, #fbbf24)", bgLight: "rgba(245,158,11,0.08)", label: "系统型" },
};

const Agents: React.FC = () => {
  const { t } = useI18n();
  const navigate = useNavigate();
  const { error, success, confirm: confirmModal } = useAlert();
  const [agents, setAgents] = useState<AgentInfo[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState("");
  const [selectedAgent, setSelectedAgent] = useState<AgentInfo | null>(null);
  const [showRegisterModal, setShowRegisterModal] = useState(false);
  const [sortBy, setSortBy] = useState<'name' | 'status' | 'type'>('name');
  const [sortOrder, setSortOrder] = useState<'asc' | 'desc'>('asc');
  const [selectedIds, setSelectedIds] = useState<Set<string>>(new Set());
  const [filterType, setFilterType] = useState<string>('all');

  useEffect(() => {
    loadAgents();
  }, []);

  const filteredAndSortedAgents = useMemo(() => {
    let result = [...agents];

    if (searchTerm) {
      result = result.filter(agent =>
        agent.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
        (agent.type && agent.type.toLowerCase().includes(searchTerm.toLowerCase()))
      );
    }

    if (filterType !== 'all') {
      result = result.filter(agent => agent.type === filterType);
    }

    result.sort((a, b) => {
      let cmp = 0;
      switch (sortBy) {
        case 'name':
          cmp = a.name.localeCompare(b.name);
          break;
        case 'status':
          cmp = (a.status === 'running' ? 0 : 1) - (b.status === 'running' ? 0 : 1);
          break;
        case 'type':
          cmp = (a.type || '').localeCompare(b.type || '');
          break;
      }
      return sortOrder === 'asc' ? cmp : -cmp;
    });

    return result;
  }, [agents, searchTerm, sortBy, sortOrder, filterType]);

  const [actionLoading, setActionLoading] = useState<string | null>(null);
  const [showConfigModal, setShowConfigModal] = useState<string | null>(null);

  const loadAgents = async () => {
    setLoading(true);
    try {
      const data = await sdk.listAgents();
      setAgents(data || []);
    } catch (err) {
      error("加载失败", `无法加载智能体列表: ${err}`);
    } finally {
      setLoading(false);
    }
  };

  const handleRegister = async (name: string, type: string) => {
    try {
      await sdk.registerAgent(name, type);
      setShowRegisterModal(false);
      loadAgents();
      success("注册成功", `智能体 "${name}" 已成功注册`);
    } catch (err) {
      error("注册失败", `${t.agents.registerFailed}: ${err}`);
    }
  };

  const handleStartAgent = async (agentId: string) => {
    setActionLoading(agentId + "-start");
    try {
      await sdk.startAgent(agentId);
      loadAgents();
    } catch (err) {
      error("启动失败", `无法启动智能体: ${err}`);
    } finally {
      setActionLoading(null);
    }
  };

  const handleStopAgent = async (agentId: string) => {
    const confirmed = await confirmModal({
      type: 'danger',
      title: '停止智能体',
      message: '确定要停止此智能体吗？',
    });
    if (!confirmed) return;
    setActionLoading(agentId + "-stop");
    try {
      await sdk.stopAgent(agentId);
      loadAgents();
    } catch (err) {
      error("停止失败", `无法停止智能体: ${err}`);
    } finally {
      setActionLoading(null);
    }
  };

  const toggleSelectAgent = (agentId: string) => {
    setSelectedIds(prev => {
      const next = new Set(prev);
      if (next.has(agentId)) {
        next.delete(agentId);
      } else {
        next.add(agentId);
      }
      return next;
    });
  };

  const selectAllVisible = () => {
    setSelectedIds(new Set(filteredAndSortedAgents.map((a: AgentInfo) => a.id)));
  };

  const clearSelection = () => {
    setSelectedIds(new Set());
  };

  const handleBatchStart = async () => {
    if (selectedIds.size === 0) return;
    for (const id of selectedIds) {
      try { await sdk.startAgent(id); } catch {}
    }
    success("批量启动", `已发送 ${selectedIds.size} 个智能体的启动指令`);
    clearSelection();
    loadAgents();
  };

  const handleBatchStop = async () => {
    if (selectedIds.size === 0) return;
    const confirmed = await confirmModal({
      type: 'danger',
      title: '批量停止',
      message: `确定要停止选中的 ${selectedIds.size} 个智能体吗？`,
    });
    if (!confirmed) return;
    for (const id of selectedIds) {
      try { await sdk.stopAgent(id); } catch {}
    }
    success("批量停止", `已发送 ${selectedIds.size} 个智能体的停止指令`);
    clearSelection();
    loadAgents();
  };

  const toggleSort = (field: 'name' | 'status' | 'type') => {
    if (sortBy === field) {
      setSortOrder(prev => prev === 'asc' ? 'desc' : 'asc');
    } else {
      setSortBy(field);
      setSortOrder('asc');
    }
  };

  const getAgentConfig = (type: string) => {
    return agentTypeConfig[type] || { icon: Bot, color: "#94a3b8", gradient: "linear-gradient(135deg, #94a3b8, #cbd5e1)", bgLight: "rgba(148,163,184,0.08)", label: type };
  };

  return (
    <PageLayout
      title={t.agents.title}
      subtitle={t.agents.subtitle}
      actions={
        <Button
          variant="primary"
          onClick={() => setShowRegisterModal(true)}
        >
          <Plus size={16} />
          {t.agents.registerNew}
        </Button>
      }
    >
      {/* Toolbar */}
      <Card>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", flexWrap: "wrap", gap: "12px" }}>
          <div style={{ display: "flex", alignItems: "center", gap: "10px", flex: 1, minWidth: "200px" }}>
            <div style={{ position: "relative", flex: 1, maxWidth: "360px" }}>
              <Search size={15} style={{
                position: "absolute", left: "12px", top: "50%",
                transform: "translateY(-50%)", color: "var(--text-muted)"
              }} />
              <Input
                placeholder={`${t.agents.searchAgents}...`}
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                style={{ paddingLeft: "38px" }}
              />
            </div>
            {/* Type Filter */}
            <div style={{ display: 'flex', gap: '4px' }}>
              {['all', ...Object.keys(agentTypeConfig)].map(type => (
                <Button
                  key={type}
                  onClick={() => setFilterType(type)}
                  variant={filterType === type ? 'primary' : 'ghost'}
                  size="sm"
                  style={{
                    fontSize: '11.5px',
                    borderRadius: '9999px',
                  }}
                >
                  {type === 'all' ? '全部' : agentTypeConfig[type]?.label || type}
                </Button>
              ))}
            </div>
          </div>

          <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
            {selectedIds.size > 0 && (
              <>
                <span style={{ fontSize: '12px', color: 'var(--primary-color)', fontWeight: 600 }}>
                  已选 {selectedIds.size} 项
                </span>
                <Button
                  variant="ghost"
                  size="sm"
                  onClick={clearSelection}
                >
                  取消选择
                </Button>
                <Button
                  variant="success"
                  size="sm"
                  onClick={handleBatchStart}
                >
                  <Play size={13} />
                  批量启动
                </Button>
                <Button
                  variant="danger"
                  size="sm"
                  onClick={handleBatchStop}
                >
                  <Square size={13} />
                  批量停止
                </Button>
              </>
            )}
            <Button
              variant="ghost"
              size="sm"
              onClick={() => toggleSort('name')}
              title="按名称排序"
            >
              <ArrowUpDown size={14} />
              名称{sortBy === 'name' && (sortOrder === 'asc' ? '↑' : '↓')}
            </Button>
            <Button
              variant="ghost"
              size="sm"
              onClick={() => {
                if (agents.length > 0) {
                  exportToCSV(agents.map(a => ({
                    名称: a.name, 类型: a.type, 状态: a.status,
                    描述: a.description || '', 创建时间: a.created_at || '',
                  })), 'agentos_agents');
                }
              }}
              title="导出列表"
            >
              <Download size={14} />
            </Button>
          </div>
        </div>
      </Card>

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
              padding: "10px 16px", borderRadius: "8px",
              background: "var(--bg-secondary)",
              border: `1px solid var(--border-subtle)`,
              display: "flex", alignItems: "center", gap: "10px",
              minWidth: "140px",
              cursor: "pointer",
              transition: "all 0.2s ease",
            }}>
              <div style={{
                width: "32px", height: "32px", borderRadius: "6px",
                background: config.color,
                display: "flex", alignItems: "center", justifyContent: "center",
                flexShrink: 0,
              }}>
                <IconComp size={16} color="white" />
              </div>
              <div>
                <div style={{ fontSize: "12px", color: "var(--text-muted)" }}>{config.label}</div>
                <div style={{ fontSize: "17px", fontWeight: 600, color: config.color, lineHeight: 1.2 }}>{count}</div>
              </div>
            </div>
          );
        })}
      </div>

      {/* Main Content Grid */}
      <div style={{ display: "grid", gridTemplateColumns: selectedAgent ? "1fr 340px" : "1fr", gap: "20px" }}>
        {/* Agent Cards Grid */}
        <div>
          {loading ? (
            <div style={{ textAlign: "center", padding: "48px" }}>
              <div className="loading-spinner" />
            </div>
          ) : filteredAndSortedAgents.length === 0 ? (
            <Card>
              <div style={{ textAlign: "center", padding: "48px" }}>
                <Bot size={56} style={{ opacity: 0.25 }} />
                <div style={{ fontSize: "15px", fontWeight: 500, margin: "16px 0 8px 0" }}>
                  {t.agents.noAgentsRegistered}
                </div>
                <div style={{ fontSize: "13px", color: "var(--text-muted)", marginBottom: "24px" }}>
                  {t.agents.noAgentsHint}
                </div>
                <Button
                  variant="primary"
                  onClick={() => setShowRegisterModal(true)}
                >
                  <Plus size={16} />
                  {t.agents.registerNew}
                </Button>
              </div>
            </Card>
          ) : (
            <div style={{
              display: "grid",
              gridTemplateColumns: "repeat(auto-fill, minmax(300px, 1fr))",
              gap: "16px",
            }}>
              {filteredAndSortedAgents.map((agent: AgentInfo) => {
                const config = getAgentConfig(agent.type || "general");
                const IconComp = config.icon;
                const isSelected = selectedAgent?.name === agent.name;
                const isChecked = selectedIds.has(agent.id);

                return (
                  <Card key={agent.name} style={{
                    border: `2px solid`,
                    borderColor: isSelected ? config.color : isChecked ? 'var(--primary-color)' : "var(--border-subtle)",
                    background: isSelected ? `${config.color}06` : isChecked ? 'var(--primary-light)' : "var(--bg-secondary)",
                    cursor: "pointer",
                  }} onClick={() => setSelectedAgent(isSelected ? null : agent)}>
                    {/* Selection Checkbox */}
                    <div
                      onClick={(e) => { e.stopPropagation(); toggleSelectAgent(agent.id); }}
                      style={{
                        position: 'absolute', top: '12px', left: '12px',
                        width: '20px', height: '20px', borderRadius: '4px',
                        border: `2px solid ${isChecked ? 'var(--primary-color)' : 'var(--border-color)'}`,
                        background: isChecked ? 'var(--primary-color)' : 'transparent',
                        display: 'flex', alignItems: 'center', justifyContent: 'center',
                        cursor: 'pointer', zIndex: 2,
                        transition: 'all 0.2s ease',
                      }}
                    >
                      {isChecked && <CheckSquare size={12} color="white" />}
                    </div>

                    <div style={{ display: "flex", alignItems: "flex-start", gap: "14px" }}>
                      <div style={{
                        width: "40px", height: "40px", borderRadius: "6px",
                        background: config.color,
                        display: "flex", alignItems: "center", justifyContent: "center",
                        flexShrink: 0,
                      }}>
                        <IconComp size={20} color="white" />
                      </div>

                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "4px" }}>
                          <span style={{ fontWeight: 600, fontSize: "14px" }}>{agent.name}</span>
                          {isSelected && <ChevronRight size={14} style={{ color: config.color }} />}
                        </div>

                        <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "6px" }}>
                          <span style={{
                            background: "var(--bg-tertiary)",
                            color: config.color,
                            fontWeight: 500,
                            fontSize: "11px",
                            padding: "2px 8px",
                            borderRadius: "4px",
                          }}>
                            {config.label}
                          </span>
                        </div>

                        {agent.description && (
                          <p style={{
                            fontSize: "12px", color: "var(--text-secondary)",
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
                        {agent.status === "active" || agent.status === "running" ? (
                          <>
                            <span style={{
                              width: "6px", height: "6px", borderRadius: "50%",
                              background: "#22c55e",
                            }} />
                            <span style={{ fontSize: "12px", color: "#22c55e", fontWeight: 500 }}>运行中</span>
                          </>
                        ) : (
                          <>
                            <span style={{ width: "6px", height: "6px", borderRadius: "50%", background: "#94a3b8" }} />
                            <span style={{ fontSize: "12px", color: "var(--text-muted)" }}>空闲</span>
                          </>
                        )}
                      </div>
                      <div style={{ display: "flex", gap: "4px" }}>
                        {(agent.status === "idle" || agent.status === "stopped") && (
                          <Button
                            variant="ghost"
                            size="sm"
                            onClick={(e) => { e.stopPropagation(); handleStartAgent(agent.id); }}
                            disabled={actionLoading === (agent.id + "-start")}
                            title="启动智能体"
                          >
                            {actionLoading === (agent.id + "-start") ? <Loader2 size={12} className="spin" /> : <Play size={12} />}
                          </Button>
                        )}
                        {(agent.status === "running" || agent.status === "active") && (
                          <Button
                            variant="ghost"
                            size="sm"
                            onClick={(e) => { e.stopPropagation(); handleStopAgent(agent.id); }}
                            disabled={actionLoading === (agent.id + "-stop")}
                            title="停止智能体"
                            style={{ color: "#ef4444" }}
                          >
                            {actionLoading === (agent.id + "-stop") ? <Loader2 size={12} className="spin" /> : <Square size={12} />}
                          </Button>
                        )}
                        <Button
                          variant="ghost"
                          size="sm"
                          onClick={(e) => { e.stopPropagation(); setShowConfigModal(agent.id); }}
                          title="配置"
                        >
                          <Shield size={12} />
                        </Button>
                      </div>
                    </div>
                  </Card>
                );
              })}
            </div>
          )}
        </div>

        {/* Detail Panel */}
        {selectedAgent && (
          <Card style={{
            height: "fit-content",
            position: "sticky",
            top: "88px",
          }}>
            {(() => {
              const config = getAgentConfig(selectedAgent.type || "general");
              const IconComp = config.icon;

              return (
                <>
                  <div style={{
                    padding: "16px", borderBottom: "1px solid var(--border-subtle)",
                    background: "var(--bg-tertiary)",
                  }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                      <div style={{
                        width: "48px", height: "48px", borderRadius: "6px",
                        background: config.color,
                        display: "flex", alignItems: "center", justifyContent: "center",
                      }}>
                        <IconComp size="24" color="white" />
                      </div>
                      <div>
                        <h3 style={{ margin: 0, fontSize: "16px" }}>{selectedAgent.name}</h3>
                        <span style={{
                          background: "var(--bg-secondary)",
                          color: config.color,
                          fontSize: "11px",
                          padding: "2px 8px",
                          borderRadius: "4px",
                        }}>
                          {config.label}
                        </span>
                      </div>
                    </div>
                  </div>

                  <div style={{ padding: "16px", display: "flex", flexDirection: "column", gap: "16px" }}>
                    <DetailRow icon={<Star size={14} />} label="类型" value={selectedAgent.type} />
                    <DetailRow icon={<Activity size={14} />} label="状态" value={
                      <span style={{ color: selectedAgent.status === "active" ? "#22c55e" : "var(--text-muted)" }}>
                        {selectedAgent.status === "active" ? "运行中" : "空闲"}
                      </span>
                    } />
                    <DetailRow icon={<Clock size={14} />} label="描述" value={selectedAgent.description || "—"} />

                    <div style={{ marginTop: "8px", display: "flex", flexDirection: "column", gap: "8px" }}>
                      <Button
                        variant="primary"
                        onClick={async () => {
                          if (!selectedAgent) return;
                          try {
                            await sdk.startAgent(selectedAgent.id);
                            success("启动成功", `智能体 "${selectedAgent.name}" 启动指令已发送`);
                            loadAgents();
                          } catch (err) { error("启动失败", `无法启动智能体: ${err}`); }
                        }}
                      >
                        启动智能体
                      </Button>
                      <Button
                        variant="secondary"
                        onClick={() => {
                          if (selectedAgent) navigate(`/agents?id=${selectedAgent.id}`);
                        }}
                      >
                        <ExternalLink size={14} />
                        查看详情
                      </Button>
                    </div>
                  </div>
                </>
              );
            })()}
          </Card>
        )}
      </div>

      {/* Register Modal */}
      {showRegisterModal && (
        <RegisterModal
          onClose={() => setShowRegisterModal(false)}
          onRegister={handleRegister}
        />
      )}
    </PageLayout>
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
      <Card style={{ maxWidth: "480px", margin: "0 auto" }} onClick={(e) => e.stopPropagation()}>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", padding: "16px 20px", borderBottom: "1px solid var(--border-subtle)" }}>
          <h2 style={{ margin: 0, fontSize: "18px", fontWeight: 600 }}>{t.agents.registerNew}</h2>
          <button
            onClick={onClose}
            style={{
              background: "none",
              border: "none",
              fontSize: "20px",
              cursor: "pointer",
              color: "var(--text-muted)",
              padding: "4px",
              borderRadius: "4px",
              transition: "all 0.2s ease",
            }}
            onMouseEnter={(e) => e.currentTarget.style.background = "var(--bg-tertiary)"}
            onMouseLeave={(e) => e.currentTarget.style.background = "none"}
          >
            ×
          </button>
        </div>

        {step === "info" ? (
          <div style={{ padding: "24px", display: "flex", flexDirection: "column", gap: "20px" }}>
            <div style={{ textAlign: "center", marginBottom: "8px" }}>
              <div style={{
                width: "56px", height: "56px", borderRadius: "12px",
                background: "var(--primary-color)",
                display: "flex", alignItems: "center", justifyContent: "center",
                margin: "0 auto 12px",
              }}>
                <Plus size={24} color="white" />
              </div>
              <p style={{ color: "var(--text-secondary)", fontSize: "14px", margin: 0 }}>
                创建一个新的 AI 智能体，赋予它独特的能力和角色
              </p>
            </div>

            <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
              <label style={{ fontSize: "14px", fontWeight: 500 }}>{t.agents.agentName}</label>
              <Input
                value={agentName}
                onChange={(e) => setAgentName(e.target.value)}
                placeholder="例如：Research Assistant、Code Reviewer..."
                onKeyDown={(e) => e.key === "Enter" && handleSubmit()}
                autoFocus
              />
              <p style={{ fontSize: "12px", color: "var(--text-muted)", margin: 0 }}>{t.agents.agentNameHelp}</p>
            </div>

            <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
              <label style={{ fontSize: "14px", fontWeight: 500 }}>{t.agents.agentType}</label>
              <div style={{
                display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: "12px",
              }}>
                {Object.entries(agentTypeConfig).map(([key, cfg]) => {
                  const IconComp = cfg.icon;
                  const isActive = agentType === key;
                  return (
                    <div
                      key={key}
                      onClick={() => setAgentType(key)}
                      style={{
                        padding: "16px", borderRadius: "8px",
                        border: `2px solid`, borderColor: isActive ? cfg.color : "var(--border-subtle)",
                        background: isActive ? `${cfg.color}06` : "var(--bg-tertiary)",
                        cursor: "pointer",
                        transition: "all 0.2s ease",
                        display: "flex", alignItems: "center", gap: "12px",
                      }}
                    >
                      <div style={{
                        width: "32px", height: "32px", borderRadius: "6px",
                        background: isActive ? cfg.color : "var(--bg-secondary)",
                        display: "flex", alignItems: "center", justifyContent: "center",
                        flexShrink: 0,
                      }}>
                        <IconComp size={16} color={isActive ? "white" : "var(--text-muted)"} />
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

            <div style={{ display: "flex", gap: "12px", marginTop: "8px" }}>
              <Button
                variant="secondary"
                onClick={onClose}
                style={{ flex: 1 }}
              >
                取消
              </Button>
              <Button
                variant="primary"
                onClick={handleSubmit}
                disabled={!agentName.trim() || submitting}
                style={{ flex: 1 }}
              >
                下一步 <ChevronRight size={16} />
              </Button>
            </div>
          </div>
        ) : (
          <div style={{ padding: "24px", display: "flex", flexDirection: "column", gap: "20px", alignItems: "center", textAlign: "center" }}>
            <div style={{
              width: "48px", height: "48px", borderRadius: "24px",
              background: "#22c55e",
              display: "flex", alignItems: "center", justifyContent: "center",
            }}>
              <CheckCircle2 size={24} color="white" />
            </div>
            <div>
              <h3 style={{ margin: "0 0 8px 0", fontSize: "16px", fontWeight: 600 }}>确认注册</h3>
              <p style={{ color: "var(--text-secondary)", fontSize: "14px", margin: 0 }}>
                即将注册以下智能体：
              </p>
            </div>

            <Card style={{ width: "100%", background: "var(--bg-tertiary)" }}>
              <div style={{ display: "grid", gridTemplateColumns: "100px 1fr", gap: "12px", fontSize: "14px" }}>
                <span style={{ color: "var(--text-muted)" }}>名称</span>
                <strong>{agentName}</strong>
                <span style={{ color: "var(--text-muted)" }}>类型</span>
                <span>
                  <span style={{
                    background: "var(--bg-secondary)",
                    color: agentTypeConfig[agentType]?.color,
                    fontSize: "11px",
                    padding: "2px 8px",
                    borderRadius: "4px",
                  }}>
                    {agentTypeConfig[agentType]?.label}
                  </span>
                </span>
              </div>
            </Card>

            <div style={{ display: "flex", gap: "12px", width: "100%" }}>
              <Button
                variant="secondary"
                onClick={() => setStep("info")}
                style={{ flex: 1 }}
              >
                返回修改
              </Button>
              <Button
                variant="success"
                onClick={handleSubmit}
                disabled={submitting}
                style={{ flex: 1 }}
              >
                {submitting ? <Loader2 size={16} className="spin" /> : <CheckCircle2 size={16} />}
                确认注册
              </Button>
            </div>
          </div>
        )}
      </Card>
    </div>
  );
}

export default Agents;
