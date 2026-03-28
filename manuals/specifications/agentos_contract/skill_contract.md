# AgentOS Skill 契约规范

**版本**: Doc V1.6  
**状态**: 正式发布  
**备选名称**: OpenApex (开源极境 / 极境OS)  
**归属**: AgentOS 开放生态核心规范  
**许可证**: GPL-3.0  
**理论基础**: 工程两论（控制论与系统工程）、双系统认知理论、微内核哲学、设计美学  
**关联规范**: [Agent 契约规范](./agent_contract.md)、[架构设计原则](../架构设计原则.md)、[统一术语表](../TERMINOLOGY.md)

---

## 编制说明

### 本文档定位

Skill 契约规范是 AgentOS 规范体系的核心组成部分，属于**操作层规范**。本规范定义了技能 (Skill) 的标准化能力描述格式，指导技能开发者如何封装、发布和共享可复用的执行单元，是技能市场、工具服务和权限裁决的技术依据。

### 与设计哲学的关系

本规范根植于 AgentOS 五维正交设计体系，是架构设计原则在技能层的具体体现：

- **系统观（S维度）**: 技能按类型分层（tool/code/api/file/browser/db/shell）体现层次分解方法，每类技能有特定的执行环境和权限边界
- **内核观（K维度）**: 技能外置于内核，通过系统调用与内核交互，体现微内核思想，保证内核纯净和服务隔离
- **认知观（C维度）**: 技能作为认知扩展，通过统一接口支持双系统认知模型的策略选择和资源匹配
- **工程观（E维度）**: 通过 `required_permissions` 和 `dependencies` 字段将安全和依赖管理内嵌于契约定义中，体现安全内生原则；通过结构化错误码支持错误可追溯原则
- **设计美学（A维度）**: 技能契约 JSON 结构遵循简约、对称、自解释的美学原则，确保人类可读性和机器可处理性的平衡

### 适用范围

本规范适用于以下场景：

1. **Skill 开发者**: 编写符合 AgentOS 标准的 Skill 契约文件
2. **工具服务 (`tool_d`)**: 解析和验证 Skill 契约，加载和执行技能
3. **市场服务 (`market_d`)**: 存储和管理技能注册信息，提供安装和分发功能
4. **权限引擎 (`domain`)**: 根据契约声明进行权限裁决和资源分配

### 术语定义

本规范使用的术语定义见 [统一术语表](../TERMINOLOGY.md),关键术语包括：

| 术语 | 简要定义 | 来源 |
|------|---------|------|
| Skill(技能) | 可复用的执行单元，为 Agent 提供具体能力 | [Design_Principles.md](../../philosophy/Design_Principles.md) |
| 执行单元 (Execution Unit) | 行动层的基本执行单位，如工具、代码、API 等 | [Cognition_Theory.md](../../philosophy/Cognition_Theory.md) |
| 契约 (Contract) | 机器可读的能力描述文件 | 本规范 |
| 工具服务 (`tool_d`) | 负责加载和执行技能的守护进程 | 本规范 |

---

## 第 1 章 引言

### 1.1 背景与意义

AgentOS 的行动层通过执行单元 (Execution Unit) 来执行具体操作，例如调用 API、运行代码、操作文件等。**技能 (Skill)** 是可复用的执行单元，可以被安装、卸载和动态加载，为 Agent 提供具体能力。**技能市场 (Skill Market)** 是社区贡献技能的中心，用户可以通过 `agentos skill install` 命令安装技能，并通过 `agentos skill execute` 调用。

**技能契约 (Skill Contract)** 是技能的能力描述文件，类似于 Agent 契约，但面向执行单元。它定义了技能的输入输出、权限需求、依赖、版本等元数据，供工具服务 (`tool_d`) 和调度官使用。

### 1.2 目标与范围

本规范旨在实现以下目标：

