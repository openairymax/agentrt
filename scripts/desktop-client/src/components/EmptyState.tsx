import React from 'react';
import {
  Inbox,
  Search,
  WifiOff,
  AlertCircle,
  Plus,
  ArrowRight,
  RefreshCw,
  Sparkles,
} from 'lucide-react';

export type EmptyStateType = 'no-data' | 'search-empty' | 'offline' | 'error' | 'success' | 'custom';

interface EmptyStateProps {
  type?: EmptyStateType;
  title: string;
  description?: string;
  icon?: React.ReactNode;
  actionLabel?: string;
  onAction?: () => void;
  secondaryLabel?: string;
  onSecondaryAction?: () => void;
  compact?: boolean;
}

const ICON_MAP: Record<EmptyStateType, React.ReactNode> = {
  'no-data': <Inbox size={48} />,
  'search-empty': <Search size={48} />,
  'offline': <WifiOff size={48} />,
  'error': <AlertCircle size={48} />,
  'success': <Sparkles size={48} />,
  'custom': <Inbox size={48} />,
};

const EmptyState: React.FC<EmptyStateProps> = ({
  type = 'no-data',
  title,
  description,
  icon,
  actionLabel,
  onAction,
  secondaryLabel,
  onSecondaryAction,
  compact = false,
}) => {
  if (compact) {
    return (
      <div className="empty-state empty-state-compact">
        {icon || ICON_MAP[type]}
        <div>
          <strong>{title}</strong>
          {description && <p>{description}</p>}
        </div>
        {actionLabel && onAction && (
          <button className="btn btn-primary btn-sm" onClick={onAction}>
            {actionLabel}
          </button>
        )}
      </div>
    );
  }

  return (
    <div className="empty-state">
      <div className="empty-state-icon">
        {icon || ICON_MAP[type]}
      </div>

      <div className="empty-state-text">{title}</div>

      {description && (
        <div className="empty-state-hint">{description}</div>
      )}

      {(actionLabel || secondaryLabel) && (
        <div style={{ display: 'flex', gap: '10px', justifyContent: 'center', marginTop: '20px' }}>
          {secondaryLabel && onSecondaryAction && (
            <button className="btn btn-secondary" onClick={onSecondaryAction}>
              <RefreshCw size={14} />
              {secondaryLabel}
            </button>
          )}
          {actionLabel && onAction && (
            <button className="btn btn-primary" onClick={onAction}>
              {type === 'no-data' ? <Plus size={14} /> : <ArrowRight size={14} />}
              {actionLabel}
            </button>
          )}
        </div>
      )}
    </div>
  );
};

export default EmptyState;

export const EmptyStates = {
  noData: (props: Omit<EmptyStateProps, 'type' | 'title'>) => (
    <EmptyState type="no-data" title="No data available" {...props} />
  ),
  searchEmpty: (props: Omit<EmptyStateProps, 'type' | 'title'>) => (
    <EmptyState type="search-empty" title="No results found" {...props} />
  ),
  offline: (props: Omit<EmptyStateProps, 'type' | 'title'>) => (
    <EmptyState type="offline" title="Connection lost" {...props} />
  ),
  error: (props: Omit<EmptyStateProps, 'type' | 'title'>) => (
    <EmptyState type="error" title="Something went wrong" {...props} />
  ),
};
