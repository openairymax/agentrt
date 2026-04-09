Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 统一术语表

**版本**: Doc V1.8  
**状态**: 正式发布  
**维护者**: AgentOS 架构委员会  
**作者**: LirenWang  
**最后更新**: 2026-04-09  
**理论基础**: 工程两论（工程控制论、系统工程）、五维正交系统（系统观、内核观、认知观、工程观、设计美学）、双系统认知理论、微内核哲学  

---

## 编制说明

### 一、术语表目的

本术语表是 AgentOS 规范体系的统一术语参考，旨在：

1. **统一术语使用**: 确保所有规范文档使用一致的术语定义
2. **建立交叉引用**: 提供术语与相关规范章节的快速链接
3. **支持快速检索**: 按字母顺序组织，支持快速查找
4. **促进理解**: 为开发者、审计人员、研究人员提供权威解释

### 二、术语来源

本术语表的术语来源于以下核心文档：

- [Agent 契约规范](./agentos_contract/agent_contract.md)
- [Skill 契约规范](./agentos_contract/skill_contract.md)
- [通信协议规范](./agentos_contract/protocol_contract.md)
- [系统调用 API 规范](./agentos_contract/syscall_api_contract.md)
- [日志格式规范](./agentos_contract/logging_format.md)
- [架构设计原则](../ARCHITECTURAL_PRINCIPLES.md)
- [认知层理论](./philosophy/Cognition_Theory.md)
- [设计哲学](./philosophy/Design_Principles.md)

### 三、使用方法

1. **文档编写**: 所有规范文档必须使用本术语表的定义
2. **术语引用**: 使用 `[术语名称](#术语名称)` 格式进行交叉引用
3. **术语更新**: 新术语需通过架构委员会审核后加入
4. **版本管理**: 术语表版本与规范体系版本同步

### 四、术语分类

- **A. 核心概念**: Agent、Skill、Contract 等基础概念
- **B. 架构组件**: 微内核、安全穹顶、三层循环等架构元素
- **C. 技术术语**: JSON-RPC、TraceID、错误码等技术细节
- **D. 工程原则**: 反馈闭环、层次分解、安全内生等设计原则

---

## 术语定义（按字母顺序）

### A

#### Agent (智能体)

**定义**: 具有认知能力的实体，能够感知环境、进行推理决策并执行行动。Agent 是 AgentOS 的基本执行单元。

**中文优先**: 智能体 (Agent)

