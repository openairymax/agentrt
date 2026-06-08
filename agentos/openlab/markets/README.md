# Markets — 市场与模板

**模块路径**: `agentos/openlab/markets/`
**版本**: v0.0.5

> **Status**: 本模块作为 AgentOS 的正式组成部分，API 持续演进中。本模块通过 JSON-RPC 2.0 协议与 AgentOS 核心运行时集成。

## 概述

Markets 是 OpenLab 生态系统的市场与模板层，提供 Agent、Skill 的分发机制和项目模板。该模块包含三大核心子系统：Agent 市场（契约验证、安装器、注册索引）、Skill 市场（契约验证、安装器、注册索引）和项目模板（Python Agent 模板、Rust Skill 模板），构建完整的 AgentOS 生态分发链路。

## 架构定位

```
+-------------------------------------------------------------------+
|                         OpenLab 开放生态                            |
+-------------------------------------------------------------------+
|  +------------------+  +------------------+  +------------------+ |
|  |   Applications   |  |   Contributions  |  |    Markets       | |
|  |  (app/)          |  |  (contrib/)      |  |  (markets/)      | |
|  |                  |  |                  |  |                  | |
|  | • DocGen         |  | • Skills         |  | • Templates      | |
|  | • E-Commerce     |  | • Strategies     |  | • Agent Market   | |
|  | • Research       |  | • Agents         |  | • Skill Market   | |
|  | • VideoEdit      |  |                  |  |                  | |
|  +------------------+  +------------------+  +------------------+ |
+-------------------------------------------------------------------+
```

Markets 作为 OpenLab 顶层三大模块之一，负责生态组件的分发和模板管理，与 Contributions（社区贡献）和 Applications（智能应用）并列。

## 目录结构

```
markets/
├── agents/                         # Agent 市场
│   ├── contracts/                  # 契约验证
│   │   ├── schema.json             # Agent 契约 JSON Schema（Draft 7）
│   │   ├── validator.py            # AgentContractValidator 验证器
│   │   └── example_contract.json   # 契约示例（architect-agent）
│   ├── installer/                  # 安装器
│   │   └── cli.py                  # AgentCLI 命令行工具
│   └── registry/                   # 注册索引
│       └── index.json              # Agent 注册表
├── skills/                         # Skill 市场
│   ├── contracts/                  # 契约验证
│   │   ├── schema.json             # Skill 契约 JSON Schema
│   │   └── validator.py            # SkillContractValidator 验证器
│   ├── installer/                  # 安装器
│   │   └── cli.py                  # SkillInstallerCLI 命令行工具
│   └── registry/                   # 注册索引
│       └── index.json              # Skill 注册表
├── templates/                      # 项目模板
│   ├── python-agent/               # Python Agent 模板（详见其 README）
│   └── rust-skill/                 # Rust Skill 模板（详见其 README）
└── README.md                       # 本文件
```

## 核心组件

### 1. Agent 市场 (`agents/`)

Agent 市场提供 Agent 组件的契约验证、安装管理和注册索引功能。

#### AgentContractValidator (`agents/contracts/validator.py`)

基于 JSON Schema Draft 7 的 Agent 契约验证器，提供两层验证机制：

| 验证层 | 说明 |
|--------|------|
| Schema 验证 | 基于 `schema.json` 的结构校验，验证必填字段和类型约束 |
| 语义验证 | 超越 Schema 的业务逻辑校验，包括入口点格式、能力去重、依赖完整性、资源配置合理性 |

核心数据结构：

| 类 | 说明 |
|----|------|
| `AgentContractValidator` | 验证器主类，支持从字典/JSON 字符串/文件路径加载契约 |
| `ValidationResult` | 验证结果，包含 `is_valid`、`issues`、`validated_data` |
| `ValidationIssue` | 验证问题，包含 `severity`（ERROR/WARNING/INFO）、`message`、`path` |
| `ValidationSeverity` | 严重程度枚举（ERROR/WARNING/INFO） |

Agent 契约 Schema 必填字段：`agent_id`、`agent_type`、`version`、`capabilities`、`description`、`config_schema`、`entry_point`。

支持的 `agent_type` 枚举：`architect`、`backend`、`frontend`、`devops`、`security`、`tester`、`product_manager`、`data_analyst`、`researcher`、`custom`。

#### AgentCLI (`agents/installer/cli.py`)

Agent 安装管理命令行工具，支持多种输出格式和交互式操作：