1. **机器可读**: 技能契约必须能被工具服务自动解析，用于安装、验证和执行
2. **接口明确**: 清晰定义技能的输入和输出 JSON Schema，确保调用方正确传参
3. **可验证**: 契约必须通过 JSON Schema 验证，保证格式正确
4. **依赖管理**: 支持技能依赖的外部库、系统工具、其他技能，实现自动化安装
5. **安全声明**: 技能必须声明所需权限，供权限引擎裁决

### 1.3 设计哲学

Skill 契约的设计遵循 AgentOS 的一贯思想 [1]：

- **契约即文档**: 技能契约是技能与系统之间的唯一接口，所有交互均基于此
- **最小权限原则**: 技能只申请完成任务所必需的最小权限，避免权限滥用
- **可验证性**: 契约必须通过严格校验才能被注册中心接受，保证生态质量
- **版本兼容**: 技能版本遵循语义化版本，契约版本与技能版本解耦，支持渐进式升级

---

## 第 2 章 契约结构

### 2.1 顶层结构

Skill 契约采用层次化 JSON 对象结构，符合系统工程中的"层次分解"方法。

```json
{
  "schema_version": "string",
  "skill_id": "string",
  "skill_name": "string",
  "version": "string",
  "description": "string",
  "type": "string",
  "executable": {},
  "tools": [],
  "dependencies": {},
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
| `skill_id` | string | 是 | Skill 唯一标识，全局唯一，建议使用反向域名或 UUID | `"com.example.github_skill.v1"` |
| `skill_name` | string | 是 | Skill 可读名称 | `"GitHub Integration Skill"` |
| `version` | string | 是 | Skill 自身版本号，遵循语义化版本规范 (MAJOR.MINOR.PATCH) | `"2.1.0"` |
| `description` | string | 是 | Skill 的简要描述，供人类阅读 | `"提供 GitHub 仓库操作、PR 管理等功能"` |
| `type` | string | 是 | Skill 类型，对应执行单元类型，决定执行环境 | `"tool"` |

#### 2.2.2 Skill 类型枚举

`type` 字段定义如下枚举值，每种类型对应不同的执行环境和安全策略：

| 类型 | 说明 | 执行环境 | 典型用途 |
|------|------|---------|---------|
| `"tool"` | 工具类技能 | 动态链接库 (.so/.dll) 或脚本 | 系统工具、CLI 命令封装 |
| `"code"` | 代码类技能 | 沙箱中的代码解释器 | Python/JavaScript 代码执行 |
| `"api"` | API 类技能 | HTTP 客户端 | RESTful API 调用 |
| `"file"` | 文件类技能 | 受限文件系统访问 | 文件读写、目录操作 |
| `"browser"` | 浏览器类技能 | 无头浏览器实例 | Web 自动化、爬虫 |
| `"db"` | 数据库类技能 | 数据库连接池 | SQL 查询、数据操作 |
| `"shell"` | Shell 类技能 | 受限 Shell 环境 | 系统命令执行 |

#### 2.2.3 能力与配置字段

| 字段 | 类型 | 必需 | 说明 | 章节 |
|------|------|------|------|------|
| `executable` | object | 条件 | 可执行文件信息，对于编译型技能必需 | 2.3 节 |
| `tools` | array | 条件 | 技能提供的工具列表，对于工具类技能必需 | 2.4 节 |
| `dependencies` | object | 否 | 依赖声明，描述运行时所需的资源 | 2.5 节 |
| `required_permissions` | array | 是 | 所需权限列表，字符串数组 | 2.6 节 |

#### 2.2.4 评估与扩展字段

| 字段 | 类型 | 必需 | 说明 | 章节 |
|------|------|------|------|------|
| `cost_profile` | object | 是 | 成本概览，提供 Token 消耗和 API 成本预估 | 2.7 节 |
| `trust_metrics` | object | 是 | 信任指标，用于市场排序和调度优化 | 2.8 节 |
| `extensions` | object | 否 | 扩展字段，用于社区自定义 | 2.9 节 |

### 2.3 可执行文件信息 (executable)

对于编译型技能 (如 C/C++ 编写的动态库),需要提供可执行文件信息。

#### 2.3.1 Executable 结构

| 字段 | 类型 | 必需 | 说明 | 示例 |
|------|------|------|------|------|
| `path` | string | 是 | 可执行文件或动态库的相对路径 | `"./github_skill.so"` |
| `entry` | string | 是 | 入口函数名称，符合 C ABI 规范 | `"github_skill_entry"` |
| `args` | array | 否 | 命令行参数列表 (可选) | `["--verbose", "--timeout=30"]` |

**示例:**

```json
{
  "executable": {
    "path": "./github_skill.so",
    "entry": "github_skill_entry",
    "args": ["--verbose"]
  }
}
```

> **注意**: 对于脚本型技能 (如 Python/JavaScript),不需要 `executable` 字段，工具服务会根据文件扩展名自动选择解释器。

### 2.4 工具定义 (tools)

每个技能可以提供一个或多个工具 (类似于函数)。每个工具对象定义如下：

#### 2.4.1 工具结构

| 字段 | 类型 | 必需 | 说明 | 示例 |
|------|------|------|------|------|
| `name` | string | 是 | 工具名称，建议使用蛇形命名法 (snake_case) | `"github_create_repo"` |
| `description` | string | 是 | 工具描述，清晰说明该工具的用途和限制 | `"创建 GitHub 仓库"` |
| `input_schema` | object | 是 | 输入数据的 JSON Schema，遵循 Draft-07 规范 | 见下方示例 |
| `output_schema` | object | 是 | 输出数据的 JSON Schema，遵循 Draft-07 规范 | 见下方示例 |
| `estimated_tokens` | integer | 否 | 单次调用预估 Token 数 (平均值) | `50` |
| `avg_duration_ms` | integer | 否 | 平均执行时间 (毫秒) | `500` |
| `success_rate` | number | 否 | 历史成功率 (0-1) | `0.98` |

#### 2.4.2 输入输出 Schema 规范

`input_schema` 和 `output_schema` 遵循 **JSON Schema Draft-07**,可使用 `$ref` 引用公共定义。

**示例 1: 创建 GitHub 仓库**

```json
{
  "name": "github_create_repo",
  "description": "创建 GitHub 仓库",
  "input_schema": {
    "type": "object",
    "properties": {
      "name": {
        "type": "string",
        "description": "仓库名称"
      },
      "private": {
        "type": "boolean",
        "description": "是否为私有仓库",
        "default": true
      },
      "description": {
        "type": "string",
        "description": "仓库描述"
      }
    },
    "required": ["name"]
  },
  "output_schema": {
    "type": "object",
    "properties": {
      "repo_url": {
        "type": "string",
        "description": "仓库 HTML URL"
      },
      "clone_url": {
        "type": "string",
        "description": "仓库克隆 URL"
      },
      "id": {
        "type": "integer",
        "description": "仓库 ID"
      }
    }
  }
}
```

**示例 2: 列出 Pull Requests**

```json
{
  "name": "github_list_prs",
  "description": "列出仓库的 Pull Requests",
  "input_schema": {
    "type": "object",
    "properties": {
      "owner": {
        "type": "string",
        "description": "仓库所有者"
      },
      "repo": {
        "type": "string",
        "description": "仓库名称"
      },
      "state": {
        "type": "string",
        "enum": ["open", "closed", "all"],
        "description": "PR 状态",
        "default": "open"
      }
    },
    "required": ["owner", "repo"]
  },
  "output_schema": {
    "type": "array",
    "items": {
      "type": "object",
      "properties": {
        "number": {
          "type": "integer",
          "description": "PR 编号"
        },
        "title": {
          "type": "string",
          "description": "PR 标题"
        },
        "state": {
          "type": "string",
          "description": "PR 状态"
        },
        "user": {
          "type": "string",
          "description": "提交者用户名"
        }
      }
    }
  }
}
```

### 2.5 依赖声明 (dependencies)

依赖对象用于描述技能运行时所需的资源，支持自动化安装和冲突检测。

#### 2.5.1 依赖结构

| 字段 | 类型 | 必需 | 说明 | 示例 |
|------|------|------|------|------|
| `libraries` | array | 否 | 系统库名称列表，用于动态链接 | `["libcurl", "openssl"]` |
| `packages` | array | 否 | 编程语言包，每个对象包含 `language` 和 `name` | 见下方示例 |
| `commands` | array | 否 | 外部命令名称，必须在系统 PATH 中 | `["git", "docker"]` |
| `skills` | array | 否 | 依赖的其他 Skill ID,支持技能组合 | `["com.github.json_parser.v1"]` |

**示例:**

```json
{
  "dependencies": {
    "libraries": ["libcurl", "libssl"],
    "packages": [
      {
        "language": "python",
        "name": "requests>=2.28.0"
      },
      {
        "language": "python",
        "name": "PyGithub>=1.55"
      }
    ],
    "commands": ["git", "docker"],
    "skills": []
  }
}
```

#### 2.5.2 依赖解析策略

工具服务在安装技能时，会按以下顺序解析依赖：

1. **系统库检查**: 检查 `libraries` 中列出的库是否在系统中存在
2. **包管理器安装**: 对于 `packages`,调用相应的包管理器 (pip/npm/cargo 等) 安装
3. **命令检查**: 检查 `commands` 中的命令是否可执行
4. **技能依赖**: 递归安装 `skills` 中列出的其他技能

### 2.6 权限声明 (required_permissions)

所需权限列表，用于安全层的权限裁决。遵循"最小权限原则",只申请完成任务所必需的权限。

**示例:**

```json
{
  "required_permissions": [
    "network:outbound:api.github.com",
    "read:repo",
    "write:repo"
  ]
}
```

**权限命名规范:**

- 格式：`<资源>:<操作>:<范围>`
- 资源类型：`network`, `filesystem`, `process`, `registry` 等
- 操作类型：`inbound`, `outbound`, `read`, `write`, `execute` 等
- 范围限定：具体的资源路径、域名或端口

**常见权限示例:**

```json
{
  "network:outbound:api.github.com": "访问 GitHub API",
  "network:outbound:*:443": "访问所有 HTTPS 服务",
  "filesystem:read:/tmp/*": "读取/tmp 目录下的文件",
  "filesystem:write:./output/*": "写入当前目录下的 output 文件夹",
  "process:execute:git": "执行 git 命令",
  "registry:read:HKEY_CURRENT_USER\\Software\\MyApp": "读取注册表项"
}
```

### 2.7 成本概览 (cost_profile)

提供 Token 消耗和 API 成本的预估，供调度官进行多目标优化。

#### 2.7.1 成本结构

| 字段 | 类型 | 必需 | 说明 | 示例 |
|------|------|------|------|------|
| `token_per_task_avg` | integer | 是 | 单次任务平均 Token 消耗 (预估值) | `500` |
| `api_cost_per_task` | number | 是 | 单次任务平均 API 成本 (美元，预估值) | `0.001` |
| `maintenance_level` | string | 是 | 维护级别，反映技能的质量和可靠性 | `"verified"` |

#### 2.7.2 维护级别

`maintenance_level` 字段定义如下枚举值：

| 级别 | 说明 | 审核要求 |
|------|------|---------|
| `"community"` | 社区维护，未经官方审核 | 无特殊要求 |
| `"verified"` | 已验证，通过基础功能和安全性测试 | 自动化测试 + 人工初审 |
| `"official"` | 官方维护，经过全面审计和优化 | 完整审计流程 + 定期复审 |

### 2.8 信任指标 (trust_metrics)

信任指标用于市场排序和调度优化，可由注册中心动态更新。

#### 2.8.1 信任指标结构

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
    "install_count": 3420,
    "rating": 4.8,
    "verified_provider": true,
    "last_audit": "2026-03-15"
  }
}
```

