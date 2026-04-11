import React, { useState, useEffect, useRef, useMemo } from 'react';
import {
  Terminal,
  Search,
  Settings as SettingsIcon,
  LayoutDashboard,
  Server,
  Users,
  ClipboardList,
  Brain,
  FileText,
  ArrowRight,
  Command,
  Zap,
  Palette,
  Globe,
  Download,
  Trash2,
  RefreshCw,
  X,
} from 'lucide-react';
import { useNavigate } from 'react-router-dom';
import { useI18n } from '../i18n';
import { useToast } from './Toast';

interface CommandItem {
  id: string;
  label: string;
  shortcut?: string[];
  icon: React.ElementType;
  category: 'navigation' | 'actions' | 'settings' | 'tools';
  action?: () => void;
  path?: string;
  keywords: string[];
}

const COMMANDS: CommandItem[] = [
  {
    id: 'goto-dash', label: 'Go to Dashboard', shortcut: ['Ctrl', '1'],
    icon: LayoutDashboard, category: 'navigation', path: '/',
    keywords: ['dashboard', 'home', 'overview', 'main'],
  },
  {
    id: 'goto-serv', label: 'Go to Services', shortcut: ['Ctrl', '2'],
    icon: Server, category: 'navigation', path: '/services',
    keywords: ['services', 'docker', 'containers'],
  },
  {
    id: 'goto-agen', label: 'Go to Agents', shortcut: ['Ctrl', '3'],
    icon: Users, category: 'navigation', path: '/agents',
    keywords: ['agents', 'ai', 'bots'],
  },
  {
    id: 'goto-task', label: 'Go to Tasks', shortcut: ['Ctrl', '4'],
    icon: ClipboardList, category: 'navigation', path: '/tasks',
    keywords: ['tasks', 'jobs'],
  },
  {
    id: 'goto-llm', label: 'AI Model Configuration',
    icon: Brain, category: 'navigation', path: '/llm-config',
    keywords: ['ai', 'llm', 'model', 'openai', 'claude', 'api'],
  },
  {
    id: 'goto-conf', label: 'Configuration Editor',
    icon: SettingsIcon, category: 'navigation', path: '/config',
    keywords: ['config', 'yaml', 'env', 'settings'],
  },
  {
    id: 'goto-logs', label: 'View System Logs',
    icon: FileText, category: 'navigation', path: '/logs',
    keywords: ['logs', 'errors', 'debugging'],
  },
  {
    id: 'goto-term', label: 'Open Terminal',
    icon: Terminal, category: 'navigation', path: '/terminal',
    keywords: ['terminal', 'shell', 'command', 'bash'],
  },
  {
    id: 'goto-sett', label: 'Settings & Preferences',
    icon: SettingsIcon, category: 'navigation', path: '/settings',
    keywords: ['preferences', 'theme', 'language'],
  },
  {
    id: 'toggle-sidebar', label: 'Toggle Sidebar',
    shortcut: ['Ctrl', 'B'], icon: Search, category: 'actions',
    keywords: ['sidebar', 'panel', 'toggle', 'hide', 'show'],
    action: () => { window.dispatchEvent(new CustomEvent('toggle-sidebar')); },
  },
  {
    id: 'refresh-page', label: 'Refresh Page',
    shortcut: ['F5'], icon: RefreshCw, category: 'actions',
    keywords: ['refresh', 'reload', 'restart'],
    action: () => { window.location.reload(); },
  },
  {
    id: 'clear-cache', label: 'Clear Cache',
    icon: Trash2, category: 'tools',
    keywords: ['cache', 'clear', 'storage'],
    action: () => { localStorage.clear(); sessionStorage.clear(); },
  },
];

const CATEGORIES = [
  { key: 'all' as const, label: 'All Commands' },
  { key: 'navigation' as const, label: 'Navigation' },
  { key: 'actions' as const, label: 'Actions' },
  { key: 'settings' as const, label: 'Settings' },
  { key: 'tools' as const, label: 'Tools' },
];

