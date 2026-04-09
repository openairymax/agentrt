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
import { invoke } from "../utils/tauriCompat";
import { useI18n } from "../i18n";

const Config: React.FC = () => {
  const { t } = useI18n();

  const CONFIG_FILES = [
    {
      name: t.config.envVariables,
      path: ".env.production",
      description: t.config.envVariablesDesc,
      icon: "🔐",
      sensitive: true,
    },
    {
      name: t.config.mainConfig,
      path: "config/agentos.yaml",
      description: t.config.mainConfigDesc,
      icon: "⚙️",
      sensitive: false,
    },
    {
      name: t.config.dockerComposeDev,
      path: "docker/docker-compose.yml",
      description: t.config.dockerComposeDevDesc,
      icon: "🐳",
      sensitive: false,
    },
    {
      name: t.config.dockerComposeProd,
      path: "docker/docker-compose.prod.yml",
      description: t.config.dockerComposeProdDesc,
      icon: "🏭",
      sensitive: false,
    },
  ];

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
      setMessage({ type: "error", text: `${t.config.failedToLoad}: ${error}` });
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
      setMessage({ type: "success", text: t.config.saveSuccess });
    } catch (error) {
      console.error("Failed to save config file:", error);
      setMessage({ type: "error", text: `${t.config.failedToSave}: ${error}` });
    } finally {
      setSaving(false);
      setTimeout(() => setMessage(null), 3000);
    }
  };

  const handleDiscardChanges = () => {
    setFileContent(originalContent);
    setMessage({ type: "warning", text: t.config.changesDiscarded });
    setTimeout(() => setMessage(null), 2000);
  };

  const hasUnsavedChanges = fileContent !== originalContent;
  const selectedConfig = CONFIG_FILES.find((f) => f.path === selectedFile);

  return (
    <div>
      <div className="card" style={{ marginBottom: "20px" }}>
        <h3 className="card-title" style={{ marginBottom: 0 }}>
          <Settings size={20} />
          {t.config.managementTitle}
        </h3>
      </div>

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
        <div className="card">
          <h3 className="card-title">
            <FolderOpen size={20} />
            {t.config.configurationFiles}
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
                    {t.config.sensitive}
                  </span>
                )}
              </div>
            ))}
          </div>
        </div>

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
              {selectedConfig?.name || t.config.selectFile}
            </h3>

            <div style={{ display: "flex", gap: "8px" }}>
              <button
                className="btn btn-secondary"
                onClick={() => selectedFile && loadConfigFile(selectedFile)}
                disabled={loading}
              >
                <RefreshCw size={16} />
                {t.config.reload}
              </button>

              {hasUnsavedChanges && (
                <button
                  className="btn btn-secondary"
                  onClick={handleDiscardChanges}
                >
                  {t.config.discard}
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
                    {t.config.saving}
                  </>
                ) : (
                  <>
                    <Save size={16} />
                    {t.config.save}
                  </>
                )}
              </button>
            </div>
          </div>

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
              {t.config.youHaveUnsavedChanges}
            </div>
          )}

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
              placeholder={t.config.selectFileToEdit}
            />
          )}

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
              <span>{t.config.path}: {selectedFile}</span>
              <span>
                {t.config.lines}: {fileContent.split("\n").length} • {t.config.size}:{" "}
                {(new TextEncoder().encode(fileContent).length / 1024).toFixed(1)} KB
              </span>
            </div>
          )}
        </div>
      </div>

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
              <h4 style={{ color: "#f59e0b", marginBottom: "8px" }}>{t.config.securityNotice}</h4>
              <p style={{ color: "var(--text-secondary)", fontSize: "14px", lineHeight: "1.6" }}>
                {t.config.securityNoticeDesc}
              </p>
              <ul style={{ color: "var(--text-secondary)", paddingLeft: "20px", marginTop: "8px" }}>
                <li>{t.config.doNotCommit}</li>
                <li>{t.config.useStrongPasswords}</li>
                <li>{t.config.rotateSecrets}</li>
                <li>{t.config.neverShare}</li>
              </ul>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default Config;
