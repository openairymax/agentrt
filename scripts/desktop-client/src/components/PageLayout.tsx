import React from 'react';

interface PageLayoutProps {
  title: string;
  subtitle?: string;
  children: React.ReactNode;
  actions?: React.ReactNode;
}

export const PageLayout: React.FC<PageLayoutProps> = ({
  title,
  subtitle,
  children,
  actions,
}) => {
  return (
    <div className="page-container">
      {/* Page Header */}
      <div className="page-header">
        <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
          <h1>{title}</h1>
          {subtitle && (
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: 0 }}>
              {subtitle}
            </p>
          )}
        </div>
        {actions && (
          <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
            {actions}
          </div>
        )}
      </div>

      {/* Main Content */}
      <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
        {children}
      </div>
    </div>
  );
};
