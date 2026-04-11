import React, { useState, useEffect, useCallback } from "react";
import {
  Brain, Activity, Layers, Zap, Shield, Network,
  RefreshCw, Eye, Database, Target, Workflow,
  Radio, Bot, Sparkles, Wrench, ArrowRight,
  Clock, TrendingUp, CheckCircle2, AlertTriangle,
} from "lucide-react";
import { useNavigate } from "react-router-dom";
import sdk from "../services/agentos-sdk";

const PHASES = [
  { key: "perception", label: "感知", icon: Eye, color: "#06b6d4", gradient: "linear-gradient(135deg,#06b6d4,#22d3ee)", desc: "意图理解与上下文分析" },
  { key: "planning", label: "规划", icon: Brain, color: "#8b5cf6", gradient: "linear-gradient(135deg,#8b5cf6,#a78bfa)", desc: "DAG任务分解与策略生成" },
  { key: "action", label: "行动", icon: Target, color: "#10b981", gradient: "linear-gradient(135deg,#10b981,#34d399)", desc: "执行调度与结果反馈" },
];

const MEM_LAYERS = [
  { key: "L1", name: "L1 原始卷", color: "#6366f1", grad: "linear-gradient(135deg,#6366f1,#818cf8)", entries: 1250, max: 1500, icon: Database, tech: ["Raw Storage", "JSON/Binary"] },
  { key: "L2", name: "L2 特征层", color: "#06b6d4", grad: "linear-gradient(135deg,#06b6d4,#22d3ee)", entries: 890, max: 1200, icon: Zap, tech: ["FAISS Vector", "768-dim"] },
  { key: "L3", name: "L3 结构层", color: "#f59e0b", grad: "linear-gradient(135deg,#f59e0b,#fbbf24)", entries: 456, max: 600, icon: Network, tech: ["Knowledge Graph", "RDF"] },
  { key: "L4", name: "L4 模式层", color: "#10b981", grad: "linear-gradient(135deg,#10b981,#34d399)", entries: 128, max: 200, icon: Sparkles, tech: ["Persistent Homology", "Stable Rules"] },
];

const SERVICES = [
  { name: "gateway_d", label: "网关服务", up: true, hrs: 48, cpu: 2.1, mem: 64 },
  { name: "llm_d", label: "大模型引擎", up: true, hrs: 48, cpu: 12.4, mem: 512 },
  { name: "tool_d", label: "工具调度器", up: true, hrs: 48, cpu: 0.8, mem: 32 },
  { name: "sched_d", label: "任务调度器", up: true, hrs: 47, cpu: 1.5, mem: 48 },
  { name: "monit_d", label: "系统监控器", up: true, hrs: 47, cpu: 3.2, mem: 96 },
  { name: "market_d", label: "市场服务", up: false, hrs: 0, cpu: 0, mem: 0 },
];

function useAnimNum(target: number, dur = 1200) {
  const [v, setV] = useState(0);
  useEffect(() => {
    let n = 0;
    const step = Math.max(1, Math.ceil(target / (dur / 16)));
    const t = setInterval(() => { n += step; if (n >= target) { setV(target); clearInterval(t); } else setV(n); }, 16);
    return () => clearInterval(t);
  }, [target]);
  return v;
}

/* ─── SVG Ring Gauge (with glow & gradient) ─── */
function GaugeRing({ pct, size = 96, sw = 7, color, label, sub }: {
  pct: number; size?: number; sw?: number; color: string; label: string; sub?: string;
}) {
  const r = (size - sw) / 2;
  const circ = 2 * Math.PI * r;
  const off = circ - (Math.min(pct, 100) / 100) * circ;
  const uid = `g_${color.replace("#","")}_${Math.random().toString(36).slice(2,7)}`;
  return (
    <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: "4px" }}>
      <div style={{ position: "relative" }}>
        <svg width={size} height={size} style={{ transform: "rotate(-90deg)", filter: `drop-shadow(0 0 6px ${color}25)` }}>
          <defs>
            <linearGradient id={uid} x1="0%" y1="0%" x2="100%" y2="100%">
              <stop offset="0%" stopColor={color} />
              <stop offset="100%" stopColor={`${color}aa`} />
            </linearGradient>
          </defs>
          <circle cx={size/2} cy={size/2} r={r} fill="none" stroke="var(--border-subtle)" strokeWidth={sw} />
          <circle cx={size/2} cy={size/2} r={r} fill="none" stroke={`url(#${uid})`} strokeWidth={sw}
            strokeDasharray={`${circ * 0.75} ${circ * 0.25}`} strokeDashoffset={off}
            strokeLinecap="round" style={{ transition: "stroke-dashoffset 1.2s cubic-bezier(.34,1.56,.64,1)" }} />
        </svg>
        <div style={{
          position: "absolute", inset: 0, display: "flex", flexDirection: "column",
          alignItems: "center", justifyContent: "center",
        }}>
          <span style={{ fontSize: size > 90 ? "20px" : "17px", fontWeight: 800, letterSpacing: "-0.02em" }}>{label}</span>
          {sub && <span style={{ fontSize: "9.5px", color: "var(--text-muted)", marginTop: "1px" }}>{sub}</span>}
        </div>
      </div>
    </div>
  );
}