| 命令 | 说明 |
|------|------|
| `install` | 安装 Agent（支持 file/url/git/registry 四种来源） |
| `uninstall` | 卸载 Agent（支持指定版本） |
| `list` | 列出已安装 Agent（支持详细模式） |
| `info` | 查看 Agent 详细信息 |
| `validate` | 验证 Agent 契约（支持严格模式） |

特性：支持 Rich 格式化输出、JSON/YAML 输出格式、异步安装进度显示。

### 2. Skill 市场 (`skills/`)

Skill 市场提供 Skill 组件的契约验证、安装管理和注册索引功能。

#### SkillContractValidator (`skills/contracts/validator.py`)

Skill 契约验证器，支持 YAML 和 JSON 格式的契约文件：

| 验证项 | 说明 |
|--------|------|
| 必填字段 | `name`、`version`、`description`、`capabilities`、`interface`、`permissions` |
| 接口类型 | `stdio`、`http_rest`、`websocket`、`grpc`、`message_queue`、`function_call` |
| 权限范围 | `filesystem:read/write`、`network:http/ws`、`process:spawn`、`memory:read/write`、`storage:local/cache`、`system:info/metrics` |

#### SkillInstallerCLI (`skills/installer/cli.py`)

Skill 安装管理命令行工具：

| 命令 | 说明 |
|------|------|
| `install` | 安装 Skill 包（支持 zip/tar.gz/目录，支持 `--force` 重装） |
| `list` | 列出已安装 Skill |
| `remove` | 移除已安装 Skill |

安装路径：`~/.agentos/skills/`，通过 `manifest.json` 或 `skill.json` 识别 Skill 元信息。

### 3. 项目模板 (`templates/`)

提供标准化的项目脚手架，加速 Agent 和 Skill 开发：

| 模板 | 路径 | 语言 | 说明 |
|------|------|------|------|
| **Python Agent** | `templates/python-agent/` | Python | 基于 Agent 基类的 Agent 开发模板，支持技能注册、记忆管理、协议处理 |
| **Rust Skill** | `templates/rust-skill/` | Rust | 基于 Skill trait 的高性能 Skill 开发模板，利用 Rust 内存安全和零成本抽象 |

> 各模板的详细文档请参阅其目录下的 README.md。

## 使用指南

### 验证 Agent 契约

```python
from markets.agents.contracts.validator import AgentContractValidator, validate_contract

# 便捷函数
result = validate_contract("path/to/contract.json")
print(f"Valid: {result.is_valid}")
for error in result.get_errors():
    print(f"  ERROR: {error}")
for warning in result.get_warnings():
    print(f"  WARNING: {warning}")

# 使用验证器类
validator = AgentContractValidator()
result = validator.validate_file("contract.json")
```

### 验证 Skill 契约

```bash
# 命令行
python -m agentos.openlab.markets.skills.contracts.validator --skill ./skill_contract.yaml
```

```python
from markets.skills.contracts.validator import SkillContractValidator

validator = SkillContractValidator()
contract = validator.load(Path("skill_contract.yaml"))
errors = validator.validate()
if errors:
    for err in errors:
        print(f"  [ERROR] {err}")
```

### 安装管理

```bash
# Agent 安装
agentos-agent install ./agent_contract.json
agentos-agent install https://github.com/user/agent-repo.git --type git
agentos-agent list --detailed
agentos-agent uninstall architect-agent --version 1.0.0

# Skill 安装
agentos-skill-installer install ./my_skill.zip
agentos-skill-installer install ./my_skill_dir/ --force
agentos-skill-installer list
agentos-skill-installer remove my-skill
```

### 使用模板创建项目

```bash
# 创建 Python Agent 项目
market install python-agent my-first-agent
cd my-first-agent
pip install -r requirements.txt
python agent.py

# 创建 Rust Skill 项目
market install rust-skill my-rust-skill
cd my-rust-skill
cargo build --release
```

## 与其他模块的关系

| 模块 | 关系 |
|------|------|
| **contrib/agents/** | 社区贡献的 Agent 通过 Markets 的契约验证和安装器进行分发 |
| **contrib/skills/** | 社区贡献的 Skill 通过 Markets 的契约验证和安装器进行分发 |
| **openlab/core/** | Markets 的契约 Schema 与 Core 的 Agent/Task/Tool 抽象对齐 |
| **app/** | 应用层可使用 Markets 的模板快速创建新的 Agent 应用 |

## 依赖关系

- **核心依赖**: Python >= 3.10, Pydantic, PyYAML, jsonschema >= 4.17.0
- **可选依赖**: Rich（格式化输出）, PyGithub（模板集成）
- **协议依赖**: AgentOS protocols 层（JSON-RPC 2.0）

---

© 2026 SPHARX Ltd. All Rights Reserved.
