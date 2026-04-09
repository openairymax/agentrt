import React, { useState, useEffect } from "react";
import { BrowserRouter as Router, Routes, Route, NavLink, useLocation } from "react-router-dom";
import {
  LayoutDashboard,
  Server,
  Users,
  ClipboardList,
  Settings as SettingsIcon,
  FileText,
  Terminal,
  Activity,
  Rocket,
  Brain,
  ChevronRight,
  PanelLeftClose,
  PanelLeftOpen,
  CircleDot,
  GitBranch,
  Cpu,
  Wifi,
  HelpCircle,
  Keyboard,
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
    { path: "/llm-config", icon: Brain, labelKey: "nav.llmConfig" },
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
              <span className={`titlebar-breadcrumb-item ${idx === breadcrumbs.length - 1 ? '' : ''}`}>
                {crumb}
              </span>
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
        <div className="statusbar-item clickable" style={{ cursor: 'pointer' }} data-tooltip="Keyboard Shortcuts (?)">
          <Keyboard size={13} />
          <span style={{ marginLeft: '4px' }}>⌘K</span>
        </div>
        <div className="statusbar-item">
          <Cpu size={13} />
          <span>v0.3.1</span>
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

  const getNestedValue = (obj: any, path: string): string => {
    return path.split('.').reduce((acc, part) => acc?.[part], obj) || path;
  };

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

          {navConfig[1].items.slice(0, 4).map((item) => (
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
          {navConfig.map((section) => (
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
                  <item.icon size={18} />
                  <span>{getNestedValue(t, item.labelKey)}</span>
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
        <TitleBarContent />

        <div className="content-area stagger-enter">
          <ErrorBoundary>
            <Routes>
              <Route path="/" element={<Dashboard />} />
              <Route path="/services" element={<Services />} />
              <Route path="/agents" element={<Agents />} />
              <Route path="/tasks" element={<Tasks />} />
              <Route path="/llm-config" element={<LLMConfig />} />
              <Route path="/config" element={<Config />} />
              <Route path="/logs" element={<Logs />} />
              <Route path="/terminal" element={<TerminalPage />} />
              <Route path="/settings" element={<Settings />} />
            </Routes>
          </ErrorBoundary>
        </div>

        <StatusBar />
      </main>

      {/* Modals */}
      <KeyboardShortcutsModal isOpen={showShortcuts} onClose={() => setShowShortcuts(false)} />
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
