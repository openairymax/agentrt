import React from 'react';

interface ButtonProps {
  children: React.ReactNode;
  onClick?: (e: React.MouseEvent<HTMLButtonElement>) => void;
  variant?: 'primary' | 'secondary' | 'ghost' | 'danger' | 'success';
  size?: 'sm' | 'md' | 'lg';
  disabled?: boolean;
  loading?: boolean;
  className?: string;
  style?: React.CSSProperties;
  title?: string;
}

export const Button: React.FC<ButtonProps> = ({
  children,
  onClick,
  variant = 'primary',
  size = 'md',
  disabled = false,
  loading = false,
  className = '',
  style = {},
  title,
}) => {
  const baseStyles: React.CSSProperties = {
    display: 'inline-flex',
    alignItems: 'center',
    justifyContent: 'center',
    gap: '6px',
    border: 'none',
    borderRadius: '8px',
    fontWeight: 'var(--font-weight-normal)',
    cursor: disabled || loading ? 'not-allowed' : 'pointer',
    transition: 'all 0.2s ease',
    position: 'relative',
    overflow: 'hidden',
    fontFamily: 'inherit',
    letterSpacing: 'var(--letter-spacing-normal)',
    whiteSpace: 'nowrap',
  };

  const sizeStyles: Record<string, React.CSSProperties> = {
    sm: {
      padding: '6px 12px',
      fontSize: 'var(--font-size-sm)',
      gap: '4px',
    },
    md: {
      padding: '8px 16px',
      fontSize: 'var(--font-size-md)',
    },
    lg: {
      padding: '10px 20px',
      fontSize: 'var(--font-size-lg)',
    },
  };

  const variantStyles: Record<string, React.CSSProperties> = {
    primary: {
      backgroundColor: 'var(--primary-color)',
      color: 'white',
    },
    secondary: {
      backgroundColor: 'var(--bg-secondary)',
      color: 'var(--text-primary)',
    },
    ghost: {
      backgroundColor: 'transparent',
      color: 'var(--text-primary)',
    },
    danger: {
      backgroundColor: 'var(--error-color)',
      color: 'white',
    },
    success: {
      backgroundColor: 'var(--success-color)',
      color: 'white',
    },
  };

  const hoverStyles: Record<string, React.CSSProperties> = {
    primary: {
      backgroundColor: 'var(--primary-hover)',
      transform: 'translateY(-1px)',
      boxShadow: '0 4px 12px rgba(0, 113, 227, 0.25)',
    },
    secondary: {
      backgroundColor: 'var(--bg-tertiary)',
      transform: 'translateY(-1px)',
    },
    ghost: {
      backgroundColor: 'var(--bg-tertiary)',
      transform: 'translateY(-1px)',
    },
    danger: {
      backgroundColor: 'var(--error-hover)',
      transform: 'translateY(-1px)',
      boxShadow: '0 4px 12px rgba(255, 59, 48, 0.25)',
    },
    success: {
      backgroundColor: 'var(--success-hover)',
      transform: 'translateY(-1px)',
      boxShadow: '0 4px 12px rgba(52, 199, 89, 0.25)',
    },
  };

  return (
    <button
      onClick={onClick}
      disabled={disabled || loading}
      className={className}
      style={{
        ...baseStyles,
        ...sizeStyles[size],
        ...variantStyles[variant],
        ...style,
        opacity: disabled || loading ? 0.5 : 1,
      }}
      title={title}
      onMouseEnter={(e) => {
        if (!disabled && !loading) {
          Object.assign(e.currentTarget.style, hoverStyles[variant]);
        }
      }}
      onMouseLeave={(e) => {
        if (!disabled && !loading) {
          Object.assign(e.currentTarget.style, variantStyles[variant]);
        }
      }}
      onMouseDown={(e) => {
        if (!disabled && !loading) {
          e.currentTarget.style.transform = 'translateY(0)';
        }
      }}
      onMouseUp={(e) => {
        if (!disabled && !loading) {
          Object.assign(e.currentTarget.style, hoverStyles[variant]);
        }
      }}
    >
      {loading ? (
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <div style={{
            width: size === 'sm' ? '12px' : size === 'md' ? '14px' : '16px',
            height: size === 'sm' ? '12px' : size === 'md' ? '14px' : '16px',
            border: `2px solid ${variant === 'primary' || variant === 'danger' || variant === 'success' ? 'rgba(255, 255, 255, 0.3)' : 'rgba(0, 0, 0, 0.3)'}`,
            borderTopColor: variant === 'primary' || variant === 'danger' || variant === 'success' ? 'white' : 'var(--text-primary)',
            borderRadius: '50%',
            animation: 'spin 1s linear infinite',
          }} />
          <span>处理中...</span>
        </div>
      ) : (
        children
      )}
    </button>
  );
};
