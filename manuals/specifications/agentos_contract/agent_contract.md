# AgentOS Agent 契约规范

**版本**: Doc V1.7  
**状态**: 正式发布  
**归属**: AgentOS 开放生态核心规范  
**作者**: LirenWang  
**最后更新**: 2026-03-31  
**许可证**: GPL-3.0  
**理论基础**: 工程两论（控制论与系统工程）、双系统认知理论、微内核哲学、设计美学  
**关联规范**: [架构设计原则](../architecture/ARCHITECTURAL_PRINCIPLES.md)、[统一术语表](../TERMINOLOGY.md)

---

## 编制说明

### 本文档定位

Agent 契约规范是 AgentOS 规范体系的核心组成部分，属于**操作层规范**。本规范直接指导 Agent 开发者和生态贡献者如何定义和实现符合 AgentOS 标准的智能体，是 Agent 注册、发现、调度和评估的技术依据。

### 与设计哲学的关系

本规范根植于 AgentOS 五维正交设计体系，是架构设计原则在契约层的具体体现：

- **系统观（S维度）**: Agent 契约通过层次分解方法体现系统工程思想，从顶层结构到能力细节逐层展开，符合"从定性到定量的综合集成"理念
- **内核观（K维度）**: 契约设计遵循微内核"最小信息原则"，只包含系统决策所必需的信息，保证内核纯净和服务外置
- **认知观（C维度）**: Agent 契约体现了"双系统认知模型"，通过 `models` 字段明确区分 System 1（快系统）和 System 2（慢系统）的配置，支持认知层的策略选择和资源匹配
- **工程观（E维度）**: 通过 `required_permissions` 字段将安全机制内嵌于契约定义中，体现安全内生原则；通过结构化错误码支持错误可追溯原则
- **设计美学（A维度）**: 契约 JSON 结构遵循简约、对称、自解释的美学原则，确保人类可读性和机器可处理性的平衡

### 适用范围

本规范适用于以下场景：

1. **Agent 开发者**: 编写符合 AgentOS 标准的 Agent 契约文件
2. **市场服务**: 解析和验证 Agent 契约，提供发现和推荐功能
3. **调度官组件**: 基于契约信息进行多目标优化和 Agent 选择
4. **进化委员会**: 根据契约指标评估 Agent 质量和进化方向

### 术语定义

本规范使用的术语定义见 [统一术语表](../TERMINOLOGY.md)，关键术语包括：

| 术语 | 简要定义 | 来源 |
|------|---------|------|
| Agent | 具有认知能力的智能体 | [Cognition_Theory.md](../../philosophy/Cognition_Theory.md) |
| 契约 (Contract) | 机器可读的能力描述文件 | 本规范 |
| 调度官 (Dispatcher) | 负责选择 Agent 执行任务的组件 | [Cognition_Theory.md](../../philosophy/Cognition_Theory.md) |
| 信任边界 (Trust Boundary) | 区分可信与不可信数据的界限 | [架构设计原则.md](../架构设计原则.md) |

---

## 第 1 章 引言

### 1.1 背景与意义

AgentOS 的核心愿景是实现"动态团队组建"——根据任务需求，从角色市场中自动选择最合适的 Agent 组成临时团队，任务完成后团队解散。这一过程要求系统能够发现、理解、评估每个 Agent 的能力，而无需人工介入。

**Agent 契约 (Agent Contract)** 正是为解决这一需求而设计的标准化能力描述文件。它是 Agent 的"简历"和"职位说明书",以机器可读的 JSON 格式定义 Agent 的能力、接口、成本、信任等元数据，供调度官、市场服务、进化委员会等组件使用。

### 1.2 目标与范围

本规范旨在实现以下目标：

1. **机器可读**: 契约必须能被程序自动解析，无需人工干预
2. **能力明确**: 清晰描述 Agent 能做什么、输入是什么、输出是什么
3. **可验证**: 契约本身应能通过 JSON Schema 验证，确保格式正确
4. **可比较**: 提供成本、性能、信任等量化指标，支持多目标优化选择
5. **可扩展**: 允许社区添加自定义字段，同时保证核心字段的兼容性

