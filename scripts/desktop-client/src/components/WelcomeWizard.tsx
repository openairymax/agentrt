import React, { useState } from 'react';
import { invoke } from '../utils/tauriCompat';
import { useI18n } from '../i18n';
import {
  ChevronRight,
  Globe,
  FolderOpen,
  Server,
  CheckCircle2,
  Sparkles,
  Brain,
  ArrowRight,
} from 'lucide-react';

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
    { id: 1, title: '欢迎', icon: Sparkles },
    { id: 2, title: '语言', icon: Globe },
    { id: 3, title: '服务', icon: Server },
    { id: 4, title: '完成', icon: CheckCircle2 },
  ];

  const handleComplete = async () => {
    setIsConfiguring(true);
    try {
      await invoke('save_settings', {
        settings: { language, projectPath, serviceMode },
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
    <div style={{
      minHeight: '100vh',
      display: 'flex',
      background: 'var(--bg-primary)',
      fontFamily: "'Inter', -apple-system, BlinkMacSystemFont, sans-serif",
    }}>
      {/* Left Panel - Branding */}
      <div style={{
        width: '400px',
        background: 'linear-gradient(135deg, #6366f1 0%, #818cf8 50%, #a78bfa 100%)',
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'center',
        alignItems: 'center',
        padding: '60px 40px',
        position: 'relative',
        overflow: 'hidden',
        flexShrink: 0,
      }}>
        {/* Decorative Circles */}
        <div style={{
          position: 'absolute', top: '-80px', right: '-80px',
          width: '300px', height: '300px', borderRadius: '50%',
          background: 'rgba(255,255,255,0.08)',
        }} />
        <div style={{
          position: 'absolute', bottom: '-60px', left: '-40px',
          width: '200px', height: '200px', borderRadius: '50%',
          background: 'rgba(255,255,255,0.05)',
        }} />
        <div style={{
          position: 'absolute', top: '40%', right: '10%',
          width: '80px', height: '80px', borderRadius: '50%',
          background: 'rgba(255,255,255,0.06)',
        }} />

        <div style={{ position: 'relative', zIndex: 1, textAlign: 'center' }}>
          <div style={{
            width: '80px', height: '80px', borderRadius: '20px',
            background: 'rgba(255,255,255,0.2)',
            backdropFilter: 'blur(10px)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            margin: '0 auto 24px',
            fontSize: '36px', fontWeight: 800, color: 'white',
          }}>A</div>

          <h1 style={{ fontSize: '32px', fontWeight: 800, color: 'white', margin: '0 0 12px' }}>
            AgentOS
          </h1>
          <p style={{ fontSize: '16px', color: 'rgba(255,255,255,0.8)', lineHeight: 1.6, margin: 0 }}>
            工业级 AI 智能体操作系统
          </p>
          <p style={{ fontSize: '13px', color: 'rgba(255,255,255,0.6)', marginTop: '16px' }}>
            Industrial-grade AI Agent Operating System
          </p>

          {/* Step Indicators */}
          <div style={{
            display: 'flex', gap: '8px', justifyContent: 'center',
            marginTop: '48px',
          }}>
            {steps.map((s) => (
              <div key={s.id} style={{
                display: 'flex', alignItems: 'center', gap: '8px',
              }}>
                <div style={{
                  width: step === s.id ? '32px' : step > s.id ? '32px' : '8px',
                  height: '8px',
                  borderRadius: '4px',
                  background: step === s.id
                    ? 'rgba(255,255,255,0.9)'
                    : step > s.id
                    ? 'rgba(255,255,255,0.6)'
                    : 'rgba(255,255,255,0.2)',
                  transition: 'all 0.3s ease',
                }} />
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Right Panel - Steps */}
      <div style={{
        flex: 1,
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'center',
        padding: '60px 80px',
        maxWidth: '640px',
      }}>
        {/* Step 1: Welcome */}
        {step === 1 && (
          <div style={{ animation: 'fadeIn 0.4s ease-out' }}>
            <div style={{
              width: '56px', height: '56px', borderRadius: '16px',
              background: 'var(--primary-light)',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              marginBottom: '24px',
            }}>
              <Sparkles size={28} color="var(--primary-color)" />
            </div>
            <h2 style={{ fontSize: '28px', fontWeight: 700, margin: '0 0 12px', color: 'var(--text-primary)' }}>
              欢迎使用 AgentOS
            </h2>
            <p style={{ fontSize: '16px', color: 'var(--text-secondary)', lineHeight: 1.7, margin: '0 0 32px' }}>
              让我们快速完成初始设置，仅需几步即可开始使用。
              AgentOS 将帮助你轻松管理 AI 智能体、配置大模型、监控服务状态。
            </p>

            <div style={{
              display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)',
              gap: '12px', marginBottom: '32px',
            }}>
              {[
                { icon: Brain, title: 'AI 智能体管理', desc: '创建和管理 AI 助手' },
                { icon: Server, title: '服务监控', desc: '实时监控系统状态' },
                { icon: Globe, title: '多模型支持', desc: 'OpenAI/Claude/本地模型' },
                { icon: Sparkles, title: '智能建议', desc: 'AI 驱动的操作指引' },
              ].map((feature, idx) => (
                <div key={idx} style={{
                  padding: '16px',
                  borderRadius: 'var(--radius-lg)',
                  background: 'var(--bg-tertiary)',
                  border: '1px solid var(--border-subtle)',
                }}>
                  <feature.icon size={20} color="var(--primary-color)" style={{ marginBottom: '8px' }} />
                  <div style={{ fontWeight: 600, fontSize: '13px', marginBottom: '4px' }}>{feature.title}</div>
                  <div style={{ fontSize: '12px', color: 'var(--text-muted)' }}>{feature.desc}</div>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Step 2: Language */}
        {step === 2 && (
          <div style={{ animation: 'fadeIn 0.4s ease-out' }}>
            <div style={{
              width: '56px', height: '56px', borderRadius: '16px',
              background: 'var(--primary-light)',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              marginBottom: '24px',
            }}>
              <Globe size={28} color="var(--primary-color)" />
            </div>
            <h2 style={{ fontSize: '28px', fontWeight: 700, margin: '0 0 12px' }}>
              选择语言
            </h2>
            <p style={{ fontSize: '16px', color: 'var(--text-secondary)', marginBottom: '32px' }}>
              选择你偏好的界面语言，随时可以在设置中更改
            </p>

            <div style={{ display: 'flex', gap: '16px' }}>
              {availableLanguages.map((lang) => (
                <button
                  key={lang.code}
                  onClick={() => setLanguage(lang.code as 'en' | 'zh')}
                  style={{
                    flex: 1, padding: '24px',
                    borderRadius: 'var(--radius-xl)',
                    border: `2px solid ${language === lang.code ? 'var(--primary-color)' : 'var(--border-color)'}`,
                    background: language === lang.code ? 'var(--primary-light)' : 'var(--bg-tertiary)',
                    cursor: 'pointer',
                    transition: 'all var(--transition-fast)',
                    textAlign: 'center',
                  }}
                >
                  <div style={{ fontSize: '32px', marginBottom: '12px' }}>
                    {lang.code === 'zh' ? '🇨🇳' : '🇺🇸'}
                  </div>
                  <div style={{
                    fontWeight: 700, fontSize: '16px',
                    color: language === lang.code ? 'var(--primary-color)' : 'var(--text-primary)',
                    marginBottom: '4px',
                  }}>
                    {lang.name}
                  </div>
                  <div style={{ fontSize: '12px', color: 'var(--text-muted)' }}>
                    {lang.code.toUpperCase()}
                  </div>
                </button>
              ))}
            </div>
          </div>
        )}

        {/* Step 3: Service Mode */}
        {step === 3 && (
          <div style={{ animation: 'fadeIn 0.4s ease-out' }}>
            <div style={{
              width: '56px', height: '56px', borderRadius: '16px',
              background: 'var(--primary-light)',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              marginBottom: '24px',
            }}>
              <Server size={28} color="var(--primary-color)" />
            </div>
            <h2 style={{ fontSize: '28px', fontWeight: 700, margin: '0 0 12px' }}>
              服务运行模式
            </h2>
            <p style={{ fontSize: '16px', color: 'var(--text-secondary)', marginBottom: '32px' }}>
              选择服务的运行方式
            </p>

            <div style={{ display: 'flex', gap: '16px' }}>
              {[
                { mode: 'docker' as const, icon: '🐳', title: 'Docker Compose', desc: '使用容器化部署，一键启动所有服务（推荐）', tag: '推荐' },
                { mode: 'local' as const, icon: '💻', title: '本地开发模式', desc: '直接在本地运行各服务组件，适合开发者调试', tag: '开发' },
              ].map((opt) => (
                <button
                  key={opt.mode}
                  onClick={() => setServiceMode(opt.mode)}
                  style={{
                    flex: 1, padding: '24px',
                    borderRadius: 'var(--radius-xl)',
                    border: `2px solid ${serviceMode === opt.mode ? 'var(--primary-color)' : 'var(--border-color)'}`,
                    background: serviceMode === opt.mode ? 'var(--primary-light)' : 'var(--bg-tertiary)',
                    cursor: 'pointer',
                    transition: 'all var(--transition-fast)',
                    textAlign: 'left',
                    position: 'relative',
                  }}
                >
                  {opt.tag && (
                    <span style={{
                      position: 'absolute', top: '12px', right: '12px',
                      padding: '2px 8px', borderRadius: '4px',
                      background: opt.mode === 'docker' ? 'var(--primary-color)' : 'var(--bg-tertiary)',
                      color: opt.mode === 'docker' ? 'white' : 'var(--text-muted)',
                      fontSize: '11px', fontWeight: 600,
                    }}>{opt.tag}</span>
                  )}
                  <div style={{ fontSize: '36px', marginBottom: '16px' }}>{opt.icon}</div>
                  <div style={{
                    fontWeight: 700, fontSize: '16px',
                    color: serviceMode === opt.mode ? 'var(--primary-color)' : 'var(--text-primary)',
                    marginBottom: '8px',
                  }}>{opt.title}</div>
                  <div style={{ fontSize: '13px', color: 'var(--text-secondary)', lineHeight: 1.5 }}>{opt.desc}</div>
                </button>
              ))}
            </div>
          </div>
        )}

        {/* Step 4: Complete */}
        {step === 4 && (
          <div style={{ animation: 'fadeIn 0.4s ease-out', textAlign: 'center' }}>
            <div style={{
              width: '72px', height: '72px', borderRadius: '50%',
              background: 'var(--success-light)',
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              margin: '0 auto 24px',
            }}>
              <CheckCircle2 size={36} color="#22c55e" />
            </div>
            <h2 style={{ fontSize: '28px', fontWeight: 700, margin: '0 0 12px' }}>
              设置完成！
            </h2>
            <p style={{ fontSize: '16px', color: 'var(--text-secondary)', marginBottom: '32px' }}>
              AgentOS 已准备就绪，开始探索吧
            </p>

            <div style={{
              background: 'var(--bg-tertiary)',
              borderRadius: 'var(--radius-lg)',
              padding: '20px',
              textAlign: 'left',
            }}>
              {[
                { label: '语言', value: availableLanguages.find(l => l.code === language)?.name || language },
                { label: '服务模式', value: serviceMode === 'docker' ? 'Docker Compose' : '本地开发' },
                { label: '项目路径', value: projectPath || '默认路径' },
              ].map((item, idx) => (
                <div key={idx} style={{
                  display: 'flex', justifyContent: 'space-between',
                  padding: '10px 0',
                  borderBottom: idx < 2 ? '1px solid var(--border-subtle)' : 'none',
                  fontSize: '14px',
                }}>
                  <span style={{ color: 'var(--text-muted)' }}>{item.label}</span>
                  <span style={{ fontWeight: 500 }}>{item.value}</span>
                </div>
              ))}
            </div>
          </div>
        )}

        {/* Navigation Buttons */}
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginTop: '40px',
          paddingTop: '24px',
          borderTop: '1px solid var(--border-subtle)',
        }}>
          {step > 1 ? (
            <button className="btn btn-secondary" onClick={() => setStep(step - 1)}>
              上一步
            </button>
          ) : (
            <div />
          )}

          {step < 4 ? (
            <button className="btn btn-primary btn-lg" onClick={() => setStep(step + 1)}>
              继续
              <ArrowRight size={18} />
            </button>
          ) : (
            <button
              className="btn btn-primary btn-lg"
              onClick={handleComplete}
              disabled={isConfiguring}
            >
              {isConfiguring ? '正在配置...' : '开始使用'}
              {!isConfiguring && <Sparkles size={18} />}
            </button>
          )}
        </div>
      </div>
    </div>
  );
}

export default WelcomeWizard;
