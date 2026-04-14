import React, { useState, useEffect } from 'react';
import { CheckCircle2, XCircle, AlertTriangle, Info, X } from 'lucide-react';

export type FeedbackType = 'success' | 'error' | 'warning' | 'info';

interface OperationFeedbackProps {
  type: FeedbackType;
  title: string;
  message?: string;
  duration?: number;
  onClose?: () => void;
}

const feedbackConfig = {
  success: {
    icon: CheckCircle2,
    color: 'var(--success-color)',
    bgColor: 'var(--success-light)',
    borderColor: 'rgba(52, 199, 89, 0.25)',
  },
  error: {
    icon: XCircle,
    color: 'var(--error-color)',
    bgColor: 'var(--error-light)',
    borderColor: 'rgba(255, 59, 48, 0.25)',
  },
  warning: {
    icon: AlertTriangle,
    color: 'var(--warning-color)',
    bgColor: 'var(--warning-light)',
    borderColor: 'rgba(255, 149, 0, 0.25)',
  },
  info: {
    icon: Info,
    color: 'var(--primary-color)',
    bgColor: 'var(--primary-light)',
    borderColor: 'rgba(0, 113, 227, 0.25)',
  },
};

export const OperationFeedback: React.FC<OperationFeedbackProps> = ({
  type,
  title,
  message,
  duration = 3000,
  onClose,
}) => {
  const [isVisible, setIsVisible] = useState(true);

  useEffect(() => {
    if (duration > 0) {
      const timer = setTimeout(() => {
        setIsVisible(false);
        setTimeout(() => {
          onClose?.();
        }, 300);
      }, duration);

      return () => clearTimeout(timer);
    }
  }, [duration, onClose]);

  const config = feedbackConfig[type];
  const Icon = config.icon;

  return (
    <div
      style={{
        display: isVisible ? 'flex' : 'none',
        background: config.bgColor,
        borderLeft: `3px solid ${config.color}`,
        borderRadius: '0 8px 8px 0',
        padding: '16px 20px',
        alignItems: 'flex-start',
        gap: '12px',
        maxWidth: '400px',
        animation: isVisible ? 'slideInRight 0.3s ease-out' : 'slideOut 0.3s ease-in',
        transform: isVisible ? 'translateX(0)' : 'translateX(-100%)',
        position: 'relative',
        transition: 'all 0.3s ease',
      }}
    >
      <div style={{
        color: config.color,
        flexShrink: 0,
        marginTop: '2px',
      }}>
        <Icon size={20} />
      </div>
      <div style={{
        flex: 1,
        minWidth: 0,
      }}>
        <h3 style={{
          fontSize: '14px',
          fontWeight: 500,
          color: 'var(--text-primary)',
          margin: '0 0 4px 0',
        }}>
          {title}
        </h3>
        {message && (
          <p style={{
            fontSize: '13px',
            lineHeight: 1.4,
            color: 'var(--text-secondary)',
            margin: 0,
          }}>
            {message}
          </p>
        )}
      </div>
      {onClose && (
        <button
          onClick={onClose}
          style={{
            background: 'none',
            border: 'none',
            color: 'var(--text-muted)',
            cursor: 'pointer',
            padding: '4px',
            borderRadius: '4px',
            transition: 'all 0.2s ease',
            flexShrink: 0,
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.background = 'var(--bg-tertiary)';
            e.currentTarget.style.color = 'var(--text-secondary)';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.background = 'none';
            e.currentTarget.style.color = 'var(--text-muted)';
          }}
        >
          <X size={16} />
        </button>
      )}
    </div>
  );
};

interface OperationFeedbackProviderProps {
  children: React.ReactNode;
}

export const OperationFeedbackProvider: React.FC<OperationFeedbackProviderProps> = ({ children }) => {
  const [feedbacks, setFeedbacks] = useState<Array<{
    id: string;
    props: OperationFeedbackProps;
  }>>([]);

  const addFeedback = (props: OperationFeedbackProps) => {
    const id = `feedback-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
    setFeedbacks(prev => [...prev, { id, props: { ...props, onClose: () => removeFeedback(id) } }]);
  };

  const removeFeedback = (id: string) => {
    setFeedbacks(prev => prev.filter(f => f.id !== id));
  };

  return (
    <div style={{ position: 'relative' }}>
      {children}
      <div style={{
        position: 'fixed',
        top: '80px',
        left: '20px',
        zIndex: 1000,
        display: 'flex',
        flexDirection: 'column',
        gap: '12px',
        pointerEvents: 'none',
      }}>
        {feedbacks.map(feedback => (
          <div key={feedback.id} style={{ pointerEvents: 'auto' }}>
            <OperationFeedback {...feedback.props} />
          </div>
        ))}
      </div>
    </div>
  );
};

export const useOperationFeedback = () => {
  return {
    success: (title: string, message?: string, duration?: number) => {
      const event = new CustomEvent('addFeedback', {
        detail: { type: 'success' as FeedbackType, title, message, duration },
      });
      window.dispatchEvent(event);
    },
    error: (title: string, message?: string, duration?: number) => {
      const event = new CustomEvent('addFeedback', {
        detail: { type: 'error' as FeedbackType, title, message, duration },
      });
      window.dispatchEvent(event);
    },
    warning: (title: string, message?: string, duration?: number) => {
      const event = new CustomEvent('addFeedback', {
        detail: { type: 'warning' as FeedbackType, title, message, duration },
      });
      window.dispatchEvent(event);
    },
    info: (title: string, message?: string, duration?: number) => {
      const event = new CustomEvent('addFeedback', {
        detail: { type: 'info' as FeedbackType, title, message, duration },
      });
      window.dispatchEvent(event);
    },
  };
};