### 1.3 设计哲学

Agent 契约的设计遵循 AgentOS 的一贯思想 [1]：

- **契约即文档**: 契约是 Agent 与系统之间的唯一可信接口，所有交互均基于此
- **最小信息原则**: 契约只包含系统决策所必需的信息，不包含冗余或隐私数据
- **可验证性**: 契约必须通过严格校验才能被注册中心接受，保证生态质量
- **版本演进**: 契约版本与 Agent 版本解耦，支持向后兼容的更新

---

## 第 2 章 契约结构

### 2.1 顶层结构

Agent 契约采用层次化 JSON 对象结构，符合系统工程中的"层次分解"方法。

```json
{
  "schema_version": "string",
  "agent_id": "string",
  "agent_name": "string",
  "version": "string",
  "role": "string",
  "description": "string",
  "capabilities": [],
  "models": {},
  "required_permissions": [],
  "cost_profile": {},
  "trust_metrics": {},
  "extensions": {}
}
```

### 2.2 字段详细定义

#### 2.2.1 基础标识字段

| 字段 | 类型 | 必需 | 说明 | 示例 |
|------|------|------|------|------|
| `schema_version` | string | 是 | 契约规范的版本号，当前为 `"1.0.0"` | `"1.0.0"` |
| `agent_id` | string | 是 | Agent 唯一标识，全局唯一，建议使用反向域名或 UUID | `"com.example.product_manager.v1"` |
| `agent_name` | string | 是 | Agent 可读名称 | `"Product Manager Agent"` |
| `version` | string | 是 | Agent 自身版本号，遵循语义化版本规范 (MAJOR.MINOR.PATCH) | `"1.2.0"` |
| `role` | string | 是 | 角色分类，用于任务匹配和团队组建 | `"product_manager"` |
| `description` | string | 是 | Agent 的简要描述，供人类阅读 | `"理解用户需求，撰写产品需求文档"` |

#### 2.2.2 能力与配置字段

| 字段 | 类型 | 必需 | 说明 | 章节 |
|------|------|------|------|------|
| `capabilities` | array | 是 | 能力列表，每个能力是一个对象，描述 Agent 的具体功能 | 2.3 节 |
| `models` | object | 是 | 模型配置，定义 Agent 使用的双模型 (System 1 / System 2) | 2.4 节 |
| `required_permissions` | array | 是 | 所需权限列表，字符串数组，用于安全裁决 | 2.5 节 |

#### 2.2.3 评估与扩展字段

| 字段 | 类型 | 必需 | 说明 | 章节 |
|------|------|------|------|------|
| `cost_profile` | object | 是 | 成本概览，提供 Token 消耗和 API 成本预估 | 2.6 节 |
| `trust_metrics` | object | 是 | 信任指标，用于市场排序和调度优化 | 2.7 节 |
| `extensions` | object | 否 | 扩展字段，用于社区自定义，不影响核心功能 | 2.8 节 |

### 2.3 能力定义 (capabilities)

每个能力是一个对象，描述 Agent 的一项具体功能。能力是 Agent 契约的核心，决定了 Agent 能被用于什么场景。

#### 2.3.1 能力结构

| 字段 | 类型 | 必需 | 说明 | 示例 |
|------|------|------|------|------|
| `name` | string | 是 | 能力名称，建议使用蛇形命名法 (snake_case) | `"requirement_analysis"` |
| `description` | string | 是 | 能力描述，清晰说明该能力的用途和限制 | `"将用户模糊需求转化为结构化需求文档"` |
| `input_schema` | object | 是 | 输入数据的 JSON Schema，遵循 Draft-07 规范 | 见下方示例 |
| `output_schema` | object | 是 | 输出数据的 JSON Schema，遵循 Draft-07 规范 | 见下方示例 |
| `estimated_tokens` | integer | 否 | 单次调用预估 Token 数 (平均值),用于成本估算 | `5000` |
| `avg_duration_ms` | integer | 否 | 平均执行时间 (毫秒),用于性能评估 | `3000` |
| `success_rate` | number | 否 | 历史成功率 (0-1),可由注册中心动态更新 | `0.92` |

