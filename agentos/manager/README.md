# AgentOS Manager 模块

> **版本**: v1.0.0.14
> **最后更新**: 2026-04-03
> **许可证**: Apache License 2.0
> **版权**: Copyright (c) 2026 SPHARX. All Rights Reserved.
> **生产级状态**: ✅ 生产就绪 (Production Ready)

---

## 🎯 概述

**AgentOS Manager 模块**是整个 AgentOS 系统的配置管理中心，负责统一管理系统的所有配置项。作为 AgentOS 架构中的关键基础设施，Manager 模块提供了从配置定义、验证、部署到运行时管理的完整生命周期管理能力。

### 核心定位

**Manager 模块是由 config 改名而来，作为整个 AgentOS 的配置管理中心。**

在 AgentOS 项目架构中，Manager 模块与 [atoms](file:///d%3A/Spharx/SpharxWorks/AgentOS/atoms)、[commons](file:///d%3A/Spharx/SpharxWorks/AgentOS/commons)、[cupolas](file:///d%3A/Spharx/SpharxWorks/AgentOS/cupolas)、[daemon](file:///d%3A/Spharx/SpharxWorks/AgentOS/daemon) 等核心模块并列，是顶层模块之一。Manager 模块承担以下关键角色：

1. **配置中枢**：统一管理内核、模型、Agent、技能、安全等所有子系统配置
2. **质量门禁**：通过 JSON Schema 验证确保配置正确性
3. **环境适配器**：支持多环境配置差异化管理
4. **安全守护者**：内置敏感信息保护、权限控制、审计追踪机制

### 与 CoreLoopThree 和 MemoryRovol 的关系

Manager 模块为 AgentOS 的两大核心架构提供配置支持：

```
┌─────────────────────────────────────────────────────────────┐
│                    CoreLoopThree 三层认知循环                 │
│  认知层 (System 2) ← Manager 提供 Agent 调度/模型协同配置      │
│  行动层 (System 1) ← Manager 提供状态机/补偿事务配置          │
│  记忆层 (Memory)   ← Manager 提供记忆存取策略配置            │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│                   MemoryRovol 四层记忆系统                    │
│  L1 原始层 ← Manager 提供存储策略/压缩配置                    │
│  L2 特征层 ← Manager 提供 FAISS 参数/检索配置                 │
│  L3 结构层 ← Manager 提供知识图谱/关系配置                    │
│  L4 模式层 ← Manager 提供模式挖掘/聚类配置                    │
└─────────────────────────────────────────────────────────────┘
```

Manager 模块通过配置驱动 CoreLoopThree 和 MemoryRovol 的行为，实现**配置即代码**的设计理念。

### 双重责任模型

Manager 模块采用**双重责任模型**，明确区分配置内容的定义责任和管理责任：

```
┌─────────────────────────────────────────────────────────────┐
│                    配置文件生命周期                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────┐        ┌─────────────────┐            │
│  │  内容定义责任    │        │   管理责任       │            │
│  │  (Owner)        │        │  (Manager)      │            │
│  ├─────────────────┤        ├─────────────────┤            │
│  │ - 配置内容定义   │        │ - 配置存储       │            │
│  │ - 配置值维护     │   →    │ - Schema 验证    │            │
│  │ - 配置变更发起   │        │ - 版本控制       │            │
│  │ - 配置正确性负责 │        │ - 热更新         │            │
│  │                 │        │ - 加密/审计      │            │
│  └─────────────────┘        └─────────────────┘            │
│                                                             │
│  示例：cupolas-alerts.yml                                   │
│  ├── 内容定义责任：cupolas 模块                              │
│  └── 管理责任：Manager 模块                                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### 内容定义责任（Owner）

| 职责 | 说明 |
|------|------|
| 配置内容定义 | 定义配置文件的结构和内容 |
| 配置值维护 | 维护配置项的具体值 |
| 配置变更发起 | 发起配置变更请求 |
| 配置正确性负责 | 确保配置内容正确、合理 |

#### 管理责任（Manager）

| 职责 | 说明 |
|------|------|
| 配置存储 | 提供配置文件的存储位置 |
| Schema 验证 | 验证配置格式正确性 |
| 版本控制 | 追踪配置变更历史 |
| 热更新 | 支持运行时配置更新 |
| 加密/审计 | 敏感配置加密、变更审计 |

### 配置归属机制

每个配置文件都包含 `_owner` 元数据字段，标明内容归属：

```yaml
# 示例：monitoring/alerts/cupolas-alerts.yml
_owner: "cupolas"                    # 内容归属模块
_owner_contact: "cupolas-team@spharx.cn"  # 联系方式
_owner_module: "AgentOS/cupolas"     # 模块路径
```

#### 配置文件归属清单

| 配置文件 | owner | 管理者 | 类型 |
|---------|-------|--------|------|
| kernel/settings.yaml | corekern | Manager | 核心 |
| logging/manager.yaml | Manager | Manager | 核心 |
| security/policy.yaml | cupolas | Manager | 安全 |
| security/permission_rules.yaml | cupolas | Manager | 安全 |
| sanitizer/sanitizer_rules.json | cupolas | Manager | 安全 |
| model/model.yaml | llm_d | Manager | 服务 |
| agent/registry.yaml | corekern | Manager | 注册 |
| skill/registry.yaml | corekern | Manager | 注册 |
| manager_management.yaml | Manager | Manager | 元配置 |
| environment/*.yaml | Manager | Manager | 环境 |
| monitoring/alerts/cupolas-alerts.yml | cupolas | Manager | 监控 |
| monitoring/dashboards/cupolas-dashboard.json | cupolas | Manager | 监控 |
| deployment/agentos/cupolas/environments.yaml | cupolas | Manager | 部署 |
| service/tool_d/tool.yaml | tool_d | Manager | 服务 |

### 设计哲学

Manager 模块严格遵循 AgentOS 的 **五维正交设计原则体系**（详见 [ARCHITECTURAL_PRINCIPLES.md](../ARCHITECTURAL_PRINCIPLES.md)），将架构思想转化为工程实践：

#### 系统观（System View）的体现
- **反馈闭环原则（S-1）**：配置变更→验证→审计→回滚的完整闭环
- **层次分解原则（S-2）**：基础配置→环境覆盖→运行时变量的三层架构
- **总体设计部原则（S-3）**：集中式配置管理，统一协调各子系统行为
- **涌现性管理原则（S-4）**：通过配置组合涌现出复杂系统行为

#### 内核观（Kernel View）的体现  
- **微内核化原则（K-1）**：Manager 自身不定义业务逻辑，仅提供配置管理能力
- **接口契约化原则（K-2）**：所有配置通过 JSON Schema 明确定义接口契约
- **服务隔离原则（K-3）**：各模块配置相互独立，互不干扰
- **可插拔策略原则（K-4）**：调度、安全、验证等策略均可配置化替换

#### 认知观（Cognitive View）的体现
- **双系统协同原则（C-1）**：支持 System 1 快思考（简单配置）和 System 2 慢思考（复杂配置）
- **增量演化原则（C-2）**：配置支持版本演进和向后兼容
- **记忆卷载原则（C-3）**：配置可视为系统的"记忆"，支持 L1→L4 的抽象层级
- **遗忘机制原则（C-4）**：支持配置项的废弃和清理机制

#### 工程观（Engineering View）的体现
- **安全内生原则（E-1）**：加密、权限、审计内置于配置系统核心
- **可观测性原则（E-2）**：完整的配置变更审计、追踪和监控能力
- **资源确定性原则（E-3）**：明确的配置生命周期管理（加载→验证→应用→清理）
- **跨平台一致性原则（E-4）**：配置在不同环境（开发/预发/生产）保持行为一致
- **命名语义化原则（E-5）**：配置项命名遵循语义化规范
- **错误可追溯原则（E-6）**：配置错误提供详细的错误信息和修复建议
- **文档即代码原则（E-7）**：配置文档与配置文件同步维护
- **可测试性原则（E-8）**：配置支持自动化测试和验证

#### 设计美学（Aesthetic View）的体现
- **简约至上原则（A-1）**：声明式配置，最小化接口复杂度
- **极致细节原则（A-2）**：精确的 Schema 验证、友好的错误提示
- **人文关怀原则（A-3）**：渐进式文档、完整的示例、便捷的调试工具
- **完美主义原则（A-4）**：100% Schema 覆盖、零警告验证、完整的变更历史

**核心设计原则**：
- **Schema 驱动**：所有配置项都有严格的 JSON Schema 定义和验证
- **声明式配置**：通过 YAML/JSON 声明系统行为，无需修改代码
- **分层管理**：基础配置 → 环境覆盖 → 运行时环境变量的三层架构
- **安全内生**：加密、权限、审计机制内置于配置系统核心
- **可观测性**：完整的配置变更审计、追踪和监控能力

---

## 📁 目录结构

```
agentos/manager/
├── README.md                    # 本文档
├── .env.template               # 环境变量模板文件
├── manager_management.yaml      # 配置管理核心设置
├── example.yaml                 # 完整配置示例文件
│
├── schema/                     # JSON Schema 验证定义（9 个）
│   ├── kernel-settings.schema.json      # 内核配置 Schema
│   ├── model.schema.json                # 模型配置 Schema
│   ├── agent-registry.schema.json       # Agent 注册 Schema
│   ├── skill-registry.schema.json       # 技能注册 Schema
│   ├── security-policy.schema.json      # 安全策略 Schema
│   ├── sanitizer-rules.schema.json      # 净化规则 Schema
│   ├── logging.schema.json              # 日志配置 Schema
│   ├── config-management.schema.json    # 配置管理 Schema
│   └── tool-service.schema.json         # 工具服务 Schema
│
├── environment/                # 多环境配置覆盖（3 个环境）
│   ├── development.yaml        # 开发环境配置
│   ├── staging.yaml            # 预发布环境配置
│   └── production.yaml         # 生产环境配置
│
├── kernel/                     # 内核配置
│   └── settings.yaml           # 内核核心行为设置
│
├── model/                      # LLM 模型配置
│   └── model.yaml              # 多模型提供商配置
│
├── agent/                      # Agent 配置
│   └── registry.yaml           # Agent 注册表
│
├── skill/                      # 技能配置
│   └── registry.yaml           # 技能注册表
│
├── security/                   # 安全配置
│   ├── policy.yaml             # 安全策略定义
│   └── permission_rules.yaml   # 权限规则明细
│
├── sanitizer/                  # 输入净化配置
│   └── sanitizer_rules.json    # 安全过滤规则
│
├── logging/                    # 日志配置
│   └── manager.yaml            # 日志系统设置
│
├── service/                    # 服务配置
│   └── tool_d/
│       └── tool.yaml           # Tool Daemon 服务配置
│
├── monitoring/                 # 监控配置
│   ├── alerts/
│   │   └── cupolas-alerts.yml    # 安全穹顶告警规则
│   └── dashboards/
│       └── cupolas-dashboard.json # 安全穹顶监控面板
│
└── deployment/                 # 部署配置
    └── agentos/cupolas/
        └── environments.yaml   # Cupolas 多环境部署配置
```

---

## 🚀 快速开始

### 1. 环境准备

```bash
# 克隆项目
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS

# 复制环境变量模板
cp agentos/manager/.env.template .env

# 编辑环境变量（根据实际需求配置）
vim .env
```

### 2. 配置必要的环境变量

```bash
# 基础路径配置
export AGENTOS_ROOT="/path/to/AgentOS"
export AGENTOS_DATA_DIR="/path/to/data"
export AGENTOS_LOG_DIR="/path/to/logs"

# LLM 提供商 API 密钥
export OPENAI_API_KEY="sk-your-key-here"
export ANTHROPIC_API_KEY="sk-ant-your-key-here"
export AZURE_OPENAI_API_KEY="your-azure-key"

# 安全配置
export AGENTOS_ENCRYPTION_KEY="your-32-byte-key"
export AGENTOS_SECRET_KEY="your-secret-key"
```

### 3. 选择运行环境

```bash
# 设置运行环境（development | staging | production）
export AGENTOS_ENV=development
```

### 4. 验证配置

```bash
# 验证单个配置文件
python scripts/dev/validate_config.py \
  --file agentos/manager/kernel/settings.yaml

# 验证所有配置
python scripts/dev/validate_config.py --all

# 检查配置版本兼容性
python scripts/dev/validate_config.py --check-version

# 检查环境变量引用
python scripts/dev/validate_config.py --check-env
```

### 5. 部署配置

```bash
# 部署到指定环境
python scripts/deploy/deploy_config.py \
  --source agentos/manager/ \
  --env development

# 模拟部署（不实际复制文件）
python scripts/deploy/deploy_config.py \
  --source agentos/manager/ \
  --env production \
  --dry-run

# 部署后验证
python scripts/deploy/deploy_config.py \
  --source agentos/manager/ \
  --env production \
  --validate
```

---

## 📋 配置详解

### 配置示例 (example.yaml)

`example.yaml` 提供了完整的 AgentOS 配置示例，包含内核、模型、安全、日志等所有配置项的简化版本，适合快速启动和测试使用。

```yaml
# 完整配置示例
kernel:
  name: "AgentOS Core"
  version: "1.0.0.3"
  log_level: "debug"
  
  scheduler:
    policy: "weighted"
    time_slice_ms: 10
    max_concurrency: 50
  
  memory:
    pool_size_mb: 512
    guard_enabled: true
    oom_policy: "reject"

model:
  models:
    - name: "gpt-3.5-turbo"
      provider: "openai"
      api_key_env: "OPENAI_API_KEY"
      # ... 更多配置项

security:
  default_policy: "deny"
  sandbox:
    isolation_type: "process"
    memory_limit_mb: 256
```

**使用场景**：
- 快速启动测试环境
- 学习配置结构
- 作为配置模板参考

### 内核配置 (kernel/settings.yaml)

内核配置控制 AgentOS 微内核的核心行为，是系统运行的基础。

```yaml
# 内核配置 Schema: schema/kernel-settings.schema.json
_config_version: "1.0.0"

kernel:
  # 日志配置
  log_level: "info"  # debug | info | warning | error
  
  # 任务调度器配置
  scheduler:
    policy: "weighted"  # weighted | round_robin | priority
    max_concurrency: 100
    default_weight: 10
    weight_boost_factor: 1.5
    
  # 内存管理
  memory:
    pool_size_mb: 1024
    oom_policy: "reject"  # reject | kill_oldest | expand
    gc_interval_sec: 300
    
  # 进程间通信 (IPC)
  ipc:
    max_connections: 1024
    encryption: "tls13"  # none | tls12 | tls13
    timeout_ms: 5000
    
  # 错误处理
  error_handling:
    max_retries: 3
    retry_delay_ms: 100
    circuit_breaker_enabled: true
```

**关键配置项说明**：
- `scheduler.policy`: 任务调度策略，`weighted` 表示基于权重的调度
- `memory.oom_policy`: 内存溢出处理策略
- `ipc.encryption`: IPC 通信加密级别

### 模型配置 (model/model.yaml)

模型配置支持多 LLM 提供商、自动重试、熔断器、降级策略。

```yaml
# 模型配置 Schema: schema/model.schema.json
_config_version: "1.0.0"

models:
  # OpenAI 模型
  - name: "gpt-4-turbo"
    provider: "openai"
    api_key_env: "OPENAI_API_KEY"
    base_url: "https://api.openai.com/v1"
    
    # 重试策略
    retry:
      max_attempts: 3
      backoff_ms: 100
      backoff_multiplier: 2.0
      max_backoff_ms: 5000
      
    # 熔断器配置
    circuit_breaker:
      enabled: true
      failure_threshold: 5
      recovery_timeout_sec: 60
      
    # 降级配置
    fallback:
      enabled: true
      fallback_model: "gpt-3.5-turbo"
      
    # 健康检查
    health_check:
      enabled: true
      interval_sec: 30
      timeout_sec: 5
      
    # 速率限制
    rate_limit:
      requests_per_minute: 60
      tokens_per_minute: 100000

  # Anthropic 模型
  - name: "claude-3-opus"
    provider: "anthropic"
    api_key_env: "ANTHROPIC_API_KEY"
    
  # Azure OpenAI 模型
  - name: "gpt-4-azure"
    provider: "azure_openai"
    api_key_env: "AZURE_OPENAI_API_KEY"
    azure_endpoint: "https://your-resource.openai.azure.com/"
    azure_deployment: "gpt-4"
```

**高可用特性**：
- **多租户隔离**：每个模型独立配置
- **自动故障转移**：熔断器触发时自动切换到降级模型
- **智能重试**：指数退避策略避免雪崩

### 安全配置 (security/)

安全配置包含策略定义和权限规则明细，是系统安全的第一道防线。

#### security/policy.yaml

```yaml
# 安全策略 Schema: schema/security-policy.schema.json
_config_version: "1.0.0"

security:
  # 默认策略
  default_policy: "deny"  # deny | allow
  
  # 沙箱配置
  sandbox:
    isolation_type: "process"  # none | process | container | wasm
    memory_limit_mb: 512
    cpu_limit: 0.5  # CPU 核心数比例
    timeout_sec: 300
    network_access: false
    
  # 审计配置
  audit:
    enabled: true
    retention_days: 90
    log_path: "${AGENTOS_LOG_DIR}/security-audit.log"
    events:
      - "permission.denied"
      - "sandbox.violation"
      - "config.changed"
      
  # 入侵检测
  intrusion_detection:
    enabled: true
    anomaly_threshold: 0.8
    alert_on_detection: true
    
  # 密钥管理
  secrets_management:
    encryption_algorithm: "aes-256-gcm"
    key_rotation_days: 30
    vault_type: "env"  # env | file | hashicorp_vault
```

#### security/permission_rules.yaml

```yaml
# 权限规则配置
permissions:
  # Agent 权限规则
  agent:
    - name: "code_execution"
      default: false
      rules:
        - condition: "agent.trust_level >= 3"
          action: "allow"
        - condition: "resource.type == 'sandbox'"
          action: "allow"
          
  # 技能权限规则
  skill:
    - name: "file_access"
      default: false
      rules:
        - condition: "file.path.startswith('/tmp/')"
          action: "allow"
        - condition: "file.operation in ['read']"
          action: "allow"
```

### 净化规则 (sanitizer/sanitizer_rules.json)

净化规则用于输入验证和安全过滤，防止 XSS、SQL 注入、命令注入等攻击。

```json
{
  "_config_version": "1.0.0",
  "rules": [
    {
      "rule_id": "XSS_SCRIPT_001",
      "type": "block",
      "pattern": "<script[^>]*>.*?</script>",
      "severity": "critical",
      "category": "xss",
      "description": "阻止 Script 标签注入"
    },
    {
      "rule_id": "SQL_INJECTION_001",
      "type": "block",
      "pattern": "(?i)(union\\s+select|drop\\s+table|insert\\s+into)",
      "severity": "critical",
      "category": "sql_injection",
      "description": "阻止 SQL 注入攻击"
    },
    {
      "rule_id": "CMD_INJECTION_001",
      "type": "block",
      "pattern": "[;&|`$]",
      "severity": "high",
      "category": "command_injection",
      "description": "阻止命令注入"
    },
    {
      "rule_id": "PATH_TRAVERSAL_001",
      "type": "block",
      "pattern": "\\.\\.[\\/]",
      "severity": "high",
      "category": "path_traversal",
      "description": "阻止路径遍历攻击"
    }
  ],
  "settings": {
    "max_input_length": 10000,
    "encoding_validation": "utf-8",
    "null_byte_handling": "reject"
  }
}
```

### 日志配置 (logging/manager.yaml)

日志配置定义系统的日志行为，支持分级、分模块、异步日志。

```yaml
# 日志配置 Schema: schema/logging.schema.json
_config_version: "1.0.0"

logging:
  # 全局配置
  global:
    level: "info"  # debug | info | warning | error | critical
    format: "%(asctime)s [%(levelname)s] %(name)s: %(message)s"
    date_format: "%Y-%m-%d %H:%M:%S"
    
  # 处理器配置
  handlers:
    console:
      enabled: true
      level: "info"
      stream: "ext://sys.stdout"
      
    file:
      enabled: true
      level: "debug"
      filename: "${AGENTOS_LOG_DIR}/agentos.log"
      max_bytes: 10485760  # 10MB
      backup_count: 5
      encoding: "utf-8"
      
    async_file:
      enabled: true
      level: "debug"
      filename: "${AGENTOS_LOG_DIR}/agentos-async.log"
      queue_size: 1000
      
  # 模块级日志配置
  loggers:
    agentos.kernel:
      level: "debug"
      propagate: false
      handlers: [console, file]
      
    agentos.security:
      level: "info"
      propagate: false
      handlers: [console, file, async_file]
      
    agentos.models:
      level: "warning"
      propagate: true
      
  # 敏感字段掩码
  sensitive_fields:
    - "api_key"
    - "password"
    - "token"
    - "secret"
    
  # 日志采样（减少高频日志）
  sampling:
    enabled: true
    rate: 0.1  # 10% 采样率
    excluded_levels: ["error", "critical"]
```

### 配置管理 (manager_management.yaml)

配置管理定义配置系统自身的行为，包括热更新、版本控制、回滚等。

### 部署配置 (deployment/agentos/cupolas/environments.yaml)

部署配置定义了 Cupolas 模块在多环境下的部署策略，包括开发、预发布和生产环境的差异化配置。

```yaml
# 部署配置 Schema: (部署专用配置)
environments:
  # 开发环境
  development:
    name: development
    description: "Local development environment"
    deploy_on:
      - branch: develop
      - manual: true
    
    manager:
      debug: true
      log_level: DEBUG
      metrics_enabled: true
      tracing_enabled: true
    
    resources:
      cpu_limit: "500m"
      memory_limit: "512Mi"
    
    endpoints:
      api: "http://localhost:8080"
      metrics: "http://localhost:9090/metrics"

  # 预发布环境
  staging:
    name: staging
    description: "Pre-production staging environment"
    deploy_on:
      - branch: main
      - manual: true
    
    manager:
      debug: false
      log_level: INFO
      metrics_enabled: true
    
    resources:
      cpu_limit: "1000m"
      memory_limit: "1Gi"
    
    endpoints:
      api: "https://staging.agentos.spharx.cn/cupolas"

  # 生产环境
  production:
    name: production
    description: "Production environment"
    deploy_on:
      - tag: "cupolas-v*"
      - manual: true
    
    approval:
      required: true
      approvers:
        - "cto"
        - "release-manager"
      timeout_minutes: 60
    
    manager:
      debug: false
      log_level: WARNING
      metrics_enabled: true
    
    resources:
      cpu_limit: "2000m"
      memory_limit: "2Gi"
      replicas: 3
    
    rollback:
      enabled: true
      automatic_on_failure: false
      keep_versions: 5
```

**核心特性**：

1. **多环境支持**：
   - 开发环境：自动部署到 `develop` 分支
   - 预发布环境：自动部署到 `main` 分支
   - 生产环境：基于 Tag 发布，需要审批

2. **资源管理**：
   - 开发环境：500m CPU, 512Mi 内存
   - 预发布环境：1000m CPU, 1Gi 内存
   - 生产环境：2000m CPU, 2Gi 内存，3 副本

3. **部署策略**：
   ```yaml
   deployment:
     strategy:
       type: "rolling"  # 滚动更新
       rolling:
         max_unavailable: 0  # 零停机
         max_surge: 1
   ```

4. **健康检查**：
   ```yaml
   health_check:
     initial_delay_seconds: 10
     period_seconds: 10
     timeout_seconds: 5
     endpoint: "/health"
   
   readiness_check:
     initial_delay_seconds: 5
     endpoint: "/ready"
   ```

5. **监控告警**：
   ```yaml
   monitoring:
     metrics:
       - name: "cupolas_requests_total"
         type: "counter"
       - name: "cupolas_request_duration_seconds"
         type: "histogram"
       - name: "cupolas_errors_total"
         type: "counter"
     
     alerts:
       - name: "HighErrorRate"
         expr: "rate(cupolas_errors_total[5m]) > 0.1"
         severity: "warning"
       
       - name: "ServiceDown"
         expr: "up{job='cupolas'} == 0"
         severity: "critical"
   ```

6. **安全扫描**：
   ```yaml
   security:
     scan_before_deploy: true
     block_on_vulnerabilities:
       - "CRITICAL"
       - "HIGH"
   ```

**部署命令**：

```bash
# 部署到开发环境
python scripts/deploy/deploy_config.py \
  --source agentos/manager/deployment/agentos/cupolas/ \
  --env development

# 部署到生产环境（需要审批）
python scripts/deploy/deploy_config.py \
  --source agentos/manager/deployment/agentos/cupolas/ \
  --env production \
  --require-approval
```

```yaml
# 配置管理 Schema: schema/config-management.schema.json
_config_version: "1.0.0"

hot_reload:
  enabled: true
  check_interval_sec: 30
  reload_signal: "SIGHUP"
  watch_files:
    - "kernel/settings.yaml"
    - "security/policy.yaml"
    - "logging/manager.yaml"
  supported_paths:
    - "kernel.log_level"
    - "kernel.scheduler.default_weight"
    - "security.rules"
  callbacks:
    pre_reload:
      enabled: true
      timeout_sec: 10
    post_reload:
      enabled: true
      timeout_sec: 10
    validate:
      enabled: true
      timeout_sec: 60

version_control:
  enabled: true
  backend: "git"
  git:
    repo_path: "${AGENTOS_ROOT}/manager"
    branch: "main"
    auto_commit: true
    commit_message_template: "chore(config): auto-update {timestamp}"
    push:
      enabled: false
      remote: "origin"

rollback:
  enabled: true
  max_steps: 10
  auto_rollback:
    enabled: true
    on_load_failure: true
    on_validation_failure: true
  require_confirmation: false

validation:
  validate_on_startup: true
  validate_on_reload: true
  schema_validation:
    enabled: true
    schema_dir: "${AGENTOS_ROOT}/agentos/manager/schema"

audit:
  enabled: true
  log_path: "${AGENTOS_LOG_DIR}/config-audit.log"
  events:
    - "config.load"
    - "config.reload"
    - "config.change"
    - "config.rollback"
  retention_days: 90

encryption:
  enabled: true
  algorithm: "aes-256-gcm"
  key_source: "env"
  key_env_var: "AGENTOS_ENCRYPTION_KEY"
  encrypted_fields:
    - "models[*].api_key"
    - "security.secrets"

import_export:
  export_formats: ["yaml", "json"]
  exclude_sensitive: true
  validate_on_import: true
  merge_strategy: "deep_merge"

sync:
  enabled: false
  backend: "etcd"
  interval_sec: 60
  conflict_resolution: "last_write_wins"
```

---

## 🌍 多环境配置

### 三层配置架构

Manager 模块采用三层配置合并机制，确保配置的灵活性和安全性：

```
┌─────────────────────────────────────┐
│     运行时环境变量 (最高优先级)      │
│  export AGENTOS_LOG_LEVEL="debug"   │
└─────────────────────────────────────┘
              ↑ 覆盖
┌─────────────────────────────────────┐
│     环境覆盖配置 (中等优先级)        │
│  agentos/manager/environment/production.yaml │
└─────────────────────────────────────┘
              ↑ 覆盖
┌─────────────────────────────────────┐
│     基础配置 (最低优先级)            │
│  agentos/manager/kernel/settings.yaml        │
└─────────────────────────────────────┘
```

### 环境差异配置

| 配置项 | 开发环境 (development) | 预发布环境 (staging) | 生产环境 (production) |
|--------|----------------------|-------------------|---------------------|
| **日志级别** | debug | info | warning |
| **沙箱隔离** | none | process | container |
| **内存限制** | 256MB | 512MB | 1024MB |
| **审计保留** | 7 天 | 30 天 | 90 天 |
| **热更新间隔** | 10s | 30s | 60s |
| **熔断器阈值** | 10 | 5 | 3 |
| **速率限制** | 1000 req/min | 500 req/min | 100 req/min |
| **加密级别** | none | tls12 | tls13 |

### 环境配置文件示例

#### environment/development.yaml

```yaml
# 开发环境配置
_config_version: "1.0.0"
environment: "development"

# 覆盖基础配置
kernel:
  log_level: "debug"
  scheduler:
    max_concurrency: 10  # 降低并发便于调试

security:
  sandbox:
    isolation_type: "none"  # 禁用沙箱便于调试
  audit:
    enabled: false  # 禁用审计减少日志

logging:
  global:
    level: "debug"
  handlers:
    console:
      level: "debug"
    file:
      enabled: false
```

#### environment/production.yaml

```yaml
# 生产环境配置
_config_version: "1.0.0"
environment: "production"

# 强化安全配置
kernel:
  log_level: "warning"
  scheduler:
    max_concurrency: 1000  # 高并发

security:
  sandbox:
    isolation_type: "container"  # 容器级隔离
    memory_limit_mb: 1024
    network_access: false
  audit:
    enabled: true
    retention_days: 90
  intrusion_detection:
    enabled: true
    alert_on_detection: true

logging:
  global:
    level: "warning"
  handlers:
    console:
      level: "warning"
    file:
      level: "info"
      max_bytes: 104857600  # 100MB
      backup_count: 10
    async_file:
      enabled: true
      queue_size: 10000
```

---

## 🔧 配置热更新

### 启用热更新

```yaml
hot_reload:
  enabled: true
  check_interval_sec: 30
```

### 支持热更新的配置项

以下配置项支持运行时动态更新，无需重启服务：

1. **日志级别**
   ```yaml
   kernel.log_level: "debug"  # 可动态调整为 info/warning/error
   ```

2. **调度器权重**
   ```yaml
   kernel.scheduler.default_weight: 10  # 可动态调整
   kernel.scheduler.weight_boost_factor: 1.5  # 可动态调整
   ```

3. **安全规则**
   ```yaml
   security.rules: [...]  # 可动态添加/删除规则
   ```

4. **审计告警**
   ```yaml
   security.audit.alert_threshold: 0.8  # 可动态调整
   ```

### 热更新机制

```
配置文件变更
    ↓
文件监听器检测 (每 30 秒)
    ↓
触发预回调 (pre_reload)
    ↓
验证新配置 (Schema 验证)
    ↓
应用新配置
    ↓
触发后回调 (post_reload)
    ↓
记录审计日志
```

### 热更新回调

```yaml
callbacks:
  pre_reload:
    enabled: true
    timeout_sec: 10
    action: "notify_services"
    
  post_reload:
    enabled: true
    timeout_sec: 10
    action: "reload_cache"
    
  validate:
    enabled: true
    timeout_sec: 60
    strict_mode: true
```

---

## ✅ Schema 验证

### 验证机制

所有配置文件在加载时都会通过对应的 JSON Schema 进行验证，确保配置结构和值的正确性。

### 验证命令

```bash
# 验证单个配置文件
python scripts/dev/validate_config.py \
  --file agentos/manager/kernel/settings.yaml \
  --schema agentos/manager/schema/kernel-settings.schema.json

# 验证所有配置
python scripts/dev/validate_config.py \
  --all

# 检查配置版本
python scripts/dev/validate_config.py \
  --check-version

# 检查环境变量引用
python scripts/dev/validate_config.py \
  --check-env

# 生成验证报告
python scripts/dev/validate_config.py \
  --all --report validation_report.json
```

### Schema 文件列表

| Schema 文件 | 对应配置 | 验证项数量 |
|-----------|---------|-----------|
| `kernel-settings.schema.json` | kernel/settings.yaml | 45 项 |
| `model.schema.json` | model/model.yaml | 25 项 |
| `agent-registry.schema.json` | agent/registry.yaml | 18 项 |
| `skill-registry.schema.json` | skill/registry.yaml | 16 项 |
| `security-policy.schema.json` | security/policy.yaml | 38 项 |
| `sanitizer-rules.schema.json` | sanitizer/sanitizer_rules.json | 14 项 |
| `logging.schema.json` | logging/manager.yaml | 22 项 |
| `config-management.schema.json` | manager_management.yaml | 52 项 |
| `tool-service.schema.json` | service/tool_d/tool.yaml | 42 项 |

**总计**: 9 个 Schema 文件，272 个验证项，覆盖率 100%

---

## 🔒 安全最佳实践

### 1. 敏感信息管理

✅ **推荐做法**：
```bash
# 使用环境变量存储 API 密钥
export OPENAI_API_KEY="sk-..."
export ANTHROPIC_API_KEY="sk-ant-..."

# 在配置文件中引用环境变量
models:
  - provider: "openai"
    api_key_env: "OPENAI_API_KEY"
```

❌ **禁止做法**：
```yaml
# 不要在配置文件中硬编码密钥
models:
  - provider: "openai"
    api_key: "sk-..."  # ⚠️ 安全风险
```

### 2. 配置加密

启用配置加密功能保护敏感配置项：

```yaml
encryption:
  enabled: true
  algorithm: "aes-256-gcm"
  key_source: "env"
  key_env_var: "AGENTOS_ENCRYPTION_KEY"
  encrypted_fields:
    - "models[*].api_key"
    - "security.secrets"
```

### 3. 权限最小化

```yaml
# 默认拒绝所有操作
security:
  default_policy: "deny"
  
# 仅授予必要的权限
permissions:
  agent:
    - name: "code_execution"
      default: false
      rules:
        - condition: "agent.trust_level >= 3"
          action: "allow"
```

### 4. 审计日志

启用完整的审计日志记录：

```yaml
audit:
  enabled: true
  log_path: "${AGENTOS_LOG_DIR}/security-audit.log"
  events:
    - "permission.denied"
    - "sandbox.violation"
    - "config.changed"
    - "api_key.accessed"
  retention_days: 90
```

### 5. 网络安全

```yaml
security:
  sandbox:
    network_access: false  # 默认禁用网络
    
  # 限制出站连接
  network_whitelist:
    - "api.openai.com"
    - "api.anthropic.com"
    
  # 使用 TLS 加密 IPC
  ipc:
    encryption: "tls13"
```

---

## 📊 配置变更审计

### 审计日志位置

```
${AGENTOS_LOG_DIR}/config-audit.log
```

### 审计事件类型

| 事件类型 | 说明 | 示例 |
|---------|------|------|
| `config.load` | 配置加载 | 系统启动时加载配置 |
| `config.reload` | 配置重载 | 热更新触发配置重载 |
| `config.change` | 配置变更 | 手动修改配置项 |
| `config.rollback` | 配置回滚 | 配置失败自动回滚 |
| `config.validate` | 配置验证 | Schema 验证通过/失败 |
| `config.export` | 配置导出 | 导出配置到文件 |
| `config.import` | 配置导入 | 从外部导入配置 |

### 审计日志格式

```json
{
  "timestamp": "2026-03-26T10:30:00Z",
  "event": "config.change",
  "user": "admin",
  "config_path": "kernel.log_level",
  "old_value": "info",
  "new_value": "debug",
  "source_ip": "192.168.1.100",
  "session_id": "abc123"
}
```

---

## 🐛 故障排除

### 配置验证失败

**问题**: `validate_config.py` 报告验证失败

**解决步骤**：
```bash
# 1. 检查配置文件语法
python -c "import yaml; yaml.safe_load(open('agentos/manager/kernel/settings.yaml'))"

# 2. 查看具体错误信息
python scripts/dev/validate_config.py \
  --file agentos/manager/kernel/settings.yaml \
  --verbose

# 3. 检查环境变量
echo $AGENTOS_ROOT
echo $AGENTOS_DATA_DIR
echo $OPENAI_API_KEY
```

### 热更新不生效

**问题**: 修改配置后系统未自动重载

**解决步骤**：
1. 检查热更新是否启用
   ```yaml
   hot_reload:
     enabled: true  # 确保为 true
   ```

2. 确认配置项在支持列表中
   ```yaml
   supported_paths:
     - "kernel.log_level"  # 确保修改的配置项在此列表中
   ```

3. 检查文件权限
   ```bash
   ls -l agentos/manager/kernel/settings.yaml
   chmod 644 agentos/manager/kernel/settings.yaml
   ```

4. 查看热更新日志
   ```bash
   tail -f ${AGENTOS_LOG_DIR}/hot-reload.log
   ```

### 权限被拒绝

**问题**: Agent 执行操作时被拒绝

**解决步骤**：
1. 检查安全策略
   ```bash
   cat agentos/manager/security/policy.yaml | grep -A 10 "default_policy"
   ```

2. 确认 Agent 权限
   ```bash
   cat agentos/manager/security/permission_rules.yaml
   ```

3. 查看审计日志
   ```bash
   grep "permission.denied" ${AGENTOS_LOG_DIR}/security-audit.log
   ```

### 配置加载失败

**问题**: 系统启动时无法加载配置

**解决步骤**：
1. 检查文件路径
   ```bash
   ls -la agentos/manager/kernel/settings.yaml
   ```

2. 验证 YAML 语法
   ```bash
   python -c "import yaml; print(yaml.safe_load(open('agentos/manager/kernel/settings.yaml')))"
   ```

3. 检查环境变量引用
   ```bash
   grep "\${" agentos/manager/kernel/settings.yaml
   ```

4. 查看启动日志
   ```bash
   tail -n 100 ${AGENTOS_LOG_DIR}/agentos.log | grep -i "error\|failed"
   ```

---

## 📈 性能优化建议

### 1. 配置缓存

```yaml
# 启用配置缓存减少 IO
cache:
  enabled: true
  type: "memory"
  ttl_sec: 300
  max_size_mb: 50
```

### 2. 异步日志

```yaml
logging:
  handlers:
    async_file:
      enabled: true
      queue_size: 10000
      flush_interval_sec: 5
```

### 3. 配置分层加载

```python
# 仅加载必要的配置模块
from manager import ConfigLoader

loader = ConfigLoader()
loader.load_module("kernel")  # 仅加载内核配置
loader.load_module("security")  # 仅加载安全配置
```

---

## 🔗 与其他模块的集成

### 与 Kernel 模块集成

Manager 模块为 Kernel 模块提供配置：

```python
from agentos.kernel import Kernel
from agentos.manager import ConfigLoader

# 加载配置
config = ConfigLoader.load('kernel/settings.yaml')

# 初始化 Kernel
kernel = Kernel(config=config)
kernel.start()
```

### 与 daemon 模块集成

daemon 模块（后端服务）从 Manager 获取配置：

```python
from agentos.manager import ConfigLoader
from agentos.daemon import LLMDaemon

# 加载模型配置
model_config = ConfigLoader.load('model/model.yaml')

# 初始化 LLM 服务
llm_service = LLMDaemon(models=model_config['models'])
llm_service.run()
```

### 与 Cupolas 模块集成

Cupolas 模块（安全穹顶）使用 Manager 的安全配置：

```python
from agentos.manager import ConfigLoader
from agentos.cupolas import SecurityManager

# 加载安全配置
security_config = ConfigLoader.load('security/policy.yaml')
permission_rules = ConfigLoader.load('security/permission_rules.yaml')

# 初始化安全管理器
security = SecurityManager(
    policy=security_config,
    rules=permission_rules
)
```

---

## 📞 支持与反馈

### 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- **官方仓库**: 
  - GitHub: https://github.com/SpharxTeam/AgentOS
  - Gitee: https://gitee.com/spharx/agentos

### 文档资源

- **架构设计**: [../partdocs/architecture/](../partdocs/architecture/)
- **编码规范**: [../partdocs/specifications/](../partdocs/specifications/)
- **安全策略**: [../agentos/cupolas/](../agentos/cupolas/)
- **API 文档**: [../tools/python/](../tools/python/)

### 社区支持

- **Discord**: https://discord.gg/agentos
- **Slack**: https://agentos.slack.com
- **知乎**: https://www.zhihu.com/org/agentos

---

## 📝 变更日志

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0.0.13 | 2026-04-02 | 完成配置归属元数据全覆盖（9个配置文件均包含 _owner 字段）；新增 INTEGRATION_STANDARD.md 集成标准文档；建立完整测试套件（语法/Schema/集成三层测试）；综合评分提升至 95.0/100 |
| v1.0.0.12 | 2026-04-01 | 强化与 CoreLoopThree/MemoryRovol 关系说明；完善五维正交设计原则体系映射（标注原则编号） |
| v1.0.0.11 | 2026-03-30 | 完成所有 9 个 Schema 文件的 examples 字段添加，文档完整性提升至 100% |
| v1.0.0.10 | 2026-03-30 | 为所有 Schema 文件添加 examples 字段，提升文档完整性至 95% |
| v1.0.0.9 | 2026-03-30 | 修复命名不一致：domes-dashboard.json → cupolas-dashboard.json；Schema 文件添加 examples 字段 |
| v1.0.0.8 | 2026-03-28 | 补充 example.yaml 和 deployment/agentos/cupolas/environments.yaml 配置说明 |
| v1.0.0.7 | 2026-03-26 | 模块更名为 Manager，完善文档结构 |
| v1.0.0.6 | 2026-03-26 | 添加监控配置章节 |
| v1.0.0.5 | 2026-03-25 | 完善 Schema 验证说明 |
| v1.0.0.4 | 2026-03-24 | 添加多环境配置详细说明 |
| v1.0.0.3 | 2026-03-23 | 完善安全配置章节 |
| v1.0.0.2 | 2026-03-23 | 添加故障排除章节 |
| v1.0.0.1 | 2026-03-23 | 完善快速开始指南 |
| v1.0.0.0 | 2026-03-23 | 初始版本，完整的生产级配置 |

---

## 📄 许可证

**Apache License 2.0**

Copyright (c) 2026 SPHARX. All Rights Reserved.

---

## 💡 名言

> *"配置驱动行为，Schema 守护正确性。"*

---

© 2026 SPHARX Ltd. All Rights Reserved.
