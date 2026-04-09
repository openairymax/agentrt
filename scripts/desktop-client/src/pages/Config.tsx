import React, { useState, useEffect } from "react";
import {
  Settings,
  Save,
  FolderOpen,
  RefreshCw,
  FileJson,
  AlertTriangle,
  CheckCircle2,
} from "lucide-react";
import { invoke } from "@tauri-apps/api/core";

const CONFIG_FILES = [
  {
    name: "Environment Variables",
    path: ".env.production",
    description: "Database, Redis, JWT, and API configuration",
    icon: "🔐",
    sensitive: true,
  },
  {
    name: "Main Configuration",
    path: "config/agentos.yaml",
    description: "Core AgentOS settings and parameters",
    icon: "⚙️",
    sensitive: false,
  },
  {
    name: "Docker Compose (Dev)",
    path: "docker/docker-compose.yml",
    description: "Development environment service definitions",
    icon: "🐳",
    sensitive: false,
  },
  {
    name: "Docker Compose (Prod)",
    path: "docker/docker-compose.prod.yml",
    description: "Production environment service definitions",
    icon: "🏭",
    sensitive: false,
  },
];

const Config: React.FC = () => {
  const [selectedFile, setSelectedFile] = useState<string | null>(null);
  const [fileContent, setFileContent] = useState("");
  const [originalContent, setOriginalContent] = useState("");
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState<{ type: "success" | "error" | "warning"; text: string } | null>(null);

  useEffect(() => {
    if (CONFIG_FILES.length > 0 && !selectedFile) {
      setSelectedFile(CONFIG_FILES[0].path);
    }
  }, []);

  useEffect(() => {
    if (selectedFile) {
      loadConfigFile(selectedFile);
    }
  }, [selectedFile]);

  const loadConfigFile = async (filePath: string) => {
    setLoading(true);
    setMessage(null);
    try {
      const content = await invoke<string>("read_config_file", { path: filePath });
      setFileContent(content);
      setOriginalContent(content);
    } catch (error) {
      console.error("Failed to load config file:", error);
      setFileContent("");
      setOriginalContent("");
      setMessage({ type: "error", text: `Failed to load file: ${error}` });
    } finally {
      setLoading(false);
    }
  };

  const handleSaveFile = async () => {
    if (!selectedFile) return;

    setSaving(true);
    setMessage(null);

    try {
      await invoke("write_config_file", {
        path: selectedFile,
        content: fileContent,
      });

      setOriginalContent(fileContent);
      setMessage({ type: "success", text: "Configuration saved successfully!" });
    } catch (error) {
      console.error("Failed to save config file:", error);
      setMessage({ type: "error", text: `Failed to save: ${error}` });
    } finally {
      setSaving(false);
      setTimeout(() => setMessage(null), 3000);
    }
  };

  const handleDiscardChanges = () => {
    setFileContent(originalContent);
    setMessage({ type: "warning", text: "Changes discarded" });
    setTimeout(() => setMessage(null), 2000);
  };

  const hasUnsavedChanges = fileContent !== originalContent;
  const selectedConfig = CONFIG_FILES.find((f) => f.path === selectedFile);

  return (
    <div>
      {/* Header */}
      <div className="card" style={{ marginBottom: "20px" }}>
        <h3 className="card-title" style={{ marginBottom: 0 }}>
          <Settings size={20} />
          Configuration Management
        </h3>
      </div>

      {/* Message Banner */}
      {message && (
        <div
          style={{
            padding: "12px 20px",
            borderRadius: "8px",
            marginBottom: "20px",
            display: "flex",
            alignItems: "center",
            gap: "10px",
            background:
              message.type === "success"
                ? "rgba(16, 185, 129, 0.15)"
                : message.type === "error"
                ? "rgba(239, 68, 68, 0.15)"
                : "rgba(245, 158, 11, 0.15)",
            border: `1px solid ${
              message.type === "success"
                ? "#10b981"
                : message.type === "error"
                ? "#ef4444"
                : "#f59e0b"
            }`,
          }}
        >
          {message.type === "success" && <CheckCircle2 size={18} color="#10b981" />}
          {message.type === "error" && <AlertTriangle size={18} color="#ef4444" />}
          {message.type === "warning" && <AlertTriangle size={18} color="#f59e0b" />}
          <span>{message.text}</span>
        </div>
      )}

      <div className="grid-2">
        {/* File List */}
        <div className="card">
          <h3 className="card-title">
            <FolderOpen size={20} />
            Configuration Files
          </h3>

          <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
            {CONFIG_FILES.map((file) => (
              <div
                key={file.path}
                onClick={() => setSelectedFile(file.path)}
                style={{
                  display: "flex",
                  alignItems: "center",
                  gap: "12px",
                  padding: "14px",
                  background:
                    selectedFile === file.path
                      ? "rgba(59, 130, 246, 0.1)"
                      : "var(--bg-tertiary)",
                  borderRadius: "10px",
                  cursor: "pointer",
                  border: selectedFile === file.path
                    ? "1px solid var(--primary-color)"
                    : "1px solid transparent",
                  transition: "all 0.2s ease",
                }}
              >
                <span style={{ fontSize: "24px" }}>{file.icon}</span>
                <div style={{ flex: 1 }}>
                  <div style={{ fontWeight: 600, fontSize: "14px" }}>{file.name}</div>
                  <div style={{ fontSize: "12px", color: "var(--text-muted)" }}>
                    {file.description}
                  </div>
                </div>
                {file.sensitive && (
                  <span
                    className="status-badge status-warning"
                    style={{ fontSize: "11px" }}
                  >
                    Sensitive
                  </span>
                )}
              </div>
            ))}
          </div>
        </div>

        {/* Editor */}
        <div className="card">
          <div
            style={{
              display: "flex",
              justifyContent: "space-between",
              alignItems: "center",
              marginBottom: "16px",
            }}
          >
            <h3 className="card-title" style={{ marginBottom: 0 }}>
              <FileJson size={20} />
              {selectedConfig?.name || "Select a file"}
            </h3>

            <div style={{ display: "flex", gap: "8px" }}>
              <button
                className="btn btn-secondary"
                onClick={() => selectedFile && loadConfigFile(selectedFile)}
                disabled={loading}
              >
                <RefreshCw size={16} />
                Reload
              </button>

              {hasUnsavedChanges && (
                <button
                  className="btn btn-secondary"
                  onClick={handleDiscardChanges}
                >
                  Discard
                </button>
              )}

              <button
                className="btn btn-primary"
                onClick={handleSaveFile}
                disabled={saving || !hasUnsavedChanges}
              >
                {saving ? (
                  <>
                    <span className="loading-spinner" style={{ width: 16, height: 16 }} />
                    Saving...
                  </>
                ) : (
                  <>
                    <Save size={16} />
                    Save
                  </>
                )}
              </button>
            </div>
          </div>

          {/* Unsaved Changes Indicator */}
          {hasUnsavedChanges && (
            <div
              style={{
                padding: "8px 12px",
                background: "rgba(245, 158, 11, 0.15)",
                border: "1px solid #f59e0b",
                borderRadius: "6px",
                marginBottom: "12px",
                fontSize: "13px",
                color: "#f59e0b",
                display: "flex",
                alignItems: "center",
                gap: "8px",
              }}
            >
              <AlertTriangle size={14} />
              You have unsaved changes
            </div>
          )}

          {/* Editor Area */}
          {loading ? (
            <div style={{ textAlign: "center", padding: "60px" }}>
              <div className="loading-spinner" />
            </div>
          ) : (
            <textarea
              className="textarea-field"
              value={fileContent}
              onChange={(e) => setFileContent(e.target.value)}
              style={{
                minHeight: "450px",
                fontFamily: "'Fira Code', 'Cascadia Code', 'JetBrains Mono', monospace",
                fontSize: "13px",
                lineHeight: "1.6",
                tabSize: 2,
              }}
              placeholder="Select a configuration file to edit..."
            />
          )}

          {/* File Info */}
          {selectedFile && !loading && (
            <div
              style={{
                marginTop: "12px",
                paddingTop: "12px",
                borderTop: "1px solid var(--border-color)",
                display: "flex",
                justifyContent: "space-between",
                fontSize: "12px",
                color: "var(--text-muted)",
              }}
            >
              <span>Path: {selectedFile}</span>
              <span>
                Lines: {fileContent.split("\n").length} • Size:{" "}
                {(new TextEncoder().encode(fileContent).length / 1024).toFixed(1)} KB
              </span>
            </div>
          )}
        </div>
      </div>

      {/* Warning for sensitive files */}
      {selectedConfig?.sensitive && (
        <div
          className="card"
          style={{
            marginTop: "20px",
            background: "rgba(245, 158, 11, 0.08)",
            borderColor: "#f59e0b",
          }}
        >
          <div style={{ display: "flex", gap: "12px", alignItems: "start" }}>
            <AlertTriangle size={24} color="#f59e0b" style={{ flexShrink: 0 }} />
            <div>
              <h4 style={{ color: "#f59e0b", marginBottom: "8px" }}>Security Notice</h4>
              <p style={{ color: "var(--text-secondary)", fontSize: "14px", lineHeight: "1.6" }}>
                This configuration file contains sensitive information such as passwords,
                API keys, and secret tokens. Please ensure you:
              </p>
              <ul style={{ color: "var(--text-secondary)", paddingLeft: "20px", marginTop: "8px" }}>
                <li>Do not commit this file to version control</li>
                <li>Use strong, unique passwords</li>
                <li>Rotate secrets regularly in production</li>
                <li>Never share these values publicly</li>
              </ul>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default Config;