#### 2.3.2 输入输出 Schema 规范

`input_schema` 和 `output_schema` 遵循 **JSON Schema Draft-07** 规范，可使用 `$ref` 引用公共定义，减少重复。

**示例 1: 直接使用内联 Schema**

```json
{
  "name": "requirement_analysis",
  "description": "将用户模糊需求转化为结构化需求文档",
  "input_schema": {
    "type": "object",
    "properties": {
      "goal": {
        "type": "string",
        "description": "用户目标描述"
      },
      "context": {
        "type": "object",
        "description": "业务上下文信息"
      }
    },
    "required": ["goal"]
  },
  "output_schema": {
    "type": "object",
    "properties": {
      "prd": {
        "type": "string",
        "description": "产品需求文档"
      },
      "user_stories": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "用户故事列表"
      }
    }
  }
}
```

**示例 2: 使用引用 Schema**

```json
{
  "input_schema": {
    "$ref": "https://agentos.org/schemas/user_intent.json"
  },
  "output_schema": {
    "$ref": "https://agentos.org/schemas/analysis_result.json"
  }
}
```

### 2.4 模型配置 (models)

定义 Agent 使用的双模型 (System 1 / System 2),体现认知层的"双系统认知模型"理论。

#### 2.4.1 模型结构

| 字段 | 类型 | 必需 | 说明 | 示例 |
|------|------|------|------|------|
| `system1` | string | 是 | System 1 模型名称 (轻量、快速),用于快速响应和交叉验证 | `"gpt-3.5-turbo"` |
| `system2` | string | 是 | System 2 模型名称 (强大、深度),用于深度规划和复杂推理 | `"gpt-4"` |

> **注意**: 模型名称应与 `manager/models.yaml` 中定义的名称一致。

#### 2.4.2 双模型协同机制

根据认知层理论 [2],双模型协同的工作方式如下:

- **System 1(辅思考者)**: 负责快速响应、模式识别、交叉验证，耗能低
- **System 2(主思考者)**: 负责深度规划、逻辑推理、反思调整，耗能高
- **仲裁机制**: 当主辅模型结论不一致时，触发内部辩论或人工确认

### 2.5 权限声明 (required_permissions)

所需权限列表，用于安全层的权限裁决。遵循"最小权限原则",只申请完成任务所必需的权限。

**示例:**

```json
{
  "required_permissions": [
    "read_project_context",
    "write_project_artifacts",
    "network:outbound:api.github.com"
  ]
}
```

**权限命名规范:**

- 格式：`<资源>:<操作>:<范围>`
- 资源类型：`read`, `write`, `execute`, `network`, `filesystem` 等
- 操作类型：`inbound`, `outbound`, `internal` 等
- 范围限定：具体的资源路径或域名

### 2.6 成本概览 (cost_profile)

提供 Token 消耗和 API 成本的预估，供调度官进行多目标优化。

#### 2.6.1 成本结构

| 字段 | 类型 | 必需 | 说明 | 示例 |
|------|------|------|------|------|
| `token_per_task_avg` | integer | 是 | 单次任务平均 Token 消耗 (预估值) | `8000` |
| `api_cost_per_task` | number | 是 | 单次任务平均 API 成本 (美元，预估值) | `0.02` |
| `maintenance_level` | string | 是 | 维护级别，反映 Agent 的质量和可靠性 | `"verified"` |

#### 2.6.2 维护级别

`maintenance_level` 字段定义如下枚举值：

| 级别 | 说明 | 审核要求 |
|------|------|---------|
| `"community"` | 社区维护，未经官方审核 | 无特殊要求 |
| `"verified"` | 已验证，通过基础功能和安全性测试 | 自动化测试 + 人工初审 |
| `"official"` | 官方维护，经过全面审计和优化 | 完整审计流程 + 定期复审 |

### 2.7 信任指标 (trust_metrics)

信任指标用于市场排序和调度优化，可由注册中心动态更新。

#### 2.7.1 信任指标结构