> **注意**: `install_count`、`rating`、`last_audit` 可由注册中心动态更新，契约文件可提供初始值。

### 2.9 扩展字段 (extensions)

扩展字段用于社区自定义，支持未来功能演进，不影响核心兼容性。

**示例:**

```json
{
  "extensions": {
    "author": "agentos-community",
    "license": "MIT",
    "homepage": "https://github.com/agentos/github-skill",
    "repository": {
      "type": "git",
      "url": "https://github.com/agentos/github-skill.git"
    },
    "keywords": ["github", "git", "repository", "pull-request"]
  }
}
```

---

## 第 3 章 完整示例

以下是一个 GitHub 集成技能的完整契约示例，展示所有字段的实际应用：

```json
{
  "schema_version": "1.0.0",
  "skill_id": "com.agentos.github_skill.v1",
  "skill_name": "GitHub Integration Skill",
  "version": "2.1.0",
  "description": "提供 GitHub 仓库操作、PR 管理、代码审查等功能。",
  "type": "tool",
  "executable": {
    "path": "./github_skill.so",
    "entry": "github_skill_entry"
  },
  "tools": [
    {
      "name": "github_create_repo",
      "description": "创建 GitHub 仓库",
      "input_schema": {
        "type": "object",
        "properties": {
          "name": {
            "type": "string",
            "description": "仓库名称"
          },
          "private": {
            "type": "boolean",
            "description": "是否为私有仓库",
            "default": true
          }
        },
        "required": ["name"]
      },
      "output_schema": {
        "type": "object",
        "properties": {
          "repo_url": {
            "type": "string",
            "description": "仓库 HTML URL"
          },
          "clone_url": {
            "type": "string",
            "description": "仓库克隆 URL"
          }
        }
      },
      "estimated_tokens": 50,
      "avg_duration_ms": 500,
      "success_rate": 0.98
    },
    {
      "name": "github_list_prs",
      "description": "列出仓库的 Pull Requests",
      "input_schema": {
        "type": "object",
        "properties": {
          "owner": {
            "type": "string",
            "description": "仓库所有者"
          },
          "repo": {
            "type": "string",
            "description": "仓库名称"
          },
          "state": {
            "type": "string",
            "enum": ["open", "closed", "all"],
            "description": "PR 状态",
            "default": "open"
          }
        },
        "required": ["owner", "repo"]
      },
      "output_schema": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "number": {
              "type": "integer",
              "description": "PR 编号"
            },
            "title": {
              "type": "string",
              "description": "PR 标题"
            },
            "state": {
              "type": "string",
              "description": "PR 状态"
            }
          }
        }
      }
    }
  ],
  "dependencies": {
    "libraries": ["libcurl", "libssl"],
    "packages": [
      {
        "language": "python",
        "name": "PyGithub>=1.55"
      }
    ],
    "commands": ["git"],
    "skills": []
  },
  "required_permissions": [
    "network:outbound:api.github.com",
    "read:repo",
    "write:repo"
  ],
  "cost_profile": {
    "token_per_task_avg": 500,
    "api_cost_per_task": 0.001,
    "maintenance_level": "verified"
  },
  "trust_metrics": {
    "install_count": 3420,
    "rating": 4.8,
    "verified_provider": true,
    "last_audit": "2026-03-15"
  },
  "extensions": {
    "author": "agentos-community",
    "license": "MIT",
    "homepage": "https://github.com/agentos/github-skill"
  }
}
```

