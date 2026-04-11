import React, { useState, useEffect } from "react";
import {
  Brain,
  Plus,
  CheckCircle2,
  XCircle,
  Loader2,
  ExternalLink,
  Zap,
  Shield,
  Globe,
  Cpu,
  Sparkles,
  Key,
  Eye,
  EyeOff,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import type { LLMProviderConfig } from "../services/agentos-sdk";
import { useI18n } from "../i18n";

const providerConfig: Record<string, { icon: typeof Brain; color: string; gradient: string; bgLight: string; label: string; models: string[] }> = {
  openai: {
    icon: Zap, color: "#10b981", gradient: "linear-gradient(135deg, #10b981, #34d399)",
    bgLight: "rgba(16,185,129,0.08)", label: "OpenAI",
    models: ["gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "gpt-3.5-turbo"],
  },
  anthropic: {
    icon: Brain, color: "#d97706", gradient: "linear-gradient(135deg, #d97706, #f59e0b)",
    bgLight: "rgba(217,119,6,0.08)", label: "Anthropic",
    models: ["claude-3.5-sonnet", "claude-3-opus", "claude-3-haiku"],
  },
  localai: {
    icon: Cpu, color: "#6366f1", gradient: "linear-gradient(135deg, #6366f1, #818cf8)",
    bgLight: "rgba(99,102,241,0.08)", label: "Local AI",
    models: ["llama-3-70b", "llama-3-8b", "mistral-7b", "qwen2-72b"],
  },
};

const pricingData: Record<string, Record<string, string>> = {
  openai: { "gpt-4o": "$5/$15", "gpt-4o-mini": "$0.15/$0.60", "gpt-4-turbo": "$30/$60", "gpt-3.5-turbo": "$0.50/$1.50" },
  anthropic: { "claude-3.5-sonnet": "$3/$15", "claude-3-opus": "$15/$75", "claude-3-haiku": "$0.25/$1.25" },
  localai: { "llama-3-70b": "Free (Local)", "llama-3-8b": "Free (Local)", "mistral-7b": "Free (Local)" },
};

const LLMConfig: React.FC = () => {
  const { t } = useI18n();
  const [providers, setProviders] = useState<LLMProviderConfig[]>([]);
  const [loading, setLoading] = useState(true);
  const [showAddModal, setShowAddModal] = useState(false);
  const [expandedProvider, setExpandedProvider] = useState<string | null>(null);
  const [testingId, setTestingId] = useState<string | null>(null);
  const [newProviderType, setNewProviderType] = useState("openai");
  const [newApiKey, setNewApiKey] = useState("");
  const [showApiKey, setShowApiKey] = useState(false);
  const [newBaseUrl, setNewBaseUrl] = useState("");
  const [newModel, setNewModel] = useState("gpt-4o");
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    loadProviders();
  }, []);

  const loadProviders = async () => {
    setLoading(true);
    try {
      const data = await sdk.listLLMProviders();
      setProviders(data || []);
    } catch (error) {
      console.error("Failed to load providers:", error);
    } finally {
      setLoading(false);
    }
  };

  const handleTest = async (providerId: string) => {
    setTestingId(providerId);
    try {
      const result = await sdk.testLLMConnection(providerId);
      alert(result.success ? `${t.llmConfig.testSuccess} (${result.latency_ms}ms)` : `${t.llmConfig.testFailed}: ${result.message}`);
    } catch (error) {
      alert(`${t.llmConfig.testFailed}: ${error}`);
    } finally {
      setTestingId(null);
    }
  };

  const handleSaveProvider = async () => {
    if (!newApiKey.trim()) return;
    setSaving(true);
    try {
      const saved = await sdk.saveLLMProvider({
        id: `${newProviderType}-${Date.now()}`,
        type: newProviderType as LLMProviderConfig["type"],
        name: providerConfig[newProviderType]?.label || newProviderType,
        api_key: newApiKey,
        base_url: newBaseUrl || (newProviderType === "openai" ? "https://api.openai.com/v1" : newProviderType === "anthropic" ? "https://api.anthropic.com/v1" : "http://localhost:8080/v1"),
        model: newModel,
        configured: true,
      });
      setProviders(prev => [...prev, saved]);
      setShowAddModal(false);
      setNewApiKey("");
      setNewBaseUrl("");
      alert("LLM 提供商配置已保存");
    } catch (error) {
      alert(`保存失败: ${error}`);
    } finally {
      setSaving(false);
    }
  };

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
          <div style={{
            width: "44px", height: "44px", borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#10b981,#34d399)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(16,185,129,0.35), 0 0 0 1px rgba(255,255,255,0.08) inset",
          }}>
            <Brain size={20} color="white" />
          </div>
          <div>
            <h1 style={{ display: "flex", alignItems: "center", gap: "10px" }}>
              {t.llmConfig.title}
              <span className={`badge ${providers.some(p => p.configured) ? 'status-running' : 'status-stopped'}`}>
                {providers.filter(p => p.configured).length}/{providers.length}
              </span>
            </h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: 0 }}>
              {t.llmConfig.subtitle}
            </p>
          </div>
        </div>
      </div>

      {/* Quick Setup Banner */}
      {!providers.some(p => p.configured) && (
        <div className="card card-elevated" style={{
          marginBottom: "20px", background: "var(--primary-gradient)",
          border: "none", color: "white", position: "relative", overflow: "hidden",
        }}>
          <div style={{
            position: "absolute", top: "-30px", right: "-20px",
            width: "160px", height: "160px", borderRadius: "50%",
            background: "rgba(255,255,255,0.07)",
          }} />
          <div style={{ display: "flex", alignItems: "center", gap: "14px", position: "relative" }}>
            <div style={{
              width: "48px", height: "48px", borderRadius: "var(--radius-md)",
              background: "rgba(255,255,255,0.15)",
              display: "flex", alignItems: "center", justifyContent: "center",
            }}>
              <Sparkles size={24} />
            </div>
            <div style={{ flex: 1 }}>
              <h3 style={{ margin: 0, fontSize: "16px" }}>配置 AI 模型以解锁全部功能</h3>
              <p style={{ opacity: 0.85, fontSize: "13px", margin: "4px 0 0 0" }}>
                添加 OpenAI、Anthropic 或本地模型提供商，让智能体具备强大的推理能力
              </p>
            </div>
            <button
              onClick={() => setShowAddModal(true)}
              style={{
                padding: "10px 22px", borderRadius: "var(--radius-md)",
                border: "none", background: "rgba(255,255,255,0.2)",
                color: "white", cursor: "pointer", fontWeight: 600, fontSize: "13.5px",
                display: "flex", alignItems: "center", gap: "6px",
                transition: "all var(--transition-fast)",
              }}
            >
              <Plus size={16} /> 立即配置
            </button>
          </div>
        </div>
      )}

      {/* Provider Cards Grid */}
      {loading ? (
        <div style={{ textAlign: "center", padding: "48px" }}>
          <div className="loading-spinner" />
        </div>
      ) : (
        <div style={{
          display: "grid",
          gridTemplateColumns: "repeat(auto-fill, minmax(360px, 1fr))",
          gap: "16px",
        }}>
          {Object.entries(providerConfig).map(([key, cfg], idx) => {
            const IconComp = cfg.icon;
            const provider = providers.find((p) => p.type === key);
            const isExpanded = expandedProvider === key;

            return (
              <div
                key={key}
                className="card-hover-lift"
                style={{
                  borderRadius: "var(--radius-lg)",
                  border: `1px solid ${provider?.configured ? `${cfg.color}35` : "var(--border-subtle)"}`,
                  overflow: "hidden",
                  animation: `staggerFadeIn 0.35s ease-out ${idx * 80}ms both`,
                  transition: "border-color var(--transition-fast)",
                }}
              >
                {/* Card Header */}
                <div
                  onClick={() => setExpandedProvider(isExpanded ? null : key)}
                  style={{
                    padding: "20px",
                    cursor: "pointer",
                    background: provider?.configured ? cfg.bgLight : "var(--bg-secondary)",
                    borderBottom: isExpanded ? "1px solid var(--border-subtle)" : "none",
                    transition: "background var(--transition-fast)",
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
                    <div style={{
                      width: "46px", height: "46px", borderRadius: "var(--radius-md)",
                      background: cfg.gradient,
                      display: "flex", alignItems: "center", justifyContent: "center",
                      flexShrink: 0, boxShadow: `0 4px 12px ${cfg.color}25`,
                    }}>
                      <IconComp size="22" color="white" />
                    </div>

                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                        <span style={{ fontWeight: 600, fontSize: "16px" }}>{cfg.label}</span>
                        {provider?.configured ? (
                          <CheckCircle2 size={16} color="#22c55e" />
                        ) : (
                          <XCircle size={16} color="#94a3b8" />
                        )}
                      </div>
                      <div style={{ fontSize: "12.5px", color: "var(--text-muted)", marginTop: "2px" }}>
                        {provider?.model || "未配置"} · {provider?.base_url || "—"}
                      </div>
                    </div>

                    <div style={{
                      width: "28px", height: "28px", borderRadius: "var(--radius-sm)",
                      background: "var(--bg-primary)", display: "flex", alignItems: "center", justifyContent: "center",
                      transform: isExpanded ? "rotate(180deg)" : "",
                      transition: "transform 0.25s ease-out",
                    }}>
                      <svg width="12" height="12" viewBox="0 0 12 12" fill="none">
                        <path d="M2 4L6 8L10 4" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" />
                      </svg>
                    </div>
                  </div>
                </div>

                {/* Expanded Content */}
                {isExpanded && (
                  <div style={{ padding: "20px", animation: "fadeIn 0.25s ease-out" }}>
                    {/* Models List */}
                    <div style={{ marginBottom: "16px" }}>
                      <label style={{ fontSize: "11.5px", fontWeight: 600, color: "var(--text-muted)", textTransform: "uppercase", letterSpacing: "0.05em", display: "block", marginBottom: "8px" }}>
                        支持的模型
                      </label>
                      <div style={{ display: "flex", flexDirection: "column", gap: "6px" }}>
                        {cfg.models.map(model => (
                          <div key={model} style={{
                            display: "flex", justifyContent: "space-between", alignItems: "center",
                            padding: "8px 12px", borderRadius: "var(--radius-sm)",
                            background: "var(--bg-tertiary)", border: "1px solid var(--border-subtle)",
                            fontSize: "13px",
                          }}>
                            <span style={{ fontFamily: "'JetBrains Mono', monospace", fontWeight: 500 }}>{model}</span>
                            <span style={{
                              fontSize: "11.5px", color: "var(--text-muted)",
                              fontFamily: "'JetBrains Mono', monospace",
                            }}>
                              {pricingData[key]?.[model] || "—"}
                            </span>
                          </div>
                        ))}
                      </div>
                    </div>

                    {/* Actions */}
                    <div style={{ display: "flex", gap: "8px" }}>
                      {provider?.configured && (
                        <button
                          className={`btn btn-secondary btn-block`}
                          onClick={(e) => { e.stopPropagation(); handleTest(provider.id); }}
                          disabled={testingId !== null}
                        >
                          {testingId === provider.id ? <Loader2 size={15} className="spin" /> : <Globe size={15} />}
                          测试连接
                        </button>
                      )}
                      <button
                        className={`btn ${provider?.configured ? "btn-ghost" : "btn-primary"} btn-block`}
                        onClick={(e) => { e.stopPropagation(); setShowAddModal(true); }}
                      >
                        {provider?.configured ? <ExternalLink size={15} /> : <Key size={15} />}
                        {provider?.configured ? "编辑配置" : "添加配置"}
                      </button>
                    </div>

                    {/* Status indicator when testing */}
                    {testingId !== null && provider && testingId === provider.id && (
                      <div style={{
                        marginTop: "12px",
                        padding: "10px 14px",
                        borderRadius: "var(--radius-md)",
                        background: "rgba(99,102,241,0.08)",
                        border: "1px solid rgba(99,102,241,0.15)",
                        display: "flex", alignItems: "center", gap: "8px", fontSize: "12.5px",
                        animation: "fadeIn 0.3s ease-out",
                      }}>
                        <Loader2 size={14} className="spin" style={{ color: "var(--primary-color)" }} />
                        正在测试与 {cfg.label} 的连接...
                      </div>
                    )}
                  </div>
                )}
              </div>
            );
          })}

          {/* Add Provider Card */}
          <div
            onClick={() => setShowAddModal(true)}
            className="card-hover-lift"
            style={{
              borderRadius: "var(--radius-lg)",
              border: "2px dashed var(--border-color)",
              display: "flex",
              flexDirection: "column",
              alignItems: "center",
              justifyContent: "center",
              minHeight: "180px",
              cursor: "pointer",
              transition: "all var(--transition-fast)",
              animation: "staggerFadeIn 0.35s ease-out 240ms both",
            }}
          >
            <div style={{
              width: "52px", height: "52px", borderRadius: "var(--radius-lg)",
              background: "var(--bg-tertiary)",
              display: "flex", alignItems: "center", justifyContent: "center",
              marginBottom: "12px",
            }}>
              <Plus size="24" color="var(--text-muted)" />
            </div>
            <span style={{ fontWeight: 600, fontSize: "14px", color: "var(--text-secondary)" }}>添加新提供商</span>
            <span style={{ fontSize: "12px", color: "var(--text-muted)", marginTop: "4px" }}>支持自定义 API 端点</span>
          </div>
        </div>
      )}

      {/* Info Banner */}
      <div className="card card-elevated" style={{ marginTop: "20px" }}>
        <div style={{
          display: "flex", alignItems: "center", gap: "12px",
          padding: "16px 20px", background: "var(--bg-tertiary)",
          borderRadius: "var(--radius-md)", border: "1px solid var(--border-subtle)",
        }}>
          <Shield size={20} style={{ color: "var(--primary-color)", flexShrink: 0 }} />
          <div>
            <strong style={{ fontSize: "13.5px" }}>安全提示</strong>
            <p style={{ fontSize: "12.5px", color: "var(--text-secondary)", margin: "4px 0 0 0", lineHeight: 1.5 }}>
              所有 API 密钥均存储在本地，不会上传至任何服务器。建议使用环境变量或密钥管理服务来保护您的凭据。
            </p>
          </div>
        </div>
      </div>

      {/* Add Modal */}
      {showAddModal && (
        <div className="modal-overlay" onClick={() => setShowAddModal(false)}>
          <div className="modal-content" style={{ maxWidth: "500px" }} onClick={(e) => e.stopPropagation()}>
            <div className="modal-header">
              <h2 className="modal-title">添加 LLM 提供商</h2>
              <button className="modal-close-btn" onClick={() => setShowAddModal(false)}>×</button>
            </div>
            <div style={{ padding: "24px", display: "flex", flexDirection: "column", gap: "16px" }}>
              <div className="form-group">
                <label className="form-label">提供商类型</label>
                <select className="form-select" value={newProviderType} onChange={(e) => setNewProviderType(e.target.value)}>
                  {Object.entries(providerConfig).map(([key, cfg]) => (
                    <option key={key} value={key}>{cfg.label}</option>
                  ))}
                </select>
              </div>
              <div className="form-group">
                <label className="form-label">API Key</label>
                <div style={{ position: "relative" }}>
                  <input type={showApiKey ? "text" : "password"} className="form-input" placeholder="sk-..." value={newApiKey} onChange={(e) => setNewApiKey(e.target.value)} style={{ paddingRight: "42px" }} />
                  <button style={{
                    position: "absolute", right: "10px", top: "50%", transform: "translateY(-50%)",
                    background: "none", border: "none", cursor: "pointer", color: "var(--text-muted)",
                  }} onClick={() => setShowApiKey(!showApiKey)}>
                    {showApiKey ? <EyeOff size={16} /> : <Eye size={16} />}
                  </button>
                </div>
              </div>
              <div className="form-group">
                <label className="form-label">Base URL（可选）</label>
                <input type="text" className="form-input" placeholder="https://api.openai.com/v1" value={newBaseUrl} onChange={(e) => setNewBaseUrl(e.target.value)} />
              </div>
              <div className="form-group">
                <label className="form-label">默认模型</label>
                <select className="form-select" value={newModel} onChange={(e) => setNewModel(e.target.value)}>
                  {(providerConfig[newProviderType]?.models || ["gpt-4o"]).map(m => (
                    <option key={m} value={m}>{m}</option>
                  ))}
                </select>
              </div>
              <div style={{ display: "flex", gap: "10px" }}>
                <button className="btn btn-secondary btn-lg" onClick={() => setShowAddModal(false)} style={{ flex: 1 }}>取消</button>
                <button className="btn btn-primary btn-lg" onClick={handleSaveProvider} disabled={!newApiKey.trim() || saving} style={{ flex: 1 }}>
                  {saving ? <Loader2 size={16} className="spin" /> : <Key size={16} />}
                  保存配置
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default LLMConfig;