| 字段 | 类型 | 必需 | 说明 | 数据来源 |
|------|------|------|------|----------|
| `install_count` | integer | 是 | 安装次数，由市场服务统计 | 市场服务动态更新 |
| `rating` | number | 是 | 用户评分 (1-5),反映用户满意度 | 用户评价聚合 |
| `verified_provider` | boolean | 是 | 是否经过官方认证的服务提供商 | 认证系统 |
| `last_audit` | string | 是 | 上次审计日期，ISO 8601 格式 | 审计记录 |

**示例:**

```json
{
  "trust_metrics": {
    "install_count": 1240,
    "rating": 4.7,
    "verified_provider": true,
    "last_audit": "2026-03-01"
  }
}
```

> **注意**: `install_count`、`rating`、`last_audit` 可由注册中心动态更新，契约文件可提供初始值。

### 2.8 扩展字段 (extensions)

扩展字段用于社区自定义，支持未来功能演进，不影响核心兼容性。

**示例:**

```json
{
  "extensions": {
    "preferred_llm_provider": "openai",
    "custom_metadata": {
      "author": "agentos-community",
      "license": "MIT",
      "homepage": "https://github.com/agentos/product-manager"
    }
  }
}
```

---

## 第 3 章 完整示例

以下是一个产品经理 Agent 的完整契约示例，展示所有字段的实际应用：

```json
{
  "schema_version": "1.0.0",
  "agent_id": "com.agentos.product_manager.v1",
  "agent_name": "Product Manager Agent",
  "version": "1.2.0",
  "role": "product_manager",
  "description": "理解用户需求，撰写产品需求文档 (PRD),拆解用户故事。",
  "capabilities": [
    {
      "name": "requirement_analysis",
      "description": "将用户模糊需求转化为结构化需求文档",
      "input_schema": {
        "type": "object",
        "properties": {
          "goal": {
            "type": "string",
            "description": "用户目标描述"
          },
          "context": {
            "type": "object",
            "description": "业务上下文信息"
          }
        },
        "required": ["goal"]
      },
      "output_schema": {
        "type": "object",
        "properties": {
          "prd": {
            "type": "string",
            "description": "产品需求文档"
          },
          "user_stories": {
            "type": "array",
            "items": {
              "type": "string"
            },
            "description": "用户故事列表"
          }
        }
      },
      "estimated_tokens": 5000,
      "avg_duration_ms": 3000,
      "success_rate": 0.92
    },
    {
      "name": "prd_generation",
      "description": "生成完整的产品需求文档",
      "input_schema": {
        "type": "object",
        "properties": {
          "structured_requirements": {
            "type": "object",
            "description": "结构化需求数据"
          }
        }
      },
      "output_schema": {
        "type": "object",
        "properties": {
          "prd": {
            "type": "string",
            "description": "完整的 PRD 文档"
          }
        }
      }
    }
  ],
  "models": {
    "system1": "gpt-3.5-turbo",
    "system2": "gpt-4"
  },
  "required_permissions": [
    "read_project_context",
    "write_project_artifacts"
  ],
  "cost_profile": {
    "token_per_task_avg": 8000,
    "api_cost_per_task": 0.02,
    "maintenance_level": "verified"
  },
  "trust_metrics": {
    "install_count": 1240,
    "rating": 4.7,
    "verified_provider": true,
    "last_audit": "2026-03-01"
  },
  "extensions": {
    "preferred_llm_provider": "openai"
  }
}
```

---

## 第 4 章 契约验证

### 4.1 JSON Schema 验证

契约必须符合 JSON Schema 定义。AgentOS 提供官方 Schema 文件 [`agent_contract_schema.json`](./agent_contract_schema.json),可通过任何 JSON Schema 验证器检查。

**验证内容包括:**

1. **必需字段检查**: 所有标记为"是"的字段必须存在
2. **字段类型检查**: 字段类型必须与定义一致 (string, number, object, array, boolean)
3. **枚举值检查**: 对于有限定值的字段 (如 `role`, `maintenance_level`),必须在允许的枚举范围内
4. **Schema 有效性检查**: `input_schema` 和 `output_schema` 必须是有效的 JSON Schema

**验证工具使用示例:**

