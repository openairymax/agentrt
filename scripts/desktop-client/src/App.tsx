import React, { useState, useEffect } from "react";
import { BrowserRouter as Router, Routes, Route, NavLink, useLocation, useNavigate } from "react-router-dom";
import {
  LayoutDashboard,
  Server,
  Users,
  ClipboardList,
  Settings as SettingsIcon,
  FileText,
  Terminal,
  Activity,
  Brain,
  ChevronRight,
  PanelLeftClose,
  PanelLeftOpen,
  CircleDot,
  GitBranch,
  Cpu,
  Wifi,
  Keyboard,
  Sparkles,
  X,
  MessageCircle,
} from "lucide-react";
import Dashboard from "./pages/Dashboard";
import Services from "./pages/Services";
import Agents from "./pages/Agents";
import Tasks from "./pages/Tasks";
import Config from "./pages/Config";
import Logs from "./pages/Logs";
import TerminalPage from "./pages/Terminal";
import Settings from "./pages/Settings";
import LLMConfig from "./pages/LLMConfig";
import AgentRuntime from "./pages/AgentRuntime";
import SystemMonitor from "./pages/SystemMonitor";
import AIChat from "./components/AIChat";
import WelcomeWizard from "./components/WelcomeWizard";
import ErrorBoundary from "./components/ErrorBoundary";
import NotificationCenter from "./components/NotificationCenter";
import GlobalSearch from "./components/GlobalSearch";
import KeyboardShortcutsModal, { useKeyboardShortcuts } from "./components/KeyboardShortcuts";
import CommandPalette from "./components/CommandPalette";
import { ToastProvider } from "./components/Toast";
import { useI18n } from "./i18n";

const navConfig = [
  { section: "nav.main", items: [
    { path: "/", icon: LayoutDashboard, labelKey: "nav.dashboard" },
    { path: "/services", icon: Server, labelKey: "nav.services" },
    { path: "/agents", icon: Users, labelKey: "nav.agents" },
    { path: "/tasks", icon: ClipboardList, labelKey: "nav.tasks" },
  ]},
  { section: "nav.system", items: [
    { path: "/agent-runtime", icon: Cpu, labelKey: "nav.agentRuntime" },
    { path: "/system-monitor", icon: Activity, labelKey: "nav.systemMonitor" },
    { path: "/ai-chat", icon: Brain, labelKey: "nav.aiChat" },
    { path: "/llm-config", icon: Sparkles, labelKey: "nav.llmConfig" },
    { path: "/config", icon: SettingsIcon, labelKey: "nav.config" },
    { path: "/logs", icon: FileText, labelKey: "nav.logs" },
    { path: "/terminal", icon: Terminal, labelKey: "nav.terminal" },
    { path: "/settings", icon: SettingsIcon, labelKey: "nav.settings" },
  ]},
];

function FloatingAIButton() {
  const [isOpen, setIsOpen] = useState(false);
  const location = useLocation();
  const navigate = useNavigate();

  if (location.pathname === '/ai-chat') return null;

  return (
    <>
      <button
        className="floating-ai-btn"
        onClick={() => navigate('/ai-chat')}
        title="AI 助手"
      >
        <Sparkles size={22} />
        <span className="floating-ai-pulse" />
      </button>
    </>
  );
}

function TitleBarContent() {
  const location = useLocation();
  const { t } = useI18n();

  const routeLabels: Record<string, string> = {
    "/": t.nav.dashboard,
    "/services": t.nav.services,
    "/agents": t.nav.agents,
    "/tasks": t.nav.tasks,
    "/agent-runtime": "AgentOS 运行时",
    "/ai-chat": "AI 助手",
    "/llm-config": t.nav.llmConfig,
    "/config": t.nav.config,
    "/logs": t.nav.logs,
    "/terminal": t.nav.terminal,
    "/settings": t.nav.settings,
  };

  const currentLabel = routeLabels[location.pathname] || "AgentOS";
  const breadcrumbs = [t.app.title, currentLabel];

  return (
    <div className="titlebar">
      <div className="titlebar-left">
        <div className="titlebar-breadcrumbs">
          {breadcrumbs.map((crumb, idx) => (
            <React.Fragment key={idx}>
              <span className="titlebar-breadcrumb-item">{crumb}</span>
              {idx < breadcrumbs.length - 1 && (
                <ChevronRight size={14} className="titlebar-breadcrumb-separator" />
              )}
            </React.Fragment>
          ))}
        </div>
      </div>

      <div className="titlebar-center" style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <GlobalSearch />
        <CommandPalette />
      </div>

      <div className="titlebar-actions">
        <NotificationCenter />
        <div className="avatar" title="User">U</div>
      </div>
    </div>
  );
}

