import React, { useState, useRef, useEffect } from "react";
import {
  Terminal,
  Send,
  Trash2,
  ChevronUp,
  ChevronDown,
  Copy,
  Play,
  History,
} from "lucide-react";
import { invoke } from "../utils/tauriCompat";
import { useI18n } from "../i18n";

interface CommandHistory {
  command: string;
  timestamp: string;
  success: boolean;
}

const TerminalPage: React.FC = () => {
  const { t } = useI18n();
  const [currentCommand, setCurrentCommand] = useState("");
  const [output, setOutput] = useState<string[]>([]);
  const [commandHistory, setCommandHistory] = useState<CommandHistory[]>([]);
  const [historyIndex, setHistoryIndex] = useState(-1);
  const [workingDir, setWorkingDir] = useState("");
  const [executing, setExecuting] = useState(false);

  const inputRef = useRef<HTMLInputElement>(null);
  const outputRef = useRef<HTMLDivElement>(null);

  const PRESET_COMMANDS = [
    { name: t.terminal.checkEnvironment, command: "./install.sh --check-only" },
    { name: t.terminal.startDevServices, command: "docker compose up -d" },
    { name: t.terminal.viewContainerStatus, command: "docker compose ps" },
    { name: t.terminal.viewResourceUsage, command: "docker stats --no-stream" },
    { name: t.terminal.showLogsTail, command: "docker compose logs --tail=50" },
    { name: t.terminal.healthCheck, command: "curl -sf http://localhost:18789/api/v1/health" },
    { name: t.terminal.stopAllServices, command: "docker compose down" },
    { name: t.terminal.cleanUpResources, command: "docker system prune -f" },
  ];

  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [output]);

  useEffect(() => {
    loadWorkingDirectory();
  }, []);

  const loadWorkingDirectory = async () => {
    try {
      const dir = await invoke<string>("read_config_file", { path: "." });
      setWorkingDir(dir || "~");
    } catch {
      setWorkingDir("~");
    }
  };

  const executeCommand = async (cmd?: string) => {
    const commandToExecute = cmd || currentCommand.trim();
    if (!commandToExecute) return;

    setCurrentCommand("");
    setHistoryIndex(-1);
    setExecuting(true);

    const timestamp = new Date().toLocaleTimeString();

    addOutputLine(`$ ${commandToExecute}`, "input");

    try {
      const parts = commandToExecute.split(" ");
      const program = parts[0];
      const args = parts.slice(1);

      const result = await invoke<{
        success: boolean;
        stdout: string;
        stderr: string;
        exit_code: number;
        duration_ms: number;
      }>("execute_cli_command", {
        command: program,
        args,
      });

      if (result.stdout) {
        result.stdout.split("\n").forEach((line) => {
          addOutputLine(line, "stdout");
        });
      }

      if (result.stderr) {
        result.stderr.split("\n").forEach((line) => {
          addOutputLine(line, "stderr");
        });
      }

      addOutputLine(
        `[${t.terminal.exitCode}: ${result.exit_code}] ${t.terminal.completedIn} ${result.duration_ms}ms`,
        result.success ? "success" : "error"
      );

      setCommandHistory((prev) => [
        { command: commandToExecute, timestamp, success: result.success },
        ...prev.slice(0, 49),
      ]);
    } catch (error) {
      addOutputLine(`${t.common.error}: ${error}`, "error");
      setCommandHistory((prev) => [
        { command: commandToExecute, timestamp, success: false },
        ...prev.slice(0, 49),
      ]);
    } finally {
      setExecuting(false);
      inputRef.current?.focus();
    }
  };

  const addOutputLine = (text: string, type: "input" | "stdout" | "stderr" | "error" | "success" | "info") => {
    setOutput((prev) => [...prev, JSON.stringify({ text, type })]);
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter" && !e.shiftKey) {
      e.preventDefault();
      executeCommand();
    } else if (e.key === "ArrowUp") {
      e.preventDefault();
      if (commandHistory.length > 0) {
        const newIndex =
          historyIndex === -1
            ? 0
            : Math.min(historyIndex + 1, commandHistory.length - 1);
        setHistoryIndex(newIndex);
        setCurrentCommand(commandHistory[newIndex].command);
      }
    } else if (e.key === "ArrowDown") {
      e.preventDefault();
      if (historyIndex > 0) {
        const newIndex = historyIndex - 1;
        setHistoryIndex(newIndex);
        setCurrentCommand(commandHistory[newIndex].command);
      } else if (historyIndex === 0) {
        setHistoryIndex(-1);
        setCurrentCommand("");
      }
    } else if (e.key === "l" && e.ctrlKey) {
      e.preventDefault();
      clearTerminal();
    }
  };

  const clearTerminal = () => {
    setOutput([]);
  };

  const copyOutput = () => {
    const text = output
      .map((line) => {
        try {
          const parsed = JSON.parse(line);
          return parsed.text;
        } catch {
          return line;
        }
      })
      .join("\n");

    navigator.clipboard.writeText(text).then(() => {
      addOutputLine(t.terminal.copiedToClipboard, "info");
    });
  };

  const renderOutputLine = (line: string, index: number) => {
    let parsed: { text: string; type: string };
    try {
      parsed = JSON.parse(line);
    } catch {
      parsed = { text: line, type: "stdout" };
    }

    const colorMap: Record<string, string> = {
      input: "#58a6ff",
      stdout: "#c9d1d9",
      stderr: "#d29922",
      error: "#f85149",
      success: "#3fb950",
      info: "#8b949e",
    };

    return (
      <div
        key={index}
        style={{
          color: colorMap[parsed.type] || "#c9d1d9",
          fontFamily: "'Fira Code', monospace",
          fontSize: "13px",
          lineHeight: "1.6",
          padding: "2px 4px",
          whiteSpace: "pre-wrap",
          wordBreak: "break-all",
        }}
      >
        {parsed.text}
      </div>
    );
  };

  return (
    <div>
      <div className="card" style={{ marginBottom: "20px", display: "flex", justifyContent: "space-between", alignItems: "center" }}>
        <h3 className="card-title" style={{ marginBottom: 0 }}>
          <Terminal size={20} />
          {t.terminal.title}
        </h3>

        <div style={{ display: "flex", gap: "8px" }}>
          <button className="btn btn-secondary" onClick={copyOutput} disabled={output.length === 0}>
            <Copy size={16} />
            {t.terminal.copy}
          </button>
          <button className="btn btn-secondary" onClick={clearTerminal} disabled={output.length === 0}>
            <Trash2 size={16} />
            {t.terminal.clear}
          </button>
        </div>
      </div>

      <div className="card" style={{ marginBottom: "20px" }}>
        <h4 style={{ fontSize: "14px", fontWeight: 600, marginBottom: "12px", color: "var(--text-secondary)" }}>
          <Play size={16} style={{ display: "inline", marginRight: 6 }} />
          {t.terminal.quickCommands}
        </h4>
        <div style={{ display: "flex", gap: "8px", flexWrap: "wrap" }}>
          {PRESET_COMMANDS.map((preset) => (
            <button
              key={preset.name}
              className="btn btn-secondary"
              style={{ padding: "6px 14px", fontSize: "13px" }}
              onClick={() => executeCommand(preset.command)}
              disabled={executing}
              title={preset.command}
            >
              {preset.name}
            </button>
          ))}
        </div>
      </div>

      <div
        style={{
          background: "#0d1117",
          border: "1px solid var(--border-color)",
          borderRadius: "10px",
          overflow: "hidden",
          height: "calc(100vh - 420px)",
          minHeight: "400px",
          display: "flex",
          flexDirection: "column",
        }}
      >
        <div
          style={{
            background: "#161b22",
            padding: "10px 16px",
            display: "flex",
            alignItems: "center",
            justifyContent: "space-between",
            borderBottom: "1px solid #30363d",
          }}
        >
          <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
            <div style={{ width: 12, height: 12, borderRadius: "50%", background: "#ff5f56" }} />
            <div style={{ width: 12, height: 12, borderRadius: "50%", background: "#ffbd2e" }} />
            <div style={{ width: 12, height: 12, borderRadius: "50%", background: "#27c93f" }} />
            <span
              style={{
                marginLeft: "12px",
                fontFamily: "'Fira Code', monospace",
                fontSize: "13px",
                color: "#8b949e",
              }}
            >
              agentos@desktop:~{workingDir}$
            </span>
          </div>

          <div style={{ fontSize: "12px", color: "#8b949e" }}>
            {executing ? `● ${t.terminal.running}` : `● ${t.terminal.ready}`}
          </div>
        </div>

        <div
          ref={outputRef}
          style={{
            flex: 1,
            overflowY: "auto",
            padding: "16px",
            fontFamily: "'Fira Code', 'Cascadia Code', monospace",
          }}
        >
          {output.length === 0 && (
            <div style={{ textAlign: "center", color: "#484f58", padding: "40px 0" }}>
              <Terminal size={48} style={{ opacity: 0.3 }} />
              <p style={{ marginTop: "12px" }}>{t.terminal.terminalReady}</p>
              <p style={{ fontSize: "12px", marginTop: "4px" }}>
                {t.terminal.terminalReadyHint}
              </p>
            </div>
          )}

          {output.map((line, index) => renderOutputLine(line, index))}

          {executing && (
            <div
              style={{
                color: "#8b949e",
                fontFamily: "'Fira Code', monospace",
                animation: "blink 1s infinite",
              }}
            >
              ▌
            </div>
          )}
        </div>

        <div
          style={{
            padding: "12px 16px",
            borderTop: "1px solid #30363d",
            display: "flex",
            alignItems: "center",
            gap: "12px",
            background: "#161b22",
          }}
        >
          <span style={{ color: "#58a6ff", fontFamily: "'Fira Code', monospace", fontSize: "14px" }}>
            $
          </span>

          <input
            ref={inputRef}
            type="text"
            className="input-field"
            value={currentCommand}
            onChange={(e) => setCurrentCommand(e.target.value)}
            onKeyDown={handleKeyDown}
            placeholder={t.terminal.placeholder}
            disabled={executing}
            style={{
              flex: 1,
              background: "transparent",
              border: "none",
              outline: "none",
              color: "#c9d1d9",
              fontFamily: "'Fira Code', monospace",
              fontSize: "14px",
              padding: 0,
            }}
            autoFocus
          />

          <button
            className="btn btn-primary"
            onClick={() => executeCommand()}
            disabled={!currentCommand.trim() || executing}
            style={{ padding: "6px 16px" }}
          >
            {executing ? (
              <span className="loading-spinner" style={{ width: 16, height: 16 }} />
            ) : (
              <Send size={16} />
            )}
          </button>
        </div>
      </div>

      {commandHistory.length > 0 && (
        <div className="card" style={{ marginTop: "20px" }}>
          <h4 style={{ fontSize: "14px", fontWeight: 600, marginBottom: "12px", display: "flex", alignItems: "center", gap: "8px" }}>
            <History size={16} />
            {t.terminal.recentCommands} ({commandHistory.length})
          </h4>

          <div
            style={{
              maxHeight: "200px",
              overflowY: "auto",
              display: "flex",
              flexDirection: "column",
              gap: "6px",
            }}
          >
            {commandHistory.map((item, index) => (
              <div
                key={index}
                onClick={() => {
                  setCurrentCommand(item.command);
                  inputRef.current?.focus();
                }}
                style={{
                  display: "flex",
                  justifyContent: "space-between",
                  alignItems: "center",
                  padding: "8px 12px",
                  background: "var(--bg-tertiary)",
                  borderRadius: "6px",
                  cursor: "pointer",
                  fontSize: "13px",
                  fontFamily: "'Fira Code', monospace",
                  transition: "background 0.15s ease",
                }}
                onMouseEnter={(e) =>
                  (e.currentTarget.style.background = "rgba(59, 130, 246, 0.15)")
                }
                onMouseLeave={(e) =>
                  (e.currentTarget.style.background = "var(--bg-tertiary)")
                }
              >
                <span
                  style={{
                    flex: 1,
                    overflow: "hidden",
                    textOverflow: "ellipsis",
                    whiteSpace: "nowrap",
                  }}
                >
                  {item.command}
                </span>
                <div style={{ display: "flex", alignItems: "center", gap: "8px", marginLeft: "12px" }}>
                  <span
                    style={{
                      fontSize: "11px",
                      color: item.success ? "#3fb950" : "#f85149",
                    }}
                  >
                    {item.success ? "✓" : "✗"}
                  </span>
                  <span style={{ fontSize: "11px", color: "#8b949e" }}>{item.timestamp}</span>
                </div>
              </div>
            ))}
          </div>
        </div>
      )}
    </div>
  );
};

export default TerminalPage;