```bash
# 使用官方 CLI 工具验证
agentos validate-contract ./my_agent/contract.json

# 使用在线验证器
curl -X POST https://agentos.org/api/validate \
  -H "Content-Type: application/json" \
  -d @contract.json
```

### 4.2 语义验证

除格式外，还应进行语义验证，确保契约的实际可用性：

1. **唯一性检查**: `agent_id` 必须在注册中心中唯一，由注册中心保证
2. **版本规范性检查**: `version` 字段必须符合语义化版本规范 (MAJOR.MINOR.PATCH)
3. **模型存在性检查**: `models` 中的模型名称必须在 `manager/models.yaml` 中存在
4. **权限合法性检查**: `required_permissions` 中的权限必须在安全策略中定义
5. **能力合理性检查**: 能力描述与实际功能是否一致，避免夸大或误导

### 4.3 验证工具

AgentOS 提供命令行工具 `agentos validate-contract`,用于快速验证契约文件：

```bash
# 基本用法
agentos validate-contract ./my_agent/contract.json

# 详细输出模式
agentos validate-contract ./my_agent/contract.json --verbose

# 仅检查语法，不检查语义
agentos validate-contract ./my_agent/contract.json --syntax-only
```

**验证流程:**

1. 词法和语法分析，检查 JSON 格式
2. JSON Schema 验证，检查字段完整性
3. 语义验证，检查业务规则
4. 输出验证报告，包含错误和建议

---

## 第 5 章 版本管理

### 5.1 契约版本 vs Agent 版本

Agent 契约涉及两个版本概念，需明确区分：

| 版本类型 | 字段名 | 维护方 | 递增规则 | 示例 |
|---------|--------|--------|---------|------|
| **契约规范版本** | `schema_version` | AgentOS 官方 | 当规范发生不兼容变更时递增 | `"1.0.0"` |
| **Agent 自身版本** | `version` | Agent 开发者 | 遵循语义化版本规范，功能变更时递增 | `"1.2.0"` |

**语义化版本规范 (Semantic Versioning):**

- **MAJOR**(主版本号): 当发生不兼容的功能变更时递增
- **MINOR**(次版本号): 当新增向后兼容的功能时递增
- **PATCH**(修订号): 当进行向后兼容的问题修复时递增

### 5.2 兼容性原则

为确保生态系统的稳定性，契约设计遵循以下兼容性原则：

#### 5.2.1 向后兼容 (Backward Compatibility)

新版本契约应能兼容旧版本消费者：

- **消费者忽略未知字段**: 旧版本的调度官在解析新版本的契约时，应忽略不认识的扩展字段
- **默认值处理**: 对于新增的可选字段，应提供合理的默认值
- **渐进式升级**: 允许新旧版本契约并存，逐步迁移

#### 5.2.2 向前兼容 (Forward Compatibility)

旧版本契约应能被新版本消费者正确解析：

- **版本判断**: 新版本的调度官应根据 `schema_version` 判断契约版本，采用相应的解析策略
- **降级处理**: 对于不支持的新特性，应提供降级方案或明确的错误提示

### 5.3 版本变更管理

**变更类型与版本号递增规则:**

| 变更类型 | 影响范围 | `schema_version` 变更 | 示例 |
|---------|---------|---------------------|------|
| 新增可选字段 | 向后兼容 | PATCH 递增 | `1.0.0` → `1.0.1` |
| 新增必选字段 (有默认值) | 向后兼容 | MINOR 递增 | `1.0.0` → `1.1.0` |
| 删除或修改现有字段 | 不兼容 | MAJOR 递增 | `1.0.0` → `2.0.0` |
| 修改字段类型或约束 | 不兼容 | MAJOR 递增 | `1.0.0` → `2.0.0` |

---

## 第 6 章 最佳实践

### 6.1 如何编写高质量的契约

#### 实践 6-1【明确能力边界】

每个能力应只做一件事，避免"万能"能力。**单一职责原则**有助于提高可维护性和可测试性。

**✅ 推荐:**

