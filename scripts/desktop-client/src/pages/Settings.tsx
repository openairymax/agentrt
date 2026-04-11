import React, { useState, useEffect } from 'react';
import {
  Palette,
  Globe,
  Monitor,
  Sun,
  Moon,
  MonitorCheck,
  Bell,
  Zap,
  Server,
  Save,
  RotateCcw,
  Trash2,
  CheckCircle2,
  AlertTriangle,
  Info,
  ExternalLink,
  Sparkles,
} from 'lucide-react';
import { useI18n } from '../i18n';
import sdk from "../services/agentos-sdk";

const Settings: React.FC = () => {
  const { t, language, setLanguage, availableLanguages } = useI18n();
  const [backendUrl, setBackendUrl] = useState<string>('http://localhost:18789');
  const [theme, setTheme] = useState<'light' | 'dark' | 'auto'>('light');
  const [autoStart, setAutoStart] = useState<boolean>(false);
  const [notificationEnabled, setNotificationEnabled] = useState<boolean>(true);
  const [saveStatus, setSaveStatus] = useState<'idle' | 'saving' | 'saved' | 'error'>('idle');
  const [systemInfo, setSystemInfo] = useState<{ platform: string; arch: string; version: string; cpu_cores?: number; total_memory_gb?: number } | null>(null);

  useEffect(() => {
    const loadSettings = async () => {
      try {
        const savedSettings = localStorage.getItem('agentos_settings');
        if (savedSettings) {
          const settings = JSON.parse(savedSettings);
          setBackendUrl(settings.backendUrl || 'http://localhost:18789');
          setTheme(settings.theme || 'light');
          setAutoStart(settings.autoStart || false);
          setNotificationEnabled(settings.notificationEnabled !== false);
        }
      } catch (error) {
        console.error('Failed to load settings:', error);
      }
    };
    loadSettings();
  }, []);

  useEffect(() => {
    const loadSystemInfo = async () => {
      try {
        const info = await sdk.getSystemInfo();
        setSystemInfo({ platform: info.os, arch: info.architecture, version: info.os_version, cpu_cores: info.cpu_cores, total_memory_gb: info.total_memory_gb });
      } catch (error) {
        console.error('Failed to load system info:', error);
      }
    };
    loadSystemInfo();
  }, []);

  useEffect(() => {
    const applyTheme = () => {
      const root = document.documentElement;
      root.classList.add('theme-smooth-transition');
      if (theme === 'light' || (theme === 'auto' && window.matchMedia('(prefers-color-scheme: light)').matches)) {
        root.classList.add('light');
        root.classList.remove('dark');
      } else {
        root.classList.add('dark');
        root.classList.remove('light');
      }
      setTimeout(() => {
        root.classList.remove('theme-smooth-transition');
      }, 400);
    };
    applyTheme();
    const mediaQuery = window.matchMedia('(prefers-color-scheme: light)');
    const handleChange = () => applyTheme();
    mediaQuery.addEventListener('change', handleChange);
    return () => mediaQuery.removeEventListener('change', handleChange);
  }, [theme]);

  const handleSaveSettings = async () => {
    setSaveStatus('saving');
    try {
      const settings = {
        backendUrl,
        theme,
        autoStart,
        notificationEnabled,
        language,
        savedAt: new Date().toISOString(),
      };
      localStorage.setItem('agentos_settings', JSON.stringify(settings));
      if (settings.language !== language) {
        window.location.reload();
        return;
      }
      setSaveStatus('saved');
      setTimeout(() => setSaveStatus('idle'), 3000);
    } catch (error) {
      console.error('Failed to save settings:', error);
      setSaveStatus('error');
    }
  };

  const handleTestConnection = async () => {
    try {
      const result = await sdk.getHealthStatus();
      alert(result.overall === "healthy" ? t.settings.connectionSuccess.replace('{url}', backendUrl) : t.settings.connectionFailed.replace('{error}', result.overall));
    } catch (error) {
      alert(t.settings.connectionError.replace('{error}', String(error)));
    }
  };

  const handleResetSettings = () => {
    if (confirm(t.settings.resetConfirm)) {
      localStorage.removeItem('agentos_settings');
      setBackendUrl('http://localhost:18789');
      setTheme('light');
      setAutoStart(false);
      setNotificationEnabled(true);
      setLanguage('zh');
    }
  };

  const themeOptions = [
    { value: 'light' as const, icon: Sun, label: t.settings.themeLight, desc: '明亮主题', colors: { bg: '#f8fafc', sidebar: '#ffffff', text: '#0f172a', accent: '#6366f1', border: '#e2e8f0' } },
    { value: 'dark' as const, icon: Moon, label: t.settings.themeDark, desc: '暗色主题', colors: { bg: '#0a0a0f', sidebar: '#111118', text: '#ededed', accent: '#6366f1', border: '#27273a' } },
    { value: 'auto' as const, icon: MonitorCheck, label: t.settings.themeAuto, desc: '跟随系统', colors: { bg: 'linear-gradient(135deg, #f8fafc 50%, #0a0a0f 50%)', sidebar: 'linear-gradient(135deg, #ffffff 50%, #111118 50%)', text: '#6366f1', accent: '#6366f1', border: '#94a3b8' } },
  ];

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
            <Palette size={20} color="white" />
          </div>
          <div>
            <h1>{t.settings.title}</h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: 0 }}>
              {t.settings.description}
            </p>
          </div>
        </div>
      </div>

      {/* Save Status Banner */}
      {saveStatus === 'saved' && (
        <div className="card card-elevated" style={{ marginBottom: "20px", background: "rgba(34, 197, 94, 0.1)", borderColor: "#22c55e", padding: "14px 20px", display: "flex", alignItems: "center", gap: "10px" }}>
          <CheckCircle2 size={20} color="#22c55e" />
          <span style={{ color: "#22c55e", fontWeight: 500 }}>{t.settings.savedSuccess}</span>
        </div>
      )}
      {saveStatus === 'error' && (
        <div className="card card-elevated" style={{ marginBottom: "20px", background: "rgba(239, 68, 68, 0.1)", borderColor: "#ef4444", padding: "14px 20px", display: "flex", alignItems: "center", gap: "10px" }}>
          <AlertTriangle size={20} color="#ef4444" />
          <span style={{ color: "#ef4444", fontWeight: 500 }}>{t.settings.saveError}</span>
        </div>
      )}

      {/* Settings Grid */}
      <div className="grid-2" style={{ marginBottom: "24px" }}>
        {/* Appearance Card */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Palette size={18} />
            {t.settings.general}
          </h3>

          {/* Language Selector */}
          <div className="form-group">
            <label className="form-label">
              <Globe size={14} style={{ display: "inline", marginRight: "6px", verticalAlign: "middle" }} />
              {t.settings.language}
            </label>
            <select
              className="form-select"
              value={language}
              onChange={(e) => setLanguage(e.target.value as 'en' | 'zh')}
            >
              {availableLanguages.map((lang) => (
                <option key={lang.code} value={lang.code}>
                  {lang.name}
                </option>
              ))}
            </select>
            <p className="form-help">{t.settings.languageHelp}</p>
          </div>

          {/* Theme Selector - Visual Preview Cards */}
          <div className="form-group">
            <label className="form-label">
              <Monitor size={14} style={{ display: "inline", marginRight: "6px", verticalAlign: "middle" }} />
              {t.settings.theme}
            </label>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: "10px", marginTop: "8px" }}>
              {themeOptions.map((opt) => {
                const IconComp = opt.icon;
                const isSelected = theme === opt.value;
                const c = opt.colors;
                const isAuto = opt.value === 'auto';
                return (
                  <div
                    key={opt.value}
                    className={`theme-preview-card ${isSelected ? 'selected' : ''}`}
                    onClick={() => setTheme(opt.value)}
                  >
                    <div className="theme-preview-mini">
                      <div className="theme-preview-titlebar" style={{ background: isAuto ? c.bg : c.bg }}>
                        <div className="theme-preview-dot" style={{ background: '#ef4444' }} />
                        <div className="theme-preview-dot" style={{ background: '#f59e0b' }} />
                        <div className="theme-preview-dot" style={{ background: '#22c55e' }} />
                      </div>
                      <div className="theme-preview-body">
                        <div className="theme-preview-sidebar" style={{ background: isAuto ? c.sidebar : c.sidebar }} />
                        <div className="theme-preview-content" style={{ background: isAuto ? c.bg : c.bg }}>
                          <div className="theme-preview-line" style={{ background: isAuto ? c.text : c.border, opacity: 0.5 }} />
                          <div className="theme-preview-line" style={{ background: c.accent, opacity: 0.6 }} />
                          <div className="theme-preview-line" style={{ background: isAuto ? c.text : c.border, opacity: 0.3 }} />
                        </div>
                      </div>
                    </div>
                    <div style={{ display: "flex", alignItems: "center", justifyContent: "center", gap: "6px" }}>
                      <IconComp size={16} color={isSelected ? "var(--primary-color)" : "var(--text-muted)"} />
                      <span style={{ fontSize: "13px", fontWeight: 600, color: isSelected ? "var(--primary-color)" : "var(--text-secondary)" }}>
                        {opt.label}
                      </span>
                    </div>
                    <div style={{ fontSize: "11px", color: "var(--text-muted)", textAlign: "center", marginTop: "2px" }}>
                      {opt.desc}
                    </div>
                  </div>
                );
              })}
            </div>
            <p className="form-help">{t.settings.themeHelp}</p>
          </div>

          {/* Toggle Options */}
          <div style={{ display: "flex", flexDirection: "column", gap: "16px", marginTop: "8px" }}>
            <label className="checkbox-label" style={{ padding: "12px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)" }}>
              <input type="checkbox" checked={autoStart} onChange={(e) => setAutoStart(e.target.checked)} />
              <div>
                <span style={{ fontWeight: 500 }}>{t.settings.autoStart}</span>
                <p style={{ fontSize: "12px", color: "var(--text-muted)", margin: "4px 0 0 0" }}>{t.settings.autoStartHelp}</p>
              </div>
            </label>

            <label className="checkbox-label" style={{ padding: "12px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)" }}>
              <input type="checkbox" checked={notificationEnabled} onChange={(e) => setNotificationEnabled(e.target.checked)} />
              <div>
                <span style={{ fontWeight: 500 }}>
                  <Bell size={14} style={{ display: "inline", marginRight: "6px", verticalAlign: "middle" }} />
                  {t.settings.notifications}
                </span>
                <p style={{ fontSize: "12px", color: "var(--text-muted)", margin: "4px 0 0 0" }}>{t.settings.notificationsHelp}</p>
              </div>
            </label>
          </div>
        </div>

        {/* Connection Card */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Server size={18} />
            {t.settings.connection}
          </h3>

          <div className="form-group">
            <label className="form-label">{t.settings.backendUrl}</label>
            <input
              type="text"
              className="form-input"
              value={backendUrl}
              onChange={(e) => setBackendUrl(e.target.value)}
              placeholder="http://localhost:18789"
              style={{ fontFamily: "'JetBrains Mono', monospace" }}
            />
            <p className="form-help">{t.settings.backendUrlHelp}</p>
          </div>

          <button
            className="btn btn-secondary btn-lg"
            onClick={handleTestConnection}
            disabled={saveStatus === 'saving'}
            style={{ width: "100%", marginBottom: "16px" }}
          >
            <Zap size={16} />
            {t.settings.testConnection}
          </button>
          <p className="form-help" style={{ textAlign: "center" }}>{t.settings.testConnectionHelp}</p>

          {/* Quick Tips */}
          <div style={{
            marginTop: "20px",
            padding: "16px",
            background: "var(--primary-light)",
            borderRadius: "var(--radius-md)",
            border: "1px solid rgba(99, 102, 241, 0.15)",
          }}>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "8px" }}>
              <Sparkles size={16} style={{ color: "var(--primary-color)" }} />
              <span style={{ fontSize: "13px", fontWeight: 600, color: "var(--primary-color)" }}>快速提示</span>
            </div>
            <div style={{ fontSize: "12.5px", color: "var(--text-secondary)", lineHeight: 1.6 }}>
              确保后端服务已启动并运行在正确的端口上。如果使用 Docker，请检查容器状态和网络配置。
            </div>
          </div>
        </div>

        {/* System Info Card */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Info size={18} />
            {t.settings.system}
          </h3>

          {systemInfo ? (
            <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
              <InfoRow label={t.settings.platform} value={systemInfo.platform} />
              <InfoRow label={t.settings.architecture} value={systemInfo.arch} />
              <InfoRow label={t.settings.version} value={`v${systemInfo.version}`} />
              {systemInfo.cpu_cores && <InfoRow label={t.settings.cpuCores || "CPU Cores"} value={`${systemInfo.cpu_cores} cores`} />}
              {systemInfo.total_memory_gb && <InfoRow label="Total Memory" value={`${systemInfo.total_memory_gb.toFixed(1)} GB`} />}
            </div>
          ) : (
            <div style={{ textAlign: "center", padding: "32px 0", color: "var(--text-muted)" }}>
              <div className="loading-spinner" style={{ margin: "0 auto 12px" }} />
              <p>{t.settings.loadingSystemInfo}</p>
            </div>
          )}
        </div>

        {/* Actions Card */}
        <div className="card card-elevated">
          <h3 className="card-title">
            <Save size={18} />
            {t.settings.actions}
          </h3>

          <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
            <button
              className="btn btn-primary btn-lg"
              onClick={handleSaveSettings}
              disabled={saveStatus === 'saving'}
              style={{ width: "100%" }}
            >
              {saveStatus === 'saving' ? (
                <>
                  <span className="loading-spinner" style={{ width: 16, height: 16, borderWidth: "2px" }} />
                  {t.settings.saving}
                </>
              ) : (
                <>
                  <Save size={16} />
                  {t.settings.save}
                </>
              )}
            </button>

            <div style={{ display: "flex", gap: "10px" }}>
              <button className="btn btn-secondary" onClick={handleResetSettings} style={{ flex: 1 }}>
                <RotateCcw size={16} />
                {t.settings.reset}
              </button>
              <button
                className="btn btn-danger"
                onClick={() => {
                  if (confirm(t.settings.clearCacheConfirm)) {
                    localStorage.clear();
                    alert(t.settings.cacheCleared);
                  }
                }}
                style={{ flex: 1 }}
              >
                <Trash2 size={16} />
                {t.settings.clearCache}
              </button>
            </div>
          </div>
        </div>
      </div>

      {/* Footer */}
      <div className="card card-elevated" style={{ textAlign: "center", padding: "24px" }}>
        <p style={{ color: "var(--text-muted)", fontSize: "13px", margin: 0 }}>
          {t.settings.footer}
        </p>
        <p style={{ color: "var(--text-muted)", fontSize: "13px", margin: "8px 0 0 0" }}>
          {t.settings.documentation}:{' '}
          <a
            href="https://docs.agentos.io"
            style={{ color: "var(--primary-color)", textDecoration: "none", fontWeight: 500 }}
            onClick={(e) => {
              e.preventDefault();
              sdk.openBrowser('https://docs.agentos.io');
            }}
          >
            docs.agentos.io
            <ExternalLink size={12} style={{ display: "inline", marginLeft: "4px" }} />
          </a>
        </p>
      </div>
    </div>
  );
};

function InfoRow({ label, value }: { label: string; value: string }) {
  return (
    <div style={{
      display: "flex",
      justifyContent: "space-between",
      alignItems: "center",
      padding: "12px 0",
      borderBottom: "1px solid var(--border-subtle)",
      fontSize: "13.5px"
    }}>
      <span style={{ color: "var(--text-secondary)" }}>{label}</span>
      <span style={{ fontWeight: 500, fontFamily: "'JetBrains Mono', monospace", fontSize: "13px" }}>{value}</span>
    </div>
  );
}

export default Settings;