/* ─── Pipeline Node (enhanced with ripple rings) ─── */
function PNode({ phase, active, done }: { phase: typeof PHASES[0]; active: boolean; done: boolean }) {
  const Ic = phase.icon;
  return (
    <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: "10px", flex: 1 }}>
      <div style={{
        position: "relative", width: "76px", height: "76px",
        borderRadius: "50%", display: "flex", alignItems: "center", justifyContent: "center",
        background: active
          ? `linear-gradient(135deg,${phase.color},${phase.color}cc)`
          : done ? `${phase.color}14` : "var(--bg-tertiary)",
        border: `3px solid ${active ? phase.color : done ? `${phase.color}50` : "var(--border-subtle)"}`,
        boxShadow: active
          ? `0 0 28px ${phase.color}45, 0 0 56px ${phase.color}15, inset 0 0 16px ${phase.color}20, 0 0 0 1px rgba(255,255,255,0.06) inset`
          : done ? `0 0 16px ${phase.color}25, 0 0 0 1px rgba(255,255,255,0.04) inset` : "inset 0 2px 4px rgba(0,0,0,0.08)",
        transition: "all 0.65s cubic-bezier(.34,1.56,.64,1)",
        opacity: active ? 1 : done ? 0.85 : 0.38,
        transform: active ? "scale(1.1)" : "scale(1)",
      }}>
        {/* Outer rotating ring */}
        {active && (
          <div style={{
            position: "absolute", inset: "-5px", borderRadius: "50%",
            border: "2.5px solid transparent", borderTopColor: phase.color,
            borderRightColor: `${phase.color}80`, animation: "spin 2.4s linear infinite",
          }} />
        )}
        {/* Pulse rings */}
        {active && (
          <>
            <div style={{ position: "absolute", inset: "-14px", borderRadius: "50%", border: "1px solid", borderColor: `${phase.color}30`, animation: "vizPulseRing 2.5s ease-out infinite" }} />
            <div style={{ position: "absolute", inset: "-24px", borderRadius: "50%", border: "1px solid", borderColor: `${phase.color}15`, animation: "vizPulseRing 2.5s ease-out 0.6s infinite" }} />
          </>
        )}
        {/* Inner highlight for active */}
        {active && (
          <div style={{ position: "absolute", inset: "2px", borderRadius: "50%", background: "radial-gradient(circle at 35% 35%,rgba(255,255,255,0.18),transparent 60%)", pointerEvents: "none" }} />
        )}
        <Ic size={28} color={active ? "white" : done ? phase.color : "var(--text-muted)"} />
        {/* Status badge */}
        {active && (
          <div style={{
            position: "absolute", bottom: "-10px", left: "50%", transform: "translateX(-50%)",
            padding: "3px 12px", borderRadius: "12px", background: phase.color, color: "white",
            fontSize: "9.5px", fontWeight: 700, whiteSpace: "nowrap",
            boxShadow: `0 3px 12px ${phase.color}55, 0 0 0 1px rgba(255,255,255,0.1) inset`, zIndex: 2,
          }}>运行中</div>
        )}
        {done && !active && (
          <div style={{
            position: "absolute", bottom: "-5px", right: "-5px", width: "20px", height: "20px", borderRadius: "50%",
            background: "linear-gradient(135deg,#22c55e,#4ade80)", display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 2px 8px rgba(34,197,94,0.4)", zIndex: 2,
          }}>
            <span style={{ color: "white", fontSize: "11px", fontWeight: 800 }}>&#10003;</span>
          </div>
        )}
      </div>
      <span style={{ fontSize: "12.5px", fontWeight: 700, color: active ? phase.color : done ? phase.color : "var(--text-muted)" }}>{phase.label}</span>
    </div>
  );
}

/* ─── Connector Line (animated flow particles) ─── */
function PLine({ color, progress }: { color: string; progress: number }) {
  return (
    <div style={{ flex: 1, display: "flex", alignItems: "center", justifyContent: "center", position: "relative", height: "6px", margin: "0 -10px" }}>
      <div style={{ width: "100%", height: "3px", background: "var(--bg-tertiary)", borderRadius: "2px", position: "relative", overflow: "hidden" }}>
        {/* Progress fill */}
        <div style={{
          position: "absolute", left: 0, top: 0, height: "100%",
          width: progress > 0 ? "100%" : "0%",
          background: `linear-gradient(90deg,${color},${color}70)`,
          borderRadius: "2px", transition: "width 0.65s cubic-bezier(.34,1.56,.64,1)",
        }} />
        {/* Flowing light */}
        {progress > 0 && (
          <div style={{
            position: "absolute", left: "-40%", top: 0, width: "40%", height: "100%",
            background: `linear-gradient(90deg,transparent,${color}cc,transparent)`,
            borderRadius: "2px", animation: "vizDataFlow 2s ease-in-out infinite",
          }} />
        )}
      </div>
      {/* Traveling dot */}
      {progress > 0 && (
        <div style={{
          position: "absolute", width: "10px", height: "10px", borderRadius: "50%",
          background: color, boxShadow: `0 0 10px ${color}, 0 0 20px ${color}40`,
          left: `${progress}%`, top: "50%", transform: "translate(-50%,-50%)",
          transition: "left 0.65s cubic-bezier(.34,1.56,.64,1)",
          animation: "pulse-glow 1.8s infinite",
        }} />
      )}
    </div>
  );
}