```json
{
  "capabilities": [
    {
      "name": "requirement_analysis",
      "description": "将用户模糊需求转化为结构化需求文档"
    },
    {
      "name": "prd_generation",
      "description": "基于结构化需求生成完整的 PRD 文档"
    }
  ]
}
```

**❌ 不推荐:**

```json
{
  "capabilities": [
    {
      "name": "do_everything",
      "description": "处理所有产品管理相关任务，包括需求分析、PRD 撰写、用户故事拆解、原型设计等"
    }
  ]
}
```

#### 实践 6-2【提供清晰示例】

在 `description` 中可附带示例输入输出，帮助调用方理解如何使用该能力。但示例不应影响机器解析，建议放在描述的末尾。

**示例:**

```json
{
  "description": "将用户模糊需求转化为结构化需求文档。\n\n示例输入:\n{\"goal\": \"我想做一个电商应用\"}\n\n示例输出:\n{\"prd\": \"...\", \"user_stories\": [...]}"
}
```

#### 实践 6-3【合理预估成本】

基于真实测试给出 Token 和成本预估，避免严重偏离。建议在不同场景下进行多次测试，取平均值。

**测试方法:**

1. 准备代表性测试集 (至少 10 个样本)
2. 在典型环境下运行测试
3. 记录每次的 Token 消耗和执行时间
4. 计算平均值和标准差
5. 在契约中填写平均值，并在 `extensions` 中可选地提供标准差

#### 实践 6-4【定期审计更新】

保持 `trust_metrics.last_audit` 更新，反映 Agent 的最新状态。建议至少每季度审计一次，或在重大更新后立即审计。

**审计内容包括:**

- 功能完整性测试
- 性能基准测试
- 安全性扫描
- 依赖项更新检查
- 文档和示例更新

#### 实践 6-5【使用公共 Schema 引用】

对于通用数据类型 (如 `user_intent`, `task_result`),使用 `$ref` 引用官方 Schema，减少重复定义，提高一致性。

**官方 Schema 仓库:**

- GitHub: https://github.com/agentos/schemas
- Gitee: https://gitee.com/agentos/schemas

### 6.2 常见错误与避免方法

#### 错误 6-1【缺失必需字段】

**问题**: 导致验证失败，无法注册。

**解决方法**: 使用验证工具在提交前检查，或集成到 CI/CD 流程中自动验证。

#### 错误 6-2【输入输出 Schema 过于宽松】

**问题**: 导致调用方无法确定数据结构，增加集成难度。

**❌ 不推荐:**

```json
{
  "input_schema": {
    "type": "object"
  }
}
```

**✅ 推荐:**

```json
{
  "input_schema": {
    "type": "object",
    "properties": {
      "goal": {
        "type": "string",
        "description": "用户目标描述"
      },
      "constraints": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "约束条件列表"
      }
    },
    "required": ["goal"]
  }
}
```

#### 错误 6-3【信任指标虚高】

**问题**: 系统会动态调整，但初始值应如实填写，否则会影响用户信任。

**建议**: 新 Agent 可不填写 `install_count` 和 `rating`,由注册中心从 0 开始统计。

#### 错误 6-4【版本号不规范】

**问题**: 应严格遵循语义化版本规范，避免使用非标准格式。

**❌ 不推荐:** `"v1.2"`, `"1.2.beta"`, `"latest"`

**✅ 推荐:** `"1.2.0"`, `"2.0.0-beta.1"`, `"1.2.3-rc.2"`

---

## 第 7 章 与其他模块的协同关系

### 7.1 与调度官的协同

**调度官 (Dispatcher)** 根据契约信息进行多目标优化选择 [2]:

- **能力匹配**: 通过 `capabilities` 和 `role` 字段匹配任务需求
- **成本优化**: 基于 `cost_profile` 选择性价比最优的 Agent
- **信任优先**: 根据 `trust_metrics` 优先选择高信任度 Agent
- **性能权衡**: 结合 `avg_duration_ms` 和 `success_rate` 平衡速度与质量

**调度公式示例:**

```
Score(agent) = w1 * (1 / cost) + w2 * success_rate + w3 * trust_score + w4 * (1 / duration)
```

权重 `w1, w2, w3, w4` 可配置，支持成本优先、性能优先、信任优先等策略。