---

## 第 4 章 契约验证

### 4.1 JSON Schema 验证

契约必须符合官方 JSON Schema。AgentOS 提供 [`skill_contract_schema.json`](./skill_contract_schema.json),可用任何 JSON Schema 验证器检查。

**验证内容包括:**

1. **必需字段检查**: 所有标记为"是"或"条件必需"的字段必须存在
2. **字段类型检查**: 字段类型必须与定义一致 (string, number, object, array, boolean)
3. **枚举值检查**: 对于有限定值的字段 (如 `type`, `maintenance_level`),必须在允许的枚举范围内
4. **Schema 有效性检查**: `input_schema` 和 `output_schema` 必须是有效的 JSON Schema

**验证工具使用示例:**

```bash
# 使用官方 CLI 工具验证
agentos validate-skill ./my_skill/skill.json

# 详细输出模式
agentos validate-skill ./my_skill/skill.json --verbose

# 仅检查语法，不检查语义
agentos validate-skill ./my_skill/skill.json --syntax-only
```

### 4.2 语义验证

除格式外，还应进行语义验证，确保技能的实际可用性：

1. **唯一性检查**: `skill_id` 必须在注册中心中唯一，由注册中心保证
2. **版本规范性检查**: `version` 字段必须符合语义化版本规范 (MAJOR.MINOR.PATCH)
3. **依赖合理性检查**: `dependencies` 中的库名、包名、命令名是否合理且可获取
4. **权限合法性检查**: `required_permissions` 中的权限是否在安全策略中定义
5. **工具一致性检查**: 工具名称是否与可执行文件中的导出函数匹配

