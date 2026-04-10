import React, { Component, ErrorInfo, ReactNode } from 'react';
import { AlertTriangle, RefreshCw, Home, Bug, Copy } from 'lucide-react';

interface Props {
  children: ReactNode;
  fallback?: ReactNode;
}

interface State {
  hasError: boolean;
  error: Error | null;
  errorInfo: ErrorInfo | null;
}

class ErrorBoundary extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = { hasError: false, error: null, errorInfo: null };
  }

  static getDerivedStateFromError(error: Error): Partial<State> {
    return { hasError: true, error };
  }

  componentDidCatch(error: Error, errorInfo: ErrorInfo) {
    console.error('[AgentOS ErrorBoundary]', error, errorInfo);
    this.setState({ errorInfo });
  }

  handleReset = () => {
    this.setState({ hasError: false, error: null, errorInfo: null });
  };

  handleCopyError = () => {
    if (this.state.error) {
      const text = `${this.state.error.message}\n\n${this.state.errorInfo?.componentStack || ''}`;
      navigator.clipboard?.writeText(text);
    }
  };

  render() {
    if (this.state.hasError) {
      if (this.props.fallback) return this.props.fallback;

      return (
        <div style={{
          display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center",
          minHeight: "400px", padding: "40px 20px",
        }}>
          <div style={{
            width: "80px", height: "80px", borderRadius: "50%",
            background: "linear-gradient(135deg, #ef4444, #f87171)",
            display: "flex", alignItems: "center", justifyContent: "center",
            marginBottom: "24px", boxShadow: "0 8px 32px rgba(239,68,68,0.25)",
          }}>
            <AlertTriangle size="36" color="white" />
          </div>

          <h2 style={{ margin: "0 0 12px 0", fontSize: "22px", fontWeight: 700 }}>页面渲染出错</h2>
          <p style={{ margin: "0 0 24px 0", color: "var(--text-secondary)", fontSize: "14px", textAlign: "center", maxWidth: "480px" }}>
            AgentOS 客户端在渲染此组件时遇到了错误。这可能是由于数据格式异常或内部状态不一致导致的。
          </p>

          {this.state.error && (
            <div style={{
              width: "100%", maxWidth: "560px", background: "var(--bg-tertiary)",
              borderRadius: "var(--radius-md)", border: "1px solid var(--border-subtle)",
              marginBottom: "24px", overflow: "hidden",
            }}>
              <div style={{
                display: "flex", alignItems: "center", justifyContent: "space-between",
                padding: "10px 16px", background: "rgba(239,68,68,0.06)",
                borderBottom: "1px solid var(--border-subtle)",
              }}>
                <span style={{ fontWeight: 600, fontSize: "13px", color: "#ef4444" }}>
                  <Bug size={14} style={{ marginRight: "6px", verticalAlign: "middle" }} />
                  错误信息
                </span>
                <button onClick={this.handleCopyError} style={{
                  padding: "3px 10px", border: "none", borderRadius: "var(--radius-sm)",
                  background: "transparent", cursor: "pointer", color: "var(--text-muted)",
                  fontSize: "11.5px", display: "flex", alignItems: "center", gap: "4px",
                }}>
                  <Copy size={12} /> 复制
                </button>
              </div>
              <pre style={{
                margin: 0, padding: "16px", fontFamily: "'JetBrains Mono', monospace",
                fontSize: "12px", lineHeight: 1.6, color: "#ef4444",
                overflow: "auto", maxHeight: "200px", whiteSpace: "pre-wrap",
                wordBreak: "break-word",
              }}>{this.state.error.message}</pre>
              {this.state.errorInfo && (
                <pre style={{
                  margin: 0, padding: "12px 16px", fontFamily: "'JetBrains Mono', monospace",
                  fontSize: "11px", lineHeight: 1.5, color: "var(--text-muted)",
                  overflow: "auto", maxHeight: "150px", whiteSpace: "pre-wrap",
                  borderTop: "1px solid var(--border-subtle)",
                }}>{this.state.errorInfo.componentStack}</pre>
              )}
            </div>
          )}

          <div style={{ display: "flex", gap: "10px" }}>
            <button
              onClick={this.handleReset}
              className="btn btn-primary btn-lg"
            >
              <RefreshCw size={16} /> 重试加载
            </button>
            <button
              onClick={() => window.location.href = '/'}
              className="btn btn-secondary btn-lg"
            >
              <Home size={16} /> 返回首页
            </button>
          </div>

          <p style={{ marginTop: "20px", fontSize: "11.5px", color: "var(--text-muted)" }}>
            如果问题持续存在，请在 GitHub 提交 issue 或联系技术支持
          </p>
        </div>
      );
    }

    return this.props.children;
  }
}

export default ErrorBoundary;