### 7.2 与市场服务的协同

**市场服务 (Market Service)** 基于契约信息提供排序和推荐：

- **分类展示**: 根据 `role` 字段对 Agent 进行分类
- **搜索索引**: 基于 `agent_name`, `description`, `capabilities.name` 建立全文索引
- **排行榜**: 基于 `trust_metrics.install_count` 和 `rating` 生成热门榜单
- **个性化推荐**: 根据用户历史和偏好推荐相关 Agent

### 7.3 与团队委员会的协同

**团队委员会 (Team Committee)** 根据运行反馈更新契约指标：

- **动态更新**: 根据实际运行数据更新 `success_rate`, `avg_duration_ms`
- **异常检测**: 检测契约声明值与实际值的偏差，发出警告
- **进化建议**: 基于统计数据提出契约优化建议

### 7.4 与安全服务的协同

**安全服务 (Security Service)** 根据 `required_permissions` 进行权限裁决：

- **权限预分配**: 在 Agent 启动时根据契约分配权限
- **运行时检查**: 在敏感操作前检查权限是否足够
- **审计日志**: 记录权限使用情况，用于事后审计

---

## 第 8 章 结语

Agent 契约是 AgentOS 开放生态的基石。它使得智能体可以像微服务一样被动态发现、组合和替换，为"动态团队组建"提供了技术基础。我们鼓励社区开发者遵循本规范，贡献高质量的 Agent，共同构建繁荣的智能体市场。

**展望未来:**

- **阶段 1(已完成)**: 制定 Agent Contract 规范，开发验证工具
- **阶段 2(进行中)**: 开发参考实现 Agent，建立认证机制
- **阶段 3(计划中)**: 支持动态团队组建，实现跨平台技能市场

---

## 附录 A: 快速索引

### A.1 按主题分类

**契约结构**: 第 2 章  
**字段定义**: 2.2 节  
**能力定义**: 2.3 节  
**模型配置**: 2.4 节  
**权限声明**: 2.5 节  
**成本概览**: 2.6 节  
**信任指标**: 2.7 节  
**完整示例**: 第 3 章  
**契约验证**: 第 4 章  
**版本管理**: 第 5 章  
**最佳实践**: 第 6 章  

### A.2 按规则编号

- **实践 6-1**: 明确能力边界
- **实践 6-2**: 提供清晰示例
- **实践 6-3**: 合理预估成本
- **实践 6-4**: 定期审计更新
- **实践 6-5**: 使用公共 Schema 引用

---

## 附录 B: 与其他规范的引用关系

| 引用规范 | 关系说明 |
|---------|---------||
| [架构设计原则](../../architecture/架构设计原则.md) | 本规范是架构原则在 AgentOS 生态的具体实现，特别是微内核思想和安全内生原则 |
| [统一术语表](../TERMINOLOGY.md) | 本规范使用的术语定义和解释，如 Agent、契约、调度官等 |
| [C&C++ 安全编程指南](../../specifications/coding_standard/C&Cpp-secure-coding-guide.md) | Agent 的实现应遵循安全编程指南，特别是在权限检查和输入验证方面 |
| [日志打印规范](../../specifications/coding_standard/Log_guide.md) | Agent 运行时应遵循日志规范，记录关键操作和异常情况 |

---

## 参考文献

[1] AgentOS 设计哲学。../../philosophy/Design_Principles.md  
[2] AgentOS 认知层理论。../../philosophy/Cognition_Theory.md  
[3] 架构设计原则。../架构设计原则.md  
[4] 统一术语表。../TERMINOLOGY.md  
[5] JSON Schema Draft-07 Specification. https://json-schema.org/draft-07/json-schema-release.html  
[6] Semantic Versioning 2.0.0. https://semver.org/  

---

## 版本历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| v1.0.2 | 2026-03-22 | AgentOS 架构委员会 | 基于设计哲学和认知层理论进行全面重构，优化结构和表达 |
| v1.0.1 | - | 初始版本 | 初始发布 |

---

**最后更新**: 2026-03-22  
**维护者**: AgentOS 架构委员会
