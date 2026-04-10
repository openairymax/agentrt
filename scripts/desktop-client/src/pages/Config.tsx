import React, { useState } from "react";
import {
  Settings as SettingsIcon,
  Save,
  RotateCcw,
  FileText,
  FolderOpen,
  AlertTriangle,
  CheckCircle2,
  Copy,
  Download,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import { useI18n } from "../i18n";

const CONFIG_FILES = [
  { id: "docker-compose", name: "docker-compose.yml", path: "config/docker-compose.yml", icon: "🐳" },
  { id: "env", name: ".env", path: "config/.env", icon: "⚙️" },
  { id: "kernel", name: "kernel-config.yaml", path: "config/kernel-config.yaml", icon: "🔧" },
  { id: "gateway", name: "gateway.yaml", path: "config/gateway.yaml", icon: "🌐" },
];

const Config: React.FC = () => {
  const { t } = useI18n();
  const [selectedFile, setSelectedFile] = useState("docker-compose");
  const [content, setContent] = useState("");
  const [originalContent, setOriginalContent] = useState("");
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [hasChanges, setHasChanges] = useState(false);

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
    } catch (error) {
      console.error("Failed to load config:", error);
      setContent(`# Failed to load configuration\n# Error: ${error}\n`);
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
    } catch (error) {
      console.error("Failed to save config:", error);
      alert(`${t.config.saveFailed}: ${error}`);
    } finally {
      setSaving(false);
    }
  };

  const handleDiscardChanges = () => {
    if (confirm(t.config.discardConfirm)) {
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
  };

  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <h1>{t.config.title}</h1>
        <p style={{ color: "var(--text-secondary)", fontSize: "15px" }}>
          Edit and manage system configuration files
        </p>
      </div>

      {/* Main Layout */}
      <div style={{ display: "flex", gap: "16px", height: "calc(100vh - 220px)" }}>
        {/* File Tree Sidebar */}
        <div className="card card-elevated" style={{
          width: "260px", flexShrink: 0,
          display: "flex", flexDirection: "column",
          overflow: "hidden"
        }}>
          <h3 className="card-title" style={{ padding: "16px 16px 12px", margin: 0 }}>
            <FolderOpen size={16} />
            Configuration Files
          </h3>
          <div style={{ flex: 1, overflowY: "auto", padding: "0 8px 8px" }}>
            {CONFIG_FILES.map((file) => (
              <button
                key={file.id}
                onClick={() => loadConfig(file.id)}
                style={{
                  display: "flex", alignItems: "center", gap: "10px",
                  width: "100%", padding: "10px 14px",
                  borderRadius: "var(--radius-md)",
                  border: "none", background: selectedFile === file.id ? "var(--primary-light)" : "transparent",
                  cursor: "pointer", transition: "all var(--transition-fast)",
                  textAlign: "left", marginBottom: "2px"
                }}
                onMouseEnter={(e) => { if (selectedFile !== file.id) e.currentTarget.style.background = "var(--bg-tertiary)" }}
                onMouseLeave={(e) => { if (selectedFile !== file.id) e.currentTarget.style.background = "transparent" }}
              >
                <span style={{ fontSize: "18px" }}>{file.icon}</span>
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{
                    fontWeight: selectedFile === file.id ? 600 : 400,
                    fontSize: "13.5px", whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis"
                  }}>
                    {file.name}
                  </div>
                  <div style={{ fontSize: "11px", color: "var(--text-muted)", fontFamily: "'JetBrains Mono', monospace" }}>
                    {file.path}
                  </div>
                </div>
              </button>
            ))}
          </div>
        </div>

        {/* Editor Area */}
        <div className="card card-elevated" style={{ flex: 1, display: "flex", flexDirection: "column", overflow: "hidden" }}>
          {/* Editor Header */}
          <div style={{
            display: "flex", justifyContent: "space-between", alignItems: "center",
            padding: "10px 16px", background: "var(--bg-secondary)",
            borderBottom: "1px solid var(--border-subtle)",
            flexShrink: 0
          }}>
            <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
              <FileText size={15} />
              <span style={{ fontWeight: 500, fontSize: "13.5px", fontFamily: "'JetBrains Mono', monospace" }}>
                {CONFIG_FILES.find(f => f.id === selectedFile)?.name}
              </span>
              {hasChanges && (
                <span className="tag" style={{ background: "#f59e0b20", color: "#f59e0b", fontSize: "11px" }}>Modified</span>
              )}
            </div>
            <div style={{ display: "flex", gap: "6px" }}>
              <button className="btn btn-ghost btn-sm" onClick={copyToClipboard} title="Copy">
                <Copy size={13} />Copy
              </button>
              <button className="btn btn-ghost btn-sm" onClick={() => loadConfig()} disabled={loading} title="Reload">
                <RotateCcw size={13} />Reload
              </button>
              <button className="btn btn-danger btn-sm" onClick={handleDiscardChanges} disabled={!hasChanges}>
                Discard
              </button>
              <button className="btn btn-primary btn-sm" onClick={handleSaveConfig} disabled={!hasChanges || saving}>
                {saving ? <><span className="loading-spinner" style={{ width: 13, height: 13, borderWidth: "2px", margin: 0 }} /></> : <><Save size={13} />Save</>}
              </button>
            </div>
          </div>

          {/* Code Editor */}
          <div style={{ flex: 1, overflow: "auto", position: "relative" }}>
            {loading ? (
              <div style={{ display: "flex", justifyContent: "center", alignItems: "center", height: "100%" }}>
                <div className="loading-spinner" />
              </div>
            ) : (
              <>
                {/* Line Numbers + Content */}
                <div style={{
                  display: "flex", minHeight: "100%",
                  fontFamily: "'JetBrains Mono', 'Fira Code', Consolas, monospace",
                  fontSize: "13px", lineHeight: "1.7"
                }}>
                  {/* Line Numbers */}
                  <div style={{
                    padding: "16px 8px", background: "var(--bg-secondary)",
                    textAlign: "right", userSelect: "none",
                    color: "var(--text-muted)", opacity: 0.4,
                    borderRight: "1px solid var(--border-subtle)"
                  }}>
                    {content.split("\n").map((_, i) => (
                      <div key={i} style={{ height: "22px" }}>{i + 1}</div>
                    ))}
                  </div>

                  {/* Textarea */}
                  <textarea
                    value={content}
                    onChange={(e) => handleContentChange(e.target.value)}
                    spellCheck={false}
                    style={{
                      flex: 1, padding: "16px", resize: "none",
                      background: "var(--bg-primary)", color: "var(--text-secondary)",
                      border: "none", outline: "none", caretColor: "var(--primary-color)",
                      fontFamily: "inherit", fontSize: "inherit", lineHeight: "inherit",
                      tabSize: 2
                    }}
                  />
                </div>

                {/* Save Status Indicator */}
                {!hasChanges && content && (
                  <div style={{
                    position: "absolute", bottom: "12px", right: "12px",
                    display: "flex", alignItems: "center", gap: "6px",
                    padding: "6px 12px", borderRadius: "var(--radius-md)",
                    background: "rgba(34, 197, 94, 0.1)", color: "#22c55e",
                    fontSize: "12px", fontWeight: 500
                  }}>
                    <CheckCircle2 size={13} />Saved
                  </div>
                )}
              </>
            )}
          </div>
        </div>
      </div>

      {/* Info Card */}
      <div className="card card-elevated" style={{ marginTop: "16px" }}>
        <div style={{ display: "flex", gap: "24px", alignItems: "start" }}>
          <div style={{ flex: 1 }}>
            <h3 className="card-title"><AlertTriangle size={18} />{t.config.warning}</h3>
            <p style={{ color: "var(--text-secondary)", fontSize: "13.5px", lineHeight: "1.7", margin: 0 }}>
              {t.config.warningMessage}
            </p>
          </div>
          <div style={{ flex: 1 }}>
            <h3 className="card-title"><SettingsIcon size={18} />Tips</h3>
            <ul style={{ color: "var(--text-secondary)", fontSize: "13.5px", lineHeight: "1.9", paddingLeft: "20px", margin: 0 }}>
              <li>Use YAML syntax highlighting for .yaml files</li>
              <li>Environment variables in .env should be KEY=VALUE format</li>
              <li>Docker Compose files support service definitions</li>
              <li>Always backup before making changes</li>
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
};

export default Config;