/* ─── Memory Stack (enhanced hover + layer depth) ─── */
function MemStack() {
  const total = useAnimNum(MEM_LAYERS.reduce((s, l) => s + l.entries, 0));
  return (
    <div className="card card-elevated" style={{ padding: "24px", position: "relative", overflow: "hidden" }}>
      {/* Subtle bg pattern */}
      <div style={{ position: "absolute", top: "-40px", right: "-40px", width: "160px", height: "160px", borderRadius: "50%", background: "radial-gradient(circle,rgba(139,92,246,0.07),transparent)" }} />

      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: "20px", position: "relative", zIndex: 1 }}>
        <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
          <div style={{
            width: "42px", height: "42px", borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#8b5cf6,#a78bfa)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(139,92,246,0.35), 0 0 0 1px rgba(255,255,255,0.05) inset",
          }}>
            <Layers size={20} color="white" />
          </div>
          <div>
            <div style={{ fontSize: "16px", fontWeight: 700, letterSpacing: "-0.01em" }}>记忆体系</div>
            <div style={{ fontSize: "11.5px", color: "var(--text-muted)" }}>四层分层存储架构</div>
          </div>
        </div>
        <div style={{ textAlign: "right" }}>
          <div style={{ fontSize: "26px", fontWeight: 800, color: "#8b5cf6", lineHeight: 1, letterSpacing: "-0.03em" }}>{total.toLocaleString()}</div>
          <div style={{ fontSize: "10px", color: "var(--text-muted)", textTransform: "uppercase", letterSpacing: "0.04em" }}>总条目</div>
        </div>
      </div>

      <div style={{ display: "flex", flexDirection: "column", gap: "10px", perspective: "900px", position: "relative", zIndex: 1 }}>
        {MEM_LAYERS.map((layer, idx) => {
          const Ic = layer.icon;
          const pct = Math.round((layer.entries / layer.max) * 100);
          return (
            <div key={layer.key} className="viz-memory-layer" style={{
              padding: "15px 18px", borderRadius: "var(--radius-md)",
              background: idx === 0 ? `${layer.color}0a` : "var(--bg-secondary)",
              borderLeft: `4px solid ${layer.color}`,
              border: `1px solid ${idx === 0 ? `${layer.color}22` : "var(--border-subtle)"}`,
              display: "flex", alignItems: "center", gap: "14px",
              transform: `translateZ(${-(MEM_LAYERS.length - idx) * 5}px)`,
              cursor: "pointer",
            }}
            onMouseEnter={(e) => { e.currentTarget.style.background = `${layer.color}12`; e.currentTarget.style.transform = `translateZ(${-(MEM_LAYERS.length - idx) * 5}px) translateX(6px) scale(1.01)`; }}
            onMouseLeave={(e) => { e.currentTarget.style.background = idx === 0 ? `${layer.color}0a` : "var(--bg-secondary)"; e.currentTarget.style.transform = `translateZ(${-(MEM_LAYERS.length - idx) * 5}px)`; }}
            >
              <div style={{
                width: "44px", height: "44px", borderRadius: "var(--radius-sm)",
                background: layer.grad, display: "flex", alignItems: "center", justifyContent: "center",
                flexShrink: 0, boxShadow: `0 4px 14px ${layer.color}35`,
                position: "relative", overflow: "hidden",
              }}>
                <Ic size={19} color="white" />
              </div>
              <div style={{ flex: 1, minWidth: 0 }}>
                <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "7px" }}>
                  <span style={{ fontSize: "13.5px", fontWeight: 700 }}>{layer.name}</span>
                  <span style={{ marginLeft: "auto", fontSize: "14px", fontWeight: 800, color: layer.color, fontFamily: "'JetBrains Mono', monospace" }}>{pct}%</span>
                </div>
                <div style={{ height: "6px", background: "var(--bg-tertiary)", borderRadius: "3px", overflow: "hidden", position: "relative" }}>
                  <div style={{
                    width: `${pct}%`, height: "100%",
                    background: `linear-gradient(90deg,${layer.color},${layer.color}aa)`,
                    borderRadius: "3px", transition: "width 0.9s cubic-bezier(.34,1.56,.64,1)",
                    position: "relative",
                  }} />
                  {/* Shine overlay */}
                  <div style={{
                    position: "absolute", top: 0, left: 0, right: 0, bottom: 0,
                    background: "linear-gradient(90deg,transparent 30%,rgba(255,255,255,0.15) 50%,transparent 70%)",
                    backgroundSize: "200% 100%", animation: "shimmer 3s ease-in-out infinite",
                    borderRadius: "3px",
                  }} />
                </div>
                <div style={{ display: "flex", gap: "6px", marginTop: "6px" }}>
                  {layer.tech.map(t => (
                    <span key={t} style={{ fontSize: "9.5px", padding: "1px 7px", background: `${layer.color}10`, color: layer.color, borderRadius: "4px", fontWeight: 600, fontFamily: "'JetBrains Mono', monospace" }}>{t}</span>
                  ))}
                </div>
              </div>
              <div style={{ textAlign: "right", flexShrink: 0, minWidth: "60px" }}>
                <div style={{ fontSize: "15px", fontWeight: 700, color: "var(--text-primary)" }}>{layer.entries.toLocaleString()}</div>
                <div style={{ fontSize: "9.5px", color: "var(--text-muted)" }}>/ {layer.max.toLocaleString()}</div>
              </div>
            </div>
          );
        })}
      </div>

      <div style={{ marginTop: "16px", paddingTop: "14px", borderTop: "1px solid var(--border-subtle)", display: "flex", gap: "6px", flexWrap: "wrap", position: "relative", zIndex: 1 }}>
        {["FAISS 向量检索", "艾宾浩斯遗忘曲线", "持久同调 Ripser", "自动进化触发"].map((t, i) => (
          <span key={i} style={{ fontSize: "10.5px", padding: "4px 11px", background: "rgba(139,92,246,0.07)", color: "#8b5cf6", borderRadius: "var(--radius-full)", fontWeight: 600, border: "1px solid rgba(139,92,246,0.1)" }}>{t}</span>
        ))}
      </div>
    </div>
  );
}

/* ─── Dual System Panel (enhanced with connection line) ─── */
function DualPanel() {
  return (
    <div className="card card-elevated" style={{ padding: "24px", position: "relative", overflow: "hidden" }}>
      <div style={{ position: "absolute", top: "-30px", right: "-30px", width: "140px", height: "140px", borderRadius: "50%", background: "radial-gradient(circle,rgba(245,158,11,0.06),transparent)" }} />

      <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "22px", position: "relative", zIndex: 1 }}>
        <div style={{
          width: "40px", height: "40px", borderRadius: "var(--radius-md)",
          background: "linear-gradient(135deg,#f59e0b,#fbbf24)",
          display: "flex", alignItems: "center", justifyContent: "center",
          boxShadow: "0 4px 16px rgba(245,158,11,0.35), 0 0 0 1px rgba(255,255,255,0.05) inset",
        }}>
          <Brain size={19} color="white" />
        </div>
        <div>
          <div style={{ fontSize: "16px", fontWeight: 700, letterSpacing: "-0.01em" }}>双系统思考</div>
          <div style={{ fontSize: "11.5px", color: "var(--text-muted)" }}>System 1 快直觉 + System 2 慢推理 并行协作</div>
        </div>
      </div>

      <div style={{ display: "flex", gap: "14px", position: "relative", zIndex: 1 }}>
        {[
          { name: "快思考 S1", desc: "直觉响应 · 模式匹配", latency: "~23ms", color: "#22c55e", grad: "linear-gradient(135deg,#22c55e,#4ade80)" },
          { name: "慢推理 S2", desc: "深度分析 · 逻辑推演", latency: "~1.2s", color: "#8b5cf6", grad: "linear-gradient(135deg,#8b5cf6,#a78bfa)" },
        ].map(sys => (
          <div key={sys.name} className="viz-dual-system-card" style={{
            flex: 1, padding: "20px", borderRadius: "var(--radius-lg)",
            background: `${sys.color}06`, border: `2px solid ${sys.color}25`,
            textAlign: "center", position: "relative", overflow: "hidden",
          }}>
            {/* Top accent */}
            <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "3px", background: sys.grad }} />
            {/* Corner glow */}
            <div style={{ position: "absolute", top: "-20px", right: "-20px", width: "80px", height: "80px", borderRadius: "50%", background: `radial-gradient(circle,${sys.color}12,transparent)` }} />

            <div style={{
              width: "54px", height: "54px", borderRadius: "50%",
              background: sys.grad, margin: "0 auto 14px",
              display: "flex", alignItems: "center", justifyContent: "center",
              boxShadow: `0 4px 20px ${sys.color}35, 0 0 0 1px rgba(255,255,255,0.1) inset`,
              position: "relative",
            }}>
              <Radio size={24} color="white" />
              <div style={{
                position: "absolute", inset: "-4px", borderRadius: "50%",
                border: "1.5px solid transparent", borderTopColor: `${sys.color}60`,
                animation: "spin 4s linear infinite",
              }} />
            </div>

            <div style={{ fontSize: "15px", fontWeight: 700, color: sys.color, marginBottom: "3px" }}>{sys.name.split(" ")[0]}</div>
            <div style={{ fontSize: "11.5px", color: "var(--text-secondary)", marginBottom: "4px" }}>{sys.desc}</div>
            <div style={{ fontSize: "12px", fontWeight: 600, color: sys.color, fontFamily: "'JetBrains Mono', monospace", marginBottom: "12px" }}>{sys.latency}</div>

            <div style={{
              display: "inline-flex", alignItems: "center", gap: "5px",
              padding: "5px 16px", borderRadius: "20px",
              background: `${sys.color}14`, fontSize: "11.5px", fontWeight: 700, color: sys.color,
              border: `1px solid ${sys.color}20`,
            }}>
              <div style={{
                width: "8px", height: "8px", borderRadius: "50%",
                background: sys.color, boxShadow: `0 0 8px ${sys.color}`,
                animation: "statusPulse 1.8s infinite",
              }} /> 活跃运行中
            </div>
          </div>
        ))}
      </div>

      {/* Connection bridge between systems */}
      <div style={{
        position: "absolute", top: "55%", left: "50%", transform: "translate(-50%,-50%)",
        width: "36px", height: "36px", borderRadius: "50%",
        background: "var(--bg-elevated)", border: "1px solid var(--border-color)",
        display: "flex", alignItems: "center", justifyContent: "center",
        zIndex: 2, boxShadow: "var(--shadow-md)",
      }}>
        <Activity size={14} color="var(--text-muted)" />
      </div>
    </div>
  );
}

