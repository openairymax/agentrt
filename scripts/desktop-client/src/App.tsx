import React, { useState } from "react";
import { BrowserRouter as Router, Routes, Route, NavLink } from "react-router-dom";
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
  Cog,
} from "lucide-react";
import Dashboard from "./pages/Dashboard";
import Services from "./pages/Services";
import Agents from "./pages/Agents";
import Tasks from "./pages/Tasks";
import Config from "./pages/Config";
import Logs from "./pages/Logs";
import TerminalPage from "./pages/Terminal";
import Settings from "./pages/Settings";
import WelcomeWizard from "./components/WelcomeWizard";
import ErrorBoundary from "./components/ErrorBoundary";
import { useI18n } from "./i18n";

const navConfig = [
  { section: "nav.main", items: [
    { path: "/", icon: LayoutDashboard, labelKey: "nav.dashboard" },
    { path: "/services", icon: Server, labelKey: "nav.services" },
    { path: "/agents", icon: Users, labelKey: "nav.agents" },
    { path: "/tasks", icon: ClipboardList, labelKey: "nav.tasks" },
  ]},
  { section: "nav.system", items: [
    { path: "/config", icon: SettingsIcon, labelKey: "nav.config" },
    { path: "/logs", icon: FileText, labelKey: "nav.logs" },
    { path: "/terminal", icon: Terminal, labelKey: "nav.terminal" },
    { path: "/settings", icon: Cog, labelKey: "nav.settings" },
  ]},
];

function AppContent() {
  const [sidebarOpen, setSidebarOpen] = useState(true);
  const { t } = useI18n();

  const getNestedValue = (obj: any, path: string): string => {
    return path.split('.').reduce((acc, part) => acc?.[part], obj) || path;
  };

  return (
    <div className="app-layout">
      <aside className={`sidebar ${sidebarOpen ? '' : 'sidebar-collapsed'}`}>
        <div className="sidebar-header">
          <div className="sidebar-logo">A</div>
          {sidebarOpen && (
            <div>
              <div className="sidebar-title">AgentOS</div>
              <div className="sidebar-subtitle">Desktop Client v0.2</div>
            </div>
          )}
          <button
            className="sidebar-toggle"
            onClick={() => setSidebarOpen(!sidebarOpen)}
            title={sidebarOpen ? 'Collapse' : 'Expand'}
          >
            {sidebarOpen ? '◀' : '▶'}
          </button>
        </div>

        <nav className="nav-menu">
          {navConfig.map((section) => (
            <div key={section.section} className="nav-section">
              {sidebarOpen && (
                <div className="nav-section-title">{getNestedValue(t, section.section)}</div>
              )}
              {section.items.map((item) => (
                <NavLink
                  key={item.path}
                  to={item.path}
                  className={({ isActive }) =>
                    `nav-item ${isActive ? "active" : ""}`
                  }
                  title={getNestedValue(t, item.labelKey)}
                >
                  <item.icon size={20} />
                  {sidebarOpen && <span>{getNestedValue(t, item.labelKey)}</span>}
                </NavLink>
              ))}
            </div>
          ))}
        </nav>

        <div style={{ padding: "16px", borderTop: "1px solid var(--border-color)" }}>
          <div className="nav-item" style={{ justifyContent: "center", fontSize: "12px" }}>
            <Activity size={16} />
            {sidebarOpen && <span>{t.common.systemConnected}</span>}
          </div>
        </div>
      </aside>

      <main className="main-content">
        <header className="header">
          <h1 className="header-title">{t.app.title}</h1>
          <div className="header-actions">
            <button className="btn btn-secondary" title={t.common.refresh}>
              <Activity size={18} />
            </button>
            <button className="btn btn-primary" title={t.app.quickDeploy}>
              <Rocket size={18} />
              {t.app.quickDeploy}
            </button>
          </div>
        </header>

        <div className="content-area">
          <ErrorBoundary>
            <Routes>
              <Route path="/" element={<Dashboard />} />
              <Route path="/services" element={<Services />} />
              <Route path="/agents" element={<Agents />} />
              <Route path="/tasks" element={<Tasks />} />
              <Route path="/config" element={<Config />} />
              <Route path="/logs" element={<Logs />} />
              <Route path="/terminal" element={<TerminalPage />} />
              <Route path="/settings" element={<Settings />} />
            </Routes>
          </ErrorBoundary>
        </div>
      </main>
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
    <Router>
      <AppContent />
    </Router>
  );
}

export default App;
