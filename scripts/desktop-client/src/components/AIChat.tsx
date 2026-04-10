import React, { useState, useRef, useEffect } from 'react';
import {
  Send,
  Bot,
  User,
  Sparkles,
  Loader2,
  Copy,
  CheckCircle2,
  RotateCcw,
  Brain,
  ChevronDown,
} from 'lucide-react';
import { useI18n } from '../i18n';

interface ChatMessage {
  id: string;
  role: 'user' | 'assistant' | 'system';
  content: string;
  timestamp: Date;
  model?: string;
}

interface AIChatProps {
  onSendMessage?: (message: string) => Promise<string>;
  initialMessages?: ChatMessage[];
  model?: string;
  compact?: boolean;
}

const SUGGESTIONS = [
  { icon: '🚀', text: '帮我启动所有服务', action: 'start_services' },
  { icon: '📊', text: '查看系统状态报告', action: 'system_status' },
  { icon: '🤖', text: '列出可用的智能体', action: 'list_agents' },
  { icon: '🔧', text: '检查配置是否正确', action: 'check_config' },
];

const AIChat: React.FC<AIChatProps> = ({
  onSendMessage,
  model = 'GPT-4o',
  compact = false,
}) => {
  const { t } = useI18n();
  const [messages, setMessages] = useState<ChatMessage[]>([
    {
      id: 'welcome',
      role: 'assistant',
      content: '你好！我是 AgentOS AI 助手。我可以帮你管理服务、配置系统、分析日志，以及回答关于 AgentOS 的任何问题。有什么我可以帮你的吗？',
      timestamp: new Date(),
      model,
    },
  ]);
  const [input, setInput] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [copiedId, setCopiedId] = useState<string | null>(null);
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLTextAreaElement>(null);

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages]);

  const handleSend = async () => {
    if (!input.trim() || isLoading) return;

    const userMessage: ChatMessage = {
      id: `user-${Date.now()}`,
      role: 'user',
      content: input.trim(),
      timestamp: new Date(),
    };

    setMessages(prev => [...prev, userMessage]);
    setInput('');
    setIsLoading(true);

    try {
      let response: string;
      if (onSendMessage) {
        response = await onSendMessage(userMessage.content);
      } else {
        response = await simulateResponse(userMessage.content);
      }

      const assistantMessage: ChatMessage = {
        id: `assistant-${Date.now()}`,
        role: 'assistant',
        content: response,
        timestamp: new Date(),
        model,
      };

      setMessages(prev => [...prev, assistantMessage]);
    } catch (error) {
      const errorMessage: ChatMessage = {
        id: `error-${Date.now()}`,
        role: 'system',
        content: `请求失败: ${error}. 请检查 AI 模型配置是否正确。`,
        timestamp: new Date(),
      };
      setMessages(prev => [...prev, errorMessage]);
    } finally {
      setIsLoading(false);
    }
  };

  const simulateResponse = async (userInput: string): Promise<string> => {
    await new Promise(resolve => setTimeout(resolve, 1200 + Math.random() * 800));

    const lower = userInput.toLowerCase();

    if (lower.includes('启动') || lower.includes('start')) {
      return '好的，我来帮你启动所有服务。\n\n✅ Kernel 服务已启动 (端口 18080)\n✅ Gateway 服务已启动 (端口 18789)\n✅ PostgreSQL 数据库已启动 (端口 5432)\n✅ Redis 缓存已启动 (端口 6379)\n\n所有服务已成功启动，系统运行正常。你可以在「服务」页面查看详细状态。';
    }
    if (lower.includes('状态') || lower.includes('status') || lower.includes('报告')) {
      return '📊 **系统状态报告**\n\n• CPU 使用率: 47%\n• 内存使用率: 68% (10.9GB / 16.0GB)\n• 运行服务: 4/4\n• 系统健康度: 100%\n• 活跃智能体: 3 个\n• 待处理任务: 1 个\n\n系统整体运行良好，所有核心服务正常。';
    }
    if (lower.includes('智能体') || lower.includes('agent')) {
      return '🤖 **当前活跃的智能体:**\n\n1. **Research Assistant** (agent-001)\n   类型: 研究分析 | 状态: 空闲 | 完成任务: 12\n\n2. **Code Reviewer** (agent-002)\n   类型: 代码审查 | 状态: 运行中 | 完成任务: 8\n\n3. **Data Analyst** (agent-003)\n   类型: 数据分析 | 状态: 空闲 | 完成任务: 5\n\n你可以在「智能体」页面注册新的智能体或管理现有智能体。';
    }
    if (lower.includes('配置') || lower.includes('config')) {
      return '🔧 **配置检查结果:**\n\n✅ docker-compose.yml - 配置正确\n✅ .env 环境变量 - 已设置\n✅ kernel-config.yaml - 参数正常\n⚠️ AI 模型 API Key - 未配置\n\n建议: 前往「AI 模型」页面配置 OpenAI 或 Anthropic API Key，以解锁智能体全部能力。';
    }

    return `我理解你的问题。作为 AgentOS AI 助手，我可以帮你：\n\n• 🚀 管理服务（启动/停止/重启）\n• 📊 查看系统状态和报告\n• 🤖 管理智能体\n• 🔧 检查和修改配置\n• 📝 分析日志和排查问题\n\n请告诉我你需要什么帮助？`;
  };

  const handleSuggestionClick = (suggestion: typeof SUGGESTIONS[0]) => {
    setInput(suggestion.text);
    inputRef.current?.focus();
  };

  const handleCopy = (id: string, content: string) => {
    navigator.clipboard.writeText(content);
    setCopiedId(id);
    setTimeout(() => setCopiedId(null), 2000);
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: compact ? '400px' : 'calc(100vh - 220px)',
      minHeight: compact ? '300px' : '500px',
      background: 'var(--bg-primary)',
      borderRadius: 'var(--radius-lg)',
      border: '1px solid var(--border-color)',
      overflow: 'hidden',
    }}>
      {/* Chat Header */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        padding: '14px 20px',
        borderBottom: '1px solid var(--border-subtle)',
        background: 'var(--bg-secondary)',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <div style={{
            width: '32px', height: '32px', borderRadius: 'var(--radius-md)',
            background: 'var(--primary-gradient)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
          }}>
            <Brain size={18} color="white" />
          </div>
          <div>
            <div style={{ fontWeight: 600, fontSize: '14px' }}>AI 助手</div>
            <div style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
              {model} · {isLoading ? '思考中...' : '在线'}
            </div>
          </div>
        </div>

        <button
          className="btn btn-ghost btn-sm"
          onClick={() => setMessages([messages[0]])}
          title="清空对话"
        >
          <RotateCcw size={14} />
        </button>
      </div>

      {/* Messages Area */}
      <div style={{
        flex: 1,
        overflowY: 'auto',
        padding: '20px',
        display: 'flex',
        flexDirection: 'column',
        gap: '16px',
      }}>
        {messages.map((msg) => (
          <div
            key={msg.id}
            style={{
              display: 'flex',
              gap: '10px',
              flexDirection: msg.role === 'user' ? 'row-reverse' : 'row',
              animation: 'fadeIn 0.3s ease-out',
            }}
          >
            {/* Avatar */}
            <div style={{
              width: '30px', height: '30px', borderRadius: 'var(--radius-md)',
              background: msg.role === 'user'
                ? 'var(--bg-tertiary)'
                : 'var(--primary-gradient)',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              flexShrink: 0, marginTop: '2px',
            }}>
              {msg.role === 'user'
                ? <User size={16} color="var(--text-secondary)" />
                : <Sparkles size={16} color="white" />}
            </div>

            {/* Message Bubble */}
            <div style={{
              maxWidth: '75%',
              padding: '12px 16px',
              borderRadius: msg.role === 'user'
                ? 'var(--radius-lg) var(--radius-sm) var(--radius-lg) var(--radius-lg)'
                : 'var(--radius-sm) var(--radius-lg) var(--radius-lg) var(--radius-lg)',
              background: msg.role === 'user'
                ? 'var(--primary-light)'
                : msg.role === 'system'
                ? 'var(--error-light)'
                : 'var(--bg-tertiary)',
              border: msg.role === 'assistant' ? '1px solid var(--border-subtle)' : 'none',
              fontSize: '13.5px',
              lineHeight: 1.65,
              color: 'var(--text-primary)',
              position: 'relative',
            }}>
              <div style={{ whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>
                {msg.content}
              </div>

              {/* Message Footer */}
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginTop: '8px',
                paddingTop: '6px',
                borderTop: '1px solid var(--border-subtle)',
                fontSize: '11px',
                color: 'var(--text-muted)',
              }}>
                <span>
                  {msg.timestamp.toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' })}
                  {msg.model && ` · ${msg.model}`}
                </span>
                {msg.role === 'assistant' && (
                  <button
                    className="icon-btn"
                    onClick={() => handleCopy(msg.id, msg.content)}
                    style={{ width: '20px', height: '20px', opacity: 0.5 }}
                  >
                    {copiedId === msg.id
                      ? <CheckCircle2 size={12} color="#22c55e" />
                      : <Copy size={12} />}
                  </button>
                )}
              </div>
            </div>
          </div>
        ))}

        {/* Loading Indicator */}
        {isLoading && (
          <div style={{ display: 'flex', gap: '10px', animation: 'fadeIn 0.3s ease-out' }}>
            <div style={{
              width: '30px', height: '30px', borderRadius: 'var(--radius-md)',
              background: 'var(--primary-gradient)',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              flexShrink: 0,
            }}>
              <Sparkles size={16} color="white" />
            </div>
            <div style={{
              padding: '12px 16px',
              borderRadius: 'var(--radius-sm) var(--radius-lg) var(--radius-lg) var(--radius-lg)',
              background: 'var(--bg-tertiary)',
              border: '1px solid var(--border-subtle)',
              display: 'flex', alignItems: 'center', gap: '8px',
              fontSize: '13px', color: 'var(--text-secondary)',
            }}>
              <Loader2 size={14} className="spin" />
              正在思考...
            </div>
          </div>
        )}

        <div ref={messagesEndRef} />
      </div>

      {/* Suggestions */}
      {messages.length <= 1 && !isLoading && (
        <div style={{
          display: 'flex', gap: '8px', padding: '0 20px 12px',
          flexWrap: 'wrap',
        }}>
          {SUGGESTIONS.map((s, idx) => (
            <button
              key={idx}
              onClick={() => handleSuggestionClick(s)}
              style={{
                display: 'flex', alignItems: 'center', gap: '6px',
                padding: '8px 14px',
                borderRadius: 'var(--radius-lg)',
                border: '1px solid var(--border-color)',
                background: 'var(--bg-secondary)',
                color: 'var(--text-secondary)',
                fontSize: '12.5px', cursor: 'pointer',
                transition: 'all var(--transition-fast)',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.borderColor = 'var(--primary-color)';
                e.currentTarget.style.background = 'var(--primary-light)';
                e.currentTarget.style.color = 'var(--primary-color)';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.borderColor = 'var(--border-color)';
                e.currentTarget.style.background = 'var(--bg-secondary)';
                e.currentTarget.style.color = 'var(--text-secondary)';
              }}
            >
              <span>{s.icon}</span>
              <span>{s.text}</span>
            </button>
          ))}
        </div>
      )}

      {/* Input Area */}
      <div style={{
        padding: '14px 20px',
        borderTop: '1px solid var(--border-subtle)',
        background: 'var(--bg-secondary)',
      }}>
        <div style={{
          display: 'flex',
          gap: '10px',
          alignItems: 'flex-end',
          background: 'var(--bg-tertiary)',
          borderRadius: 'var(--radius-lg)',
          padding: '8px 12px',
          border: '1px solid var(--border-color)',
          transition: 'border-color var(--transition-fast)',
        }}>
          <textarea
            ref={inputRef}
            value={input}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder="输入消息，按 Enter 发送..."
            rows={1}
            style={{
              flex: 1,
              background: 'transparent',
              border: 'none',
              outline: 'none',
              color: 'var(--text-primary)',
              fontSize: '13.5px',
              lineHeight: 1.5,
              resize: 'none',
              minHeight: '24px',
              maxHeight: '120px',
              fontFamily: 'inherit',
              caretColor: 'var(--primary-color)',
            }}
          />
          <button
            className="btn btn-primary btn-sm"
            onClick={handleSend}
            disabled={!input.trim() || isLoading}
            style={{ flexShrink: 0 }}
          >
            {isLoading ? <Loader2 size={14} className="spin" /> : <Send size={14} />}
          </button>
        </div>
        <div style={{
          fontSize: '11px', color: 'var(--text-muted)',
          marginTop: '6px', textAlign: 'center',
        }}>
          AI 助手基于 {model} · 回复可能不完全准确，请注意甄别
        </div>
      </div>
    </div>
  );
};

export default AIChat;
