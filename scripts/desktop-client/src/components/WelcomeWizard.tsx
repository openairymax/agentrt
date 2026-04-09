import React, { useState } from 'react';
import { invoke } from '../utils/tauriCompat';
import { useI18n } from '../i18n';
import { ChevronRight, Globe, FolderOpen, Server, CheckCircle } from 'lucide-react';

interface WelcomeWizardProps {
  onComplete: () => void;
}

function WelcomeWizard({ onComplete }: WelcomeWizardProps) {
  const { language, setLanguage, t, availableLanguages } = useI18n();
  const [step, setStep] = useState(1);
  const [projectPath, setProjectPath] = useState('');
  const [serviceMode, setServiceMode] = useState<'docker' | 'local'>('docker');
  const [isConfiguring, setIsConfiguring] = useState(false);

  const steps = [
    { id: 1, title: t.settings.language, icon: Globe },
    { id: 2, title: t.settings.projectPath, icon: FolderOpen },
    { id: 3, title: t.services.title, icon: Server },
    { id: 4, title: t.common.success, icon: CheckCircle },
  ];

  const handleComplete = async () => {
    setIsConfiguring(true);
    try {
      await invoke('save_settings', {
        settings: {
          language,
          projectPath,
          serviceMode,
        },
      });

      localStorage.setItem('agentos-wizard-completed', 'true');
      onComplete();
    } catch (error) {
      console.error('Failed to save settings:', error);
      localStorage.setItem('agentos-wizard-completed', 'true');
      onComplete();
    } finally {
      setIsConfiguring(false);
    }
  };

  return (
    <div className="wizard-container">
      <div className="wizard-sidebar">
        <div className="wizard-logo">
          <div className="wizard-logo-icon">A</div>
          <div className="wizard-logo-text">AgentOS</div>
        </div>

        <div className="wizard-steps">
          {steps.map((s) => (
            <div
              key={s.id}
              className={`wizard-step ${step === s.id ? 'active' : ''} ${step > s.id ? 'completed' : ''}`}
            >
              <div className="wizard-step-icon">
                <s.icon size={20} />
              </div>
              <span className="wizard-step-title">{s.title}</span>
            </div>
          ))}
        </div>
      </div>

      <div className="wizard-content">
        {step === 1 && (
          <div className="wizard-page">
            <h2>{t.settings.language}</h2>
            <p className="wizard-desc">
              {language === 'zh' ? '选择您偏好的语言' : 'Select your preferred language'}
            </p>

            <div className="wizard-language-options">
              {availableLanguages.map((lang) => (
                <button
                  key={lang.code}
                  className={`wizard-language-option ${language === lang.code ? 'selected' : ''}`}
                  onClick={() => setLanguage(lang.code as 'en' | 'zh')}
                >
                  <span className="wizard-lang-name">{lang.name}</span>
                  <span className="wizard-lang-code">{lang.code.toUpperCase()}</span>
                </button>
              ))}
            </div>
          </div>
        )}

        {step === 2 && (
          <div className="wizard-page">
            <h2>{t.settings.projectPath}</h2>
            <p className="wizard-desc">{t.dashboard.systemInfo}</p>

            <div className="wizard-path-group">
              <input
                type="text"
                value={projectPath}
                onChange={(e) => setProjectPath(e.target.value)}
                placeholder={language === 'zh' ? '/path/to/agentos' : 'C:\\path\\to\\agentos'}
                className="form-input"
              />
              <button
                className="btn btn-secondary"
                onClick={async () => {
                  try {
                    const selected = await invoke<string>('select_directory');
                    if (selected) {
                      setProjectPath(selected);
                    }
                  } catch (error) {
                    console.error('Failed to select directory:', error);
                  }
                }}
              >
                {t.config.selectFile}
              </button>
            </div>

            <p className="form-help">
              {language === 'zh'
                ? '选择 AgentOS 项目的根目录路径'
                : 'Select the root directory path of your AgentOS project'}
            </p>
          </div>
        )}

        {step === 3 && (
          <div className="wizard-page">
            <h2>{t.services.title}</h2>
            <p className="wizard-desc">
              {language === 'zh' ? '选择服务运行模式' : 'Select service running mode'}
            </p>

            <div className="wizard-service-options">
              <button
                className={`wizard-service-option ${serviceMode === 'docker' ? 'selected' : ''}`}
                onClick={() => setServiceMode('docker')}
              >
                <div className="wizard-service-icon">🐳</div>
                <div className="wizard-service-name">Docker</div>
                <div className="wizard-service-desc">
                  {language === 'zh'
                    ? '使用 Docker Compose 运行服务（推荐）'
                    : 'Run services with Docker Compose (Recommended)'}
                </div>
              </button>

              <button
                className={`wizard-service-option ${serviceMode === 'local' ? 'selected' : ''}`}
                onClick={() => setServiceMode('local')}
              >
                <div className="wizard-service-icon">💻</div>
                <div className="wizard-service-name">Local</div>
                <div className="wizard-service-desc">
                  {language === 'zh'
                    ? '直接在本地运行服务（开发模式）'
                    : 'Run services locally (Development mode)'}
                </div>
              </button>
            </div>
          </div>
        )}

        {step === 4 && (
          <div className="wizard-page wizard-success-page">
            <div className="wizard-success-icon">✅</div>
            <h2>{t.common.success}!</h2>
            <p>
              {language === 'zh'
                ? 'AgentOS 桌面客户端已准备就绪'
                : 'AgentOS Desktop Client is ready to use'}
            </p>

            <div className="wizard-summary">
              <div className="wizard-summary-item">
                <span className="wizard-summary-label">{t.settings.language}:</span>
                <span className="wizard-summary-value">
                  {availableLanguages.find((l) => l.code === language)?.name}
                </span>
              </div>
              <div className="wizard-summary-item">
                <span className="wizard-summary-label">{t.settings.projectPath}:</span>
                <span className="wizard-summary-value">{projectPath || (language === 'zh' ? '未设置' : 'Not set')}</span>
              </div>
              <div className="wizard-summary-item">
                <span className="wizard-summary-label">{t.services.mode}:</span>
                <span className="wizard-summary-value">
                  {serviceMode === 'docker' ? 'Docker' : 'Local'}
                </span>
              </div>
            </div>
          </div>
        )}

        <div className="wizard-actions">
          {step > 1 && (
            <button className="btn btn-secondary" onClick={() => setStep(step - 1)}>
              {t.common.cancel}
            </button>
          )}

          {step < 4 ? (
            <button className="btn btn-primary" onClick={() => setStep(step + 1)}>
              {t.common.ok}
              <ChevronRight size={18} />
            </button>
          ) : (
            <button
              className="btn btn-primary"
              onClick={handleComplete}
              disabled={isConfiguring}
            >
              {isConfiguring ? t.common.loading : t.common.ok}
            </button>
          )}
        </div>
      </div>
    </div>
  );
}

export default WelcomeWizard;
