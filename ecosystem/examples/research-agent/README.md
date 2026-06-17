# research-agent — 多 Agent 协作 + 记忆持久化

> 难度：中级 | 预计时间：30分钟 | 主题：多 Agent 协作与记忆系统

## 项目说明

本项目演示 AgentRT 的两个核心能力：**多 Agent 协作** 和 **记忆持久化**。

`research-agent` 由两个 Agent 组成：
- **research_agent**：负责信息搜索和资料收集
- **research_coordinator**：协调者，分配任务、汇总结果、生成报告

两个 Agent 通过 A2A（Agent-to-Agent）协议协作，并利用四层记忆系统实现跨会话的知识积累。

### 你将学到

- 如何定义多个 Agent 并建立协作关系
- A2A 协议的通信机制
- AgentRT 四层记忆模型的架构和使用
- 协调者模式（Orchestrator Pattern）的设计思路

## 目录结构

```
research-agent/
├── README.md                             # 本文件
├── config.yaml                           # 运行时配置（含多 Agent 和记忆配置）
└── agents/
    ├── research.agent.yaml               # 研究 Agent：搜索和收集信息
    └── research_coordinator.agent.yaml   # 协调者 Agent：任务分配和汇总
```

## 运行方式

```bash
# 1. 启动多 Agent 系统
agentrt run --config config.yaml

# 2. 向协调者提交研究任务
curl -X POST http://localhost:8080/api/v1/chat \
  -H "Content-Type: application/json" \
  -d '{
    "message": "请帮我研究 AgentRT 的记忆系统架构",
    "agent": "research_coordinator"
  }'

# 3. 查看记忆中的研究历史
curl -X GET "http://localhost:8080/api/v1/memories/search?query=AgentRT+记忆系统&top_k=5"
```

## 关键概念

### 1. 多 Agent 协作模式

AgentRT 支持多种多 Agent 协作模式：

| 模式 | 说明 | 适用场景 |
|------|------|---------|
| **Orchestrator** | 一个协调者分配任务给多个工作者 | 复杂任务分解 |
| **Pipeline** | Agent 按顺序处理，前一个的输出是后一个的输入 | 流水线式处理 |
| **Peer-to-Peer** | Agent 之间平等协作，互相调用 | 开放式讨论 |
| **Hierarchical** | 多层级管理，上级分配、下级执行 | 大规模任务 |

本示例使用 **Orchestrator 模式**：

```
用户请求 → research_coordinator（协调者）
              ├── 分配子任务1 → research_agent（研究者）
              ├── 分配子任务2 → research_agent（研究者）
              └── 汇总结果 → 生成研究报告
```

### 2. A2A 协议

Agent-to-Agent（A2A）是 AgentRT 的 Agent 间通信协议：

- **服务发现**：Agent 启动时注册能力，其他 Agent 可发现并调用
- **消息传递**：支持同步调用和异步消息
- **超时控制**：默认 60 秒超时，可配置
- **取消机制**：支持取消正在进行的跨 Agent 调用

```yaml
# config.yaml 中的 A2A 配置
multi_agent:
  enabled: true
  communication:
    protocol: "a2a"
  collaboration:
    default_pattern: "orchestrator"
```

### 3. 四层记忆模型

AgentRT 的记忆系统分为四层，由浅到深：

| 层级 | 名称 | 用途 | 持久性 | 示例 |
|------|------|------|--------|------|
| L1 | 工作记忆 | 当前对话上下文 | 会话级 | 当前对话历史 |
| L2 | 语义记忆 | 向量化知识检索 | 持久化 | 研究资料索引 |
| L3 | 实体记忆 | 结构化实体和关系 | 持久化 | 人名、项目名、概念 |
| L4 | 情景记忆 | 经验模式和规则 | 持久化 | "用户偏好简洁回答" |

```
用户提问
  → L1 检索当前对话上下文
  → L2 语义搜索相关资料
  → L3 查找相关实体
  → L4 应用经验规则
  → 组合上下文 → LLM 推理
```

### 4. 协调者 Agent 的关键配置

```yaml
# research_coordinator.agent.yaml
collaboration:
  pattern: "orchestrator"
  workers:
    - agent: "research_agent"
      capabilities: ["search", "collect"]
```

协调者通过 `agent:invoke` 权限调用研究 Agent，研究 Agent 完成任务后将结果写入记忆系统。

## 扩展建议

- **增加专业 Agent**：添加 `data_analyst_agent`（数据分析）、`fact_checker_agent`（事实核查）等
- **Pipeline 模式**：将研究流程改为 Pipeline：搜索 → 整理 → 分析 → 报告
- **记忆去重**：利用 L4 情景记忆自动去重相似的研究结果
- **并行研究**：协调者同时分配多个子任务给多个研究 Agent 实例
- **进度追踪**：利用协调者的 `progress_tracking` 能力实时展示研究进度
