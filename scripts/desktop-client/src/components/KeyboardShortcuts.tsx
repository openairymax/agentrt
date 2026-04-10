import React, { useState, useEffect } from 'react';
import {
  X,
  Command,
  Keyboard,
  ArrowUp,
  ArrowDown,
  Search,
  Terminal,
  LayoutDashboard,
  Settings as SettingsIcon,
  HelpCircle,
} from 'lucide-react';

interface Shortcut {
  keys: string[];
  description: string;
  category: 'navigation' | 'actions' | 'system' | 'editor';
}

const SHORTCUTS: Shortcut[] = [
  // Navigation
  { keys: ['Ctrl', 'K'], description: 'Global Search', category: 'navigation' },
  { keys: ['Ctrl', '1'], description: 'Go to Dashboard', category: 'navigation' },
  { keys: ['Ctrl', '2'], description: 'Go to Services', category: 'navigation' },
  { keys: ['Ctrl', '3'], description: 'Go to Agents', category: 'navigation' },
  { keys: ['Ctrl', '4'], description: 'Go to Tasks', category: 'navigation' },
  { keys: ['Ctrl', 'B'], description: 'Toggle Sidebar', category: 'navigation' },

  // Actions
  { keys: ['Ctrl', 'S'], description: 'Save (in editor)', category: 'actions' },
  { keys: ['Ctrl', '/'], description: 'Toggle Comment', category: 'actions' },
  { keys: ['Ctrl', 'Enter'], description: 'Run Command (Terminal)', category: 'actions' },
  { keys: ['Ctrl', 'Shift', 'P'], description: 'Command Palette', category: 'actions' },

  // System
  { keys: ['Ctrl', ','], description: 'Open Settings', category: 'system' },
  { keys: ['?'], description: 'Show Keyboard Shortcuts', category: 'system' },
  { keys: ['F5'], description: 'Refresh Page', category: 'system' },
  { keys: ['Ctrl', 'Shift', 'D'], description: 'Toggle Dark/Light Theme', category: 'system' },

  // Editor
  { keys: ['Ctrl', 'Z'], description: 'Undo', category: 'editor' },
  { keys: ['Ctrl', 'Y'], description: 'Redo', category: 'editor' },
  { keys: ['Ctrl', 'A'], description: 'Select All', category: 'editor' },
  { keys: ['Ctrl', 'F'], description: 'Find in Editor', category: 'editor' },
];

const CATEGORIES = [
  { key: 'navigation', label: 'Navigation', icon: <LayoutDashboard size={14} /> },
  { key: 'actions', label: 'Actions', icon: <Terminal size={14} /> },
  { key: 'system', label: 'System', icon: <SettingsIcon size={14} /> },
  { key: 'editor', label: 'Editor', icon: <Keyboard size={14} /> },
];

interface KeyboardShortcutsModalProps {
  isOpen: boolean;
  onClose: () => void;
}

const KeyboardShortcutsModal: React.FC<KeyboardShortcutsModalProps> = ({ isOpen, onClose }) => {
  const [activeCategory, setActiveCategory] = useState<string>('all');

  useEffect(() => {
    if (!isOpen) return;
    const handleEsc = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
      if (e.key === '?') onClose();
    };
    document.addEventListener('keydown', handleEsc);
    return () => document.removeEventListener('keydown', handleEsc);
  }, [isOpen, onClose]);

  if (!isOpen) return null;

  const filtered = activeCategory === 'all'
    ? SHORTCUTS
    : SHORTCUTS.filter(s => s.category === activeCategory);

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="keyboard-shortcuts-modal" onClick={(e) => e.stopPropagation()}>
        {/* Header */}
        <div className="keyboard-shortcuts-header">
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <Command size={22} />
            <h2>Keyboard Shortcuts</h2>
          </div>
          <button className="icon-btn" onClick={onClose}>
            <X size={18} />
          </button>
        </div>

        {/* Category Tabs */}
        <div className="keyboard-shortcats-categories">
          <button
            className={`kbd-cat-btn ${activeCategory === 'all' ? 'active' : ''}`}
            onClick={() => setActiveCategory('all')}
          >
            All
          </button>
          {CATEGORIES.map(cat => (
            <button
              key={cat.key}
              className={`kbd-cat-btn ${activeCategory === cat.key ? 'active' : ''}`}
              onClick={() => setActiveCategory(cat.key)}
            >
              {cat.icon}
              <span>{cat.label}</span>
            </button>
          ))}
        </div>

        {/* Shortcuts List */}
        <div className="keyboard-shortcuts-list">
          {filtered.map((shortcut, idx) => (
            <div key={idx} className="keyboard-shortcut-row">
              <span className="keyboard-shortcut-desc">{shortcut.description}</span>
              <div className="keyboard-keys">
                {shortcut.keys.map((key, kidx) => (
                  <React.Fragment key={kidx}>
                    <kbd className="kbd-key">{key}</kbd>
                    {kidx < shortcut.keys.length - 1 && <span className="kbd-plus">+</span>}
                  </React.Fragment>
                ))}
              </div>
            </div>
          ))}
        </div>

        {/* Footer */}
        <div className="keyboard-shortcuts-footer">
          <HelpCircle size={14} />
          <span>Press <kbd className="kbd-key">?</kbd> anytime to show this dialog</span>
        </div>
      </div>
    </div>
  );
};

export default KeyboardShortcutsModal;

export function useKeyboardShortcuts() {
  const [showShortcuts, setShowShortcuts] = useState(false);

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === '?' && !isInputFocused()) {
        e.preventDefault();
        setShowShortcuts(prev => !prev);
      }
    };
    document.addEventListener('keydown', handleKeyDown);
    return () => document.removeEventListener('keydown', handleKeyDown);
  }, []);

  return { showShortcuts, setShowShortcuts };
}

function isInputFocused(): boolean {
  const el = document.activeElement;
  return el?.tagName === 'INPUT' || el?.tagName === 'TEXTAREA' || el?.getAttribute('contenteditable') === 'true';
}
