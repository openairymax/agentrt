import React, { useState, useEffect } from 'react';
import {
  Key,
  Server,
  CheckCircle2,
  XCircle,
  Loader2,
  Eye,
  EyeOff,
  Plus,
  Trash2,
  Zap,
  DollarSign,
  Info,
  Cpu,
  Globe,
  Power,
  Settings2,
  ChevronDown,
  ChevronUp,
} from 'lucide-react';
import { invoke } from '../utils/tauriCompat';
import { useI18n } from '../i18n';

interface LLMProvider {
  id: string;
  name: string;
  type: 'openai' | 'azure' | 'anthropic' | 'local' | 'custom';
  apiKey: string;
  baseUrl?: string;
  models: string[];
  selectedModel: string;
  enabled: boolean;
  lastTested?: string;
  status: 'unknown' | 'connected' | 'failed';
}

const PROVIDER_CONFIGS = [
  { type: 'openai' as const, name: 'OpenAI', icon: '🤖', color: '#10a37f', defaultBaseUrl: 'https://api.openai.com/v1', models: ['gpt-4o', 'gpt-4o-mini', 'gpt-4-turbo', 'gpt-4', 'gpt-3.5-turbo'], pricing: { 'gpt-4o': { input: 2.5, output: 10 }, 'gpt-4o-mini': { input: 0.15, output: 0.6 } } },
  { type: 'azure' as const, name: 'Azure OpenAI', icon: '☁️', color: '#0078d4', defaultBaseUrl: '', models: ['gpt-4o', 'gpt-4-turbo', 'gpt-4'], pricing: {} },
  { type: 'anthropic' as const, name: 'Anthropic Claude', icon: '🧠', color: '#d97706', defaultBaseUrl: 'https://api.anthropic.com/v1', models: ['claude-3-5-sonnet-20241022', 'claude-3-5-haiku-20241022', 'claude-3-opus-20240229'], pricing: { 'claude-3-5-sonnet-20241022': { input: 3, output: 15 } } },
  { type: 'local' as const, name: 'Local (Ollama)', icon: '🏠', color: '#22c55e', defaultBaseUrl: 'http://localhost:11434/v1', models: ['llama3.2', 'llama3.1', 'mistral', 'codellama', 'qwen2.5'], pricing: {} },
  { type: 'custom' as const, name: 'Custom API', icon: '🔧', color: '#8b5cf6', defaultBaseUrl: '', models: [], pricing: {} },
];

