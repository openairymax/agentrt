# AgentOS Agent Contract 规范集

**版本**: Doc V1.5  
**状态**: 正式发布  
**编制单位**: AgentOS 架构委员会  
**许可证**: GPL-3.0

---

## 编制说明

### 一、规范集概述

本规范集是 AgentOS 开放生态的核心技术规范，定义了智能体 (Agent)、技能 (Skill)、通信协议 (Protocol) 和系统调用 (Syscall API) 的标准化契约。通过建立统一的接口规范，实现 AgentOS 生态中各组件的互操作性、可发现性和可组合性。

### 二、规范集结构

```
agentos_contract/
├── README.md                      # 本文件：规范集总览和导航
├── glossary_index.md              # 术语表与快速索引
├── agent/                         # Agent 契约
│   ├── agent_contract.md          # Agent 契约规范
│   └── agent_contract_schema.json # Agent 契约 JSON Schema
├── skill/                         # Skill 契约
│   ├── skill_contract.md          # Skill 契约规范
│   └── skill_contract_schema.json # Skill 契约 JSON Schema
├── protocol/                      # 通信协议
│   └── protocol_contract.md       # 通信协议规范
├── syscall/                       # 系统调用 API
│   └── syscall_api_contract.md    # 系统调用 API 规范
└── log/                           # 日志格式
    └── logging_format.md          # 日志格式规范
```

### 三、与设计哲学的关系

本规范集根植于 AgentOS 设计哲学 [1] 的核心理论:

#### 1. 认知层理论的应用
- **双系统认知模型**: Agent 契约和 Skill 契约均通过 `models` 字段明确区分 System 1(快系统) 和 System 2(慢系统) 的配置
- **增量规划**: 契约支持动态更新能力指标 (`success_rate`, `avg_duration_ms`),为增量规划器提供实时反馈
- **调度官机制**: 契约中的 `cost_profile` 和 `trust_metrics` 为调度官的多目标优化提供数据基础

#### 2. 系统工程方法
- **层次分解**: 规范集按抽象层次组织：
  - **战略层**: Protocol Contract(定义整体通信架构)
  - **战术层**: Agent Contract & Skill Contract(定义组件能力)
  - **操作层**: Syscall API Contract & Logging Format(定义具体接口)
- **整体优化**: 各规范间通过引用关系形成协同，避免局部优化

#### 3. 微内核思想
- **内核纯净**: Syscall API 保持极简，仅包含任务管理、记忆管理、会话管理、可观测性四大类接口
- **服务外置**: Agent 和 Skill 作为用户态服务，通过契约与内核交互
- **安全隔离**: 所有契约均包含 `required_permissions` 字段，支持虚拟工位和权限裁决

#### 4. 模块化与可插拔
- **策略可插拔**: 契约设计支持动态替换 Agent 和 Skill，不影响系统其他组件
- **依赖管理**: Skill 契约显式声明依赖，支持自动化安装和冲突检测
- **扩展机制**: 通过 `extensions` 字段支持社区自定义，保证向后兼容

### 四、快速导航

#### 按角色查看
- **Agent 开发者**: 先阅读 [`agent_contract.md`](./agent_contract.md),了解如何定义 Agent 能力
- **Skill 贡献者**: 先阅读 [`skill_contract.md`](./skill_contract.md),了解如何封装执行单元
- **系统集成商**: 先阅读 [`protocol_contract.md`](./protocol_contract.md),了解通信机制
- **底层开发者**: 先阅读 [`syscall_api_contract.md`](./syscall_api_contract.md),了解内核接口

#### 按场景查看
- **创建新 Agent**: agent_contract.md → skill_contract.md → logging_format.md
- **调试通信问题**: protocol_contract.md → logging_format.md
- **性能优化**: syscall_api_contract.md → logging_format.md
- **安全审计**: 所有规范的"安全考虑"章节

## 五、术语表与索引

本规范集使用的术语定义见 [`TERMINOLOGY.md`](../TERMINOLOGY.md),关键术语包括:

| 术语 | 定义位置 | 简要说明 |
|------|---------|---------|
| Agent | [Cognition_Theory.md](../../philosophy/Cognition_Theory.md) | 具有认知能力的智能体 |
| Skill | [Design_Principles.md](../../philosophy/Design_Principles.md) | 可复用的执行单元 |
| 契约 (Contract) | 本规范集 | 机器可读的能力描述文件 |
| 调度官 (Dispatcher) | [Cognition_Theory.md](../../philosophy/Cognition_Theory.md) | 负责选择 Agent 执行任务的组件 |
| 信任边界 (Trust Boundary) | [架构设计原则.md](../架构设计原则.md) | 区分可信与不可信数据的界限 |

快速索引和完整术语表参见 [`glossary_index.md`](./glossary_index.md)。

### 六、与其他规范的关系

本规范集与 AgentOS 其他核心规范形成协同:

| 引用规范 | 关系说明 |
|---------|---------||
| [架构设计原则](../architecture/架构设计原则.md) | 本规范集是架构原则在 AgentOS 生态的具体实现 |
| [C&C++ 安全编程指南](./coding_standard/C&Cpp-secure-coding-guide.md) | Syscall API 的实现应遵循安全编程指南 |
| [日志打印规范](./coding_standard/Log_guide.md) | logging_format.md 是其在上层应用的细化 |
| [统一术语表](./TERMINOLOGY.md) | 本规范集使用的术语定义和解释 |

### 七、版本管理

本规范集采用统一的版本管理机制:

- **主版本号**: 当发生不兼容变更时递增 (如 Schema 结构变更)
- **次版本号**: 当新增向后兼容功能时递增
- **修订号**: 当进行错误修正或澄清时递增

各规范的版本历史见各自文档的"版本历史"章节。

### 八、实施路线图

#### 阶段 1: 基础建设 (已完成)
- ✅ 制定 Agent Contract 和 Skill Contract 规范
- ✅ 开发契约验证工具
- ✅ 建立市场服务原型

#### 阶段 2: 生态培育 (进行中)
- 🔄 开发参考实现 Agent 和 Skill
- 🔄 建立认证机制
- 📅 开展社区培训和推广

#### 阶段 3: 规模化应用 (计划中)
- 📅 支持动态团队组建
- 📅 实现跨平台技能市场
- 📅 建立进化委员会机制

---

## 参考文献

[1] AgentOS 设计哲学。../../philosophy/Design_Principles.md  
[2] AgentOS 认知层理论。../../philosophy/Cognition_Theory.md  
[3] AgentOS 记忆层理论。../../philosophy/Memory_Theory.md  
[4] 架构设计原则。../architecture/架构设计原则.md  
[5] 统一术语表。./TERMINOLOGY.md  

---

**最后更新**: 2026-03-22  
**维护者**: AgentOS 架构委员会  