### 4.3 验证流程

**验证流程分为四个阶段:**

1. **词法和语法分析**: 检查 JSON 格式是否正确，无语法错误
2. **JSON Schema 验证**: 检查字段完整性、类型正确性、枚举值合法性
3. **语义验证**: 检查业务规则、依赖关系、权限声明的合理性
4. **输出验证报告**: 包含错误列表、警告列表和改进建议

**验证失败处理:**

- **致命错误**: 缺少必需字段、格式错误、Schema 无效 → 拒绝注册
- **警告**: 依赖缺失、权限过宽、性能指标异常 → 提示修改，可选择忽略
- **建议**: 文档不完整、示例缺失、命名不规范 → 记录在案，不影响注册

---

## 第 5 章 版本管理

### 5.1 契约版本 vs 技能版本

Skill 契约涉及两个版本概念，需明确区分：

| 版本类型 | 字段名 | 维护方 | 递增规则 | 示例 |
|---------|--------|--------|---------|------|
| **契约规范版本** | `schema_version` | AgentOS 官方 | 当规范发生不兼容变更时递增 | `"1.0.0"` |
| **Skill 自身版本** | `version` | Skill 开发者 | 遵循语义化版本规范，功能变更时递增 | `"2.1.0"` |

**语义化版本规范 (Semantic Versioning):**

