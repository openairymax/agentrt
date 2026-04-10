import React, { useState, useRef, useEffect, useCallback } from "react";
import {
  Send,
  Bot,
  User,
  Copy,
  Check,
  Sparkles,
  RotateCcw,
  Trash2,
  Zap,
  Brain,
  Database,
  Clock,
} from "lucide-react";
import sdk from "../services/agentos-sdk";

interface ChatMessage {
  id: string;
  role: "user" | "assistant" | "system";
  content: string;
  timestamp: Date;
  tokenCount?: number;
  toolCalls?: Array<{ name: string; args: string }>;
}

const SUGGESTIONS = [
  { icon: Zap, text: "启动所有服务", action: "启动服务集群" },
  { icon: Brain, text: "查看智能体列表", action: "列出已注册智能体" },
  { icon: Database, text: "查看记忆系统状态", action: "查询记忆存储情况" },
  { icon: Clock, text: "获取系统信息", action: "显示硬件配置" },
];

const AIChat: React.FC<{
  onSendMessage?: (message: string) => void;
  initialMessages?: ChatMessage[];
  model?: string;
  compact?: boolean;
}> = ({ onSendMessage, initialMessages, model = "gpt-4o", compact = false }) => {
  const [messages, setMessages] = useState<ChatMessage[]>(initialMessages || []);
  const [inputValue, setInputValue] = useState("");
  const [isLoading, setIsLoading] = useState(false);
  const [copiedId, setCopiedId] = useState<string | null>(null);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLTextAreaElement>(null);

  const scrollToBottom = () => {
    messagesEndRef.current?.scrollIntoView({ behavior: "smooth" });
  };

  useEffect(() => { scrollToBottom(); }, [messages]);

  const handleSend = useCallback(async (text?: string) => {
    const content = text || inputValue.trim();
    if (!content || isLoading) return;

    const userMessage: ChatMessage = {
      id: `msg-${Date.now()}`,
      role: "user",
      content,
      timestamp: new Date(),
      tokenCount: Math.ceil(content.length / 4),
    };

    setMessages(prev => [...prev, userMessage]);
    setInputValue("");
    setIsLoading(true);

    if (onSendMessage) onSendMessage(content);

    try {
      const allMessages = [...messages, userMessage].map(m => ({
        role: m.role as "user" | "assistant",
        content: m.content,
      }));

      const response = await sdk.chat({
        provider_id: "openai",
        messages: [
          { role: "system", content: `你是 AgentOS 智能助手，一个工业级 AI 操作系统的前端界面。你的职责是帮助用户管理 AgentOS 的各项功能，包括：服务管理、智能体注册、任务提交、LLM 配置、记忆系统等。请用中文回复，保持简洁专业。` },
          ...allMessages,
        ],
        model,
        temperature: 0.7,
        max_tokens: 2048,
      });

      const assistantMessage: ChatMessage = {
        id: `msg-${Date.now()}-resp`,
        role: "assistant",
        content: response.content,
        timestamp: new Date(),
        tokenCount: response.usage.completion_tokens,
      };

      setMessages(prev => [...prev, assistantMessage]);

      // Store conversation in memory
      try {
        await sdk.memoryStore({ type: "conversation", content: `用户: ${content} → 助手: ${response.content.slice(0, 100)}...`, source: "ai-chat" });
      } catch { /* memory store is best-effort */ }
    } catch (error) {
      console.error("Chat error:", error);
      setMessages(prev => [...prev, {
        id: `msg-${Date.now()}-error`,
        role: "assistant",
        content: "抱歉，AI 对话服务暂时不可用。请检查 LLM 配置是否正确，或稍后重试。",
        timestamp: new Date(),
      }]);
    } finally {
      setIsLoading(false);
    }
  }, [inputValue, isLoading, messages, model, onSendMessage]);

  const handleCopy = async (text: string, id: string) => {
    await navigator.clipboard.writeText(text);
    setCopiedId(id);
    setTimeout(() => setCopiedId(null), 2000);
  };

  const handleSuggestionClick = (suggestion: typeof SUGGESTIONS[0]) => {
    handleSend(suggestion.action);
  };

  const handleClear = () => {
    if (confirm("确定要清空所有对话记录吗？")) {
      setMessages([]);
    }
  };

  if (compact && messages.length === 0) return null;

  return (
    <div style={{
      display: "flex",
      flexDirection: "column",
      height: "100%",
      background: "var(--bg-primary)",
      borderRadius: compact ? "var(--radius-lg)" : undefined,
      overflow: "hidden",
    }}>
      {/* Messages Area */}
      <div
        style={{
          flex: 1,
          overflowY: "auto",
          padding: compact ? "16px" : "24px",
          display: "flex",
          flexDirection: "column",
          gap: "12px",
        }}
      >
        {messages.length === 0 ? (
          <div style={{
            flex: 1,
            display: "flex",
            flexDirection: "column",
            alignItems: "center",
            justifyContent: "center",
            gap: "20px",
            color: "var(--text-muted)",
          }}>
            <div style={{
              width: "72px",
              height: "72px",
              borderRadius: "50%",
              background: "linear-gradient(135deg, #6366f1, #a78bfa)",
              display: "flex",
              alignItems: "center",
              justifyContent: "center",
              boxShadow: "0 8px 32px rgba(99,102,241,0.3)",
            }}>
              <Sparkles size={32} color="white" />
            </div>
            <div>
              <h3 style={{ margin: 0, fontSize: "18px", fontWeight: 700, color: "var(--text-primary)" }}>
                AgentOS 智能助手
              </h3>
              <p style={{ margin: "8px 0 0 0", fontSize: "14px", maxWidth: "400px", textAlign: "center", lineHeight: 1.5 }}>
                通过自然语言与 AgentOS 交互，我可以帮您管理服务、操作智能体、查询系统状态、配置模型。
              </p>
            </div>

            {/* Suggestions */}
            <div style={{ display: "flex", gap: "10px", flexWrap: "wrap", justifyContent: "center", marginTop: "8px" }}>
              {SUGGESTIONS.map((s, i) => (
                <button
                  key={i}
                  onClick={() => handleSuggestionClick(s)}
                  className="btn btn-ghost"
                  style={{
                    padding: "10px 18px",
                    fontSize: "13px",
                    border: "1px solid var(--border-subtle)",
                    borderRadius: "var(--radius-md)",
                    display: "flex",
                    alignItems: "center",
                    gap: "6px",
                    transition: "all var(--transition-fast)",
                  }}
                >
                  <s.icon size={14} />
                  {s.text}
                </button>
              ))}
            </div>
          </div>
        ) : (
          <>
            {!compact && (
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "4px" }}>
                <span style={{ fontSize: "13px", color: "var(--text-muted)" }}>
                  {messages.length} 条消息 · {messages.reduce((sum, m) => sum + (m.tokenCount || 0), 0)} tokens
                </span>
                <button onClick={handleClear} className="btn btn-ghost btn-sm">
                  <Trash2 size={14} /> 清空
                </button>
              </div>
            )}

            {messages.map((msg) => (
              <div key={msg.id} style={{
                display: "flex",
                gap: "12px",
                maxWidth: compact ? "100%" : "85%",
                alignSelf: msg.role === "user" ? "flex-end" : "flex-start",
                animation: "fadeIn 0.25s ease-out",
              }}>
                {msg.role !== "user" && (
                  <div style={{
                    width: "34px", height: "34px", borderRadius: "var(--radius-sm)",
                    background: "linear-gradient(135deg, #6366f1, #a78bfa)",
                    display: "flex", alignItems: "center", justifyContent: "center",
                    flexShrink: 0,
                  }}>
                    <Bot size={17} color="white" />
                  </div>
                )}

                <div style={{
                  maxWidth: "100%",
                  padding: "12px 16px",
                  borderRadius: msg.role === "user"
                    ? "var(--radius-lg) var(--radius-lg) var(--radius-lg) var(--radius-sm)"
                    : "var(--radius-lg) var(--radius-lg) var(--radius-sm) var(--radius-lg)",
                  background: msg.role === "user"
                    ? "linear-gradient(135deg, #6366f1, #818cf8)"
                    : "var(--bg-secondary)",
                  color: msg.role === "user" ? "white" : "var(--text-primary)",
                  boxShadow: "0 1px 3px rgba(0,0,0,0.08)",
                  lineHeight: 1.65,
                  fontSize: "14px",
                  position: "relative",
                }}>
                  {/* Render markdown-like content */}
                  <div style={{ whiteSpace: "pre-wrap", wordBreak: "break-word" }}>{msg.content}</div>

                  {/* Meta bar */}
                  <div style={{
                    display: "flex", alignItems: "center", gap: "10px",
                    marginTop: "8px", paddingTop: "6px",
                    borderTop: msg.role === "user" ? "1px solid rgba(255,255,255,0.15)" : "1px solid var(--border-subtle)",
                    fontSize: "11px",
                    opacity: 0.7,
                  }}>
                    <span>{msg.timestamp.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' })}</span>
                    {msg.tokenCount && <span>{msg.tokenCount} tokens</span>}
                    <button
                      onClick={() => handleCopy(msg.content, msg.id)}
                      style={{ background: "none", border: "none", cursor: "pointer", padding: "2px", marginLeft: "auto", opacity: 0.7 }}
                      title="复制"
                    >
                      {copiedId === msg.id ? <Check size={12} /> : <Copy size={12} />}
                    </button>
                  </div>
                </div>

                {msg.role === "user" && (
                  <div style={{
                    width: "34px", height: "34px", borderRadius: "var(--radius-sm)",
                    background: "var(--bg-tertiary)", border: "1px solid var(--border-subtle)",
                    display: "flex", alignItems: "center", justifyContent: "center",
                    flexShrink: 0,
                  }}>
                    <User size={17} />
                  </div>
                )}
              </div>
            ))}

            {isLoading && (
              <div style={{
                display: "flex", gap: "12px",
                alignSelf: "flex-start", maxWidth: "85%", animation: "fadeIn 0.25s ease-out",
              }}>
                <div style={{
                  width: "34px", height: "34px", borderRadius: "var(--radius-sm)",
                  background: "linear-gradient(135deg, #6366f1, #a78bfa)",
                  display: "flex", alignItems: "center", justifyContent: "center", flexShrink: 0,
                }}>
                  <Bot size={17} color="white" />
                </div>
                <div style={{
                  padding: "14px 18px", borderRadius: "var(--radius-lg) var(--radius-lg) var(--radius-lg) var(--radius-sm)",
                  background: "var(--bg-secondary)", border: "1px solid var(--border-subtle)",
                }}>
                  <div style={{ display: "flex", gap: "6px", alignItems: "center" }}>
                    <div className="loading-spinner" style={{ width: 14, height: 14, borderWidth: "2px" }} />
                    <span style={{ fontSize: "13px", color: "var(--text-secondary)" }}>正在思考...</span>
                  </div>
                </div>
              </div>
            )}
            <div ref={messagesEndRef} />
          </>
        )}
      </div>

      {/* Input Area */}
      <div style={{
        padding: compact ? "12px" : "16px 20px",
        borderTop: "1px solid var(--border-subtle)",
        background: "var(--bg-secondary)",
      }}>
        <div style={{
          display: "flex",
          gap: "10px",
          alignItems: "flex-end",
          background: "var(--bg-primary)",
          border: "1px solid var(--border-color)",
          borderRadius: "var(--radius-lg)",
          padding: "8px 12px",
          transition: "border-color var(--transition-fast)",
        }}
        onFocus={(e) => { e.currentTarget.style.borderColor = "var(--primary-color)"; }}
        onBlur={(e) => { e.currentTarget.style.borderColor = ""; }}
        >
          <textarea
            ref={inputRef}
            value={inputValue}
            onChange={(e) => setInputValue(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); handleSend(); }
            }}
            placeholder="输入消息... (Enter 发送)"
            rows={1}
            style={{
              flex: 1,
              resize: "none",
              border: "none",
              outline: "none",
              background: "transparent",
              color: "var(--text-primary)",
              fontSize: "14px",
              fontFamily: "inherit",
              maxHeight: "120px",
              lineHeight: 1.5,
            }}
          />
          <button
            className="btn btn-primary"
            onClick={() => handleSend()}
            disabled={!inputValue.trim() || isLoading}
            style={{ flexShrink: 0, padding: "8px 14px" }}
          >
            {isLoading ? <RotateCcw size={16} className="spin" /> : <Send size={16} />}
          </button>
        </div>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginTop: "8px", fontSize: "11.5px", color: "var(--text-muted)" }}>
          <span>Shift+Enter 换行 · Enter 发送</span>
          <span>模型: {model}</span>
        </div>
      </div>
    </div>
  );
};

export default AIChat;
