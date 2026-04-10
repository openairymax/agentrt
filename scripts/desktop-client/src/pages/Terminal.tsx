import React, { useState, useRef, useEffect } from "react";
import {
  Terminal as TerminalIcon,
  Play,
  Trash2,
  Copy,
  ChevronUp,
  ChevronDown,
  History,
  AlertCircle,
  CheckCircle2,
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

const Terminal: React.FC = () => {
  const { t } = useI18n();
  const [command, setCommand] = useState("");
  const [output, setOutput] = useState<string[]>([]);
  const [loading, setLoading] = useState(false);
  const [history, setHistory] = useState<string[]>(COMMAND_HISTORY);
  const [showHistory, setShowHistory] = useState(false);
  const [activeTab, setActiveTab] = useState<"shell" | "kernel" | "gateway">("shell");
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
  };

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <h1>{t.terminal.title}</h1>
        <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
          Execute system commands and monitor output
        </p>
      </div>

      {/* Tab Bar */}
      <div style={{
        display: "flex", gap: "4px", marginBottom: "16px",
        background: "var(--bg-secondary)", padding: "4px", borderRadius: "var(--radius-lg)",
        width: "fit-content", border: "1px solid var(--border-subtle)"
      }}>
        {[
          { id: "shell" as const, label: "Shell (Bash)" },
          { id: "kernel" as const, label: "Kernel IPC" },
          { id: "gateway" as const, label: "Gateway API" },
        ].map(tab => (
          <button
            key={tab.id}
            onClick={() => setActiveTab(tab.id)}
            style={{
              padding: "8px 20px", border: "none", borderRadius: "var(--radius-md)",
              background: activeTab === tab.id ? "var(--primary-color)" : "transparent",
              color: activeTab === tab.id ? "white" : "var(--text-secondary)",
              cursor: "pointer", fontWeight: 500, fontSize: "13px",
              transition: "all var(--transition-fast)"
            }}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Main Terminal Card */}
      <div className="card card-elevated" style={{
        background: "#0a0a0f", borderColor: "#1e1e2e",
        overflow: "hidden"
      }}>
        {/* Terminal Header */}
        <div style={{
          display: "flex", justifyContent: "space-between", alignItems: "center",
          padding: "12px 16px", background: "#111118", borderBottom: "1px solid #1e1e2e"
        }}>
          <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
            <div style={{ display: "flex", gap: "6px" }}>
              <span style={{ width: "12px", height: "12px", borderRadius: "50%", background: "#ef4444" }} />
              <span style={{ width: "12px", height: "12px", borderRadius: "50%", background: "#f59e0b" }} />
              <span style={{ width: "12px", height: "12px", borderRadius: "50%", background: "#22c55e" }} />
            </div>
            <span style={{ fontSize: "13px", color: "#9ca3af", fontFamily: "'JetBrains Mono', monospace" }}>
              user@agentos ~ {activeTab === 'shell' ? 'bash' : activeTab === 'kernel' ? 'kernel-ipc' : 'gateway-api'}
            </span>
          </div>
          <div style={{ display: "flex", gap: "6px" }}>
            <button className="icon-btn" onClick={copyOutput} title="Copy Output" style={{ width: "28px", height: "28px", background: "transparent", color: "#9ca3af" }}>
              <Copy size={14} />
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
            minHeight: "360px", maxHeight: "calc(100vh - 380px)",
            overflowY: "auto", padding: "16px",
            fontFamily: "'JetBrains Mono', 'Fira Code', Consolas, monospace",
            fontSize: "13px", lineHeight: "1.7", color: "#e5e7eb"
          }}
        >
          {output.length === 0 ? (
            <div style={{ textAlign: "center", padding: "60px 0", color: "#4b5563" }}>
              <TerminalIcon size={48} style={{ opacity: 0.3, margin: "0 auto 16px" }} />
              <p>{t.terminal.welcomeMessage}</p>
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
                }}
              >{line}</div>
            ))
          )}
        </div>

        {/* Command Input */}
        <div style={{
          display: "flex", alignItems: "center", gap: "10px",
          padding: "12px 16px", borderTop: "1px solid #1e1e2e", background: "#111118"
        }}>
          <span style={{
            fontFamily: "'JetBrains Mono', monospace",
            color: "#22c55e", fontWeight: 600, fontSize: "14px",
            minWidth: "fit-content"
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
              fontFamily: "'JetBrains Mono', monospace", fontSize: "13px",
              caretColor: "#6366f1"
            }}
          />

          <button
            className="btn btn-primary btn-sm"
            onClick={() => handleExecuteCommand()}
            disabled={loading || !command.trim()}
            style={{ minWidth: "80px" }}
          >
            {loading ? (
              <><span className="loading-spinner" style={{ width: 14, height: 14, borderWidth: "2px", margin: 0 }} /></>
            ) : (
              <><Play size={14} />Run</>
            )}
          </button>

          <button
            className={`btn btn-sm ${showHistory ? "btn-primary" : "btn-secondary"}`}
            onClick={() => setShowHistory(!showHistory)}
            title="Command History"
          >
            <History size={14} />
          </button>
        </div>
      </div>

      {/* Quick Commands */}
      <div className="card card-elevated" style={{ marginTop: "16px" }}>
        <h3 className="card-title"><TerminalIcon size={18} />{t.terminal.quickCommands}</h3>
        <div style={{ display: "flex", flexWrap: "wrap", gap: "8px" }}>
          {["docker ps -a", "docker stats --no-stream", "curl -s localhost:18789/health", "df -h", "free -h"].map(cmd => (
            <button
              key={cmd}
              className="tag"
              onClick={() => { setCommand(cmd); inputRef.current?.focus(); }}
              style={{
                cursor: "pointer", transition: "all var(--transition-fast)",
                fontFamily: "'JetBrains Mono', monospace", fontSize: "12px",
                padding: "6px 12px", borderRadius: "var(--radius-md)"
              }}
              onMouseEnter={(e) => e.currentTarget.style.background = "var(--primary-light)"}
              onMouseLeave={(e) => e.currentTarget.style.background = "rgba(99,102,241,0.1)"}
            >
              {cmd}
            </button>
          ))}
        </div>
      </div>

      {/* Command History Panel */}
      {showHistory && (
        <div className="card card-elevated" style={{ marginTop: "16px", animation: "slideDown 0.25s ease" }}>
          <h3 className="card-title">
            <History size={18} />
            Command History ({history.length})
          </h3>
          <div style={{ maxHeight: "200px", overflowY: "auto" }}>
            {history.map((cmd, idx) => (
              <div
                key={idx}
                onClick={() => { setCommand(cmd); setShowHistory(false); inputRef.current?.focus(); }}
                style={{
                  padding: "8px 12px", borderBottom: "1px solid var(--border-subtle)",
                  cursor: "pointer", fontFamily: "'JetBrains Mono', monospace",
                  fontSize: "12.5px", color: "var(--text-secondary)",
                  transition: "background var(--transition-fast)",
                  display: "flex", justifyContent: "space-between", alignItems: "center"
                }}
                onMouseEnter={(e) => e.currentTarget.style.background = "var(--bg-primary)"}
                onMouseLeave={(e) => e.currentTarget.style.background = "transparent"}
              >
                <span>$ {cmd}</span>
                <ChevronUp size={14} style={{ opacity: 0.4 }} />
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};

export default Terminal;
