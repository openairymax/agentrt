import React, { useState, useEffect } from 'react';
import { ArrowRight, CheckCircle2, X, LayoutDashboard, Users, ClipboardList, Server, Activity, MessageCircle, Terminal, Sparkles } from 'lucide-react';
import { Button } from './ui/Button';

interface Step {
  id: number;
  title: string;
  description: string;
  icon: React.ReactNode;
  highlightSelector?: string;
  position?: 'top' | 'bottom' | 'left' | 'right';
}

const steps: Step[] = [
  {
    id: 1,
    title: '欢迎使用 AgentOS',
    description: 'AgentOS 是一个工业级 AI 智能体系统，提供全方位的智能体管理、任务编排和系统监控功能。',
    icon: <LayoutDashboard size={24} />,
  },
  {
    id: 2,
    title: '导航栏',
    description: '使用左侧导航栏快速访问系统的各个模块，包括智能体管理、任务编排、服务监控等。',
    icon: <ArrowRight size={24} />,
    highlightSelector: '.sidebar-panel',
    position: 'right',
  },
  {
    id: 3,
    title: '智能体管理',
    description: '创建和管理 AI 智能体，配置它们的能力和行为，让智能体为您完成各种任务。',
    icon: <Users size={24} />,
    highlightSelector: '[href="/agents"]',
    position: 'right',
  },
  {
    id: 4,
    title: '任务编排',
    description: '创建和管理任务，设置任务的参数和执行条件，跟踪任务的执行状态。',
    icon: <ClipboardList size={24} />,
    highlightSelector: '[href="/tasks"]',
    position: 'right',
  },
  {
    id: 5,
    title: '系统监控',
    description: '实时监控系统的资源使用情况，查看系统日志，确保系统的稳定运行。',
    icon: <Activity size={24} />,
    highlightSelector: '[href="/system-monitor"]',
    position: 'right',
  },
  {
    id: 6,
    title: 'AI 助手',
    description: '使用内置的 AI 助手获取系统使用帮助，或者让它为您执行各种任务。',
    icon: <MessageCircle size={24} />,
    highlightSelector: '[href="/ai-chat"]',
    position: 'right',
  },
  {
    id: 7,
    title: '终端',
    description: '直接执行系统命令，查看系统状态，进行高级配置和调试。',
    icon: <Terminal size={24} />,
    highlightSelector: '[href="/terminal"]',
    position: 'right',
  },
  {
    id: 8,
    title: '开始使用',
    description: '现在您已经了解了 AgentOS 的核心功能，开始创建您的第一个智能体吧！',
    icon: <Sparkles size={24} />,
  },
];

interface StepByStepGuideProps {
  onComplete: () => void;
}

export const StepByStepGuide: React.FC<StepByStepGuideProps> = ({ onComplete }) => {
  const [currentStep, setCurrentStep] = useState(0);
  const [isVisible, setIsVisible] = useState(true);

  useEffect(() => {
    // 检查用户是否已经完成过引导
    const hasCompletedGuide = localStorage.getItem('agentos-guide-completed');
    if (hasCompletedGuide) {
      setIsVisible(false);
      onComplete();
    }
  }, [onComplete]);

  const handleNext = () => {
    if (currentStep < steps.length - 1) {
      setCurrentStep(currentStep + 1);
    } else {
      handleComplete();
    }
  };

  const handlePrevious = () => {
    if (currentStep > 0) {
      setCurrentStep(currentStep - 1);
    }
  };

  const handleComplete = () => {
    localStorage.setItem('agentos-guide-completed', 'true');
    setIsVisible(false);
    onComplete();
  };

  const handleSkip = () => {
    localStorage.setItem('agentos-guide-completed', 'true');
    setIsVisible(false);
    onComplete();
  };

  if (!isVisible) return null;

  const currentStepData = steps[currentStep];

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      background: 'rgba(0, 0, 0, 0.5)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 10000,
      backdropFilter: 'blur(8px) saturate(120%)',
      animation: 'fadeIn 0.3s ease-out',
    }}>
      <div style={{
        background: 'var(--bg-secondary)',
        borderRadius: '12px',
        boxShadow: 'var(--shadow-xl)',
        maxWidth: '500px',
        width: '90%',
        maxHeight: '80vh',
        overflow: 'auto',
        padding: '32px',
        border: '1px solid var(--border-subtle)',
        animation: 'scaleInCenter 0.3s ease-out',
      }}>
        {/* Header */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          marginBottom: '24px',
        }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
          }}>
            <div style={{
              width: '48px',
              height: '48px',
              borderRadius: '12px',
              background: 'var(--primary-gradient)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              boxShadow: '0 4px 16px rgba(99, 102, 241, 0.3)',
            }}>
              {currentStepData.icon}
            </div>
            <div>
              <h2 style={{
                fontSize: '18px',
                fontWeight: 600,
                color: 'var(--text-primary)',
                margin: '0 0 4px 0',
              }}>
                {currentStepData.title}
              </h2>
              <div style={{
                fontSize: '14px',
                color: 'var(--text-secondary)',
              }}>
                步骤 {currentStep + 1} / {steps.length}
              </div>
            </div>
          </div>
          <button
            onClick={handleSkip}
            style={{
              background: 'none',
              border: 'none',
              color: 'var(--text-muted)',
              cursor: 'pointer',
              fontSize: '14px',
              padding: '8px',
              borderRadius: '6px',
              transition: 'all 0.2s ease',
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.background = 'var(--bg-tertiary)';
              e.currentTarget.style.color = 'var(--text-secondary)';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.background = 'none';
              e.currentTarget.style.color = 'var(--text-muted)';
            }}
          >
            跳过
          </button>
        </div>

        {/* Content */}
        <div style={{
          marginBottom: '32px',
        }}>
          <p style={{
            fontSize: '14px',
            lineHeight: 1.6,
            color: 'var(--text-primary)',
            margin: 0,
          }}>
            {currentStepData.description}
          </p>
        </div>

        {/* Progress Bar */}
        <div style={{
          height: '4px',
          background: 'var(--bg-tertiary)',
          borderRadius: '2px',
          marginBottom: '24px',
          overflow: 'hidden',
        }}>
          <div style={{
            height: '100%',
            width: `${((currentStep + 1) / steps.length) * 100}%`,
            background: 'var(--primary-gradient)',
            transition: 'width 0.3s ease',
          }} />
        </div>

        {/* Navigation */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}>
          <Button
            variant="secondary"
            onClick={handlePrevious}
            disabled={currentStep === 0}
            style={{
              opacity: currentStep === 0 ? 0.5 : 1,
              cursor: currentStep === 0 ? 'not-allowed' : 'pointer',
            }}
          >
            上一步
          </Button>
          <Button
            variant={currentStep === steps.length - 1 ? 'success' : 'primary'}
            onClick={handleNext}
          >
            {currentStep === steps.length - 1 ? (
              <>
                <CheckCircle2 size={16} />
                完成
              </>
            ) : (
              <>
                下一步
                <ArrowRight size={16} />
              </>
            )}
          </Button>
        </div>
      </div>
    </div>
  );
};
