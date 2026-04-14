import React, { useState, useEffect } from "react";
import {
  Database,
  TrendingUp,
  Layers,
  ArrowUpRight,
  ArrowDownRight,
  RefreshCw,
  Download,
  Upload,
  Trash2,
  Search,
  Filter,
  GitBranch,
  Clock,
  Zap,
  Brain,
  BarChart3,
  Sparkles,
  ChevronDown,
  ChevronRight,
  AlertTriangle,
  CheckCircle2,
} from "lucide-react";
import sdk from "../services/agentos-sdk";
import { useI18n } from "../i18n";
import type { MemoryEntry } from "../services/agentos-sdk";
import { useAlert } from "../components/useAlert";

const layerConfig = [
  { key: "L1", name: "原始卷", desc: "原始数据存储", icon: Database, color: "#6366f1", gradient: "linear-gradient(135deg, #6366f1, #818cf8)" },
  { key: "L2", name: "特征层", desc: "FAISS 向量嵌入索引", icon: Brain, color: "#06b6d4", gradient: "linear-gradient(135deg, #06b6d4, #22d3ee)" },
  { key: "L3", name: "结构层", desc: "知识图谱关系编码", icon: GitBranch, color: "#f59e0b", gradient: "linear-gradient(135deg, #f59e0b, #fbbf24)" },
  { key: "L4", name: "模式层", desc: "持久同调稳定规则挖掘", icon: Sparkles, color: "#10b981", gradient: "linear-gradient(135deg, #10b981, #34d399)" },
];

const typeConfig: Record<string, { label: string; color: string; icon: typeof Database }> = {
  conversation: { label: "对话记忆", color: "var(--primary-color)", icon: Database },
  fact: { label: "事实知识", color: "var(--success-color)", icon: CheckCircle2 },
  skill: { label: "技能经验", color: "var(--warning-color)", icon: Zap },
  preference: { label: "偏好设置", color: "var(--error-color)", icon: Brain },
  error: { label: "错误记录", color: "var(--error-color)", icon: AlertTriangle },
  observation: { label: "观察记录", color: "var(--info-color)", icon: BarChart3 },
};

