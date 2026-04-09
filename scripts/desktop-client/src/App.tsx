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

const navItems = [
  { section: "Main", items: [
    { path: "/", icon: LayoutDashboard, label: "Dashboard" },
    { path: "/services", icon: Server, label: "Services" },
    { path: "/agents", icon: Users, label: "Agents" },
    { path: "/tasks", icon: ClipboardList, label: "Tasks" },
  ]},
  { section: "System", items: [
    { path: "/config", icon: SettingsIcon, label: "Configuration" },
    { path: "/logs", icon: FileText, label: "Logs" },
    { path: "/terminal", icon: Terminal, label: "Terminal" },
    { path: "/settings", icon: Cog, label: "Settings" },
  ]},
];

function App() {
  const [sidebarOpen, setSidebarOpen] = useState(true);

  return (
    <Router>
      <div className="app-layout">
        {/* Sidebar */}
        <aside className="sidebar">
          <div className="sidebar-header">
            <div className="sidebar-logo">A</div>
            <div>
              <div className="sidebar-title">AgentOS</div>
              <div className="sidebar-subtitle">Desktop Client v0.1</div>
            </div>
          </div>

          <nav className="nav-menu">
            {navItems.map((section) => (
              <div key={section.section} className="nav-section">
                <div className="nav-section-title">{section.section}</div>
                {section.items.map((item) => (
                  <NavLink
                    key={item.path}
                    to={item.path}
                    className={({ isActive }) =>
                      `nav-item ${isActive ? "active" : ""}`
                    }
                  >
                    <item.icon size={20} />
                    <span>{item.label}</span>
                  </NavLink>
                ))}
              </div>
            ))}
          </nav>

          <div style={{ padding: "16px", borderTop: "1px solid var(--border-color)" }}>
            <div className="nav-item" style={{ justifyContent: "center", fontSize: "12px" }}>
              <Activity size={16} />
              <span>System Connected</span>
            </div>
          </div>
        </aside>

        {/* Main Content */}
        <main className="main-content">
          <header className="header">
            <h1 className="header-title">AgentOS Desktop</h1>
            <div className="header-actions">
              <button className="btn btn-secondary" title="Refresh">
                <Activity size={18} />
              </button>
              <button className="btn btn-primary" title="Quick Deploy">
                <Rocket size={18} />
                Deploy
              </button>
            </div>
          </header>

          <div className="content-area">
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
          </div>
        </main>
      </div>
    </Router>
  );
}

export default App;
