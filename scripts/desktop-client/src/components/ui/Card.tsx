import React from 'react';

interface CardProps {
  children: React.ReactNode;
  className?: string;
  style?: React.CSSProperties;
  padding?: number;
}

export const Card: React.FC<CardProps> = ({
  children,
  className = '',
  style = {},
  padding = 16,
}) => {
  return (
    <div
      className={className}
      style={{
        background: 'var(--bg-secondary)',
        border: '1px solid var(--border-subtle)',
        borderRadius: '8px',
        padding: `${padding}px`,
        boxShadow: 'var(--shadow-sm)',
        transition: 'all 0.2s ease',
        ...style,
      }}
    >
      {children}
    </div>
  );
};