- **MAJOR**(主版本号): 当发生不兼容的功能变更时递增 (如删除工具、修改输入输出 Schema)
- **MINOR**(次版本号): 当新增向后兼容的功能时递增 (如新增工具、新增可选参数)
- **PATCH**(修订号): 当进行向后兼容的问题修复时递增 (如 bug 修复、性能优化)

### 5.2 兼容性原则

为确保生态系统的稳定性，契约设计遵循以下兼容性原则：

#### 5.2.1 向后兼容 (Backward Compatibility)

新版本契约应能兼容旧版本消费者：

- **消费者忽略未知字段**: 旧版本的工具服务在解析新版本的契约时，应忽略不认识的扩展字段
- **默认值处理**: 对于新增的可选字段，应提供合理的默认值
- **渐进式升级**: 允许新旧版本契约并存，逐步迁移

#### 5.2.2 向前兼容 (Forward Compatibility)

旧版本契约应能被新版本消费者正确解析：

- **版本判断**: 新版本的工具服务应根据 `schema_version` 判断契约版本，采用相应的解析策略
- **降级处理**: 对于不支持的新特性，应提供降级方案或明确的错误提示

### 5.3 版本变更管理

**变更类型与版本号递增规则:**

| 变更类型 | 影响范围 | `version` 变更 | 示例 |
|---------|---------|---------------|------|
| 新增工具 (向后兼容) | MINOR | `2.1.0` → `2.2.0` |
| 删除工具 (不兼容) | MAJOR | `2.1.0` → `3.0.0` |
| 修改工具 Schema(不兼容) | MAJOR | `2.1.0` → `3.0.0` |
| 修复 bug(向后兼容) | PATCH | `2.1.0` → `2.1.1` |
| 性能优化 (向后兼容) | PATCH | `2.1.0` → `2.1.1` |

---

## 第 6 章 最佳实践

### 6.1 如何编写高质量的技能契约

#### 实践 6-1【明确工具边界】

每个工具应只做一件事，避免"万能"工具。**单一职责原则**有助于提高可维护性和可测试性。

**✅ 推荐:**

