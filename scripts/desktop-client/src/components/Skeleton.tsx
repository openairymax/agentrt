import React from 'react';

interface SkeletonProps {
  width?: string | number;
  height?: string | number;
  borderRadius?: string;
  className?: string;
  style?: React.CSSProperties;
  animate?: boolean;
}

export const Skeleton: React.FC<SkeletonProps> = ({
  width = '100%',
  height = 16,
  borderRadius = 'var(--radius-sm)',
  className = '',
  style = {},
  animate = true,
}) => (
  <div
    className={`skeleton ${animate ? 'skeleton-animate' : ''} ${className}`}
    style={{
      width: typeof width === 'number' ? `${width}px` : width,
      height: typeof height === 'number' ? `${height}px` : height,
      borderRadius,
      ...style,
    }}
  />
);

const SkeletonCard: React.FC<{ lines?: number }> = ({ lines = 3 }) => (
  <div className="card card-elevated" style={{ padding: '20px' }}>
    <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '16px' }}>
      <Skeleton width={44} height={44} borderRadius="50%" />
      <div style={{ flex: 1 }}>
        <Skeleton width="60%" height={14} style={{ marginBottom: '6px' }} />
        <Skeleton width="40%" height={12} />
      </div>
    </div>
    {Array.from({ length: lines }).map((_, i) => (
      <Skeleton key={i} height={12} style={{ marginBottom: i < lines - 1 ? '10px' : 0 }} width={i === lines - 1 ? '70%' : '100%'} />
    ))}
  </div>
);

const SkeletonStatCard: React.FC = () => (
  <div className="stat-card">
    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
      <div style={{ flex: 1 }}>
        <Skeleton width={80} height={12} style={{ marginBottom: '12px' }} />
        <Skeleton width={48} height={28} borderRadius="var(--radius-md)" />
        <Skeleton width="70%" height={11} style={{ marginTop: '8px' }} />
      </div>
      <Skeleton width={48} height={48} borderRadius="var(--radius-md)" />
    </div>
  </div>
);

const SkeletonTable: React.FC<{ rows?: number; cols?: number }> = ({ rows = 5, cols = 4 }) => (
  <div style={{ overflowX: 'auto' }}>
    <table style={{ width: '100%', borderCollapse: 'collapse' }}>
      <thead>
        <tr>
          {Array.from({ length: cols }).map((_, i) => (
            <th key={i} style={{ padding: '14px 16px', textAlign: 'left' }}>
              <Skeleton width={`${60 + (i * 15)}%`} height={12} />
            </th>
          ))}
        </tr>
      </thead>
      <tbody>
        {Array.from({ length: rows }).map((_, rowIdx) => (
          <tr key={rowIdx}>
            {Array.from({ length: cols }).map((_, colIdx) => (
              <td key={colIdx} style={{ padding: '16px' }}>
                <Skeleton
                  width={colIdx === 0 ? `${50 + (rowIdx % 3) * 15}%` : colIdx === cols - 1 ? '40%' : '65%'}
                  height={13}
                />
              </td>
            ))}
          </tr>
        ))}
      </tbody>
    </table>
  </div>
);

const SkeletonText: React.FC<{ lines?: number }> = ({ lines = 4 }) => (
  <div>
    {Array.from({ length: lines }).map((_, i) => (
      <Skeleton
        key={i}
        height={14}
        width={i === lines - 1 ? '60%' : i === 0 ? '90%' : '100%'}
        style={{ marginBottom: i < lines - 1 ? '10px' : 0 }}
      />
    ))}
  </div>
);

export const Skeletons = {
  Card: SkeletonCard,
  StatCard: SkeletonStatCard,
  Table: SkeletonTable,
  Text: SkeletonText,
};

export const PageLoader: React.FC = () => (
  <div className="page-container" style={{ paddingTop: '40px' }}>
    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '16px', marginBottom: '24px' }}>
      <SkeletonStatCard />
      <SkeletonStatCard />
      <SkeletonStatCard />
      <SkeletonStatCard />
    </div>

    <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '16px', marginBottom: '24px' }}>
      <SkeletonCard lines={4} />
      <SkeletonCard lines={4} />
    </div>

    <SkeletonCard lines={5} />
  </div>
);
