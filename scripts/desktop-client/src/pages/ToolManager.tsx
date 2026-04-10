import React, { useState, useEffect } from "react";
import {
  Wrench,
  Plus,
  Search,
  Play,
  Square,
  RefreshCw,
  Loader2,
  Settings2,
  CheckCircle2,
  XCircle,
  AlertTriangle,
  Zap,
  Code2,
  Database,
  Globe,
  Shield,
  Cpu,
  Clock,
  Tag,
  ArrowUpRight,
  Trash2,
  ExternalLink,
  Eye,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import { useI18n } from "../i18n";

interface ToolInfo {
  name: string;
  description: string;
  category: string;
  schema: Record<string, unknown>;
  status: "registered" | "running" | "error" | "idle";
  call_count?: number;
  avg_latency_ms?: number;
  last_called?: string;
}

const categoryConfig: Record<string, { icon: typeof Wrench; color: string; gradient: string; label: string }> = {
  system: { icon: Cpu, color: "#6366f1", gradient: "linear-gradient(135deg, #6366f1, #818cf8)", label: "系统工具" },
  file: { icon: Code2, color: "#22c55e", gradient: "linear-gradient(135deg, #22c55e, #4ade80)", label: "文件操作" },
  network: { icon: Globe, color: "#06b6d4", gradient: "linear-gradient(135deg, #06b6d4, #22d3ee)", label: "网络工具" },
  data: { icon: Database, color: "#f59e0b", gradient: "linear-gradient(135deg, #f59e0b, #fbbf24)", label: "数据处理" },
  security: { icon: Shield, color: "#ef4444", gradient: "linear-gradient(135deg, #ef4444, #f87171)", label: "安全工具" },
  ai: { icon: Zap, color: "#a855f7", gradient: "linear-gradient(135deg, #a855f7, #c084fc)", label: "AI 能力" },
};

const mockTools: ToolInfo[] = [
  { name: "read_file", description: "读取文件内容，支持文本和二进制格式", category: "file", schema: { type: "object", properties: { path: { type: "string" }, encoding: { type: "string" } } }, status: "registered", call_count: 142, avg_latency_ms: 23, last_called: new Date().toISOString() },
  { name: "write_file", description: "写入内容到指定路径的文件", category: "file", schema: { type: "object", properties: { path: { type: "string" }, content: { type: "string" } } }, status: "registered", call_count: 89, avg_latency_ms: 31, last_called: new Date(Date.now() - 3600000).toISOString() },
  { name: "list_directory", description: "列出目录下的所有文件和子目录", category: "file", schema: { type: "object", properties: { path: { type: "string" }, recursive: { type: "boolean" } } }, status: "registered", call_count: 256, avg_latency_ms: 18, last_called: new Date(Date.now() - 7200000).toISOString() },
  { name: "execute_command", description: "在系统中执行 Shell 命令", category: "system", schema: { type: "object", properties: { command: { type: "string" }, args: { type: "array" } } }, status: "registered", call_count: 67, avg_latency_ms: 452, last_called: new Date(Date.now() - 1800000).toISOString() },
  { name: "web_search", description: "通过搜索引擎检索互联网信息", category: "network", schema: { type: "object", properties: { query: { type: "string" }, num_results: { type: "number" } } }, status: "registered", call_count: 203, avg_latency_ms: 1200, last_called: new Date(Date.now() - 900000).toISOString() },
  { name: "memory_store", description: "将信息存储到 AgentOS 四层记忆系统", category: "data", schema: { type: "object", properties: { content: { type: "string" }, type: { type: "string" } } }, status: "running", call_count: 512, avg_latency_ms: 12, last_called: new Date().toISOString() },
  { name: "memory_search", description: "在记忆系统中进行语义搜索", category: "data", schema: { type: "object", properties: { query: { type: "string" }, limit: { type: "number" } } }, status: "running", call_count: 389, avg_latency_ms: 35, last_called: new Date(Date.now() - 300000).toISOString() },
  { name: "code_execute", description: "在沙箱环境中执行代码片段", category: "ai", schema: { type: "object", properties: { code: { type: "string" }, language: { type: "string" } } }, status: "registered", call_count: 45, avg_latency_ms: 2300, last_called: new Date(Date.now() - 5400000).toISOString() },
];

const ToolManager: React.FC = () => {
  const { t } = useI18n();
  const [tools, setTools] = useState<ToolInfo[]>(mockTools);
  const [loading, setLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState("");
  const [filterCategory, setFilterCategory] = useState<string>("all");
  const [selectedTool, setSelectedTool] = useState<ToolInfo | null>(null);
  const [showRegisterModal, setShowRegisterModal] = useState(false);

  useEffect(() => {
    setTimeout(() => setLoading(false), 800);
  }, []);

  const filteredTools = tools.filter(tool => {
    const matchSearch = tool.name.toLowerCase().includes(searchTerm.toLowerCase()) || tool.description.toLowerCase().includes(searchTerm.toLowerCase());
    const matchCat = filterCategory === "all" || tool.category === filterCategory;
    return matchSearch && matchCat;
  });

  const catKeys = Object.keys(categoryConfig);
  const catStats = catKeys.map(cat => ({ cat, ...categoryConfig[cat], count: tools.filter(t => t.category === cat).length }));

  const totalCalls = tools.reduce((sum, t) => sum + (t.call_count || 0), 0);
  const avgLatency = tools.length > 0 ? Math.round(tools.reduce((sum, t) => sum + (t.avg_latency_ms || 0), 0) / tools.length) : 0;

  return (
    <div className="page-container">
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
          <div style={{
            width: "48px", height: "48px", borderRadius: "var(--radius-lg)",
            background: "linear-gradient(135deg, #10b981, #34d399)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 6px 20px rgba(16,185,129,0.3)",
          }}>
            <Wrench size={24} color="white" />
          </div>
          <div>
            <h1>工具管理器</h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "14px", margin: "2px 0 0 0" }}>
              注册、监控和管理智能体可用工具集
            </p>
          </div>
        </div>
        <button className="btn btn-primary" onClick={() => setShowRegisterModal(true)}>
          <Plus size={16} /> 注册新工具
        </button>
      </div>

      {/* Stats Overview */}
      <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: "14px", marginBottom: "20px" }}>
        {[
          { label: "已注册工具", value: String(tools.length), icon: Wrench, color: "#10b981" },
          { label: "总调用次数", value: totalCalls.toLocaleString(), icon: Zap, color: "#6366f1" },
          { label: "平均延迟", value: `${avgLatency}ms`, icon: Clock, color: "#f59e0b" },
          { label: "运行中", value: String(tools.filter(t => t.status === "running").length), icon: Play, color: "#22c55e" },
        ].map((stat, i) => {
          const IconComp = stat.icon;
          return (
            <div key={i} className="card card-elevated" style={{ padding: "16px" }}>
              <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "8px" }}>
                <div style={{ width: "32px", height: "32px", borderRadius: "var(--radius-sm)", background: `${stat.color}12`, display: "flex", alignItems: "center", justifyContent: "center" }}>
                  <IconComp size={15} color={stat.color} />
                </div>
                <span style={{ fontSize: "12px", color: "var(--text-muted)" }}>{stat.label}</span>
              </div>
              <div style={{ fontSize: "22px", fontWeight: 700, color: stat.color }}>{stat.value}</div>
            </div>
          );
        })}
      </div>

      <div style={{ display: "grid", gridTemplateColumns: selectedTool ? "1fr 360px" : "1fr", gap: "20px", transition: "grid-template-columns 0.35s ease-out" }}>
        {/* Main Content */}
        <div>
          {/* Category Tabs + Search */}
          <div className="card card-elevated" style={{ marginBottom: "16px" }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", gap: "16px", flexWrap: "wrap" }}>
              <div style={{ display: "flex", gap: "6px", flexWrap: "wrap" }}>
                <button
                  onClick={() => setFilterCategory("all")}
                  style={{ padding: "6px 14px", borderRadius: "20px", border: "1px solid", borderColor: filterCategory === "all" ? "var(--primary-color)" : "var(--border-subtle)", background: filterCategory === "all" ? "var(--primary-color)" : "transparent", color: filterCategory === "all" ? "white" : "var(--text-secondary)", fontSize: "12px", fontWeight: 500, cursor: "pointer", transition: "all var(--transition-fast)" }}
                >全部 ({tools.length})</button>
                {catStats.map(cs => (
                  <button
                    key={cs.cat}
                    onClick={() => setFilterCategory(cs.cat)}
                    style={{ padding: "6px 14px", borderRadius: "20px", border: "1px solid", borderColor: filterCategory === cs.cat ? cs.color : "var(--border-subtle)", background: filterCategory === cs.cat ? cs.color : "transparent", color: filterCategory === cs.cat ? "white" : "var(--text-secondary)", fontSize: "12px", fontWeight: 500, cursor: "pointer", transition: "all var(--transition-fast)", display: "flex", alignItems: "center", gap: "4px" }}
                  >
                    <cs.icon size={12} />{cs.label} ({cs.count})
                  </button>
                ))}
              </div>
              <div style={{ position: "relative" }}>
                <Search size={14} style={{ position: "absolute", left: "10px", top: "50%", transform: "translateY(-50%)", color: "var(--text-muted)" }} />
                <input type="text" className="form-input" placeholder="搜索工具..." value={searchTerm} onChange={(e) => setSearchTerm(e.target.value)} style={{ paddingLeft: "32px", width: "200px", fontSize: "13px" }} />
              </div>
            </div>
          </div>

          {/* Tool Cards */}
          {loading ? (
            <div style={{ textAlign: "center", padding: "48px" }}><div className="loading-spinner" /></div>
          ) : filteredTools.length === 0 ? (
            <div className="empty-state">
              <Wrench size={56} style={{ opacity: 0.25 }} />
              <div className="empty-state-text">未找到匹配的工具</div>
              <div className="empty-state-hint">尝试更换筛选条件或注册新工具</div>
            </div>
          ) : (
            <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(340px, 1fr))", gap: "12px" }}>
              {filteredTools.map((tool, idx) => {
                const catCfg = categoryConfig[tool.category] || categoryConfig.system;
                const CatIcon = catCfg.icon;
                const isSelected = selectedTool?.name === tool.name;
                return (
                  <div
                    key={tool.name}
                    onClick={() => setSelectedTool(isSelected ? null : tool)}
                    className="card-hover-lift"
                    style={{
                      padding: "18px", borderRadius: "var(--radius-lg)",
                      border: `2px solid ${isSelected ? catCfg.color : "var(--border-subtle)"}`,
                      background: isSelected ? `${catCfg.color}06` : "var(--bg-secondary)",
                      cursor: "pointer", position: "relative", overflow: "hidden",
                      animation: `staggerFadeIn 0.35s ease-out ${idx * 50}ms both`,
                    }}
                  >
                    <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "3px", background: catCfg.gradient, opacity: isSelected ? 1 : 0.5 }} />
                    <div style={{ display: "flex", alignItems: "flex-start", gap: "12px" }}>
                      <div style={{ width: "40px", height: "40px", borderRadius: "var(--radius-md)", background: catCfg.gradient, display: "flex", alignItems: "center", justifyContent: "center", flexShrink: 0, boxShadow: `0 4px 12px ${catCfg.color}30` }}>
                        <CatIcon size={18} color="white" />
                      </div>
                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "4px" }}>
                          <span style={{ fontWeight: 700, fontSize: "14px", fontFamily: "JetBrains Mono, monospace" }}>{tool.name}</span>
                          <span style={{
                            width: "7px", height: "7px", borderRadius: "50%",
                            background: tool.status === "running" ? "#22c55e" : tool.status === "registered" ? "#6366f1" : "#ef4444",
                            boxShadow: tool.status === "running" ? "0 0 6px rgba(34,197,94,0.5)" : "none",
                            animation: tool.status === "running" ? "statusPulse 2s infinite" : "none",
                          }} />
                        </div>
                        <p style={{ fontSize: "12.5px", color: "var(--text-secondary)", lineHeight: 1.5, margin: 0, display: "-webkit-box", WebkitLineClamp: 2, WebkitBoxOrient: "vertical", overflow: "hidden" }}>
                          {tool.description}
                        </p>
                      </div>
                    </div>
                    <div style={{ marginTop: "12px", paddingTop: "10px", borderTop: "1px solid var(--border-subtle)", display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                      <div style={{ display: "flex", gap: "12px", fontSize: "11.5px", color: "var(--text-muted)" }}>
                        <span><Zap size={10} style={{ display: "inline", marginRight: "2px" }} />{(tool.call_count || 0).toLocaleString()} 次</span>
                        <span><Clock size={10} style={{ display: "inline", marginRight: "2px" }} />{tool.avg_latency_ms || 0}ms</span>
                      </div>
                      <span className="tag" style={{ fontSize: "10px", background: `${catCfg.color}12`, color: catCfg.color }}>{catCfg.label}</span>
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </div>

        {/* Detail Panel */}
        {selectedTool && (
          <div className="card card-elevated" style={{ height: "fit-content", position: "sticky", top: "88px", animation: "slideInRight 0.3s ease-out" }}>
            {(() => {
              const catCfg = categoryConfig[selectedTool.category] || categoryConfig.system;
              const CatIcon = catCfg.icon;
              return (
                <>
                  <div style={{ padding: "18px", borderBottom: "1px solid var(--border-subtle)", background: `${catCfg.color}06`, borderRadius: "var(--radius-lg) var(--radius-lg) 0 0" }}>
                    <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
                      <div style={{ width: "44px", height: "44px", borderRadius: "var(--radius-md)", background: catCfg.gradient, display: "flex", alignItems: "center", justifyContent: "center", boxShadow: `0 4px 14px ${catCfg.color}30` }}>
                        <CatIcon size="22" color="white" />
                      </div>
                      <div>
                        <div style={{ fontSize: "16px", fontWeight: 700, fontFamily: "JetBrains Mono, monospace" }}>{selectedTool.name}</div>
                        <span className="tag" style={{ background: `${catCfg.color}12`, color: catCfg.color, fontSize: "11px" }}>{catCfg.label}</span>
                      </div>
                    </div>
                  </div>
                  <div style={{ padding: "18px", display: "flex", flexDirection: "column", gap: "14px" }}>
                    <div><div style={{ fontSize: "11px", color: "var(--text-muted)", textTransform: "uppercase", letterSpacing: "0.05em", marginBottom: "4px" }}>描述</div><div style={{ fontSize: "13.5px", lineHeight: 1.6 }}>{selectedTool.description}</div></div>
                    <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "10px" }}>
                      <div style={{ padding: "10px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}><div style={{ fontSize: "10px", color: "var(--text-muted)", marginBottom: "3px" }}>状态</div><div style={{ fontSize: "13px", fontWeight: 600, color: selectedTool.status === "running" ? "#22c55e" : "#6366f1" }}>{selectedTool.status === "running" ? "● 运行中" : "○ 已注册"}</div></div>
                      <div style={{ padding: "10px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}><div style={{ fontSize: "10px", color: "var(--text-muted)", marginBottom: "3px" }}>调用次数</div><div style={{ fontSize: "13px", fontWeight: 600 }}>{(selectedTool.call_count || 0).toLocaleString()}</div></div>
                      <div style={{ padding: "10px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}><div style={{ fontSize: "10px", color: "var(--text-muted)", marginBottom: "3px" }}>平均延迟</div><div style={{ fontSize: "13px", fontWeight: 600 }}>{selectedTool.avg_latency_ms || 0} ms</div></div>
                      <div style={{ padding: "10px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-sm)" }}><div style={{ fontSize: "10px", color: "var(--text-muted)", marginBottom: "3px" }}>分类</div><div style={{ fontSize: "13px", fontWeight: 600 }}>{catCfg.label}</div></div>
                    </div>
                    <div>
                      <div style={{ fontSize: "11px", color: "var(--text-muted)", textTransform: "uppercase", letterSpacing: "0.05em", marginBottom: "6px" }}>参数 Schema</div>
                      <pre style={{ padding: "12px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)", fontSize: "11.5px", fontFamily: "JetBrains Mono, monospace", color: "var(--text-secondary)", overflow: "auto", maxHeight: "160px", margin: 0, lineHeight: 1.6 }}>
                        {JSON.stringify(selectedTool.schema, null, 2)}
                      </pre>
                    </div>
                    <div style={{ display: "flex", gap: "8px", marginTop: "4px" }}>
                      <button className="btn btn-primary btn-block" style={{ flex: 1 }}><Play size={14} /> 测试执行</button>
                      <button className="btn btn-ghost btn-block"><Eye size={14} /> 查看日志</button>
                    </div>
                  </div>
                </>
              );
            })()}
          </div>
        )}
      </div>

      {/* Register Modal */}
      {showRegisterModal && (
        <div className="modal-overlay" onClick={() => setShowRegisterModal(false)}>
          <div className="modal-content" style={{ maxWidth: "560px" }} onClick={(e) => e.stopPropagation()}>
            <div className="modal-header">
              <h2 className="modal-title"><Plus size={18} /> 注册新工具</h2>
              <button className="modal-close-btn" onClick={() => setShowRegisterModal(false)}>×</button>
            </div>
            <div style={{ padding: "28px", display: "flex", flexDirection: "column", gap: "18px" }}>
              <div className="form-group">
                <label className="form-label">工具名称</label>
                <input type="text" className="form-input" placeholder="例如：my_custom_tool" style={{ fontFamily: "JetBrains Mono, monospace" }} />
              </div>
              <div className="form-group">
                <label className="form-label">描述</label>
                <textarea className="textarea-field" rows={3} placeholder="简要描述工具的功能和用途..." />
              </div>
              <div className="form-group">
                <label className="form-label">所属分类</label>
                <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: "8px" }}>
                  {catKeys.map(cat => {
                    const cfg = categoryConfig[cat];
                    const CfgIcon = cfg.icon;
                    return (
                      <div key={cat} style={{ padding: "12px", borderRadius: "var(--radius-md)", border: "1px solid var(--border-subtle)", textAlign: "center", cursor: "pointer", transition: "all var(--transition-fast)" }}
                        onMouseEnter={(e) => { e.currentTarget.style.borderColor = cfg.color; e.currentTarget.style.background = `${cfg.color}08`; }}
                        onMouseLeave={(e) => { e.currentTarget.style.borderColor = ""; e.currentTarget.style.background = ""; }}
                      >
                        <CfgIcon size={18} color={cfg.color} style={{ marginBottom: "4px" }} />
                        <div style={{ fontSize: "12px", fontWeight: 600 }}>{cfg.label}</div>
                      </div>
                    );
                  })}
                </div>
              </div>
              <div className="form-group">
                <label className="form-label">参数 Schema (JSON)</label>
                <textarea className="textarea-field" rows={5} placeholder='{"type": "object", "properties": {...}}' style={{ fontFamily: "JetBrains Mono, monospace", fontSize: "12px" }} />
              </div>
              <div style={{ display: "flex", gap: "10px" }}>
                <button className="btn btn-secondary btn-lg" onClick={() => setShowRegisterModal(false)} style={{ flex: 1 }}>取消</button>
                <button className="btn btn-success btn-lg" style={{ flex: 1 }}><CheckCircle2 size={16} /> 注册工具</button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default ToolManager;