```json
{
  "tools": [
    {
      "name": "github_create_repo",
      "description": "创建 GitHub 仓库"
    },
    {
      "name": "github_delete_repo",
      "description": "删除 GitHub 仓库"
    }
  ]
}
```

**❌ 不推荐:**

```json
{
  "tools": [
    {
      "name": "github_do_everything",
      "description": "处理所有 GitHub 相关操作，包括创建、删除、修改仓库，管理 PR、Issue 等"
    }
  ]
}
```

#### 实践 6-2【提供清晰示例】

在 `description` 中可附带示例输入输出，帮助调用方理解如何使用该工具。但示例不应影响机器解析，建议放在描述的末尾。

**示例:**

```json
{
  "description": "创建 GitHub 仓库。\n\n示例输入:\n{\"name\": \"my-project\", \"private\": true}\n\n示例输出:\n{\"repo_url\": \"https://github.com/user/my-project\", \"clone_url\": \"https://github.com/user/my-project.git\"}"
}
```

#### 实践 6-3【精确声明依赖】

依赖声明应完整且精确，避免缺失或过度声明。建议在多种环境下测试依赖安装过程。

**依赖测试清单:**

1. 在干净的虚拟机或容器中测试安装
2. 记录所有自动安装的依赖
3. 区分必需依赖和可选依赖
4. 指定版本范围，避免版本冲突

#### 实践 6-4【最小权限原则】

只申请完成任务所必需的最小权限，避免权限滥用。权限声明过宽可能被安全引擎拒绝。

**权限最小化示例:**

**❌ 不推荐:**

```json
{
  "required_permissions": [
    "network:outbound:*"  // 允许访问所有网络，过于宽泛
  ]
}
```

**✅ 推荐:**

```json
{
  "required_permissions": [
    "network:outbound:api.github.com:443"  // 仅允许访问 GitHub API
  ]
}
```

#### 实践 6-5【合理预估成本】

基于真实测试给出 Token 和成本预估，避免严重偏离。建议在不同场景下进行多次测试，取平均值。

**测试方法:**

1. 准备代表性测试集 (至少 10 个样本)
2. 在典型环境下运行测试
3. 记录每次的 Token 消耗和执行时间
4. 计算平均值和标准差
5. 在契约中填写平均值

#### 实践 6-6【定期审计更新】

保持 `trust_metrics.last_audit` 更新，反映技能的最新状态。建议至少每季度审计一次，或在重大更新后立即审计。

**审计内容包括:**