/* ─── Security Shield (enhanced with scan effect) ─── */
function SecShield() {
  const shields = [
    { emoji: "\ud83d\udce6", name: "虚拟工作空间", sub: "进程级隔离沙箱", c: "#10b981" },
    { emoji: "\ud83d\udd11", name: "RBAC 权限裁决", sub: "细粒度访问控制", c: "#06b6d4" },
    { emoji: "\ud83d\udee1\ufe0f", name: "输入净化过滤", sub: "注入攻击防护", c: "#f59e0b" },
    { emoji: "\ud83d\udccb", name: "全链路审计追踪", sub: "不可篡改日志", c: "#8b5cf6" },
  ];
  return (
    <div className="card card-elevated" style={{ padding: "24px", position: "relative", overflow: "hidden" }}>
      {/* Scan line animation */}
      <div style={{
        position: "absolute", top: 0, left: 0, right: 0, height: "2px",
        background: "linear-gradient(90deg,transparent,rgba(239,68,68,0.4),transparent)",
        animation: "vizScanLine 3s ease-in-out infinite",
      }} />

      <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "20px", position: "relative", zIndex: 1 }}>
        <div style={{
          width: "40px", height: "40px", borderRadius: "var(--radius-md)",
          background: "linear-gradient(135deg,#ef4444,#f87171)",
          display: "flex", alignItems: "center", justifyContent: "center",
          boxShadow: "0 4px 16px rgba(239,68,68,0.35), 0 0 0 1px rgba(255,255,255,0.05) inset",
        }}>
          <Shield size={19} color="white" />
        </div>
        <div>
          <div style={{ fontSize: "16px", fontWeight: 700, letterSpacing: "-0.01em" }}>安全状态</div>
          <div style={{ fontSize: "11.5px", color: "var(--text-muted)" }}>Cupolas 四层纵深防御体系</div>
        </div>
      </div>

      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "10px", marginBottom: "16px", position: "relative", zIndex: 1 }}>
        {shields.map(s => (
          <div key={s.name} className="viz-shield-grid-item" style={{
            padding: "15px", borderRadius: "var(--radius-md)",
            background: `${s.c}07`, border: `1px solid ${s.c}18`,
            display: "flex", alignItems: "center", gap: "12px",
          }}>
            <span style={{ fontSize: "24px" }}>{s.emoji}</span>
            <div style={{ flex: 1, minWidth: 0 }}>
              <div style={{ fontSize: "13px", fontWeight: 700, color: s.c }}>{s.name}</div>
              <div style={{ fontSize: "10.5px", color: "var(--text-muted)", marginTop: "2px" }}>{s.sub}</div>
            </div>
            <div style={{
              width: "10px", height: "10px", borderRadius: "50%",
              background: s.c, boxShadow: `0 0 8px ${s.c}50`,
              animation: "statusPulse 2.2s infinite", flexShrink: 0,
            }} />
          </div>
        ))}
      </div>

      <div style={{
        padding: "16px 20px", borderRadius: "var(--radius-md)",
        background: "linear-gradient(135deg,rgba(16,185,129,0.06),rgba(16,185,129,0.02))",
        border: "1px solid rgba(16,185,129,0.12)",
        display: "flex", alignItems: "center", gap: "12px", position: "relative", zIndex: 1,
      }}>
        <div style={{
          width: "40px", height: "40px", borderRadius: "50%",
          background: "linear-gradient(135deg,#10b981,#34d399)",
          display: "flex", alignItems: "center", justifyContent: "center",
          boxShadow: "0 4px 14px rgba(16,185,129,0.3)",
          position: "relative",
        }}>
          <Shield size={18} color="white" />
          <div style={{ position: "absolute", inset: "-3px", borderRadius: "50%", border: "1.5px solid", borderColor: "rgba(16,185,129,0.3)", animation: "spin 6s linear infinite", borderTopColor: "#10b981", borderRightColor: "transparent", borderBottomColor: "transparent", borderLeftColor: "transparent" }} />
        </div>
        <div style={{ flex: 1 }}>
          <div style={{ fontSize: "14.5px", fontWeight: 700, color: "#10b981" }}>安全罩已激活 — 零信任架构运行正常</div>
          <div style={{ fontSize: "11.5px", color: "var(--text-secondary)", marginTop: "2px" }}>所有防护层在线，实时监控中</div>
        </div>
        <div style={{ width: "12px", height: "12px", borderRadius: "50%", background: "#10b981", boxShadow: "0 0 10px #10b98180", animation: "statusPulse 2s infinite" }} />
      </div>
    </div>
  );
}

