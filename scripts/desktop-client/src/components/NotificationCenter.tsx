import React, { useState, useRef, useEffect } from 'react';
import { useNavigate } from 'react-router-dom';
import {
  Bell,
  X,
  CheckCircle2,
  AlertTriangle,
  Info,
  Zap,
  Server,
  Users,
  Brain,
  Settings as SettingsIcon,
  ChevronRight,
  Trash2,
  ExternalLink,
} from 'lucide-react';
import { useI18n } from '../i18n';

export interface NotificationItem {
  id: string;
  type: 'success' | 'warning' | 'error' | 'info' | 'system';
  title: string;
  message: string;
  timestamp: Date;
  read: boolean;
  action?: { label: string; onClick: () => void };
}

const NotificationCenter: React.FC = () => {
  const navigate = useNavigate();
  const [isOpen, setIsOpen] = useState(false);
  const [notifications, setNotifications] = useState<NotificationItem[]>([
    { id: '1', type: 'success', title: 'System Ready', message: 'All services are running normally. AgentOS is ready to use.', timestamp: new Date(Date.now() - 5 * 60000), read: false, action: { label: 'View Dashboard', onClick: () => navigate('/') } },
    { id: '2', type: 'info', title: 'New Feature Available', message: 'AI Model Configuration is now available. Configure your LLM providers.', timestamp: new Date(Date.now() - 15 * 60000), read: false, action: { label: 'Configure Now', onClick: () => navigate('/llm-config') } },
    { id: '3', type: 'warning', title: 'Memory Usage', message: 'System memory usage is above 80%. Consider stopping unused services.', timestamp: new Date(Date.now() - 30 * 60000), read: true },
    { id: '4', type: 'system', title: 'Auto-backup Completed', message: 'Configuration backup completed successfully at scheduled time.', timestamp: new Date(Date.now() - 60 * 60000), read: true },
  ]);
  const panelRef = useRef<HTMLDivElement>(null);

  const unreadCount = notifications.filter(n => !n.read).length;

  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (panelRef.current && !panelRef.current.contains(e.target as Node)) {
        setIsOpen(false);
      }
    };
    if (isOpen) document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, [isOpen]);

  useEffect(() => {
    const handleEsc = (e: KeyboardEvent) => { if (e.key === 'Escape') setIsOpen(false); };
    if (isOpen) document.addEventListener('keydown', handleEsc);
    return () => document.removeEventListener('keydown', handleEsc);
  }, [isOpen]);

  const markAsRead = (id: string) => {
    setNotifications(prev => prev.map(n => n.id === id ? { ...n, read: true } : n));
  };

  const markAllRead = () => {
    setNotifications(prev => prev.map(n => ({ ...n, read: true })));
  };

  const clearAll = () => {
    setNotifications([]);
  };

  const deleteNotification = (id: string) => {
    setNotifications(prev => prev.filter(n => n.id !== id));
  };

  const getIcon = (type: NotificationItem['type']) => {
    switch (type) {
      case 'success': return <CheckCircle2 size={16} color="#22c55e" />;
      case 'warning': return <AlertTriangle size={16} color="#f59e0b" />;
      case 'error': return <X size={16} color="#ef4444" />;
      case 'system': return <Server size={16} color="#6366f1" />;
      default: return <Info size={16} color="#6366f1" />;
    }
  };

  const getTypeColor = (type: NotificationItem['type']) => {
    switch (type) {
      case 'success': return '#22c55e';
      case 'warning': return '#f59e0b';
      case 'error': return '#ef4444';
      case 'system': return '#6366f1';
      default: return '#6366f1';
    }
  };

  const formatTimeAgo = (date: Date) => {
    const seconds = Math.floor((Date.now() - date.getTime()) / 1000);
    if (seconds < 60) return 'Just now';
    if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
    if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
    return date.toLocaleDateString();
  };

  return (
    <div style={{ position: 'relative' }} ref={panelRef}>
      {/* Bell Button */}
      <button
        className="icon-btn notification-bell-btn"
        onClick={() => setIsOpen(!isOpen)}
        title="Notifications"
        aria-label={`Notifications (${unreadCount} unread)`}
      >
        <Bell size={17} />
        {unreadCount > 0 && (
          <span className="notification-badge">{unreadCount}</span>
        )}
      </button>

      {/* Dropdown Panel */}
      {isOpen && (
        <div className="notification-panel">
          {/* Header */}
          <div className="notification-header">
            <div>
              <strong>Notifications</strong>
              {unreadCount > 0 && (
                <span className="notification-unread-count">{unreadCount} new</span>
              )}
            </div>
            <div style={{ display: 'flex', gap: '8px' }}>
              {unreadCount > 0 && (
                <button className="btn btn-ghost btn-sm" onClick={markAllRead}>
                  Mark all read
                </button>
              )}
              <button className="icon-btn" onClick={() => setIsOpen(false)}>
                <X size={14} />
              </button>
            </div>
          </div>

          {/* Notification List */}
          <div className="notification-list">
            {notifications.length === 0 ? (
              <div className="notification-empty">
                <Bell size={32} opacity={0.3} />
                <p>No notifications</p>
                <p style={{ fontSize: '12px', color: 'var(--text-muted)' }}>You're all caught up!</p>
              </div>
            ) : (
              notifications.map((notif) => (
                <div
                  key={notif.id}
                  className={`notification-item ${!notif.read ? 'unread' : ''}`}
                  onClick={() => markAsRead(notif.id)}
                  style={{ borderLeftColor: getTypeColor(notif.type) }}
                >
                  <div className="notification-icon-wrapper" style={{ background: `${getTypeColor(notif.type)}15` }}>
                    {getIcon(notif.type)}
                  </div>

                  <div className="notification-content">
                    <div className="notification-title-row">
                      <strong>{notif.title}</strong>
                      {!notif.read && <span className="notification-dot" />}
                    </div>
                    <p>{notif.message}</p>
                    <div className="notification-footer">
                      <span className="notification-time">{formatTimeAgo(notif.timestamp)}</span>
                      {notif.action && (
                        <button
                          className="btn btn-primary btn-sm"
                          onClick={(e) => { e.stopPropagation(); notif.action!.onClick(); }}
                        >
                          {notif.action.label}
                        </button>
                      )}
                    </div>
                  </div>

                  <button
                    className="icon-btn notification-delete"
                    onClick={(e) => { e.stopPropagation(); deleteNotification(notif.id); }}
                    title="Delete"
                  >
                    <Trash2 size={12} />
                  </button>
                </div>
              ))
            )}
          </div>

          {/* Footer */}
          {notifications.length > 0 && (
            <div className="notification-footer-bar">
              <button className="btn btn-ghost btn-sm" onClick={clearAll}>
                Clear all notifications
              </button>
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default NotificationCenter;
