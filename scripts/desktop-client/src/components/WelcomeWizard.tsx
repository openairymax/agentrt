import React, { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { I18nProvider, useI18n } from '../i18n';
import { ChevronRight, Globe, FolderOpen, Server, CheckCircle } from 'lucide-react';

interface WelcomeWizardProps {
  onComplete: () => void;
}

function WelcomeWizardContent({ onComplete }: WelcomeWizardProps) {
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

  const handleNext = () => {
    if (step < 4) {
      setStep(step + 1);
    }
  };

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
    } finally {
      setIsConfiguring(false);
    }
  };

  return (
    <div className="wizard-container">
      <div className="wizard-sidebar">
        <div className="wizard-logo">
          <div className="logo-icon">A</div>
          <div className="logo-text">AgentOS</div>
        </div>
        
        <div className="wizard-steps">
          {steps.map((s) => (
            <div
              key={s.id}
              className={`wizard-step ${step === s.id ? 'active' : ''} ${step > s.id ? 'completed' : ''}`}
            >
              <div className="step-icon">
                <s.icon size={20} />
              </div>
              <span className="step-title">{s.title}</span>
            </div>
          ))}
        </div>
      </div>

      <div className="wizard-content">
        {step === 1 && (
          <div className="wizard-page">
            <h2>{t.settings.language}</h2>
            <p>Select your preferred language / 选择您的语言</p>
            
            <div className="language-options">
              {availableLanguages.map((lang) => (
                <button
                  key={lang.code}
                  className={`language-option ${language === lang.code ? 'selected' : ''}`}
                  onClick={() => setLanguage(lang.code)}
                >
                  <span className="language-name">{lang.name}</span>
                  <span className="language-code">{lang.code.toUpperCase()}</span>
                </button>
              ))}
            </div>
          </div>
        )}

        {step === 2 && (
          <div className="wizard-page">
            <h2>{t.settings.projectPath}</h2>
            <p>{t.dashboard.systemInfo}</p>
            
            <div className="path-input-group">
              <input
                type="text"
                value={projectPath}
                onChange={(e) => setProjectPath(e.target.value)}
                placeholder="/path/to/agentos"
                className="path-input"
              />
              <button
                className="btn btn-secondary"
                onClick={async () => {
                  try {
                    const selected = await invoke('select_directory');
                    if (selected) {
                      setProjectPath(selected as string);
                    }
                  } catch (error) {
                    console.error('Failed to select directory:', error);
                  }
                }}
              >
                {t.config.selectFile}
              </button>
            </div>
            
            <p className="hint">
              {language === 'zh' 
                ? '选择 AgentOS 项目的根目录路径'
                : 'Select the root directory path of your AgentOS project'}
            </p>
          </div>
        )}

        {step === 3 && (
          <div className="wizard-page">
            <h2>{t.services.title}</h2>
            <p>{language === 'zh' ? '选择服务运行模式' : 'Select service running mode'}</p>
            
            <div className="service-options">
              <button
                className={`service-option ${serviceMode === 'docker' ? 'selected' : ''}`}
                onClick={() => setServiceMode('docker')}
              >
                <div className="service-icon">🐳</div>
                <div className="service-name">Docker</div>
                <div className="service-desc">
                  {language === 'zh' 
                    ? '使用 Docker Compose 运行服务（推荐）'
                    : 'Run services with Docker Compose (Recommended)'}
                </div>
              </button>
              
              <button
                className={`service-option ${serviceMode === 'local' ? 'selected' : ''}`}
                onClick={() => setServiceMode('local')}
              >
                <div className="service-icon">💻</div>
                <div className="service-name">Local</div>
                <div className="service-desc">
                  {language === 'zh' 
                    ? '直接在本地运行服务（开发模式）'
                    : 'Run services locally (Development mode)'}
                </div>
              </button>
            </div>
          </div>
        )}

        {step === 4 && (
          <div className="wizard-page success-page">
            <div className="success-icon">✅</div>
            <h2>{t.common.success}!</h2>
            <p>
              {language === 'zh' 
                ? 'AgentOS 桌面客户端已准备就绪'
                : 'AgentOS Desktop Client is ready to use'}
            </p>
            
            <div className="summary">
              <div className="summary-item">
                <span className="summary-label">{t.settings.language}:</span>
                <span className="summary-value">
                  {availableLanguages.find(l => l.code === language)?.name}
                </span>
              </div>
              <div className="summary-item">
                <span className="summary-label">{t.settings.projectPath}:</span>
                <span className="summary-value">{projectPath || 'Not set'}</span>
              </div>
              <div className="summary-item">
                <span className="summary-label">{t.services.mode}:</span>
                <span className="summary-value">
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
            <button className="btn btn-primary" onClick={handleNext}>
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

export default function WelcomeWizard({ onComplete }: WelcomeWizardProps) {
  return (
    <I18nProvider>
      <WelcomeWizardContent onComplete={onComplete} />
    </I18nProvider>
  );
}