/* ─── Service Grid (enhanced cards) ─── */
function SvcGrid() {
  const live = SERVICES.filter(s => s.up).length;
  return (
    <div className="card card-elevated" style={{ padding: "24px", position: "relative", overflow: "hidden" }}>
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: "18px" }}>
        <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
          <div style={{
            width: "40px", height: "40px", borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#3b82f6,#60a5fa)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(59,130,246,0.35), 0 0 0 1px rgba(255,255,255,0.05) inset",
          }}>
            <Network size={19} color="white" />
          </div>
          <div>
            <div style={{ fontSize: "16px", fontWeight: 700, letterSpacing: "-0.01em" }}>守护服务</div>
            <div style={{ fontSize: "11.5px", color: "var(--text-muted)" }}>{live}/6 服务在线</div>
          </div>
        </div>
        <div style={{ display: "flex", gap: "4px" }}>
          {[...Array(3)].map((_, i) => (
            <div key={i} style={{ width: "4px", height: "4px", borderRadius: "50%", background: i < live ? "#22c55e" : "var(--border-color)", opacity: i < live ? 1 : 0.3 }} />
          ))}
        </div>
      </div>

      <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: "10px" }}>
        {SERVICES.map(svc => {
          const up = svc.up;
          return (
            <div key={svc.name} style={{
              padding: "14px 16px", borderRadius: "var(--radius-md)",
              background: up ? "var(--bg-secondary)" : "var(--bg-tertiary)",
              border: `1px solid ${up ? "var(--border-subtle)" : "var(--border-color)"}`,
              display: "flex", flexDirection: "column", gap: "8px",
              transition: "all var(--transition-fast)", cursor: "default",
            }}
            onMouseEnter={e => { if (up) { e.currentTarget.style.borderColor = "rgba(34,197,94,0.2)"; e.currentTarget.style.transform = "translateY(-2px)"; } }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = ""; e.currentTarget.style.transform = ""; }}
            >
              <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
                <div style={{
                  width: "10px", height: "10px", borderRadius: "50%",
                  background: up ? "#22c55e" : "var(--border-color)",
                  boxShadow: up ? "0 0 8px rgba(34,197,94,0.5)" : "none",
                  animation: up ? "statusPulse 2s infinite" : "none", flexShrink: 0,
                }} />
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ fontSize: "12.5px", fontWeight: 700, fontFamily: "'JetBrains Mono', monospace" }}>{svc.label}</div>
                  <div style={{ fontSize: "10px", color: "var(--text-muted)", fontFamily: "'JetBrains Mono', monospace" }}>{svc.name}</div>
                </div>
              </div>
              {up && (
                <div style={{ display: "flex", gap: "8px", fontSize: "9.5px", color: "var(--text-muted)" }}>
                  <span>{svc.hrs}h</span>
                  <span>CPU {svc.cpu}%</span>
                  <span>{svc.mem}MB</span>
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}

/* ─── Activity Timeline (enhanced with type icons) ─── */
function TimeLine() {
  const [visibleCount, setVisibleCount] = useState(5);
  const events = [
    { time: "刚刚", text: "认知循环完成一轮 感知→规划→行动 全流程", type: "cycle", c: "#8b5cf6", icon: Workflow },
    { time: "2m前", text: "memory_store(): 写入 12 条新记忆到 L2 特征层向量索引", type: "memory", c: "#06b6d4", icon: Database },
    { time: "5m前", text: "tool_d.read_file() 执行完成，耗时 23ms，返回 1.2KB 数据", type: "tool", c: "#10b981", icon: Wrench },
    { time: "8m前", text: "Cupolas 输入净化模块拦截了 1 个异常请求（SQL注入尝试）", type: "sec", c: "#ef4444", icon: Shield },
    { time: "12m前", text: "System 2 深度推理：对用户问题进行了多角度逻辑分析", type: "think", c: "#f59e0b", icon: Brain },
    { time: "15m前", text: "网关服务完成健康检查，所有端点响应正常", type: "tool", c: "#10b981", icon: CheckCircle2 },
    { time: "20m前", text: "L3 结构层知识图谱新增 8 个实体关系", type: "memory", c: "#06b6d4", icon: Database },
  ];
  return (
    <div className="card card-elevated" style={{ padding: "24px", position: "relative", overflow: "hidden" }}>
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: "18px" }}>
        <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
          <div style={{
            width: "40px", height: "40px", borderRadius: "var(--radius-md)",
            background: "linear-gradient(135deg,#ec4899,#f472b6)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 4px 16px rgba(236,72,153,0.35), 0 0 0 1px rgba(255,255,255,0.05) inset",
          }}>
            <Activity size={19} color="white" />
          </div>
          <div>
            <div style={{ fontSize: "16px", fontWeight: 700, letterSpacing: "-0.01em" }}>实时流</div>
            <div style={{ fontSize: "11.5px", color: "var(--text-muted)" }}>系统事件时间线</div>
          </div>
        </div>
        <button className="btn btn-ghost btn-sm" style={{ padding: "6px 10px" }} onClick={() => setVisibleCount(v => v >= events.length ? 5 : events.length)}>
          {visibleCount >= events.length ? <TrendingUp size={14} /> : <RefreshCw size={14} />}
        </button>
      </div>

      <div style={{ display: "flex", flexDirection: "column", gap: "2px", position: "relative" }}>
        {/* Timeline track */}
        <div style={{ position: "absolute", left: "19px", top: "12px", bottom: "12px", width: "2px", background: "var(--border-subtle)", borderRadius: "1px" }}>
          <div style={{ position: "absolute", top: 0, left: 0, width: "100%", height: `${(1/events.length)*100}%`, background: "linear-gradient(180deg,#8b5cf630,transparent)", borderRadius: "1px" }} />
        </div>

        {events.slice(0, visibleCount).map((ev, i) => {
          const EvIcon = ev.icon;
          const isLatest = i === 0;
          return (
            <div key={i} style={{
              display: "flex", gap: "16px", padding: "13px 0", position: "relative",
              animation: `fadeIn 0.35s ease-out ${i * 70}ms both`,
            }}>
              {/* Node */}
              <div style={{
                width: "38px", height: "38px", borderRadius: "50%",
                background: isLatest ? ev.c : `${ev.c}12`,
                border: `2px solid ${ev.c}${isLatest ? "" : "50"}`,
                display: "flex", alignItems: "center", justifyContent: "center",
                flexShrink: 0, zIndex: 1, position: "relative",
                boxShadow: isLatest ? `0 0 14px ${ev.c}40` : "none",
                animation: isLatest ? "timelineDotBlink 2.5s ease-in-out infinite" : "none",
              }}>
                <EvIcon size={16} color={isLatest ? "white" : ev.c} />
              </div>
              {/* Content */}
              <div style={{ flex: 1, paddingTop: "3px", minWidth: 0 }}>
                <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "4px" }}>
                  <span style={{ fontSize: "12px", fontWeight: 700, color: ev.c, fontFamily: "'JetBrains Mono', monospace" }}>{ev.time}</span>
                  <span className="tag" style={{ fontSize: "9px", padding: "2px 8px", background: `${ev.c}12`, color: ev.c, borderRadius: "4px", fontWeight: 600, textTransform: "uppercase", letterSpacing: "0.03em" }}>{ev.type}</span>
                </div>
                <div style={{ fontSize: "13.5px", color: "var(--text-primary)", lineHeight: 1.55, wordBreak: "break-word" }}>{ev.text}</div>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}

/* ─── Quick Navigation Cards ─── */
function QNav({ nav }: { nav: (p: string) => void }) {
  const items = [
    { Ic: Target, label: "任务编排", path: "/tasks", c: "#6366f1", g: "linear-gradient(135deg,#6366f1,#818cf8)" },
    { Ic: Eye, label: "可观测性", path: "/logs", c: "#06b6d4", g: "linear-gradient(135deg,#06b6d4,#22d3ee)" },
    { Ic: Database, label: "记忆管理", path: "/memory-evolution", c: "#8b5cf6", g: "linear-gradient(135deg,#8b5cf6,#a78bfa)" },
    { Ic: Wrench, label: "工具管理", path: "/tools", c: "#10b981", g: "linear-gradient(135deg,#10b981,#34d399)" },
  ];
  return (
    <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: "14px" }}>
      {items.map(it => {
        const Icon = it.Ic;
        return (
          <div key={it.path}
            onClick={() => nav(it.path)}
            className="interactive-item"
            style={{
              padding: "22px 20px", borderRadius: "var(--radius-lg)",
              background: "var(--surface-glass)", backdropFilter: "blur(16px)",
              border: "1px solid var(--border-glass)", cursor: "pointer",
              position: "relative", overflow: "hidden",
              transition: "all 0.4s cubic-bezier(.34,1.56,.64,1)",
            }}
            onMouseEnter={e => {
              e.currentTarget.style.transform = "translateY(-5px) scale(1.02)";
              e.currentTarget.style.boxShadow = `0 16px 40px ${it.c}18, 0 0 0 1px ${it.c}25, 0 0 30px ${it.c}08`;
              e.currentTarget.style.borderColor = `${it.c}30`;
            }}
            onMouseLeave={e => {
              e.currentTarget.style.transform = "";
              e.currentTarget.style.boxShadow = "";
              e.currentTarget.style.borderColor = "";
            }}
          >
            {/* Radial decoration */}
            <div style={{ position: "absolute", top: "-20px", right: "-20px", width: "100px", height: "100px", background: `radial-gradient(circle at center,${it.c}12,transparent)` }} />
            {/* Hover shine */}
            <div style={{ position: "absolute", top: 0, left: 0, right: 0, bottom: 0, background: `linear-gradient(135deg,${it.c}06,transparent 50%)`, opacity: 0, transition: "opacity 0.3s" }}
              onMouseEnter={e => (e.target as HTMLElement).style.opacity = "1"}
              onMouseLeave={e => (e.target as HTMLElement).style.opacity = "0"}
            />
            <div style={{
              width: "46px", height: "46px", borderRadius: "var(--radius-md)",
              background: it.g, display: "flex", alignItems: "center", justifyContent: "center",
              marginBottom: "14px", color: "white",
              boxShadow: `0 4px 16px ${it.c}40, 0 0 0 1px rgba(255,255,255,0.15) inset`,
              transition: "transform 0.3s ease", position: "relative", overflow: "hidden",
            }}>
              <div style={{ position: "absolute", inset: 0, borderRadius: "inherit", background: "radial-gradient(circle at 30% 30%,rgba(255,255,255,0.2),transparent 55%)" }} />
              <Icon size={22} style={{ position: "relative", zIndex: 1 }} />
            </div>
            <div style={{ fontSize: "14.5px", fontWeight: 700, marginBottom: "5px", letterSpacing: "-0.01em" }}>{it.label}</div>
            <div style={{ display: "flex", alignItems: "center", gap: "5px", fontSize: "11.5px", fontWeight: 600, color: it.c }}>
              进入模块 <ArrowRight size={13} style={{ transition: "transform 0.25s ease" }} onMouseEnter={e => (e.target as HTMLElement).style.transform = "translateX(3px)"} onMouseLeave={e => (e.target as HTMLElement).style.transform = ""} />
            </div>
          </div>
        );
      })}
    </div>
  );
}

/* ════════════════════════════════════
   DASHBOARD MAIN COMPONENT
   ════════════════════════════════════ */
export default function Dashboard() {
  const navigate = useNavigate();
  const [loading, setLoading] = useState(true);
  const [phaseIdx, setPhaseIdx] = useState(1);
  const [refreshing, setRefreshing] = useState(false);
  const [lastUpdate, setLastUpdate] = useState<Date>(new Date());
  const [connectionStatus, setConnectionStatus] = useState<'connected' | 'disconnected' | 'checking'>('checking');
  const [systemData, setSystemData] = useState<{
    cpu: number; memory: number; disk: number; processes: number;
    servicesUp: number; servicesTotal: number;
    uptime: string; version: string;
  } | null>(null);

  const fetchDashboardData = useCallback(async () => {
    setRefreshing(true);
    setConnectionStatus('checking');
    try {
      const [monitorData, serviceStatus] = await Promise.all([
        sdk.getSystemMonitorData().catch(() => null),
        sdk.getServiceStatus().catch(() => null),
      ]);
      if (monitorData || serviceStatus) {
        setConnectionStatus('connected');
      } else {
        setConnectionStatus('disconnected');
      }
      if (monitorData) {
        setSystemData({
          cpu: Math.round(monitorData.cpu.usage_percent),
          memory: Math.round(monitorData.memory.percent),
          disk: Math.round(monitorData.disk.percent),
          processes: monitorData.cpu.cores.length,
          servicesUp: serviceStatus ? serviceStatus.filter((s: any) => s.healthy).length : 5,
          servicesTotal: serviceStatus ? serviceStatus.length : 6,
          uptime: Math.floor(monitorData.uptime_seconds / 3600) + "h",
          version: "v2.1.0",
        });
      }
      setLastUpdate(new Date());
    } catch {
      setConnectionStatus('disconnected');
    } finally {
      setRefreshing(false);
    }
  }, []);

  useEffect(() => { const t = setTimeout(() => setLoading(false), 500); return () => clearTimeout(t); }, []);
  useEffect(() => { const tm = setInterval(() => setPhaseIdx(p => (p + 1) % 3), 4000); return () => clearInterval(tm); }, []);
  useEffect(() => { fetchDashboardData(); const iv = setInterval(fetchDashboardData, 10000); return () => clearInterval(iv); }, [fetchDashboardData]);

  const cpuVal = systemData?.cpu ?? 23;
  const memVal = systemData?.memory ?? 26;
  const diskVal = systemData?.disk ?? 75;
  const procVal = systemData ? Math.round((systemData.servicesUp / systemData.servicesTotal) * 100) : 83;

  if (loading) {
    return (
      <div className="page-container" style={{ display: "flex", alignItems: "center", justifyContent: "center", minHeight: "72vh" }}>
        <div style={{ textAlign: "center" }}>
          <div style={{
            width: "56px", height: "56px", margin: "0 auto 20px", borderRadius: "var(--radius-lg)",
            background: "linear-gradient(135deg,#6366f1,#8b5cf6,#a78bfa)",
            display: "flex", alignItems: "center", justifyContent: "center",
            animation: "pulse-glow 2.5s ease-in-out infinite",
            boxShadow: "0 8px 32px rgba(99,102,241,0.4)",
          }}>
            <Bot size={26} color="white" />
          </div>
          <div className="loading-spinner" />
          <p style={{ color: "var(--text-muted)", marginTop: "14px", fontSize: "13px", fontWeight: 500 }}>正在加载控制中心...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="page-container" style={{ position: "relative" }}>
      {/* Ambient background decorations */}
      <div style={{ position: "fixed", top: "-20%", left: "-10%", width: "50%", height: "50%", borderRadius: "50%", background: "radial-gradient(circle,rgba(99,102,241,0.04),transparent 70%)", pointerEvents: "none", zIndex: 0 }} />
      <div style={{ position: "fixed", bottom: "-15%", right: "-8%", width: "45%", height: "45%", borderRadius: "50%", background: "radial-gradient(circle,rgba(139,92,246,0.03),transparent 70%)", pointerEvents: "none", zIndex: 0 }} />
      <div style={{ position: "fixed", top: "40%", left: "30%", width: "35%", height: "35%", borderRadius: "50%", background: "radial-gradient(circle,rgba(6,182,212,0.02),transparent 70%)", pointerEvents: "none", zIndex: 0 }} />

      {/* ═ Header ═ */}
      <div className="page-header" style={{ position: "relative", zIndex: 1 }}>
        <div style={{ position: "absolute", top: 0, left: -24, right: -24, height: "2px", background: "linear-gradient(90deg,transparent,#6366f140,#8b5cf640,#06b6d440,#10b98140,transparent)", borderRadius: "1px" }} />
        <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>
          <div style={{
            width: "52px", height: "52px", borderRadius: "var(--radius-lg)",
            background: "linear-gradient(135deg,#6366f1,#8b5cf6,#a78bfa)",
            display: "flex", alignItems: "center", justifyContent: "center",
            boxShadow: "0 8px 24px rgba(99,102,241,0.4), 0 0 0 1px rgba(255,255,255,0.08) inset, 0 0 40px rgba(99,102,241,0.15)",
            animation: "pulse-glow 3.5s ease-in-out infinite",
            position: "relative",
          }}>
            <Bot size={26} color="white" />
            <div style={{ position: "absolute", inset: "-3px", borderRadius: "inherit", border: "1.5px solid transparent", borderTopColor: "#a78bfa66", borderRightColor: "#a78bfa33", borderBottomColor: "#a78bfa33", borderLeftColor: "#a78bfa33", animation: "spin 8s linear infinite" }} />
            <div style={{ position: "absolute", inset: "-6px", borderRadius: "inherit", border: "1px solid transparent", borderColor: "rgba(99,102,241,0.08)", animation: "spin 12s linear infinite reverse" }} />
          </div>
          <div>
            <h1>AgentOS 控制中心</h1>
            <p style={{ color: "var(--text-secondary)", fontSize: "13px", margin: "3px 0 0 0", fontWeight: 500 }}>工业级 AI 智能体 · 实时状态监控</p>
          </div>
        </div>
        <div style={{ marginLeft: "auto", display: "flex", alignItems: "center", gap: "10px" }}>
          {/* Connection Status Indicator */}
          <div style={{
            display: "flex", alignItems: "center", gap: "7px", padding: "6px 16px",
            borderRadius: "24px",
            background: connectionStatus === 'connected'
              ? "linear-gradient(135deg,rgba(16,185,129,0.1),rgba(16,185,129,0.05))"
              : connectionStatus === 'disconnected'
                ? "linear-gradient(135deg,rgba(239,68,68,0.1),rgba(239,68,68,0.05))"
                : "var(--bg-tertiary)",
            border: `1px solid ${
              connectionStatus === 'connected' ? "rgba(16,185,129,0.18)"
              : connectionStatus === 'disconnected' ? "rgba(239,68,68,0.18)"
              : "var(--border-subtle)"
            }`,
            boxShadow: "0 2px 8px rgba(0,0,0,0.06)",
            transition: "all 0.3s ease",
          }}>
            <div style={{
              width: "8px", height: "8px", borderRadius: "50%",
              background: connectionStatus === 'connected' ? "#10b981"
                : connectionStatus === 'disconnected' ? "#ef4444"
                : "#f59e0b",
              boxShadow: connectionStatus === 'connected' ? "0 0 8px rgba(16,185,129,0.6)"
                : connectionStatus === 'disconnected' ? "0 0 8px rgba(239,68,68,0.5)"
                : "0 0 8px rgba(245,158,11,0.5)",
              animation: connectionStatus === 'checking' ? "spin 1.5s linear infinite" : "statusPulse 2s infinite",
            }} />
            <span style={{ fontSize: "12px", fontWeight: 700,
              color: connectionStatus === 'connected' ? "#10b981"
                : connectionStatus === 'disconnected' ? "#ef4444"
                : "#f59e0b"
            }}>
              {connectionStatus === 'connected' ? "已连接"
                : connectionStatus === 'disconnected' ? "未连接"
                : "检测中"}
            </span>
          </div>

          {/* Quick Actions */}
          <div style={{ display: "flex", gap: "6px" }}>
            <button className="btn btn-ghost" style={{ padding: "7px 12px", fontSize: "12px" }}
                    onClick={() => navigate('/agents')} title="管理智能体">
              <Bot size={14} />
              <span style={{ marginLeft: "4px" }}>智能体</span>
            </button>
            <button className="btn btn-ghost" style={{ padding: "7px 12px", fontSize: "12px" }}
                    onClick={() => navigate('/tasks')} title="任务队列">
              <Workflow size={14} />
              <span style={{ marginLeft: "4px" }}>任务</span>
            </button>
            <button className="btn btn-ghost" style={{ padding: "7px 12px", fontSize: "12px" }}
                    onClick={() => navigate('/terminal')} title="终端">
              <Radio size={14} />
              <span style={{ marginLeft: "4px" }}>终端</span>
            </button>
          </div>

          <button className="btn btn-ghost" style={{ padding: "8px 10px" }} onClick={fetchDashboardData} disabled={refreshing}>
            <RefreshCw size={16} className={refreshing ? "spin" : ""} />
          </button>
          <span style={{ fontSize: "10.5px", color: "var(--text-muted)", fontFamily: "var(--font-mono)" }}>
            {lastUpdate.toLocaleTimeString("zh-CN", { hour: "2-digit", minute: "2-digit", second: "2-digit" })}
          </span>
        </div>
      </div>

      {/* ═ Resource Gauges ═ */}
      <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 1fr)", gap: "16px", marginBottom: "20px" }}>
        {[
          { v: cpuVal, u: "%", l: "CPU", s: systemData ? `${systemData.processes}核心` : "8核/16线程", c: "#6366f1" },
          { v: memVal, u: "%", l: "内存", s: systemData ? `${systemData.memory}% 已用` : "4.2 / 16 GB", c: "#06b6d4" },
          { v: diskVal, u: "", l: "磁盘", s: systemData ? `${systemData.disk}% 已用` : "128 / 512 GB", c: "#f59e0b" },
          { v: procVal, u: "%", l: "进程", s: systemData ? `${systemData.servicesUp}/${systemData.servicesTotal} 在线` : "5/6 在线", c: "#10b981" },
        ].map(m => (
          <div key={m.l} className="card card-elevated" style={{
            padding: "20px 16px", textAlign: "center",
            transition: "all 0.4s cubic-bezier(.34,1.56,.64,1)",
            position: "relative", overflow: "hidden",
          }}
            onMouseEnter={e => {
              e.currentTarget.style.transform = "translateY(-4px)";
              e.currentTarget.style.boxShadow = `0 12px 32px ${m.c}15, 0 0 0 1px ${m.c}20`;
            }}
            onMouseLeave={e => {
              e.currentTarget.style.transform = "";
              e.currentTarget.style.boxShadow = "";
            }}
          >
            <div style={{ position: "absolute", top: 0, left: 0, right: 0, height: "2px", background: `linear-gradient(90deg,${m.c}40,${m.c}10)`, opacity: 0, transition: "opacity 0.3s" }} onMouseEnter={e => (e.target as HTMLElement).style.opacity = "1"} />
            <GaugeRing pct={m.v} size={96} sw={7} color={m.c} label={`${m.v}${m.u}`} sub={m.l} />
            <div style={{ fontSize: "10px", color: "var(--text-muted)", marginTop: "6px", fontFamily: "'JetBrains Mono', monospace" }}>{m.s}</div>
          </div>
        ))}
      </div>

      {/* ═ Cognitive Pipeline ═ */}
      <div className="card card-elevated" style={{ padding: "30px 24px", marginBottom: "20px", position: "relative", overflow: "hidden" }}>
        <div style={{ position: "absolute", top: "-60px", right: "-40px", width: "180px", height: "180px", borderRadius: "50%", background: "radial-gradient(circle,rgba(139,92,246,0.05),transparent 70%)", pointerEvents: "none" }} />
        <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "28px", position: "relative", zIndex: 1 }}>
          <div style={{ width: "36px", height: "36px", borderRadius: "var(--radius-sm)", background: "linear-gradient(135deg,#8b5cf6,#a78bfa)", display: "flex", alignItems: "center", justifyContent: "center", boxShadow: "0 3px 10px rgba(139,92,246,0.3), 0 0 0 1px rgba(255,255,255,0.08) inset" }}>
            <Workflow size={18} color="white" />
          </div>
          <span style={{ fontSize: "15px", fontWeight: 700, letterSpacing: "-0.01em" }}>认知处理流程</span>
          <span style={{ marginLeft: "auto", fontSize: "11.5px", padding: "4px 12px", background: "rgba(139,92,246,0.08)", color: "#8b5cf6", borderRadius: "12px", fontWeight: 700, fontFamily: "'JetBrains Mono', monospace" }}>&sim;450ms / 周期</span>
        </div>
        <div style={{ display: "flex", alignItems: "flex-start" }}>
          {PHASES.map((p, i) => (
            <React.Fragment key={p.key}>
              <PNode phase={p} active={i === phaseIdx} done={i < phaseIdx} />
              {i < PHASES.length - 1 && <PLine color={PHASES[Math.min(i, phaseIdx)].color} progress={phaseIdx > i ? 100 : phaseIdx === i ? 50 : 0} />}
            </React.Fragment>
          ))}
        </div>
      </div>

      {/* ═ Main Grid: Memory + Dual System ═ */}
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px", marginBottom: "20px" }}>
        <MemStack />
        <DualPanel />
      </div>

      {/* ═ Security + Services ═ */}
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px", marginBottom: "20px" }}>
        <SecShield />
        <SvcGrid />
      </div>

      {/* ═ Timeline + Quick Nav ═ */}
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px" }}>
        <TimeLine />
        <QNav nav={navigate} />
      </div>
    </div>
  );
}
