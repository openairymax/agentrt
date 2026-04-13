import React from 'react';

interface ButtonProps {
  children: React.ReactNode;
  onClick?: () => void;
  variant?: 'primary' | 'secondary' | 'ghost' | 'danger' | 'success';
  size?: 'sm' | 'md' | 'lg';
  disabled?: boolean;
  className?: string;
  style?: React.CSSProperties;
}

export const Button: React.FC<ButtonProps> = ({
  children,
  onClick,
  variant = 'primary',
  size = 'md',
  disabled = false,
  className = '',
  style = {},
}) => {
  const baseStyles: React.CSSProperties = {
    display: 'inline-flex',
    alignItems: 'center',
    justifyContent: 'center',
    gap: '8px',
    border: 'none',
    borderRadius: '6px',
    fontSize: '12px',
    fontWeight: 600,
    cursor: disabled ? 'not-allowed' : 'pointer',
    transition: 'all 0.2s ease',
    fontFamily: 'inherit',
    whiteSpace: 'nowrap',
  };

  const sizeStyles: Record<string, React.CSSProperties> = {
    sm: {
      padding: '6px 12px',
      fontSize: '11px',
    },
    md: {
      padding: '8px 16px',
      fontSize: '12px',
    },
    lg: {
      padding: '10px 20px',
      fontSize: '13px',
    },
  };

  const variantStyles: Record<string, React.CSSProperties> = {
    primary: {
      backgroundColor: 'var(--primary-color)',
      color: 'white',
    },
    secondary: {
      backgroundColor: 'transparent',
      color: 'var(--text-primary)',
      border: '1px solid var(--border-color)',
    },
    ghost: {
      backgroundColor: 'transparent',
      color: 'var(--text-secondary)',
      border: '1px solid transparent',
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
    },
    secondary: {
      backgroundColor: 'var(--bg-tertiary)',
    },
    ghost: {
      backgroundColor: 'var(--bg-tertiary)',
      color: 'var(--text-primary)',
      borderColor: 'var(--border-color)',
    },
    danger: {
      backgroundColor: '#dc2626',
    },
    success: {
      backgroundColor: '#059669',
    },
  };

  return (
    <button
      onClick={onClick}
      disabled={disabled}
      className={className}
      style={{
        ...baseStyles,
        ...sizeStyles[size],
        ...variantStyles[variant],
        ...style,
        opacity: disabled ? 0.5 : 1,
      }}
      onMouseEnter={(e) => {
        if (!disabled) {
          Object.assign(e.currentTarget.style, hoverStyles[variant]);
        }
      }}
      onMouseLeave={(e) => {
        if (!disabled) {
          Object.assign(e.currentTarget.style, variantStyles[variant]);
        }
      }}
    >
      {children}
    </button>
  );
};
