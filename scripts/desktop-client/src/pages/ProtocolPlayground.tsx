import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { useI18n } from '../i18n';
import { useAlert } from '../components/useAlert';
import {
  CheckCircle2,
  XCircle,
  Globe,
  Server,
  Code,
  Send,
  RefreshCw,
  Terminal,
  Settings,
  Zap,
  AlertCircle,
  Loader2,
} from 'lucide-react';

interface ProtocolInfo {
  id: string;
  name: string;
  description: string;
  version: string;
  status: string;
  endpoint: string;
  capabilities: string[];
  color: string;
  icon: string;
}

interface ProtocolCapability {
  name: string;
  description: string;
  params: string[];
}

interface ConnectionTestResult {
  protocol_id: string;
  endpoint: string;
  success: boolean;
  latency_ms: number;
  message: string;
  details?: Record<string, unknown>;
}

interface ProtocolMessageResponse {
  protocol: string;
  success: boolean;
  data: unknown;
  error?: string;
  latency_ms: number;
}

const ProtocolPlayground: React.FC = () => {
  const { t } = useI18n();
  const { error: alertError, success } = useAlert();
  const [protocols, setProtocols] = useState<ProtocolInfo[]>([]);
  const [selectedProtocol, setSelectedProtocol] = useState<string>('');
  const [capabilities, setCapabilities] = useState<ProtocolCapability[]>([]);
  const [testEndpoint, setTestEndpoint] = useState<string>('http://localhost:18789');
  const [testResult, setTestResult] = useState<ConnectionTestResult | null>(null);
  const [testing, setTesting] = useState(false);
  const [messageMethod, setMessageMethod] = useState<string>('');
  const [messageParams, setMessageParams] = useState<string>('{}');
  const [messageResponse, setMessageResponse] = useState<ProtocolMessageResponse | null>(null);
  const [sending, setSending] = useState(false);
  const [error, setError] = useState<string>('');

  useEffect(() => {
    loadProtocols();
  }, []);

  useEffect(() => {
    if (selectedProtocol) {
      loadCapabilities(selectedProtocol);
    }
  }, [selectedProtocol]);

  const loadProtocols = async () => {
    try {
      const result = await invoke<ProtocolInfo[]>('list_protocols');
      setProtocols(result);
      if (result.length > 0 && !selectedProtocol) {
        setSelectedProtocol(result[0].id);
      }
    } catch (e) {
      setError(`Failed to load protocols: ${e}`);
    }
  };

  const loadCapabilities = async (protocolId: string) => {
    try {
      const result = await invoke<ProtocolCapability[]>('get_protocol_capabilities', { protocolId });
      setCapabilities(result);
    } catch {
      setCapabilities([]);
    }
  };

  const testConnection = async () => {
    if (!selectedProtocol || !testEndpoint) return;
    setTesting(true);
    setTestResult(null);
    setError('');

    try {
      const result = await invoke<ConnectionTestResult>('test_protocol_connection', {
        protocolId: selectedProtocol,
        endpoint: testEndpoint,
      });
      setTestResult(result);
    } catch (e) {
      setError(`Connection test failed: ${e}`);
    } finally {
      setTesting(false);
    }
  };

  const sendMessage = async () => {
    if (!selectedProtocol || !messageMethod) return;
    setSending(true);
    setMessageResponse(null);
    setError('');

    let params;
    try {
      params = JSON.parse(messageParams || '{}');
    } catch {
      setError('Invalid JSON in parameters');
      setSending(false);
      return;
    }

    try {
      const result = await invoke<ProtocolMessageResponse>('send_protocol_message', {
        message: {
          protocol: selectedProtocol,
          method: messageMethod,
          params,
        },
      });
      setMessageResponse(result);
    } catch (e) {
      setError(`Message send failed: ${e}`);
    } finally {
      setSending(false);
    }
  };

  const selectCapability = (cap: ProtocolCapability) => {
    setMessageMethod(cap.name);
    const defaultParams: Record<string, string> = {};
    cap.params.forEach((p) => {
      defaultParams[p] = '';
    });
    setMessageParams(JSON.stringify(defaultParams, null, 2));
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
            <Globe size={20} color="white" />
          </div>
          <div>
            <h1>协议兼容性演示</h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: 0 }}>
              AgentOS UnifiedProtocol · 实时连接测试
            </p>
          </div>
        </div>
      </div>

      {/* Error Message */}
      {error && (
        <div style={{
          padding: "12px 16px", borderRadius: "var(--radius-md)",
          background: "rgba(239,68,68,0.06)", border: "1px solid rgba(239,68,68,0.12)",
          color: "#ef4444", fontSize: "13px", marginBottom: "16px",
          display: "flex", alignItems: "center", gap: "8px",
        }}>
          <AlertCircle size={16} />
          {error}
        </div>
      )}

      {/* Protocol Cards */}
      <div style={{
        display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(280px, 1fr))",
        gap: "16px", marginBottom: "20px",
      }}>
        {protocols.map((proto) => (
          <div
            key={proto.id}
            onClick={() => setSelectedProtocol(proto.id)}
            style={{
              padding: "20px", borderRadius: "var(--radius-lg)",
              border: `2px solid ${selectedProtocol === proto.id ? proto.color : "var(--border-subtle)"}`,
              background: selectedProtocol === proto.id ? `${proto.color}06` : "var(--bg-secondary)",
              cursor: "pointer", transition: "all 0.2s ease",
              boxShadow: 'var(--shadow-sm)',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.transform = 'translateY(-2px)';
              e.currentTarget.style.boxShadow = 'var(--shadow-md)';
              e.currentTarget.style.borderColor = selectedProtocol === proto.id ? proto.color : 'var(--border-color)';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.transform = 'translateY(0)';
              e.currentTarget.style.boxShadow = 'var(--shadow-sm)';
              e.currentTarget.style.borderColor = selectedProtocol === proto.id ? proto.color : "var(--border-subtle)";
            }}
          >
            <div style={{ display: "flex", alignItems: "center", gap: "12px", marginBottom: "12px" }}>
              <div style={{
                width: "42px", height: "42px", borderRadius: "var(--radius-md)",
                background: `linear-gradient(135deg,${proto.color},${proto.color}80)`,
                display: "flex", alignItems: "center", justifyContent: "center",
                boxShadow: `0 4px 12px ${proto.color}25`,
              }}>
                <Settings size={18} color="white" />
              </div>
              <div style={{ flex: 1 }}>
                <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                  <span style={{ fontWeight: 600, fontSize: "15px", color: "var(--text-primary)" }}>
                    {proto.name}
                  </span>
                  {proto.status === 'active' ? (
                    <CheckCircle2 size={14} color="#22c55e" />
                  ) : (
                    <XCircle size={14} color="var(--text-muted)" />
                  )}
                </div>
                <div style={{ fontSize: "11.5px", color: "var(--text-muted)", marginTop: "2px" }}>
                  v{proto.version}
                </div>
              </div>
            </div>
            <p style={{ fontSize: "12.5px", color: "var(--text-secondary)", margin: "0 0 12px 0", lineHeight: 1.4 }}>
              {proto.description}
            </p>
            <div style={{ display: "flex", alignItems: "center", gap: "6px" }}>
              <div style={{
                width: "6px", height: "6px", borderRadius: "50%",
                background: proto.status === 'active' ? "#22c55e" : "var(--text-muted)",
              }} />
              <span style={{ fontSize: "11.5px", color: "var(--text-muted)" }}>
                {proto.status} · {proto.endpoint}
              </span>
            </div>
          </div>
        ))}
      </div>

      {/* Main Content */}
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px", marginBottom: "20px" }}>
        {/* Connection Test */}
        <div className="card card-elevated" style={{ padding: "20px" }}>
          <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
            <div style={{
              width: "32px", height: "32px", borderRadius: "var(--radius-sm)",
              background: "rgba(99,102,241,0.1)", display: "flex", alignItems: "center", justifyContent: "center",
            }}>
              <Server size={16} color="#6366f1" />
            </div>
            <h2 style={{ fontSize: "15px", fontWeight: 600, margin: 0, color: "var(--text-primary)" }}>
              连接测试
            </h2>
          </div>
          <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
            <div>
              <label style={{ fontSize: "12px", fontWeight: 600, color: "var(--text-muted)", display: "block", marginBottom: "6px" }}>
                端点地址
              </label>
              <input
                type="text"
                value={testEndpoint}
                onChange={(e) => setTestEndpoint(e.target.value)}
                className="form-input"
                style={{ fontSize: "13px", padding: "10px 14px" }}
                placeholder="http://localhost:18789"
              />
            </div>
            <button
              onClick={testConnection}
              disabled={testing || !selectedProtocol}
              style={{
                padding: "12px 20px", borderRadius: "var(--radius-md)",
                border: "none", background: "var(--primary-color)",
                color: "white", fontWeight: 600, fontSize: "13px",
                cursor: testing || !selectedProtocol ? "not-allowed" : "pointer",
                opacity: testing || !selectedProtocol ? 0.5 : 1,
                transition: "all 0.2s ease",
                display: "flex", alignItems: "center", justifyContent: "center", gap: "8px",
              }}
              onMouseEnter={(e) => {
                if (!testing && selectedProtocol) {
                  e.currentTarget.style.background = "var(--primary-hover)";
                }
              }}
              onMouseLeave={(e) => {
                if (!testing && selectedProtocol) {
                  e.currentTarget.style.background = "var(--primary-color)";
                }
              }}
            >
              {testing ? (
                <>
                  <Loader2 size={16} className="spin" />
                  测试中...
                </>
              ) : (
                <>
                  <RefreshCw size={16} />
                  测试 {selectedProtocol.toUpperCase()} 连接
                </>
              )}
            </button>
            {testResult && (
              <div style={{
                padding: "14px", borderRadius: "var(--radius-md)",
                background: testResult.success ? "rgba(34,197,94,0.06)" : "rgba(239,68,68,0.06)",
                border: `1px solid ${testResult.success ? "rgba(34,197,94,0.12)" : "rgba(239,68,68,0.12)"}`,
                animation: "fadeIn 0.3s ease-out",
              }}>
                <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "8px" }}>
                  {testResult.success ? (
                    <CheckCircle2 size={16} color="#22c55e" />
                  ) : (
                    <XCircle size={16} color="#ef4444" />
                  )}
                  <span style={{ fontWeight: 600, fontSize: "13px", color: testResult.success ? "#22c55e" : "#ef4444" }}>
                    {testResult.message}
                  </span>
                </div>
                <div style={{ fontSize: "11.5px", color: "var(--text-muted)" }}>
                  延迟: {testResult.latency_ms}ms · 端点: {testResult.endpoint}
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Protocol Capabilities */}
        <div className="card card-elevated" style={{ padding: "20px" }}>
          <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
            <div style={{
              width: "32px", height: "32px", borderRadius: "var(--radius-sm)",
              background: "rgba(16,185,129,0.1)", display: "flex", alignItems: "center", justifyContent: "center",
            }}>
              <Zap size={16} color="#10b981" />
            </div>
            <h2 style={{ fontSize: "15px", fontWeight: 600, margin: 0, color: "var(--text-primary)" }}>
              协议能力
            </h2>
          </div>
          {capabilities.length === 0 ? (
            <div style={{
              display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center",
              padding: "32px 16px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)",
              border: "1px solid var(--border-subtle)",
            }}>
              <Settings size={32} style={{ color: "var(--text-muted)", opacity: 0.5, marginBottom: "12px" }} />
              <span style={{ fontSize: "13px", color: "var(--text-muted)" }}>
                选择协议查看可用方法
              </span>
            </div>
          ) : (
            <div style={{ maxHeight: "320px", overflowY: "auto", paddingRight: "8px" }}>
              {capabilities.map((cap) => (
                <div
                  key={cap.name}
                  onClick={() => selectCapability(cap)}
                  style={{
                    padding: "12px 14px", borderRadius: "var(--radius-md)",
                    border: `1px solid ${messageMethod === cap.name ? "var(--primary-light)" : "var(--border-subtle)"}`,
                    background: messageMethod === cap.name ? "var(--primary-light)" : "var(--bg-tertiary)",
                    cursor: "pointer", transition: "all 0.2s ease",
                    marginBottom: "8px",
                  }}
                  onMouseEnter={(e) => {
                    if (messageMethod !== cap.name) {
                      e.currentTarget.style.background = "var(--bg-secondary)";
                      e.currentTarget.style.borderColor = "var(--border-color)";
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (messageMethod !== cap.name) {
                      e.currentTarget.style.background = "var(--bg-tertiary)";
                      e.currentTarget.style.borderColor = "var(--border-subtle)";
                    }
                  }}
                >
                  <div style={{ fontFamily: "var(--font-mono)", fontWeight: 600, fontSize: "13px", color: "var(--text-primary)", marginBottom: "4px" }}>
                    {cap.name}
                  </div>
                  <div style={{ fontSize: "11.5px", color: "var(--text-secondary)" }}>
                    {cap.description}
                    {cap.params.length > 0 && (
                      <span style={{ color: "var(--text-muted)", marginLeft: "8px" }}>
                        参数: {cap.params.join(', ')}
                      </span>
                    )}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Message Sending */}
      <div className="card card-elevated" style={{ padding: "20px" }}>
        <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "16px" }}>
          <div style={{
            width: "32px", height: "32px", borderRadius: "var(--radius-sm)",
            background: "rgba(245,158,11,0.1)", display: "flex", alignItems: "center", justifyContent: "center",
          }}>
            <Send size={16} color="#f59e0b" />
          </div>
          <h2 style={{ fontSize: "15px", fontWeight: 600, margin: 0, color: "var(--text-primary)" }}>
            消息发送
          </h2>
        </div>
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px" }}>
          <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
            <div>
              <label style={{ fontSize: "12px", fontWeight: 600, color: "var(--text-muted)", display: "block", marginBottom: "6px" }}>
                方法名
              </label>
              <input
                type="text"
                value={messageMethod}
                onChange={(e) => setMessageMethod(e.target.value)}
                className="form-input"
                style={{ fontSize: "13px", fontFamily: "var(--font-mono)", padding: "10px 14px" }}
                placeholder="tools/list"
              />
            </div>
            <div>
              <label style={{ fontSize: "12px", fontWeight: 600, color: "var(--text-muted)", display: "block", marginBottom: "6px" }}>
                参数 (JSON)
              </label>
              <div style={{ position: "relative" }}>
                <textarea
                  value={messageParams}
                  onChange={(e) => setMessageParams(e.target.value)}
                  rows={6}
                  className="form-input"
                  style={{ fontSize: "13px", fontFamily: "var(--font-mono)", padding: "12px 14px", resize: "vertical" }}
                  placeholder='{"key": "value"}'
                />
              </div>
            </div>
            <button
              onClick={sendMessage}
              disabled={sending || !selectedProtocol || !messageMethod}
              style={{
                padding: "12px 20px", borderRadius: "var(--radius-md)",
                border: "none", background: "#10b981",
                color: "white", fontWeight: 600, fontSize: "13px",
                cursor: sending || !selectedProtocol || !messageMethod ? "not-allowed" : "pointer",
                opacity: sending || !selectedProtocol || !messageMethod ? 0.5 : 1,
                transition: "all 0.2s ease",
                display: "flex", alignItems: "center", justifyContent: "center", gap: "8px",
              }}
              onMouseEnter={(e) => {
                if (!sending && selectedProtocol && messageMethod) {
                  e.currentTarget.style.background = "#059669";
                }
              }}
              onMouseLeave={(e) => {
                if (!sending && selectedProtocol && messageMethod) {
                  e.currentTarget.style.background = "#10b981";
                }
              }}
            >
              {sending ? (
                <>
                  <Loader2 size={16} className="spin" />
                  发送中...
                </>
              ) : (
                <>
                  <Send size={16} />
                  发送 {selectedProtocol.toUpperCase()} 消息
                </>
              )}
            </button>
          </div>

          <div>
            <label style={{ fontSize: "12px", fontWeight: 600, color: "var(--text-muted)", display: "block", marginBottom: "6px" }}>
              响应
            </label>
            {messageResponse ? (
              <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
                <div style={{
                  display: "flex", alignItems: "center", gap: "8px", padding: "8px 12px",
                  borderRadius: "var(--radius-sm)",
                  background: messageResponse.success ? "rgba(34,197,94,0.06)" : "rgba(239,68,68,0.06)",
                  border: `1px solid ${messageResponse.success ? "rgba(34,197,94,0.12)" : "rgba(239,68,68,0.12)"}`,
                }}>
                  {messageResponse.success ? (
                    <CheckCircle2 size={14} color="#22c55e" />
                  ) : (
                    <XCircle size={14} color="#ef4444" />
                  )}
                  <span style={{ fontSize: "12px", fontWeight: 600, color: messageResponse.success ? "#22c55e" : "#ef4444" }}>
                    {messageResponse.protocol.toUpperCase()} · {messageResponse.latency_ms}ms
                  </span>
                </div>
                <div style={{
                  background: "#0a0a0f", borderRadius: "var(--radius-md)",
                  border: "1px solid var(--border-color)",
                  overflow: "hidden",
                }}>
                  <div style={{
                    background: "#111118", borderBottom: "1px solid var(--border-subtle)",
                    padding: "8px 12px", display: "flex", alignItems: "center", gap: "8px",
                  }}>
                    <Terminal size={12} style={{ color: "var(--text-muted)" }} />
                    <span style={{ fontSize: "11px", color: "var(--text-muted)", fontFamily: "var(--font-mono)" }}>
                      response.json
                    </span>
                  </div>
                  <pre style={{
                    padding: "14px", margin: 0, fontSize: "12px", fontFamily: "var(--font-mono)",
                    color: "var(--text-secondary)", lineHeight: 1.5, overflow: "auto",
                    maxHeight: "240px",
                  }}>
                    {JSON.stringify(
                      messageResponse.success ? messageResponse.data : { error: messageResponse.error },
                      null,
                      2
                    )}
                  </pre>
                </div>
              </div>
            ) : (
              <div style={{
                background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)",
                border: "1px solid var(--border-subtle)",
                display: "flex", flexDirection: "column", alignItems: "center",
                justifyContent: "center", padding: "48px 16px",
              }}>
                <Code size={32} style={{ color: "var(--text-muted)", opacity: 0.5, marginBottom: "12px" }} />
                <span style={{ fontSize: "13px", color: "var(--text-muted)" }}>
                  发送消息后查看响应
                </span>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default ProtocolPlayground;