function StatusBar() {
  const { t } = useI18n();
  const [time, setTime] = useState(new Date());

  useEffect(() => {
    const timer = setInterval(() => setTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  return (
    <div className="statusbar">
      <div className="statusbar-left">
        <div className="statusbar-item clickable">
          <GitBranch size={13} />
          <span>main</span>
        </div>
        <div className="statusbar-item clickable">
          <CircleDot size={13} />
          <span>0 problems</span>
        </div>
        <div className="statusbar-item">
          <Wifi size={13} />
          <span>{t.common.systemConnected}</span>
        </div>
      </div>
      <div className="statusbar-right">
        <div className="statusbar-item clickable" data-tooltip="Keyboard Shortcuts (?)">
          <Keyboard size={13} />
          <span style={{ marginLeft: '4px' }}>⌘K</span>
        </div>
        <div className="statusbar-item">
          <Cpu size={13} />
          <span>v0.3.2</span>
        </div>
        <div className="statusbar-item number-display">
          {time.toLocaleTimeString('en-US', { hour12: false })}
        </div>
      </div>
    </div>
  );
}

function AppContent() {
  const [sidebarOpen, setSidebarOpen] = useState(true);
  const { t } = useI18n();
  const { showShortcuts, setShowShortcuts } = useKeyboardShortcuts();
  const location = useLocation();
  const [pageKey, setPageKey] = useState(0);
  const [pageAnimating, setPageAnimating] = useState(false);

  const getNestedValue = (obj: any, path: string): string => {
    return path.split('.').reduce((acc, part) => acc?.[part], obj) || path;
  };

  const isAIChatPage = location.pathname === '/ai-chat';

  useEffect(() => {
    setPageAnimating(true);
    setPageKey(prev => prev + 1);
    const timer = setTimeout(() => setPageAnimating(false), 300);
    return () => clearTimeout(timer);
  }, [location.pathname]);

  return (
    <div className="app-layout">
      {/* Activity Bar */}
      <aside className="activity-bar">
        <div className="activity-bar-logo" title="AgentOS">A</div>

        <div className="activity-bar-items">
          {navConfig[0].items.map((item) => (
            <NavLink
              key={item.path}
              to={item.path}
              className={({ isActive }) =>
                `activity-bar-item ${isActive ? "active" : ""}`
              }
              title={getNestedValue(t, item.labelKey)}
            >
              <item.icon size={22} />
            </NavLink>
          ))}

          <div style={{ height: 8 }} />

          {/* AI Chat - Special Highlight */}
          <NavLink
            to="/ai-chat"
            className={({ isActive }) =>
              `activity-bar-item ai-chat-item ${isActive ? "active" : ""}`
            }
            title="AI 助手"
          >
            <MessageCircle size={22} />
          </NavLink>

          {navConfig[1].items.slice(1, 4).map((item) => (
            <NavLink
              key={item.path}
              to={item.path}
              className={({ isActive }) =>
                `activity-bar-item ${isActive ? "active" : ""}`
              }
              title={getNestedValue(t, item.labelKey)}
            >
              <item.icon size={22} />
            </NavLink>
          ))}
        </div>

        <div className="activity-bar-bottom">
          <NavLink
            to="/settings"
            className={({ isActive }) =>
              `activity-bar-item ${isActive ? "active" : ""}`
            }
            title={getNestedValue(t, navConfig[1].items[4].labelKey)}
          >
            <SettingsIcon size={22} />
          </NavLink>

          <button
            className="activity-bar-item"
            onClick={() => setSidebarOpen(!sidebarOpen)}
            title={sidebarOpen ? "Close Sidebar (Ctrl+B)" : "Open Sidebar"}
          >
            {sidebarOpen ? <PanelLeftClose size={20} /> : <PanelLeftOpen size={20} />}
          </button>
        </div>
      </aside>

      {/* Sidebar Panel */}
      <aside className={`sidebar-panel ${!sidebarOpen ? 'collapsed' : ''}`}>
        <div className="sidebar-panel-header">
          <span className="sidebar-panel-title">{t.app.title}</span>
          <button className="sidebar-panel-close" onClick={() => setSidebarOpen(false)}>✕</button>
        </div>

        <nav className="nav-menu">
          {navConfig.map((section, sIdx) => (
            <div key={section.section} className="nav-section">
              <div className="nav-section-title">{getNestedValue(t, section.section)}</div>
              {section.items.map((item) => (
                <NavLink
                  key={item.path}
                  to={item.path}
                  className={({ isActive }) =>
                    `nav-item ${isActive ? "active" : ""} ${item.path === '/ai-chat' ? 'nav-item-ai' : ''}`
                  }
                >
                  <item.icon size={18} />
                  <span>{item.path === '/ai-chat' ? 'AI 助手' : getNestedValue(t, item.labelKey)}</span>
                </NavLink>
              ))}
            </div>
          ))}
        </nav>

        <div style={{ padding: "16px", borderTop: "1px solid var(--border-subtle)" }}>
          <div className="nav-item" style={{ justifyContent: "flex-start", fontSize: "12px" }}>
            <Activity size={16} />
            <span>{t.common.systemConnected}</span>
          </div>
        </div>
      </aside>

      {/* Main Workspace */}
      <main className="main-workspace">
        {!isAIChatPage && <TitleBarContent />}

        <div
          key={pageKey}
          className={`content-area ${!isAIChatPage ? 'stagger-enter' : ''}`}
          style={{
            animation: pageAnimating ? 'pageFadeIn 0.3s ease-out' : undefined,
          }}
        >
          <ErrorBoundary>
            <Routes>
              <Route path="/" element={<Dashboard />} />
              <Route path="/services" element={<Services />} />
              <Route path="/agents" element={<Agents />} />
              <Route path="/tasks" element={<Tasks />} />
              <Route path="/agent-runtime" element={<AgentRuntime />} />
              <Route path="/system-monitor" element={<SystemMonitor />} />
              <Route path="/ai-chat" element={<AIChatPage />} />
              <Route path="/llm-config" element={<LLMConfig />} />
              <Route path="/config" element={<Config />} />
              <Route path="/logs" element={<Logs />} />
              <Route path="/terminal" element={<TerminalPage />} />
              <Route path="/settings" element={<Settings />} />
            </Routes>
          </ErrorBoundary>
        </div>

        {!isAIChatPage && <StatusBar />}
      </main>

      {/* Floating AI Button */}
      <FloatingAIButton />

      {/* Modals */}
      <KeyboardShortcutsModal isOpen={showShortcuts} onClose={() => setShowShortcuts(false)} />
    </div>
  );
}

function AIChatPage() {
  return (
    <div style={{ height: '100vh', display: 'flex', flexDirection: 'column' }}>
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '12px 20px', borderBottom: '1px solid var(--border-subtle)',
        background: 'var(--bg-secondary)',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <div style={{
            width: '36px', height: '36px', borderRadius: '12px',
            background: 'var(--primary-gradient)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
          }}>
            <Brain size={20} color="white" />
          </div>
          <div>
            <div style={{ fontWeight: 700, fontSize: '16px' }}>AI 助手</div>
            <div style={{ fontSize: '12px', color: 'var(--text-muted)' }}>AgentOS 智能对话 · 全功能模式</div>
          </div>
        </div>
      </div>
      <div style={{ flex: 1, overflow: 'hidden' }}>
        <AIChat model="GPT-4o" />
      </div>
    </div>
  );
}

function App() {
  const [wizardCompleted, setWizardCompleted] = useState(() => {
    return localStorage.getItem('agentos-wizard-completed') === 'true';
  });

  if (!wizardCompleted) {
    return <WelcomeWizard onComplete={() => setWizardCompleted(true)} />;
  }

  return (
    <ToastProvider>
      <Router>
        <AppContent />
      </Router>
    </ToastProvider>
  );
}

export default App;
