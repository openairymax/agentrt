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
    { id: 'dash', type: 'page', title: t.nav.dashboard, subtitle: 'System overview & monitoring', icon: LayoutDashboard, path: '/', keywords: ['dashboard', 'overview', 'home', 'main'] },
    { id: 'serv', type: 'page', title: t.nav.services, subtitle: 'Manage Docker services', icon: Server, path: '/services', keywords: ['services', 'docker', 'containers'] },
    { id: 'agen', type: 'page', title: t.nav.agents, subtitle: 'AI agents management', icon: Users, path: '/agents', keywords: ['agents', 'ai', 'bots', 'assistants'] },
    { id: 'task', type: 'page', title: t.nav.tasks, subtitle: 'Submit & track tasks', icon: ClipboardList, path: '/tasks', keywords: ['tasks', 'jobs', 'queue'] },
    { id: 'llmc', type: 'page', title: t.nav.llmConfig, subtitle: 'Configure AI models', icon: Brain, path: '/llm-config', keywords: ['ai', 'llm', 'model', 'openai', 'claude', 'api'] },
    { id: 'conf', type: 'page', title: t.nav.config, subtitle: 'Edit configuration files', icon: SettingsIcon, path: '/config', keywords: ['config', 'settings', 'yaml', 'env'] },
    { id: 'logs', type: 'page', title: t.nav.logs, subtitle: 'View system logs', icon: FileText, path: '/logs', keywords: ['logs', 'errors', 'debugging'] },
    { id: 'term', type: 'page', title: t.nav.terminal, subtitle: 'Execute commands', icon: Terminal, path: '/terminal', keywords: ['terminal', 'shell', 'command', 'bash'] },
    { id: 'sett', type: 'page', title: t.nav.settings, subtitle: 'Application preferences', icon: SettingsIcon, path: '/settings', keywords: ['settings', 'preferences', 'theme', 'language'] },
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
