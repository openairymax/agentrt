import React, { useState, useEffect, useRef, useCallback } from "react";
import { createPortal } from "react-dom";

export interface MenuItem {
  id: string;
  label: string;
  icon?: React.ReactNode;
  shortcut?: string;
  disabled?: boolean;
  danger?: boolean;
  divider?: boolean;
}

interface ContextMenuProps {
  items: MenuItem[];
  onSelect: (id: string) => void;
  position: { x: number; y: number } | null;
  onClose: () => void;
}

const ContextMenu: React.FC<ContextMenuProps> = ({ items, onSelect, position, onClose }) => {
  const menuRef = useRef<HTMLDivElement>(null);
  const [focusedIndex, setFocusedIndex] = useState(-1);

  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(e.target as Node)) {
        onClose();
      }
    };

    const handleEscape = (e: KeyboardEvent) => {
      if (e.key === 'Escape') {
        onClose();
      }
    };

    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'ArrowDown') {
        e.preventDefault();
        setFocusedIndex(prev => Math.min(prev + 1, items.filter(i => !i.disabled && !i.divider).length - 1));
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        setFocusedIndex(prev => Math.max(prev - 1, 0));
      } else if (e.key === 'Enter' && focusedIndex >= 0) {
        e.preventDefault();
        const enabledItems = items.filter(i => !i.disabled && !i.divider);
        if (enabledItems[focusedIndex]) {
          onSelect(enabledItems[focusedIndex].id);
          onClose();
        }
      }
    };

    document.addEventListener('mousedown', handleClickOutside);
    document.addEventListener('keydown', handleEscape);
    document.addEventListener('keydown', handleKeyDown);

    return () => {
      document.removeEventListener('mousedown', handleClickOutside);
      document.removeEventListener('keydown', handleEscape);
      document.removeEventListener('keydown', handleKeyDown);
    };
  }, [items, onSelect, onClose, focusedIndex]);

  useEffect(() => {
    setFocusedIndex(-1);
  }, [position]);

  const handleSelect = useCallback((id: string) => {
    onSelect(id);
    onClose();
  }, [onSelect, onClose]);

  if (!position) return null;

  let enabledIndex = -1;

  return createPortal(
    <div
      ref={menuRef}
      style={{
        position: 'fixed',
        left: position.x,
        top: position.y,
        zIndex: 10000,
        minWidth: '200px',
        maxWidth: '280px',
      }}
      className="context-menu-wrapper"
    >
      <div style={{
        background: 'var(--bg-elevated)',
        border: '1px solid var(--border-color)',
        borderRadius: 'var(--radius-md)',
        boxShadow: '0 8px 32px rgba(0, 0, 0, 0.25), 0 2px 8px rgba(0, 0, 0, 0.15)',
        padding: '6px',
        animation: 'contextMenuFadeIn 0.15s ease-out',
        overflow: 'hidden',
      }}>
        {items.map((item, index) => {
          if (item.divider) {
            return (
              <div
                key={`divider-${index}`}
                style={{
                  height: '1px',
                  background: 'var(--border-subtle)',
                  margin: '6px 8px',
                }}
              />
            );
          }

          if (item.disabled) {
            return (
              <div
                key={item.id}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                  padding: '9px 12px',
                  borderRadius: 'var(--radius-sm)',
                  color: 'var(--text-muted)',
                  opacity: 0.45,
                  cursor: 'not-allowed',
                }}
              >
                {item.icon && <span style={{ fontSize: '15px' }}>{item.icon}</span>}
                <span style={{ flex: 1, fontSize: '13.5px' }}>{item.label}</span>
                {item.shortcut && (
                  <span style={{
                    fontSize: '11px',
                    fontFamily: "'JetBrains Mono', monospace",
                    color: 'var(--text-muted)',
                  }}>
                    {item.shortcut}
                  </span>
                )}
              </div>
            );
          }

          const currentIndex = ++enabledIndex;
          const isFocused = focusedIndex === currentIndex;

          return (
            <button
              key={item.id}
              onClick={() => handleSelect(item.id)}
              onMouseEnter={() => setFocusedIndex(currentIndex)}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                width: '100%',
                padding: '9px 12px',
                border: 'none',
                borderRadius: 'var(--radius-sm)',
                background: isFocused ? 'var(--primary-light)' : 'transparent',
                color: item.danger ? 'var(--error-color)' : 'var(--text-primary)',
                cursor: 'pointer',
                transition: 'all var(--transition-fast)',
                textAlign: 'left' as const,
                fontSize: '13.5px',
                fontFamily: 'inherit',
              }}
            >
              {item.icon && <span style={{ fontSize: '15px' }}>{item.icon}</span>}
              <span style={{ flex: 1 }}>{item.label}</span>
              {item.shortcut && (
                <span style={{
                  fontSize: '11px',
                  fontFamily: "'JetBrains Mono', monospace",
                  color: 'var(--text-muted)',
                }}>
                  {item.shortcut}
                </span>
              )}
            </button>
          );
        })}
      </div>

      <style>{`
        @keyframes contextMenuFadeIn {
          from {
            opacity: 0;
            transform: scale(0.96) translateY(-4px);
          }
          to {
            opacity: 1;
            transform: scale(1) translateY(0);
          }
        }
      `}</style>
    </div>,
    document.body
  );
};

export default ContextMenu;
