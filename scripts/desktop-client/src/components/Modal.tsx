import React, { useState, useEffect, createContext, useContext, useCallback } from 'react';
import {
  AlertTriangle,
  CheckCircle2,
  Info,
  XCircle,
  X,
  HelpCircle,
} from 'lucide-react';

export type ModalType = 'confirm' | 'danger' | 'info' | 'success' | 'warning';

export interface ModalOptions {
  type?: ModalType;
  title: string;
  message: string;
  confirmText?: string;
  cancelText?: string;
  variant?: 'primary' | 'danger' | 'warning';
}

interface ModalContextValue {
  showModal: (options: ModalOptions) => Promise<boolean>;
}

const ModalContext = createContext<ModalContextValue | null>(null);

export const useModal = () => {
  const ctx = useContext(ModalContext);
  if (!ctx) throw new Error('useModal must be used within ModalProvider');
  return ctx;
};

const MODAL_CONFIGS: Record<ModalType, { icon: React.ElementType; color: string; bg: string; borderColor: string }> = {
  confirm: { icon: HelpCircle, color: '#6366f1', bg: 'rgba(99,102,241,0.08)', borderColor: 'rgba(99,102,241,0.25)' },
  danger: { icon: XCircle, color: '#ef4444', bg: 'rgba(239,68,68,0.08)', borderColor: 'rgba(239,68,68,0.25)' },
  info: { icon: Info, color: '#3b82f6', bg: 'rgba(59,130,246,0.08)', borderColor: 'rgba(59,130,246,0.25)' },
  success: { icon: CheckCircle2, color: '#22c55e', bg: 'rgba(34,197,94,0.08)', borderColor: 'rgba(34,197,94,0.25)' },
  warning: { icon: AlertTriangle, color: '#f59e0b', bg: 'rgba(245,158,11,0.08)', borderColor: 'rgba(245,158,11,0.25)' },
};

export const ModalProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [modal, setModal] = useState<ModalOptions & { id: number; resolve: (value: boolean) => void } | null>(null);

  const showModal = useCallback((options: ModalOptions): Promise<boolean> => {
    return new Promise((resolve) => {
      const id = Date.now();
      setModal({ ...options, id, resolve });
    });
  }, []);

  const handleConfirm = () => {
    if (modal) {
      modal.resolve(true);
      setModal(null);
    }
  };

  const handleCancel = () => {
    if (modal) {
      modal.resolve(false);
      setModal(null);
    }
  };

  useEffect(() => {
    const handleEsc = (e: KeyboardEvent) => {
      if (e.key === 'Escape' && modal) handleCancel();
    };
    window.addEventListener('keydown', handleEsc);
    return () => window.removeEventListener('keydown', handleEsc);
  }, [modal]);

  return (
    <ModalContext.Provider value={{ showModal }}>
      {children}
      {modal && (
        <div className="modal-overlay" onClick={handleCancel}>
          <div
            className="modal-content"
            style={{ maxWidth: '420px', padding: '28px 30px' }}
            onClick={(e) => e.stopPropagation()}
          >
            {/* Icon */}
            <div style={{
              width: '48px', height: '48px', borderRadius: 'var(--radius-md)',
              background: MODAL_CONFIGS[modal.type || 'confirm'].bg,
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              marginBottom: '18px',
            }}>
              {React.createElement(MODAL_CONFIGS[modal.type || 'confirm'].icon, {
                size: 24,
                color: MODAL_CONFIGS[modal.type || 'confirm'].color,
              })}
            </div>

            {/* Title */}
            <h2 style={{
              fontSize: '18px', fontWeight: 700, margin: '0 0 10px',
              letterSpacing: '-0.01em',
            }}>
              {modal.title}
            </h2>

            {/* Message */}
            <p style={{
              fontSize: '14px', lineHeight: '1.6', color: 'var(--text-secondary)',
              margin: '0 0 26px',
            }}>
              {modal.message}
            </p>

            {/* Actions */}
            <div style={{ display: 'flex', gap: '12px', justifyContent: 'flex-end' }}>
              <button
                className="btn btn-secondary"
                onClick={handleCancel}
                style={{ minWidth: '88px' }}
              >
                {modal.cancelText || 'Cancel'}
              </button>
              <button
                className={`btn ${modal.variant === 'danger' ? 'btn-danger' : modal.variant === 'warning' ? 'btn-warning' : 'btn-primary'}`}
                onClick={handleConfirm}
                style={{ minWidth: '88px' }}
              >
                {modal.confirmText || (modal.type === 'danger' ? 'Delete' : 'Confirm')}
              </button>
            </div>
          </div>
        </div>
      )}
    </ModalContext.Provider>
  );
};
