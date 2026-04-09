import React, { useState, useEffect } from 'react';
import { useI18n } from '../i18n';
import { invoke } from "../utils/tauriCompat";

const Settings: React.FC = () => {
  const { t, language, setLanguage, availableLanguages } = useI18n();
  const [backendUrl, setBackendUrl] = useState<string>('http://localhost:18789');
  const [theme, setTheme] = useState<'light' | 'dark' | 'auto'>('dark');
  const [autoStart, setAutoStart] = useState<boolean>(false);
  const [notificationEnabled, setNotificationEnabled] = useState<boolean>(true);
  const [saveStatus, setSaveStatus] = useState<'idle' | 'saving' | 'saved' | 'error'>('idle');
  const [systemInfo, setSystemInfo] = useState<{ platform: string; arch: string; version: string } | null>(null);

  useEffect(() => {
    const loadSettings = async () => {
      try {
        const savedSettings = localStorage.getItem('agentos_settings');
        if (savedSettings) {
          const settings = JSON.parse(savedSettings);
          setBackendUrl(settings.backendUrl || 'http://localhost:18789');
          setTheme(settings.theme || 'dark');
          setAutoStart(settings.autoStart || false);
          setNotificationEnabled(settings.notificationEnabled !== false);
        }
      } catch (error) {
        console.error('加载设置失败:', error);
      }
    };

    loadSettings();
  }, []);

  useEffect(() => {
    const loadSystemInfo = async () => {
      try {
        const info = await invoke<{ platform: string; arch: string; version: string }>('get_system_info');
        setSystemInfo(info);
      } catch (error) {
        console.error('获取系统信息失败:', error);
      }
    };

    loadSystemInfo();
  }, []);

  useEffect(() => {
    const applyTheme = () => {
      const root = document.documentElement;
      if (theme === 'light' || (theme === 'auto' && window.matchMedia('(prefers-color-scheme: light)').matches)) {
        root.classList.add('light');
        root.classList.remove('dark');
      } else {
        root.classList.add('dark');
        root.classList.remove('light');
      }
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
      console.error('保存设置失败:', error);
      setSaveStatus('error');
    }
  };

  const handleTestConnection = async () => {
    try {
      const result = await invoke<{ success: boolean; message: string }>('test_backend_connection', { url: backendUrl });
      if (result.success) {
        alert(t('settings.connectionSuccess', { url: backendUrl }));
      } else {
        alert(t('settings.connectionFailed', { error: result.message }));
      }
    } catch (error) {
      alert(t('settings.connectionError', { error: String(error) }));
    }
  };

  const handleResetSettings = () => {
    if (confirm(t('settings.resetConfirm'))) {
      localStorage.removeItem('agentos_settings');
      setBackendUrl('http://localhost:18789');
      setTheme('dark');
      setAutoStart(false);
      setNotificationEnabled(true);
      setLanguage('en');
    }
  };

  return (
    <div className="page-container">
      <div className="page-header">
        <h1>{t('settings.title')}</h1>
        <p className="text-muted">{t('settings.description')}</p>
      </div>

      <div className="grid-2 gap-6">
        <div className="card">
          <h2 className="card-title">{t('settings.general')}</h2>

          <div className="form-group">
            <label htmlFor="language">{t('settings.language')}</label>
            <select
              id="language"
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
            <p className="form-help">{t('settings.languageHelp')}</p>
          </div>

          <div className="form-group">
            <label htmlFor="theme">{t('settings.theme')}</label>
            <select
              id="theme"
              className="form-select"
              value={theme}
              onChange={(e) => setTheme(e.target.value as 'light' | 'dark' | 'auto')}
            >
              <option value="dark">{t('settings.themeDark')}</option>
              <option value="light">{t('settings.themeLight')}</option>
              <option value="auto">{t('settings.themeAuto')}</option>
            </select>
            <p className="form-help">{t('settings.themeHelp')}</p>
          </div>

          <div className="form-group">
            <label className="checkbox-label">
              <input
                type="checkbox"
                checked={autoStart}
                onChange={(e) => setAutoStart(e.target.checked)}
              />
              <span>{t('settings.autoStart')}</span>
            </label>
            <p className="form-help">{t('settings.autoStartHelp')}</p>
          </div>

          <div className="form-group">
            <label className="checkbox-label">
              <input
                type="checkbox"
                checked={notificationEnabled}
                onChange={(e) => setNotificationEnabled(e.target.checked)}
              />
              <span>{t('settings.notifications')}</span>
            </label>
            <p className="form-help">{t('settings.notificationsHelp')}</p>
          </div>
        </div>

        <div className="card">
          <h2 className="card-title">{t('settings.connection')}</h2>

          <div className="form-group">
            <label htmlFor="backendUrl">{t('settings.backendUrl')}</label>
            <input
              id="backendUrl"
              type="text"
              className="form-input"
              value={backendUrl}
              onChange={(e) => setBackendUrl(e.target.value)}
              placeholder="http://localhost:18789"
            />
            <p className="form-help">{t('settings.backendUrlHelp')}</p>
          </div>

          <div className="form-group">
            <button
              className="btn btn-secondary"
              onClick={handleTestConnection}
              disabled={saveStatus === 'saving'}
            >
              {t('settings.testConnection')}
            </button>
            <p className="form-help">{t('settings.testConnectionHelp')}</p>
          </div>
        </div>

        <div className="card">
          <h2 className="card-title">{t('settings.system')}</h2>

          {systemInfo ? (
            <div className="space-y-2">
              <div className="flex justify-between">
                <span className="text-muted">{t('settings.platform')}:</span>
                <span>{systemInfo.platform}</span>
              </div>
              <div className="flex justify-between">
                <span className="text-muted">{t('settings.architecture')}:</span>
                <span>{systemInfo.arch}</span>
              </div>
              <div className="flex justify-between">
                <span className="text-muted">{t('settings.version')}:</span>
                <span>{systemInfo.version}</span>
              </div>
            </div>
          ) : (
            <p>{t('settings.loadingSystemInfo')}</p>
          )}
        </div>

        <div className="card">
          <h2 className="card-title">{t('settings.actions')}</h2>

          <div className="flex gap-3">
            <button
              className="btn btn-primary"
              onClick={handleSaveSettings}
              disabled={saveStatus === 'saving'}
            >
              {saveStatus === 'saving' ? t('settings.saving') : t('settings.save')}
            </button>

            <button
              className="btn btn-secondary"
              onClick={handleResetSettings}
            >
              {t('settings.reset')}
            </button>

            <button
              className="btn btn-danger"
              onClick={() => {
                if (confirm(t('settings.clearCacheConfirm'))) {
                  localStorage.clear();
                  alert(t('settings.cacheCleared'));
                }
              }}
            >
              {t('settings.clearCache')}
            </button>
          </div>

          {saveStatus === 'saved' && (
            <div className="mt-3 p-2 bg-success/10 text-success rounded">
              {t('settings.savedSuccess')}
            </div>
          )}

          {saveStatus === 'error' && (
            <div className="mt-3 p-2 bg-error/10 text-error rounded">
              {t('settings.saveError')}
            </div>
          )}
        </div>
      </div>

      <div className="mt-8 text-center text-sm text-muted">
        <p>{t('settings.footer')}</p>
        <p className="mt-2">
          {t('settings.documentation')}:{' '}
          <a
            href="https://docs.agentos.io"
            className="text-primary hover:underline"
            onClick={(e) => {
              e.preventDefault();
              invoke('open_browser', { url: 'https://docs.agentos.io' });
            }}
          >
            docs.agentos.io
          </a>
        </p>
      </div>
    </div>
  );
};

export default Settings;