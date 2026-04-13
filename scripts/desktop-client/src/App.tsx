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
  Database,
  Settings2,
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
import MemoryEvolution from "./pages/MemoryEvolution";
import ToolManager from "./pages/ToolManager";
import ProtocolPlayground from "./pages/ProtocolPlayground";
import AIChat from "./components/AIChat";
import WelcomeWizard from "./components/WelcomeWizard";
import ErrorBoundary from "./components/ErrorBoundary";
import NotificationCenter from "./components/NotificationCenter";
import GlobalSearch from "./components/GlobalSearch";
import KeyboardShortcutsModal, { useKeyboardShortcuts } from "./components/KeyboardShortcuts";
import CommandPalette from "./components/CommandPalette";
import { ToastProvider } from "./components/Toast";
import { ModalProvider } from "./components/Modal";
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
    { path: "/memory-evolution", icon: Database, labelKey: "nav.memoryEvolution" },
    { path: "/tools", icon: Settings2, labelKey: "nav.tools" },
    { path: "/protocols", icon: Wifi, labelKey: "nav.protocols" },
    { path: "/ai-chat", icon: Brain, labelKey: "nav.aiChat" },
    { path: "/llm-config", icon: Sparkles, labelKey: "nav.llmConfig" },
    { path: "/config", icon: SettingsIcon, labelKey: "nav.config" },
    { path: "/logs", icon: FileText, labelKey: "nav.logs" },
    { path: "/terminal", icon: Terminal, labelKey: "nav.terminal" },
    { path: "/settings", icon: SettingsIcon, labelKey: "nav.settings" },
  ]},
];



function TitleBarContent() {
  const location = useLocation();
  const { t } = useI18n();

  const routeLabels: Record<string, string> = {
    "/": t.nav.dashboard,
    "/services": t.nav.services,
    "/agents": t.nav.agents,
    "/tasks": t.nav.tasks,
    "/agent-runtime": t.nav.agentRuntime,
    "/system-monitor": t.nav.systemMonitor,
    "/memory-evolution": t.nav.memoryEvolution,
    "/tools": t.nav.tools,
    "/ai-chat": t.nav.aiChat,
    "/llm-config": t.nav.llmConfig,
    "/config": t.nav.config,
    "/logs": t.nav.logs,
    "/terminal": t.nav.terminal,
    "/settings": t.nav.settings,
  };

  const currentLabel = routeLabels[location.pathname] || "AgentOS";

  return (
    <div className="titlebar">
      <div className="titlebar-left">
        <div className="titlebar-title">{currentLabel}</div>
      </div>

      <div className="titlebar-actions">
        <GlobalSearch />
      </div>
    </div>
  );
}

function StatusBar() {
  const { t } = useI18n();

  return (
    <div className="statusbar">
      <div className="statusbar-left">
        <div className="statusbar-item">
          <CircleDot size={13} />
          <span>{t.common.systemConnected}</span>
        </div>
      </div>
      <div className="statusbar-right">
        <div className="statusbar-item">
          <Cpu size={13} />
          <span>v0.3.2</span>
        </div>
      </div>
    </div>
  );
}

function AppContent() {
  const { t } = useI18n();
  const { showShortcuts, setShowShortcuts } = useKeyboardShortcuts();
  const location = useLocation();

  const getNestedValue = (obj: any, path: string): string => {
    return path.split('.').reduce((acc, part) => acc?.[part], obj) || path;
  };

  const isAIChatPage = location.pathname === '/ai-chat';

  return (
    <div className="app-layout">
      {/* Sidebar Panel */}
      <aside className="sidebar-panel">
        <div className="sidebar-panel-header">
          <span className="sidebar-panel-title">{t.app.title}</span>
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
                    `nav-item ${isActive ? "active" : ""}`
                  }
                >
                  <item.icon size={16} />
                  <span>{getNestedValue(t, item.labelKey)}</span>
                </NavLink>
              ))}
            </div>
          ))}
        </nav>
      </aside>

      {/* Main Workspace */}
      <main className="main-workspace">
        {!isAIChatPage && <TitleBarContent />}

        <div className="content-area">
          <ErrorBoundary>
            <Routes>
              <Route path="/" element={<Dashboard />} />
              <Route path="/services" element={<Services />} />
              <Route path="/agents" element={<Agents />} />
              <Route path="/tasks" element={<Tasks />} />
              <Route path="/agent-runtime" element={<AgentRuntime />} />
              <Route path="/system-monitor" element={<SystemMonitor />} />
              <Route path="/memory-evolution" element={<MemoryEvolution />} />
              <Route path="/tools" element={<ToolManager />} />
              <Route path="/protocols" element={<ProtocolPlayground />} />
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

      {/* Modals */}
      <KeyboardShortcutsModal isOpen={showShortcuts} onClose={() => setShowShortcuts(false)} />
    </div>
  );
}

function AIChatPage() {
  return (
    <div style={{ height: '100vh', display: 'flex', flexDirection: 'column', background: 'var(--bg-secondary)' }}>
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '16px 24px', borderBottom: '1px solid var(--border-subtle)',
        background: 'var(--bg-primary)',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{
            width: '40px', height: '40px', borderRadius: '12px',
            background: 'var(--primary-gradient)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
          }}>
            <Brain size={20} color="white" />
          </div>
          <div>
            <div style={{ fontWeight: 600, fontSize: '16px' }}>AI 助手</div>
            <div style={{ fontSize: '12px', color: 'var(--text-muted)' }}>AgentOS 智能对话</div>
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
      <ModalProvider>
        <Router>
          <AppContent />
        </Router>
      </ModalProvider>
    </ToastProvider>
  );
}

export default App;