**来源**: [agent_contract.md](./agentos_contract/agent/agent_contract.md)  
**参见**: [Skill Contract](#skill-contract-技能契约)、[Schema Version](#schema-version-契约模式版本)

#### Authentication (认证)

**定义**: 验证通信参与方身份的过程，确保消息来源可信。

**中文优先**: 认证 (Authentication)

**来源**: [protocol_contract.md - 3.4.1 节](./agentos_contract/protocol/protocol_contract.md)  
**参见**: [Authorization](#authorization-授权)、[JWT](#jwt-json-web-token)

#### Authorization (授权)

**定义**: 确定已认证实体是否有权限执行特定操作的过程。

**中文优先**: 授权 (Authorization)

**来源**: [protocol_contract.md - 3.4.2 节](./agentos_contract/protocol/protocol_contract.md)  
**参见**: [Authentication](#authentication-认证)、[Permission](#permission-权限)

### B

#### daemon (后端服务层)

**定义**: AgentOS 的核心服务层，包含 LLM 服务、市场服务、监控服务等用户态守护进程。这些进程以 `_d` 后缀命名（如 `llm_d`、`market_d`），运行在用户态，提供系统核心功能。

**中文优先**: 后端服务层 (daemon)

**来源**: [架构设计原则.md - 1.2.3 节](./架构设计原则.md)  
**参见**: [cupolas](#cupolas-安全隔离层)、[Atoms](#atoms-内核层)

### C

#### Contract (契约)

**定义**: 机器可读的能力描述文件，定义 Agent 或 Skill 的接口、能力、权限等元数据。

**中文优先**: 契约 (Contract)

**来源**: [agent_contract.md - 第 4 章](./agentos_contract/agent/agent_contract.md)、[skill_contract.md - 第 4 章](./agentos_contract/skill/skill_contract.md)  
**参见**: [Input Schema](#input-schema-输入模式)、[Output Schema](#output-schema-输出模式)



#### cupolas (安全穹顶)

**定义**: AgentOS 的安全穹顶，提供虚拟工位、权限裁决、输入净化等安全机制。

**中文优先**: 安全穹顶 (cupolas)

**来源**: [架构设计原则.md - 1.2.3 节](./架构设计原则.md)  
**参见**: [Security by Design](#security-by-design-安全内生设计)、[Sandbox](#sandbox-沙箱)

### E

#### Error Code (错误码)

**定义**: 标准化的错误标识符，用于跨组件错误传递和处理。

**中文优先**: 错误码 (Error Code)

**完整列表**: [syscall_api_contract.md - 第 7 章](./agentos_contract/syscall/syscall_api_contract.md)

### F

#### Feedback Loop (反馈闭环)

**定义**: 系统通过输出结果调整后续行为的控制机制，包括实时反馈、轮次内反馈、跨轮次反馈三个层次。

**中文优先**: 反馈闭环 (Feedback Loop)

**来源**: [架构设计原则.md - 2.1 节](./架构设计原则.md)  
**参见**: [Engineering Cybernetics](#engineering-cybernetics-工程控制论)、[System Engineering](#system-engineering-系统工程)

### I

#### Input Schema (输入模式)

**定义**: 定义 Skill 或 Agent 输入数据结构的 JSON Schema。

**中文优先**: 输入模式 (Input Schema)

**来源**: [skill_contract.md - 3.1 节](./agentos_contract/skill/skill_contract.md)  
**参见**: [Output Schema](#output-schema-输出模式)、[Contract](#contract-契约)

#### IPC (进程间通信)

**定义**: 微内核提供的进程间通信机制，包括消息传递、共享内存、信号量等。

**中文优先**: 进程间通信 (IPC)

**来源**: [架构设计原则.md - 2.3 节](./架构设计原则.md)  
**参见**: [Microkernel](#microkernel-微内核)、[Syscall](#syscall-系统调用)

### J

#### JWT (JSON Web Token)

**定义**: 用于认证和授权的紧凑、URL安全的令牌格式。

**中文优先**: JSON Web 令牌 (JWT)

**来源**: [protocol_contract.md - 3.4.3 节](./agentos_contract/protocol/protocol_contract.md)  
**参见**: [Authentication](#authentication-认证)、[Authorization](#authorization-授权)

#### JSON-RPC 2.0

**定义**: AgentOS 采用的轻量级远程过程调用协议，支持请求-响应和通知两种模式。

**中文优先**: JSON-RPC 2.0

**来源**: [protocol_contract.md - 第 2 章](./agentos_contract/protocol/protocol_contract.md)  
**参见**: [Error Code](#error-code-错误码)、[TraceID](#traceid-跟踪标识符)

### M

#### Microkernel (微内核)

**定义**: 极简的操作系统内核，只提供最基本的 IPC、内存管理、任务调度功能，其他功能作为用户态服务实现。

**中文优先**: 微内核 (Microkernel)

**来源**: [架构设计原则.md - 2.3 节](./架构设计原则.md)  
**参见**: [IPC](#ipc-进程间通信)、[Service Isolation](#service-isolation-服务隔离)

### O

#### Output Schema (输出模式)

**定义**: 定义 Skill 或 Agent 输出数据结构的 JSON Schema。

**中文优先**: 输出模式 (Output Schema)

**来源**: [skill_contract.md - 3.2 节](./agentos_contract/skill/skill_contract.md)  
**参见**: [Input Schema](#input-schema-输入模式)、[Contract](#contract-契约)

### P

#### Permission (权限)

**定义**: 执行特定操作所需的授权，如文件访问、网络通信、系统调用等。

**中文优先**: 权限 (Permission)

**来源**: [skill_contract.md - 3.1 节](./agentos_contract/skill/skill_contract.md)  
**参见**: [Sandbox](#sandbox-沙箱)、[Security Isolation](#security-isolation-安全隔离)

### S

#### Sandbox (沙箱)

**定义**: 隔离的执行环境，限制 Skill 的权限和资源访问，防止恶意或错误代码影响系统。

**中文优先**: 沙箱 (Sandbox)

**来源**: [skill_contract.md - 3.1 节](./agentos_contract/skill/skill_contract.md)  
**参见**: [Security Isolation](#security-isolation-安全隔离)、[Permission](#permission-权限)

#### Schema Version (契约模式版本)

**定义**: 契约 JSON Schema 的版本标识符，用于兼容性检查和版本迁移。

**中文优先**: 契约模式版本 (Schema Version)

**来源**: [agent_contract.md - 2.1 节](./agentos_contract/agent/agent_contract.md)  
**参见**: [Contract](#contract-契约)、[Backward Compatibility](#backward-compatibility-向后兼容性)

#### Security by Design (安全内生设计)

**定义**: 将安全机制内嵌于系统架构和设计中的理念，而非事后附加。

**中文优先**: 安全内生设计 (Security by Design)

**来源**: [架构设计原则.md - 5.1 节](./架构设计原则.md)  
**参见**: [cupolas](#cupolas-安全隔离层)、[Sandbox](#sandbox-沙箱)

#### Service Isolation (服务隔离)

**定义**: 用户态服务之间相互隔离，只能通过内核 IPC 通信的安全机制。

**中文优先**: 服务隔离 (Service Isolation)

**来源**: [架构设计原则.md - 2.3 节](./架构设计原则.md)  
**参见**: [Microkernel](#microkernel-微内核)、[IPC](#ipc-进程间通信)

#### Skill (技能)

**定义**: 可复用的执行单元，为智能体提供具体能力，如文件操作、网络请求、数据分析等。

**中文优先**: 技能 (Skill)

**来源**: [skill_contract.md - 第 1 章](./agentos_contract/skill/skill_contract.md)  
**参见**: [Agent](#agent-智能体)、[Contract](#contract-契约)

#### Syscall (系统调用)

**定义**: 用户态程序请求内核服务的标准接口，如创建任务、分配内存、发送消息等。

**中文优先**: 系统调用 (Syscall)

**来源**: [syscall_api_contract.md - 第 1 章](./agentos_contract/syscall/syscall_api_contract.md)  
**参见**: [Microkernel](#microkernel-微内核)、[IPC](#ipc-进程间通信)

### T

#### TraceID (跟踪标识符)

**定义**: 跨组件请求的全局唯一标识符，用于分布式追踪和日志关联。

**中文优先**: 跟踪标识符 (TraceID)

**来源**: [logging_format.md - 6.2 节](./agentos_contract/log/logging_format.md)  
**参见**: [Security by Design](#security-by-design-安全内生设计)、[Traceability](#traceability-可追溯性)

---

## 附录

### JSON-RPC 错误码

| 错误码 | 定义 |
|--------|------|
| -32700 | Parse error |
| -32600 | Invalid Request |
| -32601 | Method not found |
| -32602 | Invalid params |
| -32603 | Internal error |

**完整列表**: [protocol_contract.md - 4.3 节](./agentos_contract/protocol/protocol_contract.md)

---

## 参考文献

[1] AgentOS 设计哲学。./philosophy/Design_Principles.md  
[2] AgentOS 认知层理论。./philosophy/Cognition_Theory.md  
[3] AgentOS 记忆层理论。./philosophy/Memory_Theory.md  
[4] 架构设计原则。./架构设计原则.md  
[5] Agent 契约规范。./agentos_contract/agent/agent_contract.md  
[6] Skill 契约规范。./agentos_contract/skill/skill_contract.md  
[7] 通信协议规范。./agentos_contract/protocol/protocol_contract.md  
[8] 系统调用 API 规范。./agentos_contract/syscall/syscall_api_contract.md  
[9] 日志格式规范。./agentos_contract/log/logging_format.md  

---

**维护者**: AgentOS 架构委员会  
**联系方式**: architecture@agentos.org(暂)  
**贡献指南**: 欢迎通过 Pull Request 提交术语建议或修正