const MemoryEvolution: React.FC = () => {
  const { t } = useI18n();
  const { error, success, info, confirm: confirmModal } = useAlert();
  const [entries, setEntries] = useState<MemoryEntry[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState("");
  const [filterType, setFilterType] = useState<string>("all");
  const [selectedEntry, setSelectedEntry] = useState<MemoryEntry | null>(null);
  const [evolving, setEvolving] = useState(false);
  const [activeLayer, setActiveLayer] = useState<string | null>(null);

  useEffect(() => { loadEntries(); }, []);

  const loadEntries = async () => {
    setLoading(true);
    try {
      const data = await sdk.memoryList(filterType === "all" ? undefined : filterType as MemoryEntry["type"], 100);
      setEntries(data || []);
    } catch (err) {
      error("加载失败", `无法加载记忆条目: ${err}`);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { if (filterType) loadEntries(); }, [filterType]);

  const handleEvolve = async () => {
    setEvolving(true);
    try {
      const result = await sdk.memoryEvolve();
      if (result && result.evolved > 0) {
        const evolutionDetails = result.layers.map((l: { layer: string; before: number; after: number }) => `${l.layer}: ${l.before} → ${l.after}`).join("\n");
        success("进化完成", `记忆进化完成！共进化 ${result.evolved} 条记忆\n\n${evolutionDetails}`);
        loadEntries();
      } else {
        info("进化中", "正在进行记忆进化，请稍候...");
        await new Promise(r => setTimeout(r, 1500));
        loadEntries();
      }
    } catch (err) {
      error("进化失败", `记忆进化过程出错: ${err}`);
      await new Promise(r => setTimeout(r, 1500));
    } finally {
      setEvolving(false);
    }
  };

  const handleForget = async (id: string) => {
    const confirmed = await confirmModal({
      type: 'danger',
      title: '遗忘记忆',
      message: '确定要遗忘此条记忆吗？此操作无法撤销。',
    });
    if (!confirmed) return;
    try {
      await sdk.memoryDelete(id);
      setEntries(prev => prev.filter(e => e.id !== id));
      if (selectedEntry?.id === id) setSelectedEntry(null);
      success("已遗忘", "记忆条目已被成功移除");
    } catch (err) {
      error("删除失败", `无法删除记忆条目: ${err}`);
    }
  };

  const filteredEntries = entries.filter(e =>
    e.content.toLowerCase().includes(searchTerm.toLowerCase())
  );

  const typeStats = Object.entries(typeConfig).map(([key, cfg]) => ({
    key, ...cfg, count: entries.filter(e => e.type === key).length,
  }));

  return (
    <div className="page-container">
      <div className="page-header">
        <div style={{ display: "flex", alignItems: "center", gap: "14px" }}>
          <div style={{
            width: "44px", height: "44px", borderRadius: "var(--radius-md)",
            background: "var(--primary-gradient)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(0,113,227,0.35), 0 0 0 1px rgba(255,255,255,0.08) inset",
          }}>
            <Database size={20} color="white" />
          </div>
          <div>
            <h1>四层记忆卷载</h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "14px", margin: "2px 0 0 0" }}>
              MemoryEvolution · L1→L4 自动进化管理系统
            </p>
          </div>
        </div>
        <div style={{ display: "flex", gap: "8px" }}>
          <button className="btn btn-secondary" onClick={loadEntries} disabled={loading}>
            <RefreshCw size={16} className={loading ? "spin" : ""} /> 刷新
          </button>
          <button className="btn btn-primary" onClick={handleEvolve} disabled={evolving}>
            {evolving ? <><RefreshCw size={16} className="spin" /> 进化中...</> : <><TrendingUp size={16} /> 触发进化</>}
          </button>
        </div>
      </div>

      {/* Four-Layer Visualization */}
      <div className="card card-elevated" style={{ marginBottom: "20px" }}>
        <h3 className="card-title"><Layers size={18} /> 记忆层级架构</h3>
        <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: "14px" }}>
          {layerConfig.map((layer) => {
            const IconComp = layer.icon;
            const isActive = activeLayer === layer.key;
            const entryCount = entries.length;
            const layerPct = layer.key === "L1" ? Math.min(100, (entryCount / 1500) * 100)
              : layer.key === "L2" ? Math.min(100, (entryCount / 1200) * 100)
              : layer.key === "L3" ? Math.min(100, (entryCount / 600) * 100)
              : Math.min(100, (entryCount / 200) * 100);
            return (
              <div
                key={layer.key}
                onClick={() => setActiveLayer(isActive ? null : layer.key)}
                style={{
                  padding: "18px", borderRadius: "var(--radius-lg)",
                  border: `2px solid ${isActive ? layer.color : "var(--border-subtle)"}`,
                  background: isActive ? `${layer.color}08` : "var(--bg-secondary)",
                  cursor: "pointer", transition: "all var(--transition-fast)",
                  position: "relative", overflow: "hidden",
                }}
              >
                {isActive && <div style={{ position: "absolute", left: 0, top: 0, bottom: 0, width: "3px", background: layer.gradient }} />}
                <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "12px" }}>
                  <div style={{ width: "36px", height: "36px", borderRadius: "var(--radius-sm)", background: isActive ? layer.gradient : "var(--bg-tertiary)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                    <IconComp size={17} color={isActive ? "white" : "var(--text-muted)"} />
                  </div>
                  <div>
                    <div style={{ fontSize: "14px", fontWeight: 700, color: isActive ? layer.color : "var(--text-primary)" }}>{layer.name}</div>
                    <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>{layer.desc}</div>
                  </div>
                </div>
                <div style={{ height: "6px", background: "var(--bg-tertiary)", borderRadius: "3px", overflow: "hidden", marginBottom: "8px" }}>
                  <div style={{ width: `${layerPct}%`, height: "100%", background: `linear-gradient(90deg, ${layer.color}, ${layer.color}88)`, borderRadius: "3px", transition: "width 0.6s ease" }} />
                </div>
                <div style={{ display: "flex", justifyContent: "space-between", fontSize: "11.5px" }}>
                  <span style={{ color: "var(--text-muted)" }}>容量</span>
                  <span style={{ fontWeight: 600, color: layer.color }}>{Math.round(layerPct)}%</span>
                </div>
              </div>
            );
          })}
        </div>

        <div style={{ marginTop: "16px", paddingTop: "16px", borderTop: "1px solid var(--border-subtle)", display: "flex", gap: "8px", flexWrap: "wrap" }}>
          {["FAISS 向量检索", "艾宾浩斯遗忘曲线", "持久同调 Ripser", "自动进化触发"].map((tag, i) => (
            <span key={i} style={{ fontSize: "11px", padding: "4px 10px", background: "var(--primary-light)", color: "var(--primary-color)", borderRadius: "var(--radius-full)", fontWeight: 600 }}>{tag}</span>
          ))}
        </div>
      </div>

      {/* Type Stats + Controls */}
      <div style={{ display: "grid", gridTemplateColumns: "280px 1fr", gap: "20px", marginBottom: "20px" }}>
        {/* Type Distribution */}
        <div className="card card-elevated">
          <h3 className="card-title"><BarChart3 size={18} /> 类型分布</h3>
          <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
            {typeStats.map((stat) => {
              const StatIcon = stat.icon;
              const maxCount = Math.max(...typeStats.map(s => s.count), 1);
              const pct = (stat.count / maxCount) * 100;
              return (
                <div key={stat.key}
                  onClick={() => setFilterType(filterType === stat.key ? "all" : stat.key)}
                  style={{
                    padding: "10px 12px", borderRadius: "var(--radius-md)",
                    border: `1px solid ${filterType === stat.key ? stat.color : "var(--border-subtle)"}`,
                    background: filterType === stat.key ? `${stat.color}08` : "transparent",
                    cursor: "pointer", transition: "all var(--transition-fast)",
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "6px" }}>
                    <StatIcon size={13} color={stat.color} />
                    <span style={{ fontSize: "12.5px", fontWeight: 600, flex: 1 }}>{stat.label}</span>
                    <span style={{ fontSize: "14px", fontWeight: 700, color: stat.color }}>{stat.count}</span>
                  </div>
                  <div style={{ height: "4px", background: "var(--bg-tertiary)", borderRadius: "2px", overflow: "hidden" }}>
                    <div style={{ width: `${pct}%`, height: "100%", background: stat.color, borderRadius: "2px", transition: "width 0.4s ease" }} />
                  </div>
                </div>
              );
            })}
          </div>
        </div>

        {/* Entry List */}
        <div className="card card-elevated">
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "16px" }}>
            <h3 className="card-title" style={{ marginBottom: 0 }}><Database size={18} /> 记忆条目</h3>
            <div style={{ display: "flex", gap: "8px", alignItems: "center" }}>
              <div style={{ position: "relative" }}>
                <Search size={14} style={{ position: "absolute", left: "10px", top: "50%", transform: "translateY(-50%)", color: "var(--text-muted)" }} />
                <input
                  type="text" className="form-input"
                  placeholder="搜索记忆内容..."
                  value={searchTerm}
                  onChange={(e) => setSearchTerm(e.target.value)}
                  style={{ paddingLeft: "32px", width: "220px", fontSize: "13px" }}
                />
              </div>
            </div>
          </div>

          {loading ? (
            <div style={{ textAlign: "center", padding: "40px" }}><div className="loading-spinner" /></div>
          ) : filteredEntries.length === 0 ? (
            <div className="empty-state">
              <Database size={48} style={{ opacity: 0.2 }} />
              <div className="empty-state-text">暂无记忆条目</div>
              <div className="empty-state-hint">智能体的交互记忆将在此处显示</div>
            </div>
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: "8px", maxHeight: "420px", overflowY: "auto" }}>
              {filteredEntries.map((entry, idx) => {
                const tc = typeConfig[entry.type] || { label: entry.type, color: "var(--text-muted)", icon: Database };
                const TcIcon = tc.icon;
                const isSelected = selectedEntry?.id === entry.id;
                return (
                  <div
                    key={entry.id}
                    onClick={() => setSelectedEntry(isSelected ? null : entry)}
                    style={{
                      padding: "12px 14px", borderRadius: "var(--radius-md)",
                      border: `1px solid ${isSelected ? tc.color : "var(--border-subtle)"}`,
                      background: isSelected ? `${tc.color}06` : "var(--bg-tertiary)",
                      cursor: "pointer", transition: "all var(--transition-fast)",
                      display: "flex", gap: "12px", alignItems: "flex-start",
                      animation: `staggerFadeIn 0.3s ease-out ${idx * 40}ms both`,
                    }}
                  >
                    <TcIcon size={15} color={tc.color} style={{ flexShrink: 0, marginTop: "2px" }} />
                    <div style={{ flex: 1, minWidth: 0 }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "4px" }}>
                        <span className="tag" style={{ fontSize: "10px", background: `${tc.color}15`, color: tc.color }}>{tc.label}</span>
                        <span style={{ fontSize: "11px", color: "var(--text-muted)" }}>
                          <Clock size={10} style={{ display: "inline", marginRight: "3px" }} />
                          {new Date(entry.created_at).toLocaleString('zh-CN')}
                        </span>
                      </div>
                      <p style={{ fontSize: "13px", color: "var(--text-primary)", lineHeight: 1.5, margin: 0, display: "-webkit-box", WebkitLineClamp: 2, WebkitBoxOrient: "vertical", overflow: "hidden" }}>
                        {entry.content}
                      </p>
                      {entry.tokens > 0 && (
                        <span style={{ fontSize: "10.5px", color: "var(--text-muted)", marginTop: "4px", display: "inline-block" }}>
                          {entry.tokens} tokens
                        </span>
                      )}
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </div>
      </div>

      {/* Detail Panel */}
      {selectedEntry && (
        <div className="card card-elevated" style={{ animation: "fadeIn 0.3s ease-out" }}>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "16px" }}>
            <h3 className="card-title" style={{ marginBottom: 0 }}>记忆详情</h3>
            <div style={{ display: "flex", gap: "8px" }}>
              <button className="btn btn-danger btn-sm" onClick={() => handleForget(selectedEntry.id)}>
                <Trash2 size={14} /> 遗忘
              </button>
              <button className="btn btn-ghost btn-sm" onClick={() => setSelectedEntry(null)}>✕ 关闭</button>
            </div>
          </div>
          <div style={{ display: "grid", gridTemplateColumns: "1fr 300px", gap: "20px" }}>
            <div style={{ padding: "16px", background: "var(--bg-tertiary)", borderRadius: "var(--radius-md)", fontSize: "14px", lineHeight: 1.8, whiteSpace: "pre-wrap", fontFamily: "Inter, sans-serif" }}>
              {selectedEntry.content}
            </div>
            <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
              {[
                { label: "类型", value: typeConfig[selectedEntry.type]?.label || selectedEntry.type },
                { label: "ID", value: selectedEntry.id.slice(0, 12) + "...", mono: true },
                { label: "Tokens", value: String(selectedEntry.tokens) },
                { label: "创建时间", value: new Date(selectedEntry.created_at).toLocaleString('zh-CN') },
                { label: "来源", value: selectedEntry.source || "—" },
                { label: "相关度", value: selectedEntry.relevance ? `${(selectedEntry.relevance * 100).toFixed(1)}%` : "—" },
              ].map((row, i) => (
                <div key={i}>
                  <div style={{ fontSize: "11px", color: "var(--text-muted)", textTransform: "uppercase", letterSpacing: "0.05em", marginBottom: "3px" }}>{row.label}</div>
                  <div style={{ fontSize: "13.5px", fontWeight: 500, fontFamily: row.mono ? "JetBrains Mono, monospace" : "inherit" }}>{row.value}</div>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default MemoryEvolution;