const CommandPalette: React.FC = () => {
  const [isOpen, setIsOpen] = useState(false);
  const [query, setQuery] = useState('');
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [activeCategory, setActiveCategory] = useState<string>('all');
  const inputRef = useRef<HTMLInputElement>(null);
  const navigate = useNavigate();
  const { t } = useI18n();
  const { addToast } = useToast();

  const filteredCommands = useMemo(() => {
    let result = COMMANDS;
    if (activeCategory !== 'all') {
      result = result.filter(c => c.category === activeCategory);
    }
    if (query.trim()) {
      const q = query.toLowerCase();
      result = result.filter(cmd =>
        cmd.label.toLowerCase().includes(q) ||
        cmd.keywords.some(k => k.includes(q))
      );
    }
    return result;
  }, [query, activeCategory]);

  useEffect(() => {
    setSelectedIndex(0);
  }, [filteredCommands]);

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if ((e.metaKey || e.ctrlKey) && e.shiftKey && e.key === 'P') {
        e.preventDefault();
        setIsOpen(prev => !prev);
      }
      if (e.key === 'Escape') setIsOpen(false);
      if (isOpen) {
        if (e.key === 'ArrowDown') { e.preventDefault(); setSelectedIndex(i => Math.min(i + 1, filteredCommands.length - 1)); }
        if (e.key === 'ArrowUp') { e.preventDefault(); setSelectedIndex(i => Math.max(i - 1, 0)); }
        if (e.key === 'Enter' && filteredCommands[selectedIndex]) {
          e.preventDefault();
          executeCommand(filteredCommands[selectedIndex]);
        }
      }
    };
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, [isOpen, selectedIndex, filteredCommands]);

  useEffect(() => {
    if (isOpen && inputRef.current) inputRef.current.focus();
  }, [isOpen]);

  const executeCommand = (cmd: CommandItem) => {
    if (cmd.path) navigate(cmd.path);
    if (cmd.action) cmd.action();

    addToast({
      type: 'info',
      title: `Command executed: ${cmd.label}`,
      duration: 2500,
    });

    setIsOpen(false);
    setQuery('');
  };

  return (
    <>
      <button
        className="icon-btn"
        onClick={() => setIsOpen(true)}
        title="Command Palette (Ctrl+Shift+P)"
        data-tooltip="Command Palette"
      >
        <Command size={17} />
      </button>

      {isOpen && (
        <div className="modal-overlay" onClick={() => setIsOpen(false)}>
          <div
            className="global-search-modal command-palette-modal"
            onClick={(e) => e.stopPropagation()}
          >
            {/* Header */}
            <div style={{
              display: 'flex', alignItems: 'center', gap: '10px',
              padding: '14px 18px', borderBottom: '1px solid var(--border-subtle)',
            }}>
              <Command size={18} style={{ color: 'var(--primary-color)' }} />
              <input
                ref={inputRef}
                type="text"
                className="global-search-input"
                value={query}
                onChange={(e) => setQuery(e.target.value)}
                placeholder="Type a command or search..."
                autoFocus
              />
              {query && (
                <button onClick={() => setQuery('')} className="icon-btn"><X size={16} /></button>
              )}
            </div>

            {/* Category Tabs */}
            <div style={{
              display: 'flex', gap: '4px', padding: '8px 14px',
              borderBottom: '1px solid var(--border-subtle)',
            }}>
              {CATEGORIES.map(cat => (
                <button
                  key={cat.key}
                  onClick={() => setActiveCategory(cat.key)}
                  style={{
                    padding: '5px 12px', border: 'none', borderRadius: 'var(--radius-sm)',
                    background: activeCategory === cat.key ? 'var(--primary-light)' : 'transparent',
                    color: activeCategory === cat.key ? 'var(--primary-color)' : 'var(--text-secondary)',
                    fontSize: '12px', fontWeight: 500, cursor: 'pointer',
                    transition: 'all var(--transition-fast)',
                  }}
                >
                  {cat.label}
                </button>
              ))}
            </div>

            {/* Results */}
            <div className="global-search-results" style={{ maxHeight: '320px' }}>
              {filteredCommands.length === 0 ? (
                <div className="global-search-empty">
                  <p>No commands found for "{query}"</p>
                </div>
              ) : (
                filteredCommands.map((cmd, idx) => (
                  <button
                    key={cmd.id}
                    className={`global-search-result-item ${idx === selectedIndex ? 'selected' : ''}`}
                    onClick={() => executeCommand(cmd)}
                    onMouseEnter={() => setSelectedIndex(idx)}
                  >
                    <div className={`global-search-result-icon ${idx === selectedIndex ? '' : ''}`}
                      style={{
                        background: idx === selectedIndex ? 'var(--primary-color)' : 'var(--bg-tertiary)',
                        color: idx === selectedIndex ? '#fff' : 'var(--text-secondary)',
                      }}
                    >
                      <cmd.icon size={15} />
                    </div>

                    <div className="global-search-result-content">
                      <span>{cmd.label}</span>
                      <span style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
                        {(() => {
                          const sc = cmd.shortcut;
                          if (!sc) return null;
                          return sc.map((key, kidx) => (
                            <React.Fragment key={kidx}>
                              <kbd className="kbd-key" style={{ fontSize: '10px', padding: '1px 6px' }}>{key}</kbd>
                              {kidx < sc.length - 1 && <span style={{ fontSize: '9px', color: 'var(--text-muted)' }}>+</span>}
                            </React.Fragment>
                          ));
                        })()}
                      </span>
                    </div>

                    {idx === selectedIndex && <ArrowRight size={13} style={{ color: 'var(--primary-color)' }} />}
                  </button>
                ))
              )}
            </div>

            {/* Footer */}
            <div className="global-search-footer">
              <div className="global-search-shortcuts">
                <kbd>↑↓</kbd><span>Navigate</span>
                <kbd>↵</kbd><span>Execute</span>
                <kbd>Esc</kbd><span>Close</span>
                <kbd>Tab</kbd><span>Switch Category</span>
              </div>
            </div>
          </div>
        </div>
      )}
    </>
  );
};

export default CommandPalette;
