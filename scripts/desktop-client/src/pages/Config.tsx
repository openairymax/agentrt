import React, { useState, useRef, useCallback } from "react";
import {
  Settings as SettingsIcon,
  Save,
  RotateCcw,
  FileText,
  FolderOpen,
  AlertTriangle,
  CheckCircle2,
  Copy,
  ChevronRight,
  Search,
  Braces,
  Container,
  Globe,
  Cpu,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import { useI18n } from "../i18n";
import { useAlert } from "../components/useAlert";

const CONFIG_FILES = [
  { id: "docker-compose", name: "docker-compose.yml", path: "config/docker-compose.yml", icon: Container, color: "#3b82f6", desc: "Docker 服务编排" },
  { id: "env", name: ".env", path: "config/.env", icon: SettingsIcon, color: "#f59e0b", desc: "环境变量配置" },
  { id: "kernel", name: "kernel-config.yaml", path: "config/kernel-config.yaml", icon: Cpu, color: "#8b5cf6", desc: "内核参数配置" },
  { id: "gateway", name: "gateway.yaml", path: "config/gateway.yaml", icon: Globe, color: "#10b981", desc: "网关路由配置" },
];

function syntaxHighlight(line: string): React.ReactNode {
  const trimmed = line.trimStart();
  const indent = line.length - trimmed.length;

  if (trimmed.startsWith("#")) {
    return <span style={{ color: "var(--text-muted)", fontStyle: "italic" }}>{line}</span>;
  }

  const parts: React.ReactNode[] = [];
  let remaining = line;
  let keyIndex = 0;

  const yamlKeyMatch = remaining.match(/^(\s*)([\w.-]+)(:)/);
  if (yamlKeyMatch) {
    parts.push(<span key={keyIndex++} style={{ color: "#06b6d4" }}>{yamlKeyMatch[1]}{yamlKeyMatch[2]}</span>);
    parts.push(<span key={keyIndex++} style={{ color: "var(--text-muted)" }}>{yamlKeyMatch[3]}</span>);
    remaining = line.substring(yamlKeyMatch[0].length);
  }

  const envMatch = remaining.match(/^(\s*)([\w_]+)(=)/);
  if (envMatch && parts.length === 0) {
    parts.push(<span key={keyIndex++} style={{ color: "#06b6d4" }}>{envMatch[1]}{envMatch[2]}</span>);
    parts.push(<span key={keyIndex++} style={{ color: "var(--text-muted)" }}>{envMatch[3]}</span>);
    remaining = line.substring(envMatch[0].length);
  }

  if (parts.length > 0) {
    const stringMatch = remaining.match(/^(.*)$/);
    if (stringMatch) {
      const val = stringMatch[1];
      if (val.trim().startsWith('"') || val.trim().startsWith("'")) {
        parts.push(<span key={keyIndex++} style={{ color: "#22c55e" }}>{val}</span>);
      } else if (val.trim().match(/^\d+\.?\d*$/)) {
        parts.push(<span key={keyIndex++} style={{ color: "#f59e0b" }}>{val}</span>);
      } else if (val.trim().match(/^(true|false|null|yes|no)$/i)) {
        parts.push(<span key={keyIndex++} style={{ color: "#8b5cf6" }}>{val}</span>);
      } else if (val.trim()) {
        parts.push(<span key={keyIndex++} style={{ color: "var(--text-secondary)" }}>{val}</span>);
      } else {
        parts.push(<span key={keyIndex++}>{val}</span>);
      }
    }
    return <>{parts}</>;
  }

  const dashMatch = line.match(/^(\s*)(- )(.*)$/);
  if (dashMatch) {
    return (
      <>
        <span>{dashMatch[1]}</span>
        <span style={{ color: "#f59e0b" }}>{dashMatch[2]}</span>
        <span style={{ color: "var(--text-secondary)" }}>{dashMatch[3]}</span>
      </>
    );
  }

  return <span style={{ color: "var(--text-secondary)" }}>{line}</span>;
}

const Config: React.FC = () => {
  const { t } = useI18n();
  const { error, success, confirm: confirmModal } = useAlert();
  const [selectedFile, setSelectedFile] = useState("docker-compose");
  const [content, setContent] = useState(`# AgentOS Docker Compose Configuration
version: "3.8"

services:
  gateway:
    image: agentos/gateway:latest
    ports:
      - "8080:8080"
    environment:
      - LOG_LEVEL=info
      - WORKERS=4
    restart: unless-stopped

  llm-engine:
    image: agentos/llm-engine:latest
    ports:
      - "8000:8000"
    environment:
      - MODEL_PATH=/models
      - GPU_ENABLED=true
    restart: unless-stopped

  tool-daemon:
    image: agentos/tool-daemon:latest
    environment:
      - SANDBOX_ENABLED=true
    restart: unless-stopped

  scheduler:
    image: agentos/scheduler:latest
    environment:
      - MAX_CONCURRENT=10
    restart: unless-stopped

  monitor:
    image: agentos/monitor:latest
    ports:
      - "9090:9090"
    restart: unless-stopped

volumes:
  model-data:
    driver: local
  log-data:
    driver: local
`);
  const [originalContent, setOriginalContent] = useState(content);
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [hasChanges, setHasChanges] = useState(false);
  const [copied, setCopied] = useState(false);
  const textareaRef = useRef<HTMLTextAreaElement>(null);

  const loadConfig = async (fileId?: string) => {
    const targetId = fileId || selectedFile;
    setLoading(true);
    try {
      const file = CONFIG_FILES.find(f => f.id === targetId) || CONFIG_FILES[0];
      const configContent = await sdk.readConfigFile(file.path);
      setContent(configContent);
      setOriginalContent(configContent);
      setSelectedFile(targetId);
      setHasChanges(false);
    } catch {
      const fallback = `# Configuration: ${CONFIG_FILES.find(f => f.id === targetId)?.name}\n# Edit below...\n\n`;
      setContent(fallback);
      setOriginalContent(fallback);
      setSelectedFile(targetId);
      setHasChanges(false);
    } finally {
      setLoading(false);
    }
  };

  const handleSaveConfig = async () => {
    if (!hasChanges) return;
    setSaving(true);
    try {
      const file = CONFIG_FILES.find(f => f.id === selectedFile) || CONFIG_FILES[0];
      await sdk.writeConfigFile(file.path, content);
      setOriginalContent(content);
      setHasChanges(false);
      success("保存成功", "配置文件已成功保存");
    } catch (err) {
      error("保存失败", `无法保存配置文件: ${err}`);
    } finally {
      setSaving(false);
    }
  };

  const handleDiscardChanges = async () => {
    const confirmed = await confirmModal({
      type: 'warning',
      title: '丢弃更改',
      message: t.config.discardConfirm,
    });
    if (confirmed) {
      setContent(originalContent);
      setHasChanges(false);
    }
  };

  const handleContentChange = (newContent: string) => {
    setContent(newContent);
    setHasChanges(newContent !== originalContent);
  };

  const copyToClipboard = () => {
    navigator.clipboard.writeText(content);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const handleKeyDown = useCallback((e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === "Tab") {
      e.preventDefault();
      const target = e.currentTarget;
      const start = target.selectionStart;
      const end = target.selectionEnd;
      const newContent = content.substring(0, start) + "  " + content.substring(end);
      handleContentChange(newContent);
      setTimeout(() => {
        target.selectionStart = target.selectionEnd = start + 2;
      }, 0);
    }
    if ((e.ctrlKey || e.metaKey) && e.key === "s") {
      e.preventDefault();
      if (hasChanges && !saving) handleSaveConfig();
    }
  }, [content, hasChanges, saving]);

  const lines = content.split("\n");
  const currentFile = CONFIG_FILES.find(f => f.id === selectedFile) || CONFIG_FILES[0];

  return (
    <div className="page-container">
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
          <div style={{
            width: "44px", height: "44px", borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#6366f1,#818cf8)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(99,102,241,0.35), 0 0 0 1px rgba(255,255,255,0.08) inset",
          }}>
            <Braces size={20} color="white" />
          </div>
          <div>
            <h1>{t.config.title}</h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: 0 }}>
              系统配置文件编辑器 · YAML / ENV
            </p>
          </div>
        </div>
        <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
          {hasChanges && (
            <div style={{
              display: "flex", alignItems: "center", gap: "6px",
              padding: "5px 14px", borderRadius: "var(--radius-full)",
              background: "rgba(245,158,11,0.1)", border: "1px solid rgba(245,158,11,0.2)",
              fontSize: "12px", fontWeight: 600, color: "#f59e0b",
            }}>
              <div style={{ width: "6px", height: "6px", borderRadius: "50%", background: "#f59e0b", animation: "pulse 2s infinite" }} />
              未保存更改
            </div>
          )}
        </div>
      </div>

      <div style={{ display: "flex", gap: "16px", height: "calc(100vh - 240px)" }}>
        {/* File Tree Sidebar */}
        <div className="card card-elevated" style={{
          width: "260px", flexShrink: 0,
          display: "flex", flexDirection: "column",
          overflow: "hidden", padding: 0,
        }}>
          <div style={{
            padding: "16px 18px 14px", borderBottom: "1px solid var(--border-subtle)",
            display: "flex", alignItems: "center", gap: "10px",
          }}>
            <div style={{
              width: "30px", height: "30px", borderRadius: "var(--radius-sm)",
              background: "rgba(99,102,241,0.1)", display: "flex", alignItems: "center", justifyContent: "center",
            }}>
              <FolderOpen size={15} color="#6366f1" />
            </div>
            <span style={{ fontSize: "14px", fontWeight: 700, letterSpacing: "-0.01em" }}>配置文件</span>
          </div>
          <div style={{ flex: 1, overflowY: "auto", padding: "8px" }}>
            {CONFIG_FILES.map((file) => {
              const Ic = file.icon;
              const isActive = selectedFile === file.id;
              return (
                <button
                  key={file.id}
                  onClick={() => loadConfig(file.id)}
                  style={{
                    display: "flex", alignItems: "center", gap: "10px",
                    width: "100%", padding: "11px 14px",
                    borderRadius: "var(--radius-md)",
                    border: "1px solid transparent",
                    background: isActive ? `${file.color}10` : "transparent",
                    cursor: "pointer", transition: "all var(--transition-fast)",
                    textAlign: "left", marginBottom: "4px",
                    boxShadow: isActive ? `0 0 0 1px ${file.color}25` : "none",
                  }}
                  onMouseEnter={(e) => { if (!isActive) { e.currentTarget.style.background = "var(--bg-tertiary)"; e.currentTarget.style.borderColor = "var(--border-subtle)"; } }}
                  onMouseLeave={(e) => { if (!isActive) { e.currentTarget.style.background = "transparent"; e.currentTarget.style.borderColor = "transparent"; } }}
                >
                  <div style={{
                    width: "32px", height: "32px", borderRadius: "var(--radius-sm)",
                    background: isActive ? `${file.color}18` : "var(--bg-tertiary)",
                    display: "flex", alignItems: "center", justifyContent: "center",
                    transition: "all var(--transition-fast)",
                  }}>
                    <Ic size={15} color={isActive ? file.color : "var(--text-muted)"} />
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{
                      fontWeight: isActive ? 700 : 500,
                      fontSize: "13px", color: isActive ? file.color : "var(--text-primary)",
                      whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis",
                    }}>
                      {file.name}
                    </div>
                    <div style={{ fontSize: "10.5px", color: "var(--text-muted)", marginTop: "1px" }}>
                      {file.desc}
                    </div>
                  </div>
                  {isActive && <ChevronRight size={14} color={file.color} style={{ opacity: 0.6 }} />}
                </button>
              );
            })}
          </div>
          <div style={{
            padding: "12px 16px", borderTop: "1px solid var(--border-subtle)",
            display: "flex", alignItems: "center", gap: "8px",
            fontSize: "11px", color: "var(--text-muted)",
          }}>
            <FileText size={12} />
            <span>{lines.length} 行</span>
            <span>·</span>
            <span>{new Blob([content]).size} bytes</span>
          </div>
        </div>

        {/* Editor Area */}
        <div className="card card-elevated" style={{ flex: 1, display: "flex", flexDirection: "column", overflow: "hidden", padding: 0 }}>
          {/* Editor Header */}
          <div style={{
            display: "flex", justifyContent: "space-between", alignItems: "center",
            padding: "10px 18px", background: "var(--bg-secondary)",
            borderBottom: "1px solid var(--border-subtle)",
            flexShrink: 0,
          }}>
            <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
              <div style={{
                width: "26px", height: "26px", borderRadius: "var(--radius-sm)",
                background: `${currentFile.color}15`, display: "flex", alignItems: "center", justifyContent: "center",
              }}>
                <currentFile.icon size={13} color={currentFile.color} />
              </div>
              <span style={{ fontWeight: 600, fontSize: "13px", fontFamily: "var(--font-mono)", color: "var(--text-primary)" }}>
                {currentFile.name}
              </span>
              <span style={{ fontSize: "11px", color: "var(--text-muted)", fontFamily: "var(--font-mono)" }}>
                {currentFile.path}
              </span>
              {hasChanges && (
                <span style={{
                  padding: "2px 8px", borderRadius: "var(--radius-full)",
                  background: "rgba(245,158,11,0.12)", color: "#f59e0b",
                  fontSize: "10.5px", fontWeight: 700,
                }}>Modified</span>
              )}
            </div>
            <div style={{ display: "flex", gap: "4px" }}>
              <button className="btn btn-ghost btn-sm" onClick={copyToClipboard} title="Copy">
                {copied ? <CheckCircle2 size={13} color="#22c55e" /> : <Copy size={13} />}
                <span style={{ color: copied ? "#22c55e" : undefined }}>{copied ? "Copied" : "Copy"}</span>
              </button>
              <button className="btn btn-ghost btn-sm" onClick={() => loadConfig()} disabled={loading} title="Reload">
                <RotateCcw size={13} />Reload
              </button>
              <div style={{ width: "1px", background: "var(--border-subtle)", margin: "4px 4px" }} />
              <button className="btn btn-ghost btn-sm" onClick={handleDiscardChanges} disabled={!hasChanges} style={{ color: hasChanges ? "#ef4444" : undefined }}>
                Discard
              </button>
              <button className="btn btn-primary btn-sm" onClick={handleSaveConfig} disabled={!hasChanges || saving}>
                {saving ? <><span className="loading-spinner" style={{ width: 13, height: 13, borderWidth: "2px", margin: 0 }} /></> : <><Save size={13} />Save</>}
              </button>
            </div>
          </div>

          {/* Code Editor */}
          <div style={{ flex: 1, overflow: "auto", position: "relative", background: "var(--bg-primary)" }}>
            {loading ? (
              <div style={{ display: "flex", justifyContent: "center", alignItems: "center", height: "100%", flexDirection: "column", gap: "12px" }}>
                <div className="loading-spinner" />
                <span style={{ fontSize: "13px", color: "var(--text-muted)" }}>加载配置文件...</span>
              </div>
            ) : (
              <div style={{
                display: "flex", minHeight: "100%",
                fontFamily: "var(--font-mono)",
                fontSize: "13px", lineHeight: "1.7",
              }}>
                {/* Line Numbers */}
                <div style={{
                  padding: "16px 12px 16px 16px", background: "var(--bg-secondary)",
                  textAlign: "right", userSelect: "none",
                  color: "var(--text-muted)", opacity: 0.5,
                  borderRight: "1px solid var(--border-subtle)",
                  position: "sticky", left: 0, zIndex: 1,
                  fontSize: "12px",
                }}>
                  {lines.map((_, i) => (
                    <div key={i} style={{ height: "22.1px" }}>{i + 1}</div>
                  ))}
                </div>

                {/* Syntax Highlighted Overlay + Textarea */}
                <div style={{ flex: 1, position: "relative" }}>
                  {/* Syntax highlight overlay */}
                  <pre style={{
                    position: "absolute", inset: 0,
                    padding: "16px", margin: 0,
                    fontFamily: "inherit", fontSize: "inherit", lineHeight: "inherit",
                    whiteSpace: "pre-wrap", wordBreak: "break-word",
                    pointerEvents: "none", overflow: "hidden",
                    color: "transparent",
                  }} aria-hidden="true">
                    {lines.map((line, i) => (
                      <div key={i} style={{ height: "22.1px" }}>{syntaxHighlight(line)}</div>
                    ))}
                  </pre>

                  <textarea
                    ref={textareaRef}
                    value={content}
                    onChange={(e) => handleContentChange(e.target.value)}
                    onKeyDown={handleKeyDown}
                    spellCheck={false}
                    style={{
                      width: "100%", height: "100%", minHeight: "100%",
                      padding: "16px", resize: "none",
                      background: "transparent", color: "transparent",
                      caretColor: "var(--primary-color)",
                      border: "none", outline: "none",
                      fontFamily: "inherit", fontSize: "inherit", lineHeight: "inherit",
                      tabSize: 2, position: "relative", zIndex: 1,
                    }}
                  />
                </div>
              </div>
            )}

            {/* Save Status */}
            {!hasChanges && content && !loading && (
              <div style={{
                position: "absolute", bottom: "14px", right: "14px",
                display: "flex", alignItems: "center", gap: "6px",
                padding: "6px 14px", borderRadius: "var(--radius-full)",
                background: "rgba(34,197,94,0.1)", border: "1px solid rgba(34,197,94,0.15)",
                color: "#22c55e", fontSize: "11.5px", fontWeight: 600,
                boxShadow: "0 2px 8px rgba(34,197,94,0.1)",
              }}>
                <CheckCircle2 size={12} />已保存
              </div>
            )}
          </div>
        </div>
      </div>

      {/* Info Card */}
      <div className="card card-elevated" style={{ marginTop: "16px", position: "relative", overflow: "hidden" }}>
        <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: "linear-gradient(90deg,#f59e0b40,#f59e0b10)" }} />
        <div style={{ display: "flex", gap: "24px", alignItems: "start", position: "relative", zIndex: 1 }}>
          <div style={{ flex: 1 }}>
            <h3 className="card-title" style={{ color: "#f59e0b" }}>
              <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "rgba(245,158,11,0.1)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                <AlertTriangle size={15} color="#f59e0b" />
              </div>
              {t.config.warning}
            </h3>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", lineHeight: "1.7", margin: 0 }}>
              {t.config.warningMessage}
            </p>
          </div>
          <div style={{ flex: 1 }}>
            <h3 className="card-title">
              <div style={{ width: "28px", height: "28px", borderRadius: "var(--radius-sm)", background: "var(--primary-light)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                <SettingsIcon size={15} color="var(--primary-color)" />
              </div>
              快捷提示
            </h3>
            <ul style={{ color: "var(--text-secondary)", fontSize: "13px", lineHeight: "1.9", paddingLeft: "20px", margin: 0 }}>
              <li>按 <kbd style={{ padding: "1px 6px", background: "var(--bg-tertiary)", borderRadius: "4px", fontSize: "11px", fontFamily: "var(--font-mono)", border: "1px solid var(--border-color)" }}>Ctrl+S</kbd> 快速保存</li>
              <li>Tab 键插入缩进，支持 YAML 语法高亮</li>
              <li>环境变量使用 <code style={{ padding: "1px 6px", background: "var(--bg-tertiary)", borderRadius: "4px", fontSize: "11.5px", fontFamily: "var(--font-mono)" }}>KEY=VALUE</code> 格式</li>
              <li>修改前建议备份原始配置文件</li>
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Config;
