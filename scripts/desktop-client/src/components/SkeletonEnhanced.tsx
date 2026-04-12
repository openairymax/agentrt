import React from "react";

interface SkeletonProps {
  variant?: 'text' | 'circular' | 'rectangular';
  width?: string | number;
  height?: string | number;
  className?: string;
  style?: React.CSSProperties;
  animation?: 'pulse' | 'wave' | 'none';
}

const Skeleton: React.FC<SkeletonProps> = ({
  variant = 'text',
  width,
  height,
  className = '',
  style = {},
  animation = 'pulse',
}) => {
  const baseStyle: React.CSSProperties = {
    width: width || (variant === 'circular' ? '40px' : '100%'),
    height: height || (variant === 'text' ? '16px' : variant === 'circular' ? '40px' : '120px'),
    borderRadius: variant === 'circular' ? '50%' : 'var(--radius-sm)',
    display: 'inline-block',
    ...style,
  };

  return (
    <div
      className={`skeleton ${animation !== 'none' ? 'skeleton-animate' : ''} ${className}`}
      style={baseStyle}
    />
  );
};

interface SkeletonCardProps {
  showAvatar?: boolean;
  lines?: number;
  showButton?: boolean;
}

export const SkeletonCard: React.FC<SkeletonCardProps> = ({
  showAvatar = true,
  lines = 3,
  showButton = true,
}) => (
  <div style={{
    padding: '20px',
    background: 'var(--bg-secondary)',
    border: '1px solid var(--border-subtle)',
    borderRadius: 'var(--radius-lg)',
  }}>
    <div style={{ display: 'flex', alignItems: 'flex-start', gap: '16px', marginBottom: '16px' }}>
      {showAvatar && <Skeleton variant="circular" width={48} height={48} />}
      <div style={{ flex: 1 }}>
        <Skeleton width="60%" height={18} style={{ marginBottom: '10px' }} />
        <Skeleton width="40%" height={14} />
      </div>
    </div>
    {Array.from({ length: lines }).map((_, i) => (
      <Skeleton
        key={i}
        width={i === lines - 1 ? '50%' : '100%'}
        height={14}
        style={{ marginBottom: i < lines - 1 ? '8px' : 0 }}
      />
    ))}
    {showButton && (
      <div style={{ display: 'flex', gap: '8px', marginTop: '16px' }}>
        <Skeleton width={80} height={32} />
        <Skeleton width={80} height={32} />
      </div>
    )}
  </div>
);

interface SkeletonTableProps {
  rows?: number;
  columns?: number;
  showHeader?: boolean;
}

export const SkeletonTable: React.FC<SkeletonTableProps> = ({
  rows = 5,
  columns = 4,
  showHeader = true,
}) => (
  <div style={{
    background: 'var(--bg-secondary)',
    border: '1px solid var(--border-subtle)',
    borderRadius: 'var(--radius-lg)',
    overflow: 'hidden',
  }}>
    {showHeader && (
      <div style={{
        display: 'grid',
        gridTemplateColumns: `repeat(${columns}, 1fr)`,
        gap: '12px',
        padding: '14px 16px',
        borderBottom: '1px solid var(--border-subtle)',
        background: 'var(--bg-tertiary)',
      }}>
        {Array.from({ length: columns }).map((_, i) => (
          <Skeleton key={i} height={14} width="70%" />
        ))}
      </div>
    )}
    {Array.from({ length: rows }).map((_, rowIndex) => (
      <div
        key={rowIndex}
        style={{
          display: 'grid',
          gridTemplateColumns: `repeat(${columns}, 1fr)`,
          gap: '12px',
          padding: '14px 16px',
          borderBottom: rowIndex < rows - 1 ? '1px solid var(--border-subtle)' : 'none',
        }}
      >
        {Array.from({ length: columns }).map((_, colIndex) => (
          <Skeleton key={colIndex} height={14} width={colIndex === 0 ? '60%' : `${50 + Math.random() * 30}%`} />
        ))}
      </div>
    ))}
  </div>
);

interface SkeletonDashboardProps {}

export const SkeletonDashboard: React.FC<SkeletonDashboardProps> = () => (
  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '16px', marginBottom: '20px' }}>
    {Array.from({ length: 4 }).map((_, i) => (
      <div
        key={i}
        style={{
          padding: '24px 16px',
          textAlign: 'center',
          background: 'var(--bg-secondary)',
          border: '1px solid var(--border-subtle)',
          borderRadius: 'var(--radius-lg)',
        }}
      >
        <Skeleton variant="circular" width={96} height={96} style={{ margin: '0 auto 12px' }} />
        <Skeleton width="40%" height={24} style={{ margin: '0 auto' }} />
        <Skeleton width="30%" height={14} style={{ margin: '8px auto 0' }} />
      </div>
    ))}
  </div>
);

interface SkeletonListProps {
  items?: number;
  showIcon?: boolean;
}

export const SkeletonList: React.FC<SkeletonListProps> = ({
  items = 5,
  showIcon = true,
}) => (
  <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
    {Array.from({ length: items }).map((_, i) => (
      <div
        key={i}
        style={{
          display: 'flex',
          alignItems: 'center',
          gap: '14px',
          padding: '14px 16px',
          background: 'var(--bg-secondary)',
          border: '1px solid var(--border-subtle)',
          borderRadius: 'var(--radius-md)',
        }}
      >
        {showIcon && <Skeleton variant="circular" width={36} height={36} />}
        <div style={{ flex: 1 }}>
          <Skeleton width="45%" height={15} style={{ marginBottom: '6px' }} />
          <Skeleton width="70%" height={13} />
        </div>
        <Skeleton width={60} height={28} />
      </div>
    ))}
  </div>
);

export default Skeleton;
