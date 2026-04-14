import React, { useState, useEffect, useRef, useMemo } from 'react';
import {
  Search,
  X,
  LayoutDashboard,
  Server,
  Users,
  ClipboardList,
  Brain,
  Settings as SettingsIcon,
  FileText,
  Terminal,
  ArrowRight,
  Command,
  Activity,
  Database,
  Settings2,
  Wifi,
  MessageCircle,
  Sparkles,
  Cpu,
} from 'lucide-react';
import { useNavigate } from 'react-router-dom';
import { useI18n } from '../i18n';

interface SearchResult {
  id: string;
  type: 'page' | 'action' | 'setting';
  title: string;
  subtitle?: string;
  icon: React.ElementType;
  path?: string;
  action?: () => void;
  keywords: string[];
}

const GlobalSearch: React.FC = () => {
  const [isOpen, setIsOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [selectedIndex, setSelectedIndex] = useState(0);
  const inputRef = useRef<HTMLInputElement>(null);
  const navigate = useNavigate();
  const { t } = useI18n();

  const searchItems: SearchResult[] = useMemo(() => [
    { id: 'dash', type: 'page', title: t.nav.dashboard, subtitle: '系统概览与监控', icon: LayoutDashboard, path: '/', keywords: ['dashboard', 'overview', 'home', 'main', '系统', '概览'] },
    { id: 'agen', type: 'page', title: t.nav.agents, subtitle: 'AI智能体管理', icon: Users, path: '/agents', keywords: ['agents', 'ai', 'bots', 'assistants', '智能体', '机器人'] },
    { id: 'task', type: 'page', title: t.nav.tasks, subtitle: '任务编排与追踪', icon: ClipboardList, path: '/tasks', keywords: ['tasks', 'jobs', 'queue', '任务', '编排'] },
    { id: 'serv', type: 'page', title: t.nav.services, subtitle: '服务管理与监控', icon: Server, path: '/services', keywords: ['services', 'docker', 'containers', '服务', '容器'] },
    { id: 'ai-chat', type: 'page', title: t.nav.aiChat, subtitle: 'AI智能助手', icon: MessageCircle, path: '/ai-chat', keywords: ['ai', 'chat', 'assistant', '助手', '对话'] },
    { id: 'llmc', type: 'page', title: t.nav.llmConfig, subtitle: 'AI模型配置', icon: Sparkles, path: '/llm-config', keywords: ['ai', 'llm', 'model', 'openai', 'claude', 'api', '模型', '配置'] },
    { id: 'agent-runtime', type: 'page', title: t.nav.agentRuntime, subtitle: '运行时管理', icon: Cpu, path: '/agent-runtime', keywords: ['runtime', 'agent', '运行时', '管理'] },
    { id: 'memory-evolution', type: 'page', title: t.nav.memoryEvolution, subtitle: '记忆系统管理', icon: Database, path: '/memory-evolution', keywords: ['memory', 'evolution', '记忆', '系统'] },
    { id: 'system-monitor', type: 'page', title: t.nav.systemMonitor, subtitle: '系统资源监控', icon: Activity, path: '/system-monitor', keywords: ['system', 'monitor', 'resource', '系统', '监控', '资源'] },
    { id: 'logs', type: 'page', title: t.nav.logs, subtitle: '系统日志查看', icon: FileText, path: '/logs', keywords: ['logs', 'errors', 'debugging', '日志', '错误', '调试'] },
    { id: 'terminal', type: 'page', title: t.nav.terminal, subtitle: '终端命令执行', icon: Terminal, path: '/terminal', keywords: ['terminal', 'shell', 'command', 'bash', '终端', '命令'] },
    { id: 'tools', type: 'page', title: t.nav.tools, subtitle: '工具管理', icon: Settings2, path: '/tools', keywords: ['tools', '管理', '工具'] },
    { id: 'protocols', type: 'page', title: '协议测试', subtitle: '协议 测试', icon: Wifi, path: '/protocols', keywords: ['protocols', 'test', '协议', '测试'] },
    { id: 'config', type: 'page', title: t.nav.config, subtitle: '配置文件编辑', icon: SettingsIcon, path: '/config', keywords: ['config', 'settings', 'yaml', 'env', '配置', '文件'] },
    { id: 'settings', type: 'page', title: t.nav.settings, subtitle: '应用偏好设置', icon: SettingsIcon, path: '/settings', keywords: ['settings', 'preferences', 'theme', 'language', '设置', '偏好', '主题', '语言'] },
  ], [t]);

  const filteredResults = useMemo(() => {
    if (!query.trim()) return searchItems;
    const q = query.toLowerCase();
    return searchItems.filter(item =>
      item.title.toLowerCase().includes(q) ||
      item.subtitle?.toLowerCase().includes(q) ||
      item.keywords.some(k => k.includes(q))
    );
  }, [query, searchItems]);

  useEffect(() => {
    setSelectedIndex(0);
  }, [filteredResults]);

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.key === 'k') {
        e.preventDefault();
        setIsOpen(prev => !prev);
      }
      if (e.key === 'Escape') setIsOpen(false);
      if (isOpen) {
        if (e.key === 'ArrowDown') {
          e.preventDefault();
          setSelectedIndex(i => Math.min(i + 1, filteredResults.length - 1));
        }
        if (e.key === 'ArrowUp') {
          e.preventDefault();
          setSelectedIndex(i => Math.max(i - 1, 0));
        }
        if (e.key === 'Enter' && filteredResults[selectedIndex]) {
          e.preventDefault();
          handleSelect(filteredResults[selectedIndex]);
        }
      }
    };
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [isOpen, selectedIndex, filteredResults]);

  useEffect(() => {
    if (isOpen && inputRef.current) inputRef.current.focus();
  }, [isOpen]);

  const handleSelect = (result: SearchResult) => {
    if (result.path) {
      navigate(result.path);
    }
    if (result.action) result.action();
    setIsOpen(false);
    setQuery('');
  };

  return (
    <>
      {/* Trigger Button */}
      <button
        className="icon-btn global-search-trigger"
        onClick={() => setIsOpen(true)}
        title="Search (Ctrl+K)"
      >
        <Search size={17} />
        <kbd className="search-kbd">⌘K</kbd>
      </button>

      {/* Search Modal */}
      {isOpen && (
        <div className="modal-overlay" onClick={() => setIsOpen(false)}>
          <div
            className="global-search-modal"
            onClick={(e) => e.stopPropagation()}
          >
            {/* Search Input */}
            <div className="global-search-input-wrapper">
              <Search size={18} style={{ color: "var(--text-muted)" }} />
              <input
                ref={inputRef}
                type="text"
                className="global-search-input"
                value={query}
                onChange={(e) => setQuery(e.target.value)}
                placeholder="Search pages, settings, actions..."
                autoFocus
              />
              {query && (
                <button onClick={() => setQuery('')} className="icon-btn">
                  <X size={16} />
                </button>
              )}
            </div>

            {/* Results */}
            <div className="global-search-results">
              {filteredResults.length === 0 ? (
                <div className="global-search-empty">
                  <p>No results found for "{query}"</p>
                  <p style={{ fontSize: '12px', color: 'var(--text-muted)', marginTop: '4px' }}>
                    Try different keywords or check spelling
                  </p>
                </div>
              ) : (
                <div className="global-search-section-label">Pages</div>
              )}
              {filteredResults.map((result, idx) => (
                <button
                  key={result.id}
                  className={`global-search-result-item ${idx === selectedIndex ? 'selected' : ''}`}
                  onClick={() => handleSelect(result)}
                  onMouseEnter={() => setSelectedIndex(idx)}
                >
                  <div className="global-search-result-icon" style={{
                    background: idx === selectedIndex ? 'var(--primary-color)' : 'var(--bg-tertiary)',
                    color: idx === selectedIndex ? '#fff' : 'var(--text-secondary)',
                  }}>
                    <result.icon size={16} />
                  </div>

                  <div className="global-search-result-content">
                    <span>{result.title}</span>
                    <span className="global-search-result-subtitle">{result.subtitle}</span>
                  </div>

                  {idx === selectedIndex && (
                    <ArrowRight size={14} style={{ color: 'var(--primary-color)' }} />
                  )}
                </button>
              ))}
            </div>

            {/* Footer */}
            <div className="global-search-footer">
              <div className="global-search-shortcuts">
                <kbd>↑↓</kbd><span>Navigate</span>
                <kbd>↵</kbd><span>Select</span>
                <kbd>Esc</kbd><span>Close</span>
              </div>
            </div>
          </div>
        </div>
      )}
    </>
  );
};

export default GlobalSearch;