- 功能完整性测试
- 性能基准测试
- 安全性扫描 (依赖漏洞、权限滥用)
- 依赖项更新检查
- 文档和示例更新

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
      "owner": {
        "type": "string",
        "description": "仓库所有者"
      },
      "repo": {
        "type": "string",
        "description": "仓库名称"
      }
    },
    "required": ["owner", "repo"]
  }
}
```

#### 错误 6-3【依赖声明不完整】

**问题**: 导致安装失败或运行时错误。

**解决方法**: 在干净环境中测试安装过程，记录所有缺失的依赖。

#### 错误 6-4【权限声明过宽】

**问题**: 可能被安全引擎拒绝，或引起用户担忧。

**解决方法**: 遵循最小权限原则，精确限定权限范围。

#### 错误 6-5【版本号不规范】

**问题**: 应严格遵循语义化版本规范，避免使用非标准格式。

**❌ 不推荐:** `"v1.2"`, `"1.2.beta"`, `"latest"`

**✅ 推荐:** `"1.2.0"`, `"2.0.0-beta.1"`, `"1.2.3-rc.2"`

---

## 第 7 章 与其他模块的协同关系

### 7.1 与工具服务的协同

**工具服务 (`tool_d`)** 解析技能契约，加载技能动态库，验证契约，执行工具：

- **契约解析**: 读取并验证 `skill.json` 文件
- **依赖安装**: 根据 `dependencies` 自动安装所需资源
- **权限分配**: 根据 `required_permissions` 分配运行时权限
- **工具加载**: 加载动态库或脚本，注册工具到工具注册表
- **执行调度**: 接收工具调用请求，执行相应工具，返回结果

### 7.2 与市场服务的协同

**市场服务 (`market_d`)** 存储技能注册信息，提供安装、卸载、更新功能：

- **技能存储**: 将技能契约和可执行文件存储在制品仓库
- **索引建立**: 基于 `skill_name`, `description`, `tools.name` 建立全文索引
- **分类展示**: 根据 `type` 字段对技能进行分类
- **版本管理**: 维护技能的历史版本，支持回滚
- **统计分析**: 统计 `install_count`,收集用户 `rating`

### 7.3 与权限引擎的协同

**权限引擎 (`domain`)** 根据 `required_permissions` 进行权限裁决：

- **权限预分配**: 在技能启动时根据契约分配权限
- **运行时检查**: 在敏感操作前检查权限是否足够
- **越权拦截**: 拦截未授权的操作，记录审计日志
- **权限回收**: 在技能卸载或过期时回收权限

### 7.4 与调度官的协同

**调度官 (Dispatcher)** 根据 `cost_profile` 和 `trust_metrics` 选择技能 [2]：

- **成本优化**: 基于 `cost_profile.api_cost_per_task` 选择性价比最优的技能
- **信任优先**: 根据 `trust_metrics.rating` 和 `verified_provider` 优先选择高信任度技能
- **性能权衡**: 结合 `avg_duration_ms` 和 `success_rate` 平衡速度与质量

---

## 第 8 章 结语

技能契约是 AgentOS 技能市场的基石，它使技能可以像插件一样被动态发现、安装和执行。我们鼓励社区开发者遵循本规范，贡献高质量技能，共同构建丰富的技能生态。

**展望未来:**

- **阶段 1(已完成)**: 制定 Skill Contract 规范，开发验证工具
- **阶段 2(进行中)**: 开发参考实现 Skill，建立认证机制
- **阶段 3(计划中)**: 实现跨平台技能市场，支持技能组合和编排

---

## 附录 A: 快速索引

### A.1 按主题分类

**契约结构**: 第 2 章  
**字段定义**: 2.2 节  
**Executable**: 2.3 节  
**工具定义**: 2.4 节  
**依赖声明**: 2.5 节  
**权限声明**: 2.6 节  
**成本概览**: 2.7 节  
**信任指标**: 2.8 节  
**完整示例**: 第 3 章  
**契约验证**: 第 4 章  
**版本管理**: 第 5 章  
**最佳实践**: 第 6 章  

### A.2 按规则编号

- **实践 6-1**: 明确工具边界
- **实践 6-2**: 提供清晰示例
- **实践 6-3**: 精确声明依赖
- **实践 6-4**: 最小权限原则
- **实践 6-5**: 合理预估成本
- **实践 6-6**: 定期审计更新

---

## 附录 B: 与其他规范的引用关系

| 引用规范 | 关系说明 |
|---------|---------||
| [Agent 契约规范](./agent_contract.md) | 本规范与 Agent 契约规范结构相似，两者共同构成 AgentOS 的能力描述体系 |
| [架构设计原则](../../architecture/架构设计原则.md) | 本规范是架构原则在技能管理方面的具体实现，特别是微内核思想和模块化原则 |
| [统一术语表](../TERMINOLOGY.md) | 本规范使用的术语定义和解释，如 Skill、执行单元、契约等 |
| [C&C++ 安全编程指南](../../specifications/coding_standard/C&Cpp-secure-coding-guide.md) | 编译型技能的实现应遵循安全编程指南，特别是在内存管理和错误处理方面 |
| [日志打印规范](../../specifications/coding_standard/Log_guide.md) | 技能运行时应遵循日志规范，记录关键操作和异常情况 |

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
| v2.0.0 | 2026-03-22 | AgentOS 架构委员会 | 基于设计哲学和行动层模型进行全面重构，优化结构和表达 |
| v1.0.0 | - | 初始版本 | 初始发布 |

---

**最后更新**: 2026-03-22  
**维护者**: AgentOS 架构委员会