const LLMConfig: React.FC = () => {
  const { t } = useI18n();
  const [providers, setProviders] = useState<LLMProvider[]>([]);
  const [loading, setLoading] = useState(true);
  const [testingId, setTestingId] = useState<string | null>(null);
  const [showAddModal, setShowAddModal] = useState(false);
  const [editingId, setEditingId] = useState<string | null>(null);
  const [showApiKeys, setShowApiKeys] = useState<Record<string, boolean>>({});

  useEffect(() => {
    loadProviders();
  }, []);

  const loadProviders = async () => {
    setLoading(true);
    try {
      const saved = localStorage.getItem('agentos-llm-providers');
      if (saved) {
        setProviders(JSON.parse(saved));
      } else {
        const defaults: LLMProvider[] = [
          { id: 'openai-default', name: 'OpenAI', type: 'openai', apiKey: '', models: PROVIDER_CONFIGS[0].models, selectedModel: 'gpt-4o-mini', enabled: false, status: 'unknown' },
          { id: 'anthropic-default', name: 'Anthropic Claude', type: 'anthropic', apiKey: '', models: PROVIDER_CONFIGS[2].models, selectedModel: 'claude-3-5-sonnet-20241022', enabled: false, status: 'unknown' },
        ];
        setProviders(defaults);
      }
    } catch (error) {
      console.error('Failed to load providers:', error);
    } finally {
      setLoading(false);
    }
  };

  const saveProviders = (updated: LLMProvider[]) => {
    setProviders(updated);
    localStorage.setItem('agentos-llm-providers', JSON.stringify(updated));
  };

  const testConnection = async (provider: LLMProvider) => {
    if (!provider.apiKey && provider.type !== 'local') {
      alert(t.llmConfig.enterApiKey);
      return;
    }
    setTestingId(provider.id);
    try {
      const result = await invoke<{ success: boolean; message: string; models?: string[] }>('test_llm_connection', {
        provider: provider.type, apiKey: provider.apiKey, baseUrl: provider.baseUrl, model: provider.selectedModel,
      });
      const updated = providers.map(p =>
        p.id === provider.id ? { ...p, status: result.success ? 'connected' as const : 'failed' as const, lastTested: new Date().toISOString() } : p
      );
      saveProviders(updated);
      alert(result.success ? t.llmConfig.connectionSuccess.replace('{provider}', provider.name) : t.llmConfig.connectionFailed.replace('{error}', result.message));
    } catch (error) {
      const updated = providers.map(p => p.id === provider.id ? { ...p, status: 'failed' as const, lastTested: new Date().toISOString() } : p);
      saveProviders(updated);
    } finally {
      setTestingId(null);
    }
  };

  const toggleProvider = (id: string) => {
    saveProviders(providers.map(p => p.id === id ? { ...p, enabled: !p.enabled } : p));
  };

  const deleteProvider = (id: string) => {
    if (!confirm(t.llmConfig.deleteConfirm)) return;
    saveProviders(providers.filter(p => p.id !== id));
  };

  const updateProvider = (id: string, updates: Partial<LLMProvider>) => {
    saveProviders(providers.map(p => p.id === id ? { ...p, ...updates } : p));
  };

  const addProvider = (type: LLMProvider['type']) => {
    const config = PROVIDER_CONFIGS.find(c => c.type === type);
    if (!config) return;
    const newProv: LLMProvider = {
      id: `${type}-${Date.now()}`, name: config.name, type, apiKey: '',
      baseUrl: config.defaultBaseUrl, models: config.models, selectedModel: config.models[0] || '',
      enabled: false, status: 'unknown',
    };
    saveProviders([...providers, newProv]);
    setShowAddModal(false);
    setEditingId(newProv.id);
  };

  const getPricing = (model: string, type: string) => {
    const config = PROVIDER_CONFIGS.find(c => c.type === type);
    return config?.pricing[model as keyof typeof config.pricing] || null;
  };

  const getStatusBadge = (status: LLMProvider['status']) => {
    switch (status) {
      case 'connected': return <span className="badge status-running"><CheckCircle2 size={12} />{t.llmConfig.connected}</span>;
      case 'failed': return <span className="badge status-error"><XCircle size={12} />{t.llmConfig.failed}</span>;
      default: return <span className="badge status-idle">{t.llmConfig.notTested}</span>;
    }
  };

  const enabledCount = providers.filter(p => p.enabled).length;

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header" style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start" }}>
        <div>
          <h1>{t.llmConfig.title}</h1>
          <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>{t.llmConfig.description}</p>
        </div>
        <button className="btn btn-primary" onClick={() => setShowAddModal(true)}>
          <Plus size={16} />
          {t.llmConfig.addProvider}
        </button>
      </div>

      {/* Active Providers Banner */}
      {enabledCount > 0 && (
        <div className="card card-elevated" style={{ marginBottom: "24px", background: "rgba(34, 197, 94, 0.08)", borderColor: "rgba(34, 197, 94, 0.3)", padding: "16px 20px", display: "flex", alignItems: "center", gap: "14px" }}>
          <div style={{ width: "40px", height: "40px", borderRadius: "var(--radius-md)", background: "rgba(34, 197, 94, 0.15)", display: "flex", alignItems: "center", justifyContent: "center" }}>
            <Zap size={20} color="#22c55e" />
          </div>
          <div style={{ flex: 1 }}>
            <div style={{ fontWeight: 600, color: "#22c55e", fontSize: "14px" }}>{t.llmConfig.activeProviders}</div>
            <div style={{ fontSize: "13px", color: "var(--text-secondary)", marginTop: "2px" }}>
              {providers.filter(p => p.enabled).map(p => `${p.name} (${p.selectedModel})`).join(' • ')}
            </div>
          </div>
          <span className="tag" style={{ background: "rgba(34, 197, 94, 0.15)", color: "#22c55e", fontSize: "13px", padding: "6px 14px" }}>
            {enabledCount} active
          </span>
        </div>
      )}

      {/* Providers Grid */}
      <div className="card card-elevated">
        <h3 className="card-title">
          <Key size={18} />
          {t.llmConfig.providers}
          <span className="tag" style={{ marginLeft: "auto" }}>{providers.length}</span>
        </h3>

        {loading ? (
          <div style={{ textAlign: "center", padding: "60px" }}><div className="loading-spinner" /></div>
        ) : providers.length === 0 ? (
          <div className="empty-state">
            <div className="empty-state-icon">🔑</div>
            <div className="empty-state-text">{t.llmConfig.noProviders}</div>
            <div className="empty-state-hint">{t.llmConfig.noProvidersHint}</div>
          </div>
        ) : (
          <div style={{
            display: "grid",
            gridTemplateColumns: "repeat(auto-fill, minmax(420px, 1fr))",
            gap: "16px",
          }}>
            {providers.map((provider) => {
              const config = PROVIDER_CONFIGS.find(c => c.type === provider.type);
              const isEditing = editingId === provider.id;

              return (
                <div
                  key={provider.id}
                  className="card"
                  style={{
                    padding: "20px",
                    borderLeft: `4px solid ${provider.enabled ? (config?.color || 'var(--primary-color)') : 'var(--border-subtle)'}`,
                    background: provider.enabled ? "rgba(99, 102, 241, 0.04)" : "var(--bg-tertiary)",
                    transition: "all var(--transition-fast)",
                  }}
                >
                  {/* Header */}
                  <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", marginBottom: "14px" }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                      <div style={{
                        width: "44px", height: "44px", borderRadius: "var(--radius-md)",
                        background: `${config?.color || 'var(--bg-secondary)'}20`,
                        display: "flex", alignItems: "center", justifyContent: "center",
                        fontSize: "22px",
                      }}>
                        {config?.icon || '🔌'}
                      </div>
                      <div>
                        <div style={{ fontWeight: 600, fontSize: "15px" }}>{provider.name}</div>
                        <div style={{ fontSize: "12px", color: "var(--text-muted)", marginTop: "2px", display: "flex", alignItems: "center", gap: "6px" }}>
                          {getStatusBadge(provider.status)}
                        </div>
                      </div>
                    </div>

                    <div style={{ display: "flex", gap: "6px" }}>
                      <button
                        className={`icon-btn ${provider.enabled ? '' : 'btn-ghost'}`}
                        onClick={() => toggleProvider(provider.id)}
                        title={provider.enabled ? "Disable" : "Enable"}
                        style={{ width: "32px", height: "32px", borderRadius: "var(--radius-sm)", background: provider.enabled ? "var(--primary-light)" : "transparent", color: provider.enabled ? "var(--primary-color)" : "var(--text-muted)" }}
                      >
                        <Power size={16} />
                      </button>
                      <button className="icon-btn" onClick={() => setEditingId(isEditing ? null : provider.id)} title="Settings">
                        <Settings2 size={16} />
                      </button>
                      <button className="icon-btn" onClick={() => deleteProvider(provider.id)} title="Delete" style={{ color: "var(--error-color)" }}>
                        <Trash2 size={16} />
                      </button>
                    </div>
                  </div>

                  {/* Expandable Settings */}
                  {isEditing && (
                    <div style={{ marginTop: "16px", paddingTop: "16px", borderTop: "1px solid var(--border-subtle)", animation: "slideDown 0.2s ease" }}>
                      {/* Model Selector */}
                      <div className="form-group" style={{ marginBottom: "12px" }}>
                        <label className="form-label" style={{ fontSize: "12px" }}>{t.llmConfig.model}</label>
                        <select
                          className="form-select"
                          value={provider.selectedModel}
                          onChange={(e) => updateProvider(provider.id, { selectedModel: e.target.value })}
                          style={{ fontSize: "13px" }}
                        >
                          {provider.models.map(m => <option key={m} value={m}>{m}</option>)}
                        </select>
                      </div>

                      {/* API Key */}
                      <div className="form-group" style={{ marginBottom: "12px" }}>
                        <label className="form-label" style={{ fontSize: "12px" }}>{t.llmConfig.apiKey}</label>
                        <div style={{ display: "flex", gap: "8px" }}>
                          <input
                            type={showApiKeys[provider.id] ? 'text' : 'password'}
                            className="form-input"
                            value={provider.apiKey}
                            onChange={(e) => updateProvider(provider.id, { apiKey: e.target.value })}
                            placeholder={t.llmConfig.apiKeyPlaceholder}
                            style={{ flex: 1, fontFamily: "'JetBrains Mono', monospace", fontSize: "13px" }}
                          />
                          <button
                            className="btn btn-secondary"
                            onClick={() => setShowApiKeys({ ...showApiKeys, [provider.id]: !showApiKeys[provider.id] })}
                            style={{ padding: "8px 12px" }}
                          >
                            {showApiKeys[provider.id] ? <EyeOff size={16} /> : <Eye size={16} />}
                          </button>
                        </div>
                      </div>

                      {/* Base URL (if not OpenAI) */}
                      {provider.type !== 'openai' && (
                        <div className="form-group" style={{ marginBottom: "12px" }}>
                          <label className="form-label" style={{ fontSize: "12px" }}>{t.llmConfig.baseUrl}</label>
                          <input
                            type="text"
                            className="form-input"
                            value={provider.baseUrl || ''}
                            onChange={(e) => updateProvider(provider.id, { baseUrl: e.target.value })}
                            placeholder={config?.defaultBaseUrl || ''}
                            style={{ fontFamily: "'JetBrains Mono', monospace", fontSize: "13px" }}
                          />
                        </div>
                      )}

                      {/* Pricing Info + Test Button */}
                      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginTop: "12px", paddingTop: "12px", borderTop: "1px solid var(--border-subtle)" }}>
                        <div style={{ fontSize: "12px", color: "var(--text-secondary)" }}>
                          {getPricing(provider.selectedModel, provider.type) ? (
                            <span><DollarSign size={13} style={{ display: "inline", verticalAlign: "middle" }} />${getPricing(provider.selectedModel, provider.type)!.input} / ${getPricing(provider.selectedModel, provider.type)!.output} per 1M tokens</span>
                          ) : provider.type === 'local' ? (
                            <span style={{ color: "#22c55e" }}><Cpu size={13} style={{ display: "inline", verticalAlign: "middle" }} />{t.llmConfig.localModel}</span>
                          ) : null}
                        </div>
                        <button
                          className="btn btn-primary btn-sm"
                          onClick={() => testConnection(provider)}
                          disabled={testingId === provider.id}
                        >
                          {testingId === provider.id ? <><Loader2 size={14} className="spin" />{t.llmConfig.testing}</> : <><Globe size={14} />{t.llmConfig.testConnection}</>}
                        </button>
                      </div>
                    </div>
                  )}

                  {!isEditing && (
                    <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", paddingTop: "12px", borderTop: "1px solid var(--border-subtle)" }}>
                      <span className="tag">{provider.selectedModel}</span>
                      <button className="btn btn-ghost btn-sm" onClick={() => setEditingId(provider.id)}>
                        <ChevronUp size={14} /> Configure
                      </button>
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* Help Card */}
      <div className="card card-elevated" style={{ marginTop: "24px" }}>
        <h3 className="card-title"><Info size={18} />{t.llmConfig.help}</h3>
        <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(240px, 1fr))", gap: "16px", color: "var(--text-secondary)", fontSize: "13px", lineHeight: "1.7" }}>
          {PROVIDER_CONFIGS.filter(c => c.type !== 'custom').map(cfg => (
            <div key={cfg.type}>
              <strong>{cfg.icon} {cfg.name}:</strong> {t.llmConfig[`help${cfg.type.charAt(0).toUpperCase() + cfg.type.slice(1)}` as keyof typeof t.llmConfig] || ''}
            </div>
          ))}
        </div>
      </div>

      {/* Add Provider Modal */}
      {showAddModal && (
        <div className="modal-overlay" onClick={() => setShowAddModal(false)}>
          <div className="modal-content" onClick={(e) => e.stopPropagation()}>
            <h2>{t.llmConfig.selectProvider}</h2>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: "12px" }}>
              {PROVIDER_CONFIGS.map((config) => (
                <button
                  key={config.type}
                  onClick={() => addProvider(config.type)}
                  style={{
                    display: "flex", alignItems: "center", gap: "14px", padding: "20px",
                    borderRadius: "var(--radius-lg)", border: "2px solid var(--border-color)",
                    background: "var(--bg-tertiary)", cursor: "pointer", transition: "all var(--transition-fast)",
                    textAlign: "left",
                  }}
                  onMouseEnter={(e) => { e.currentTarget.style.borderColor = "var(--primary-color)"; e.currentTarget.style.background = "var(--primary-light)"; }}
                  onMouseLeave={(e) => { e.currentTarget.style.borderColor = "var(--border-color)"; e.currentTarget.style.background = "var(--bg-tertiary)"; }}
                >
                  <span style={{ fontSize: "36px" }}>{config.icon}</span>
                  <div>
                    <div style={{ fontWeight: 600, fontSize: "15px" }}>{config.name}</div>
                    <div style={{ fontSize: "12px", color: "var(--text-muted)", marginTop: "4px" }}>
                      {config.models.length > 0 ? `${config.models.length} models` : t.llmConfig.customApi}
                    </div>
                  </div>
                </button>
              ))}
            </div>
            <button className="btn btn-secondary" style={{ width: "100%", marginTop: "20px" }} onClick={() => setShowAddModal(false)}>{t.common.cancel}</button>
          </div>
        </div>
      )}
    </div>
  );
};

export default LLMConfig;
