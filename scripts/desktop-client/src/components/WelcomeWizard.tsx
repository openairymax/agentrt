import React, { useState } from 'react';
import sdk from '../services/agentos-sdk';
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
  Shield,
  Layers,
  Zap,
  Cpu,
  Activity,
  Lightbulb,
  Target,
  Database,
  Workflow,
} from 'lucide-react';

interface WelcomeWizardProps {
  onComplete: () => void;
}

const WelcomeWizard: React.FC<WelcomeWizardProps> = ({ onComplete }) => {
  const { language, setLanguage, t, availableLanguages } = useI18n();
  const [step, setStep] = useState(1);
  const [projectPath, setProjectPath] = useState('');
  const [serviceMode, setServiceMode] = useState<'docker' | 'local'>('docker');
  const [isConfiguring, setIsConfiguring] = useState(false);

  const steps = [
    { id: 1, title: '欢迎', icon: Sparkles },
    { id: 2, title: '认识 AgentOS', icon: Brain },
    { id: 3, title: '语言', icon: Globe },
    { id: 4, title: '服务', icon: Server },
    { id: 5, title: '完成', icon: CheckCircle2 },
  ];

  const handleComplete = async () => {
    setIsConfiguring(true);
    try {
      await sdk.saveSettings({ language, projectPath, serviceMode });
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
      <div style={{
        width: '420px',
        background: 'linear-gradient(135deg, #0f172a 0%, #1e293b 40%, #312e81 100%)',
        display: 'flex', flexDirection: 'column',
        justifyContent: 'center', alignItems: 'center',
        padding: '48px 36px', position: 'relative', overflow: 'hidden',
        flexShrink: 0,
      }}>
        <div style={{ position: 'absolute', top: '-60px', right: '-60px', width: '240px', height: '240px', borderRadius: '50%', background: 'rgba(99,102,241,0.12)' }} />
        <div style={{ position: 'absolute', bottom: '-80px', left: '-60px', width: '280px', height: '280px', borderRadius: '50%', background: 'rgba(139,92,246,0.08)' }} />
        <div style={{ position: 'absolute', top: '50%', left: '20%', width: '120px', height: '120px', borderRadius: '50%', background: 'rgba(99,102,241,0.06)' }} />

        <div style={{ position: 'relative', zIndex: 1, textAlign: 'center' }}>
          <div style={{
            width: '72px', height: '72px', borderRadius: '18px',
            background: 'linear-gradient(135deg, #6366f1, #8b5cf6)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            margin: '0 auto 20px', boxShadow: '0 8px 32px rgba(99,102,241,0.4)',
            fontSize: '32px', fontWeight: 800, color: 'white',
          }}>A</div>

          <h1 style={{ fontSize: '30px', fontWeight: 800, color: '#f8fafc', margin: '0 0 10px', letterSpacing: "-0.03em" }}>
            AgentOS
          </h1>
          <p style={{ fontSize: '14px', color: 'rgba(255,255,255,0.7)', lineHeight: 1.6, margin: '0 0 4px' }}>
            工业级 AI 智能体操作系统
          </p>
          <p style={{ fontSize: '12px', color: 'rgba(255,255,255,0.45)', marginTop: '8px', letterSpacing: "0.02em" }}>
            MCIS · 微内核 · 三层认知循环 · 四层记忆卷载
          </p>

          <div style={{ display: 'flex', gap: '6px', justifyContent: 'center', marginTop: '36px' }}>
            {steps.map((s) => (
              <div key={s.id} style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                <div style={{
                  width: step === s.id ? '28px' : step > s.id ? '28px' : '6px', height: '6px', borderRadius: '3px',
                  background: step === s.id ? 'rgba(255,255,255,0.9)' : step > s.id ? 'rgba(255,255,255,0.5)' : 'rgba(255,255,255,0.15)',
                  transition: 'all 0.35s cubic-bezier(0.34, 1.56, 0.64, 1)',
                }} />
              </div>
            ))}
          </div>

          <div style={{ marginTop: '36px', paddingTop: '24px', borderTop: '1px solid rgba(255,255,255,0.08)' }}>
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
              {[
                { val: '<10K', label: '微内核代码量' },
                { val: 'L1-L4', label: '四层记忆系统' },
                { val: '+500%', label: 'Token效率提升' },
                { val: '6守护', label: '后台服务' },
              ].map((stat, i) => (
                <div key={i} style={{ padding: '10px', background: 'rgba(255,255,255,0.04)', borderRadius: 'var(--radius-sm)', border: '1px solid rgba(255,255,255,0.06)', textAlign: 'center' }}>
                  <div style={{ fontSize: '15px', fontWeight: 700, color: '#fff' }}>{stat.val}</div>
                  <div style={{ fontSize: '9.5px', color: 'rgba(255,255,255,0.45)', marginTop: '2px' }}>{stat.label}</div>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>

      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', justifyContent: 'center', padding: '48px 64px', maxWidth: '580px', overflowY: 'auto' }}>
        {step === 1 && (
          <div style={{ animation: 'fadeIn 0.4s ease-out' }}>
            <div style={{ width: '52px', height: '52px', borderRadius: '14px', background: 'var(--primary-light)', display: 'flex', alignItems: 'center', justifyContent: 'center', marginBottom: '20px' }}>
              <Sparkles size={26} color="var(--primary-color)" />
            </div>
            <h2 style={{ fontSize: '26px', fontWeight: 700, margin: '0 0 10px', letterSpacing: "-0.02em" }}>欢迎使用 AgentOS</h2>
            <p style={{ fontSize: '15px', color: 'var(--text-secondary)', lineHeight: 1.7, margin: '0 0 28px' }}>
              AgentOS 是一个面向 AI 智能体的工业级操作系统。让我们快速完成初始设置，几分钟内即可开始使用。
            </p>

            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px', marginBottom: '28px' }}>
              {[
                { icon: Brain, title: '三层认知循环', desc: '感知 → 规划 → 行动', color: "#8b5cf6" },
                { icon: Layers, title: '四层记忆卷载', desc: '原始 → 特征 → 结构 → 模式', color: "#06b6d4" },
                { icon: Shield, title: '安全穹顶 Cupolas', desc: '四层纵深防御体系', color: "#ef4444" },
                { icon: Zap, title: '双系统思考', desc: '快思考 + 慢推理协同', color: "#f59e0b" },
              ].map((feature, idx) => (
                <div key={idx} style={{
                  padding: '16px', borderRadius: 'var(--radius-md)', background: feature.color + "08",
                  border: "1px solid " + feature.color + "15", transition: "all var(--transition-fast)",
                  cursor: "default",
                }}>
                  <feature.icon size={18} color={feature.color} style={{ marginBottom: "8px" }} />
                  <div style={{ fontWeight: 600, fontSize: "13px", marginBottom: "3px" }}>{feature.title}</div>
                  <div style={{ fontSize: "11.5px", color: "var(--text-muted)" }}>{feature.desc}</div>
                </div>
              ))}
            </div>
          </div>
        )}

        {step === 2 && (
          <div style={{ animation: 'fadeIn 0.4s ease-out' }}>
            <div style={{ width: '52px', height: '52px', borderRadius: '14px', background: 'var(--primary-light)', display: 'flex', alignItems: 'center', justifyContent: 'center', marginBottom: '20px' }}>
              <Brain size={26} color="var(--primary-color)" />
            </div>
            <h2 style={{ fontSize: '26px', fontWeight: 700, margin: '0 0 10px', letterSpacing: "-0.02em" }}>认识 AgentOS 架构</h2>
            <p style={{ fontSize: '14px', color: 'var(--text-secondary)', lineHeight: 1.7, margin: '0 0 24px' }}>
              了解 AgentOS 的核心设计理念与技术优势
            </p>

            <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
              <ArchFeatureCard
                title="微内核 CoreKern"
                desc="极简内核设计，仅约 10K 行 C 代码。IPC Binder 延迟 <10μs，纳秒级时间服务，智能指针内存管理。"
                icon={<Cpu size={20} />}
                color="#6366f1"
                tags={["L4权限", "<10μs延迟", "Rust安全"]}
              />
              <ArchFeatureCard
                title="三层认知循环 CoreLoopThree"
                desc="感知层（意图理解）→ 规划层（DAG任务分解）→ 行动层（执行与反馈）。支持 System 1 快思考与 System 2 慢推理双轨并行。"
                icon={<Activity size={20} />}
                color="#8b5cf6"
                tags={["S1+S2", "~450ms周期", "+500% Token"]}
              />
              <ArchFeatureCard
                title="四层记忆卷载 MemoryRovol"
                desc="L1 原始卷 → L2 特征层(FAISS向量) → L3 结构层(知识图谱) → L4 模式层(稳定规则)。支持艾宾浩斯遗忘曲线自动裁剪。"
                icon={<Database size={20} />}
                color="#06b6d4"
                tags={["FAISS", "混合检索", "自动进化"]}
              />
              <ArchFeatureCard
                title="安全穹顶 Cupolas"
                desc="虚拟工作空间隔离、RBAC 权限裁决、输入净化过滤、全链路审计追踪。四层纵深防御体系确保内生安全。"
                icon={<Shield size={20} />}
                color="#ef4444"
                tags={["零信任", "审计", "隔离"]}
              />
            </div>
          </div>
        )}

        {step === 3 && (
          <div style={{ animation: 'fadeIn 0.4s ease-out' }}>
            <div style={{ width: '52px', height: '52px', borderRadius: '14px', background: 'var(--primary-light)', display: 'flex', alignItems: 'center', justifyContent: 'center', marginBottom: '20px' }}>
              <Globe size={26} color="var(--primary-color)" />
            </div>
            <h2 style={{ fontSize: '26px', fontWeight: 700, margin: '0 0 10px', letterSpacing: "-0.02em" }}>选择界面语言</h2>
            <p style={{ fontSize: '15px', color: 'var(--text-secondary)', marginBottom: '28px' }}>
              选择你偏好的界面语言，随时可以在设置中更改
            </p>

            <div style={{ display: 'flex', gap: '14px' }}>
              {availableLanguages.map((lang) => (
                <button key={lang.code}
                  onClick={() => setLanguage(lang.code as 'en' | 'zh')}
                  style={{
                    flex: 1, padding: '22px', borderRadius: 'var(--radius-lg)',
                    border: `2px solid ${language === lang.code ? 'var(--primary-color)' : 'var(--border-color)'}`,
                    background: language === lang.code ? 'var(--primary-light)' : 'var(--bg-tertiary)',
                    cursor: 'pointer', transition: 'all var(--transition-fast)', textAlign: 'center',
                  }}
                >
                  <div style={{ fontSize: "28px", marginBottom: "10px" }}>{lang.code === 'zh' ? "🇨🇳" : "🇺🇸"}</div>
                  <div style={{ fontWeight: 700, fontSize: "15px", color: language === lang.code ? 'var(--primary-color)' : "var(--text-primary)", marginBottom: "3px" }}>{lang.name}</div>
                  <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>{lang.code.toUpperCase()}</div>
                </button>
              ))}
            </div>
          </div>
        )}

        {step === 4 && (
          <div style={{ animation: 'fadeIn 0.4s ease-out' }}>
            <div style={{ width: '52px', height: '52px', borderRadius: '14px', background: 'var(--primary-light)', display: 'flex', alignItems: 'center', justifyContent: 'center', marginBottom: '20px' }}>
              <Server size={26} color="var(--primary-color)" />
            </div>
            <h2 style={{ fontSize: '26px', fontWeight: 700, margin: '0 0 10px', letterSpacing: "-0.02em" }}>服务运行模式</h2>
            <p style={{ fontSize: '15px', color: 'var(--text-secondary)', marginBottom: '28px' }}>
              选择服务的运行方式
            </p>

            <div style={{ display: 'flex', gap: '14px' }}>
              {[
                { mode: 'docker' as const, icon: "🐳", title: "Docker Compose", desc: "使用容器化部署，一键启动所有服务（推荐新手使用）", tag: "推荐", tagColor: "#22c55e" },
                { mode: 'local' as const, icon: "💻", title: "本地开发模式", desc: "直接在本地运行各服务组件，适合开发者调试", tag: "开发", tagColor: "#f59e0b" },
              ].map((opt) => (
                <button key={opt.mode}
                  onClick={() => setServiceMode(opt.mode)}
                  style={{
                    flex: 1, padding: '22px', borderRadius: 'var(--radius-lg)',
                    border: `2px solid ${serviceMode === opt.mode ? 'var(--primary-color)' : 'var(--border-color)'}`,
                    background: serviceMode === opt.mode ? 'var(--primary-light)' : 'var(--bg-tertiary)',
                    cursor: 'pointer', transition: 'all var(--transition-fast)', textAlign: "left", position: "relative",
                  }}
                >
                  <span style={{
                    position: "absolute", top: "12px", right: "12px",
                    padding: "2px 8px", borderRadius: "4px", background: serviceMode === opt.mode ? opt.tagColor : "var(--bg-tertiary)",
                    color: serviceMode === opt.mode ? "white" : "var(--text-muted)", fontSize: "10.5px", fontWeight: 600,
                  }}>{opt.tag}</span>
                  <div style={{ fontSize: "32px", marginBottom: "12px" }}>{opt.icon}</div>
                  <div style={{ fontWeight: 700, fontSize: "15px", color: serviceMode === opt.mode ? 'var(--primary-color)' : "var(--text-primary)", marginBottom: "6px" }}>{opt.title}</div>
                  <div style={{ fontSize: "12.5px", color: "var(--text-secondary)", lineHeight: 1.5 }}>{opt.desc}</div>
                </button>
              ))}
            </div>

            <div style={{ marginTop: "20px", padding: "16px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)", border: "1px solid var(--border-subtle)" }}>
              <div style={{ fontSize: "11px", fontWeight: 600, color: "var(--text-muted)", marginBottom: "8px" }}>守护服务列表</div>
              <div style={{ display: "flex", flexWrap: "wrap", gap: "8px" }}>
                {["gateway_d 网关", "llm_d 大模型", "tool_d 工具", "sched_d 调度", "monit_d 监控", "market_d 市场"].map((svc) => (
                  <span key={svc} style={{ fontSize: "11.5px", padding: "4px 10px", background: "var(--bg-secondary)", borderRadius: "var(--radius-sm)", border: "1px solid var(--border-subtle)", fontFamily: "monospace" }}>{svc}</span>
                ))}
              </div>
            </div>
          </div>
        )}

        {step === 5 && (
          <div style={{ animation: 'fadeIn 0.4s ease-out', textAlign: "center" }}>
            <div style={{
              width: "68px", height: "68px", borderRadius: "50%",
              background: "linear-gradient(135deg, #10b981, #34d399)",
              display: "flex", alignItems: "center", justifyContent: "center",
              margin: "0 auto 20px", boxShadow: "0 8px 24px rgba(16,185,129,0.25)",
            }}>
              <CheckCircle2 size={34} color="white" />
            </div>
            <h2 style={{ fontSize: "26px", fontWeight: 700, margin: "0 0 10px", letterSpacing: "-0.02em" }}>准备就绪！</h2>
            <p style={{ fontSize: "15px", color: "var(--text-secondary)", marginBottom: "28px" }}>
              AgentOS 已完成初始化配置，开始探索工业级 AI 操作系统
            </p>

            <div style={{ background: "var(--bg-tertiary)", borderRadius: "var(--radius-lg)", padding: "20px", textAlign: "left" }}>
              {[
                { label: "界面语言", value: availableLanguages.find(l => l.code === language)?.name || language },
                { label: "服务模式", value: serviceMode === "docker" ? "Docker Compose" : "本地开发" },
                { label: "项目路径", value: projectPath || "默认路径" },
                { label: "核心模块", value: "CoreKern / CoreLoopThree / MemoryRovol / Cupolas" },
              ].map((item, idx) => (
                <div key={idx} style={{
                  display: "flex", justifyContent: "space-between", alignItems: "center",
                  padding: "12px 0", borderBottom: idx < 3 ? "1px solid var(--border-subtle)" : "none", fontSize: "13.5px",
                }}>
                  <span style={{ color: "var(--text-muted)" }}>{item.label}</span>
                  <span style={{ fontWeight: 500, color: "var(--text-primary)" }}>{item.value}</span>
                </div>
              ))}
            </div>

            <div style={{ marginTop: "20px", padding: "14px", background: "rgba(99,102,241,0.06)", borderRadius: "var(--radius-md)", border: "1px solid rgba(99,102,241,0.1)" }}>
              <div style={{ fontSize: "12px", color: "var(--text-primary)", lineHeight: 1.6, display: "flex", alignItems: "flex-start", gap: "8px" }}>
                <Lightbulb size={14} style={{ color: "#f59e0b", flexShrink: 0, marginTop: "2px" }} />
                <span><strong>提示：</strong>首次使用建议从「控制中心」开始，查看三层认知循环和四层记忆系统的实时状态。</span>
              </div>
            </div>
          </div>
        )}

        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginTop: "36px", paddingTop: "20px", borderTop: "1px solid var(--border-subtle)" }}>
          {step > 1 ? (
            <button className="btn btn-secondary" onClick={() => setStep(step - 1)}>
              上一步
            </button>
          ) : (<div />)}

          {step < 5 ? (
            <button className="btn btn-primary btn-lg" onClick={() => setStep(step + 1)}>
              继续 <ArrowRight size={17} />
            </button>
          ) : (
            <button className="btn btn-primary btn-lg" onClick={handleComplete} disabled={isConfiguring}>
              {isConfiguring ? "正在配置..." : "开始使用"} {!isConfiguring && <Sparkles size={17} />}
            </button>
          )}
        </div>
      </div>
    </div>
  );
};

function ArchFeatureCard({ title, desc, icon, color, tags }: {
  title: string; desc: string; icon: React.ReactNode; color: string; tags: string[];
}) {
  return (
    <div style={{
      padding: "16px", borderRadius: "var(--radius-md)",
      background: "var(--bg-secondary)", border: "1px solid var(--border-subtle)",
      transition: "all var(--transition-fast)",
    }}>
      <div style={{ display: "flex", alignItems: "flex-start", gap: "12px", marginBottom: "10px" }}>
        <div style={{ width: "38px", height: "38px", borderRadius: "var(--radius-sm)", background: color + "12", display: "flex", alignItems: "center", justifyContent: "center", flexShrink: 0, color }}>
          {icon}
        </div>
        <div style={{ flex: 1, minWidth: 0 }}>
          <div style={{ fontSize: "14px", fontWeight: 600, marginBottom: "3px" }}>{title}</div>
          <div style={{ fontSize: "12px", color: "var(--text-secondary)", lineHeight: 1.5 }}>{desc}</div>
        </div>
      </div>
      <div style={{ display: "flex", gap: "6px", flexWrap: "wrap" }}>
        {tags.map((tag) => (
          <span key={tag} style={{ fontSize: "10px", padding: "3px 8px", background: color + "12", color: color, borderRadius: "var(--radius-full)", fontWeight: 600 }}>{tag}</span>
        ))}
      </div>
    </div>
  );
}

export default WelcomeWizard;
