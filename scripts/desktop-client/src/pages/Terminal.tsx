import React, { useState, useRef, useEffect } from "react";
import {
  Terminal as TerminalIcon,
  Play,
  Trash2,
  Copy,
  ChevronUp,
  ChevronDown,
  History,
  CheckCircle2,
  Maximize2,
  Cpu,
  Globe,
  Code2,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import { useI18n } from "../i18n";

const COMMAND_HISTORY: string[] = [
  "docker ps",
  "docker compose up -d",
  "docker logs --tail 50 kernel",
  "systemctl status agentos-kernel",
  "curl -s http://localhost:18789/health | jq .",
];

const TABS = [
  { id: "shell" as const, label: "Shell (Bash)", icon: TerminalIcon, color: "#22c55e" },
  { id: "kernel" as const, label: "Kernel IPC", icon: Cpu, color: "#8b5cf6" },
  { id: "gateway" as const, label: "Gateway API", icon: Globe, color: "#06b6d4" },
];

const QUICK_COMMANDS = [
  { cmd: "docker ps -a", desc: "容器列表", color: "#3b82f6" },
  { cmd: "docker stats --no-stream", desc: "资源统计", color: "#22c55e" },
  { cmd: "curl -s localhost:18789/health", desc: "健康检查", color: "#8b5cf6" },
  { cmd: "df -h", desc: "磁盘使用", color: "#f59e0b" },
  { cmd: "free -h", desc: "内存状态", color: "#06b6d4" },
  { cmd: "top -bn1 | head -5", desc: "进程概览", color: "#ef4444" },
];

const Terminal: React.FC = () => {
  const { t } = useI18n();
  const [command, setCommand] = useState("");
  const [output, setOutput] = useState<string[]>([]);
  const [loading, setLoading] = useState(false);
  const [history, setHistory] = useState<string[]>(COMMAND_HISTORY);
  const [showHistory, setShowHistory] = useState(false);
  const [activeTab, setActiveTab] = useState<"shell" | "kernel" | "gateway">("shell");
  const [copied, setCopied] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);
  const outputRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [output]);

  useEffect(() => {
    inputRef.current?.focus();
  }, []);

  const handleExecuteCommand = async (cmd?: string) => {
    const cmdToExecute = cmd || command.trim();
    if (!cmdToExecute) return;

    setLoading(true);
    const timestamp = new Date().toLocaleTimeString();
    setOutput(prev => [...prev, `$ ${cmdToExecute}`, `[${timestamp}] Executing...`]);

    try {
      const result = await sdk.executeCliCommand(cmdToExecute, []);
      setOutput(prev => [...prev.slice(-100), result.stdout || (result as any).output || String(result)]);
      setHistory([cmdToExecute, ...history.filter(h => h !== cmdToExecute).slice(0, 49)]);
    } catch (error) {
      setOutput(prev => [...prev.slice(-100), `Error: ${error}`]);
    } finally {
      setCommand("");
      setLoading(false);
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      e.preventDefault();
      handleExecuteCommand();
    }
  };

  const clearTerminal = () => {
    setOutput([]);
  };

  const copyOutput = () => {
    navigator.clipboard.writeText(output.join("\n"));
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const currentTab = TABS.find(t => t.id === activeTab) || TABS[0];

  return (
    <div className="page-container">
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
          <div style={{
            width: "44px", height: "44px", borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#22c55e,#4ade80)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(34,197,94,0.35), 0 0 0 1px rgba(255,255,255,0.08) inset",
          }}>
            <TerminalIcon size={20} color="white" />
          </div>
          <div>
            <h1>{t.terminal.title}</h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: 0 }}>
              系统命令执行 · 实时输出 · 多通道切换
            </p>
          </div>
        </div>
      </div>

      {/* Tab Bar */}
      <div style={{
        display: "flex", gap: "4px", marginBottom: "16px",
        background: "var(--bg-secondary)", padding: "4px", borderRadius: "var(--radius-lg)",
        border: "1px solid var(--border-subtle)",
      }}>
        {TABS.map(tab => {
          const TabIcon = tab.icon;
          return (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              style={{
                padding: "8px 18px", border: "none", borderRadius: "var(--radius-md)",
                background: activeTab === tab.id ? tab.color : "transparent",
                color: activeTab === tab.id ? "white" : "var(--text-secondary)",
                cursor: "pointer", fontWeight: 600, fontSize: "12.5px",
                transition: "all var(--transition-fast)",
                display: "flex", alignItems: "center", gap: "6px",
                boxShadow: activeTab === tab.id ? `0 2px 8px ${tab.color}40` : "none",
              }}
            >
              <TabIcon size={14} />
              {tab.label}
            </button>
          );
        })}
      </div>

      {/* Main Terminal Card */}
      <div className="card card-elevated" style={{
        background: "#0a0a0f", borderColor: "#1e1e2e",
        overflow: "hidden", padding: 0,
      }}>
        {/* Terminal Header */}
        <div style={{
          display: "flex", justifyContent: "space-between", alignItems: "center",
          padding: "10px 16px", background: "#111118", borderBottom: "1px solid #1e1e2e",
        }}>
          <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
            <div style={{ display: "flex", gap: "7px" }}>
              <div style={{ width: "12px", height: "12px", borderRadius: "50%", background: "#ef4444", boxShadow: "0 0 6px rgba(239,68,68,0.3)" }} />
              <div style={{ width: "12px", height: "12px", borderRadius: "50%", background: "#f59e0b", boxShadow: "0 0 6px rgba(245,158,11,0.3)" }} />
              <div style={{ width: "12px", height: "12px", borderRadius: "50%", background: "#22c55e", boxShadow: "0 0 6px rgba(34,197,94,0.3)" }} />
            </div>
            <div style={{
              display: "flex", alignItems: "center", gap: "6px",
              padding: "3px 10px", borderRadius: "var(--radius-full)",
              background: `${currentTab.color}15`, border: `1px solid ${currentTab.color}25`,
            }}>
              <currentTab.icon size={11} color={currentTab.color} />
              <span style={{ fontSize: "11.5px", color: currentTab.color, fontFamily: "var(--font-mono)", fontWeight: 600 }}>
                user@agentos:~/{activeTab === 'shell' ? 'bash' : activeTab === 'kernel' ? 'kernel-ipc' : 'gateway-api'}
              </span>
            </div>
          </div>
          <div style={{ display: "flex", gap: "4px" }}>
            <button className="icon-btn" onClick={copyOutput} title="Copy Output" style={{ width: "28px", height: "28px", background: "transparent", color: copied ? "#22c55e" : "#9ca3af" }}>
              {copied ? <CheckCircle2 size={14} /> : <Copy size={14} />}
            </button>
            <button className="icon-btn" onClick={clearTerminal} title="Clear" style={{ width: "28px", height: "28px", background: "transparent", color: "#ef4444" }}>
              <Trash2 size={14} />
            </button>
          </div>
        </div>

        {/* Output Area */}
        <div
          ref={outputRef}
          style={{
            minHeight: "360px", maxHeight: "calc(100vh - 420px)",
            overflowY: "auto", padding: "16px",
            fontFamily: "var(--font-mono)",
            fontSize: "12.5px", lineHeight: "1.7", color: "#e5e7eb",
          }}
        >
          {output.length === 0 ? (
            <div style={{ textAlign: "center", padding: "60px 0", color: "#4b5563" }}>
              <div style={{
                width: "56px", height: "56px", borderRadius: "var(--radius-lg)",
                background: "rgba(34,197,94,0.08)", margin: "0 auto 16px",
                display: "flex", alignItems: "center", justifyContent: "center",
              }}>
                <TerminalIcon size={24} style={{ color: "#22c55e", opacity: 0.5 }} />
              </div>
              <p style={{ fontSize: "14px", fontWeight: 500 }}>{t.terminal.welcomeMessage}</p>
              <p style={{ marginTop: "8px", fontSize: "12px", opacity: 0.6 }}>{t.terminal.enterCommandHint}</p>
            </div>
          ) : (
            output.map((line, i) => (
              <div
                key={i}
                style={{
                  whiteSpace: "pre-wrap", wordBreak: "break-all",
                  color: line.startsWith("$") ? "#22c55e" :
                        line.startsWith("Error:") ? "#ef4444" :
                        line.startsWith("[") ? "#6366f1" :
                        line.includes("✓") ? "#22c55e" :
                        line.includes("✗") ? "#ef4444" :
                        "#e5e7eb",
                  animation: i === output.length - 1 ? "fadeIn 0.3s ease" : undefined,
                  padding: "1px 0",
                }}
              >{line}</div>
            ))
          )}
        </div>

        {/* Command Input */}
        <div style={{
          display: "flex", alignItems: "center", gap: "10px",
          padding: "10px 16px", borderTop: "1px solid #1e1e2e", background: "#111118",
        }}>
          <span style={{
            fontFamily: "var(--font-mono)",
            color: currentTab.color, fontWeight: 700, fontSize: "14px",
            minWidth: "fit-content",
          }}>❯</span>

          <input
            ref={inputRef}
            type="text"
            className="form-input"
            value={command}
            onChange={(e) => setCommand(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder={t.terminal.enterCommand}
            disabled={loading}
            autoFocus
            style={{
              flex: 1, background: "transparent", border: "none",
              color: "#e5e7eb", outline: "none", boxShadow: "none",
              fontFamily: "var(--font-mono)", fontSize: "13px",
              caretColor: currentTab.color,
            }}
          />

          <button
            className="btn btn-primary btn-sm"
            onClick={() => handleExecuteCommand()}
            disabled={loading || !command.trim()}
            style={{ minWidth: "72px", background: `linear-gradient(135deg,${currentTab.color},${currentTab.color}cc)` }}
          >
            {loading ? (
              <><span className="loading-spinner" style={{ width: 14, height: 14, borderWidth: "2px", margin: 0 }} /></>
            ) : (
              <><Play size={14} />Run</>
            )}
          </button>

          <button
            className={`btn btn-sm ${showHistory ? "btn-primary" : "btn-ghost"}`}
            onClick={() => setShowHistory(!showHistory)}
            title="Command History"
          >
            <History size={14} />
          </button>
        </div>
      </div>

      {/* Quick Commands + History side by side */}
      <div style={{ display: "grid", gridTemplateColumns: showHistory ? "1fr 1fr" : "1fr", gap: "16px", marginTop: "16px" }}>
        <div className="card card-elevated" style={{ position: "relative", overflow: "hidden" }}>
          <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#22c55e40,#22c55e10)" }} />
          <h3 className="card-title" style={{ padding: "16px 18px 12px", margin: 0 }}>
            <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "rgba(34,197,94,0.1)", display: "flex", alignItems: "center", justifyContent: "center" }}>
              <Code2 size={14} color="#22c55e" />
            </div>
            {t.terminal.quickCommands}
          </h3>
          <div style={{ display: "flex", flexWrap: "wrap", gap: "8px", padding: "0 18px 18px" }}>
            {QUICK_COMMANDS.map(qc => (
              <button
                key={qc.cmd}
                onClick={() => { setCommand(qc.cmd); inputRef.current?.focus(); }}
                style={{
                  cursor: "pointer", transition: "all var(--transition-fast)",
                  fontFamily: "var(--font-mono)", fontSize: "11.5px",
                  padding: "7px 14px", borderRadius: "var(--radius-md)",
                  background: `${qc.color}08`, border: `1px solid ${qc.color}15`,
                  color: qc.color, fontWeight: 500,
                  display: "flex", alignItems: "center", gap: "8px",
                }}
                onMouseEnter={(e) => { e.currentTarget.style.background = `${qc.color}15`; e.currentTarget.style.borderColor = `${qc.color}30`; }}
                onMouseLeave={(e) => { e.currentTarget.style.background = `${qc.color}08`; e.currentTarget.style.borderColor = `${qc.color}15`; }}
              >
                <span style={{ fontSize: "10px", color: "var(--text-muted)", fontWeight: 600, textTransform: "uppercase" }}>{qc.desc}</span>
                <span style={{ opacity: 0.7 }}>{qc.cmd}</span>
              </button>
            ))}
          </div>
        </div>

        {showHistory && (
          <div className="card card-elevated" style={{ animation: "slideInRight 0.25s ease", position: "relative", overflow: "hidden" }}>
            <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#8b5cf640,#8b5cf610)" }} />
            <h3 className="card-title" style={{ padding: "16px 18px 12px", margin: 0 }}>
              <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "var(--purple-light)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                <History size={14} color="#8b5cf6" />
              </div>
              Command History
              <span style={{ marginLeft: "auto", fontSize: "11px", color: "var(--text-muted)", fontWeight: 400 }}>{history.length}</span>
            </h3>
            <div style={{ maxHeight: "200px", overflowY: "auto", padding: "0 8px 8px" }}>
              {history.map((cmd, idx) => (
                <div
                  key={idx}
                  onClick={() => { setCommand(cmd); setShowHistory(false); inputRef.current?.focus(); }}
                  style={{
                    padding: "8px 12px", borderRadius: "var(--radius-sm)",
                    cursor: "pointer", fontFamily: "var(--font-mono)",
                    fontSize: "12px", color: "var(--text-secondary)",
                    transition: "background var(--transition-fast)",
                    display: "flex", justifyContent: "space-between", alignItems: "center",
                    marginBottom: "2px",
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.background = "var(--bg-tertiary)"}
                  onMouseLeave={(e) => e.currentTarget.style.background = "transparent"}
                >
                  <span style={{ color: "#22c55e", marginRight: "8px" }}>$</span>
                  <span style={{ flex: 1 }}>{cmd}</span>
                  <ChevronUp size={12} style={{ opacity: 0.3 }} />
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default Terminal;
