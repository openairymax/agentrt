# Multi-Agent Debate - 4种协作模式

> 难度：高级 | 主题：多 Agent 协作与辩论

## 项目说明

本项目演示 AgentRT 的四种多 Agent 协作模式，以辩论场景为例。四个 Agent 分别扮演正方、反方、裁判和主持人，通过不同的协作模式完成一场结构化辩论。

### 四种协作模式

| 模式 | 说明 | 辩论中的体现 |
|------|------|-------------|
| **Sequential（顺序）** | Agent 按固定顺序依次执行 | 主持人 → 正方 → 反方 → 裁判 |
| **Parallel（并行）** | 多个 Agent 同时独立执行 | 正方和反方同时准备论点 |
| **Debate（辩论）** | Agent 之间交替反驳，多轮交互 | 正方与反方交替发言，互相驳斥 |
| **Hierarchical（层级）** | 上级 Agent 分配任务给下级 | 主持人分配辩题，裁判汇总评判 |

## 目录结构

```
multi-agent-debate/
├── README.md                        # 本文件
├── config.yaml                      # 辩论流程配置
└── agents/
    ├── proponent.agent.yaml         # 正方 Agent
    ├── opponent.agent.yaml          # 反方 Agent
    ├── judge.agent.yaml             # 裁判 Agent
    └── moderator.agent.yaml         # 主持人 Agent
```

## 运行方式

```bash
# 1. 确保 AgentRT 已安装
pip install agentrt

# 2. 运行辩论（默认使用 Debate 模式）
agentrt run --config config.yaml

# 3. 指定协作模式
agentrt run --config config.yaml --mode sequential
agentrt run --config config.yaml --mode parallel
agentrt run --config config.yaml --mode debate
agentrt run --config config.yaml --mode hierarchical

# 4. 自定义辩题
agentrt run --config config.yaml --topic "AI是否会取代人类创造力"
```

## 关键概念

### 1. Sequential 模式 — 顺序执行

Agent 按配置顺序依次执行，前一个 Agent 的输出作为后一个的输入。

```
主持人(宣布辩题) → 正方(陈述观点) → 反方(陈述观点) → 裁判(评判)
```

**适用场景**：流水线式处理，每个步骤依赖前一步的结果。

### 2. Parallel 模式 — 并行执行

多个 Agent 同时独立执行，结果最终汇总。

```
        ┌→ 正方(准备论点) ─┐
主持人 → │                   │ → 裁判(综合评判)
        └→ 反方(准备论点) ─┘
```

**适用场景**：独立子任务、多视角分析、竞品对比。

### 3. Debate 模式 — 辩论交互

Agent 之间多轮交替发言，互相反驳，直到达成共识或达到轮次上限。

```
正方(立论) ⇄ 反方(驳斥)  [多轮]
        ↓
    裁判(评判)
```

**适用场景**：决策辩论、方案评审、对抗性测试。

### 4. Hierarchical 模式 — 层级调度

上级 Agent 负责任务分配和协调，下级 Agent 执行具体任务。

```
主持人(分配辩题)
    ├── 正方(执行辩论任务)
    ├── 反方(执行辩论任务)
    └── 裁判(执行评判任务)
```

**适用场景**：复杂任务分解、项目管理、多级审批。

### Agent 定义格式

每个 Agent 通过 `.agent.yaml` 文件定义：

```yaml
name: agent-name
version: "1.0.0"
model: gpt-4o
system_prompt: |
  Agent 的系统提示词...
tools: []      # 可用工具列表
skills: []     # 可用技能列表
hooks: []      # 生命周期钩子
```

## 扩展建议

- **增加辩论轮次**：修改 `config.yaml` 中的 `max_rounds` 参数，让辩论更深入
- **引入专家证人**：添加新的 Agent 角色，提供专业领域知识
- **动态分组**：根据辩题自动选择正反方 Agent
- **评分可视化**：将裁判评分结果输出为图表
- **混合模式**：在同一流程中组合多种协作模式，如先并行准备再辩论
