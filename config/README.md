# AgentOS 配置模块

> **版本**: v1.0.0  
> **最后更新**: 2026-03-23  
> **版权**: Copyright (c) 2026 SPHARX. All Rights Reserved.

---

## 概述

AgentOS 配置模块提供了完整的、生产级的配置管理解决方案。遵循 AgentOS 的设计哲学，配置系统具有以下特点：

- **Schema 验证**：所有配置文件都有对应的 JSON Schema，确保配置正确性
- **跨平台支持**：使用环境变量实现跨平台路径配置
- **多环境支持**：支持开发、预发布、生产环境的配置覆盖
- **热更新**：支持配置热更新，无需重启服务
- **安全内生**：敏感配置加密、权限最小化、审计追踪

---

## 目录结构

```
config/
├── README.md                    # 本文档
├── .env.template               # 环境变量模板
├── config_management.yaml      # 配置管理（热更新、版本控制、加密）
│
├── schema/                     # JSON Schema 验证文件
│   ├── kernel-settings.schema.json
│   ├── model.schema.json
│   ├── agent-registry.schema.json
│   ├── security-policy.schema.json
│   ├── sanitizer-rules.schema.json
│   ├── logging.schema.json
│   └── skill-registry.schema.json
│
├── environment/                # 多环境配置覆盖
│   ├── development.yaml        # 开发环境
│   ├── staging.yaml            # 预发布环境
│   └── production.yaml         # 生产环境
│
├── kernel/                     # 内核配置
│   └── settings.yaml           # 内核设置
│
├── model/                      # 模型配置
│   └── model.yaml              # LLM 模型配置
│
├── agent/                      # Agent 配置
│   └── registry.yaml           # Agent 注册表
│
├── skill/                      # 技能配置
│   └── registry.yaml           # 技能注册表
│
├── security/                   # 安全配置
│   ├── policy.yaml             # 安全策略
│   └── permission_rules.yaml   # 权限规则
│
├── sanitizer/                  # 净化器配置
│   └── sanitizer_rules.json    # 净化规则
│
├── logging/                    # 日志配置
│   └── config.yaml             # 日志设置
│
└── service/                    # 服务配置
    └── tool_d/
        └── tool.yaml           # Tool Daemon 配置
```

---

## 快速开始

### 1. 配置环境变量

```bash
# 复制环境变量模板
cp config/.env.template .env

# 编辑环境变量
vim .env

# 设置必要的 API 密钥
export OPENAI_API_KEY="sk-your-key-here"
export ANTHROPIC_API_KEY="sk-ant-your-key-here"
```

### 2. 选择运行环境

```bash
# 设置运行环境
export AGENTOS_ENV=development  # 或 staging, production
```

### 3. 验证配置

```bash
# 使用 JSON Schema 验证配置
python scripts/validate_config.py --config config/ --schema config/schema/
```

---

## 配置说明

### 内核配置 (kernel/settings.yaml)

内核配置控制 AgentOS 微内核的核心行为：

```yaml
kernel:
  log_level: "info"
  
  scheduler:
    policy: "weighted"
    max_concurrency: 100
    
  memory:
    pool_size_mb: 1024
    oom_policy: "reject"
    
  ipc:
    max_connections: 1024
    encryption: "tls13"
```

### 模型配置 (model/model.yaml)

模型配置支持多提供商、重试、熔断、降级：

```yaml
models:
  - name: "gpt-4-turbo"
    provider: "openai"
    api_key_env: "OPENAI_API_KEY"
    
    retry:
      max_attempts: 3
      backoff_ms: 100
      
    circuit_breaker:
      enabled: true
      failure_threshold: 5
      
    fallback:
      enabled: true
      fallback_model: "gpt-3.5-turbo"
```

### 安全配置 (security/)

安全配置包含策略和权限规则：

```yaml
security:
  default_policy: "deny"
  
  sandbox:
    isolation_type: "process"
    memory_limit_mb: 512
    
  audit:
    retention_days: 90
```

### 净化规则 (sanitizer/sanitizer_rules.json)

净化规则用于输入验证和安全过滤：

```json
{
  "rules": [
    {
      "rule_id": "XSS_SCRIPT_001",
      "type": "block",
      "pattern": "<script[^>]*>.*?</script>",
      "severity": "critical",
      "category": "xss"
    }
  ]
}
```

---

## 多环境配置

配置系统支持三层配置合并：

1. **基础配置**：`config/` 目录下的配置文件
2. **环境覆盖**：`config/environment/{env}.yaml`
3. **环境变量**：运行时环境变量

配置合并优先级：环境变量 > 环境覆盖 > 基础配置

### 环境差异

| 配置项 | 开发环境 | 预发布环境 | 生产环境 |
|--------|---------|-----------|---------|
| 日志级别 | debug | info | warning |
| 沙箱隔离 | none | process | container |
| 审计保留 | 7 天 | 30 天 | 90 天 |
| 配置热更新 | 10s | 30s | 60s |

---

## 配置热更新

启用配置热更新后，系统会自动检测配置文件变化并重新加载：

```yaml
hot_reload:
  enabled: true
  check_interval_sec: 30
  supported_paths:
    - "kernel.log_level"
    - "security.rules"
```

支持热更新的配置项：
- 日志级别
- 调度器优先级权重
- 安全规则
- 审计告警配置

---

## Schema 验证

所有配置文件都有对应的 JSON Schema，用于验证配置正确性：

```bash
# 验证单个配置文件
python scripts/validate_config.py \
  --file config/kernel/settings.yaml \
  --schema config/schema/kernel-settings.schema.json

# 验证所有配置
python scripts/validate_config.py --all
```

---

## 安全最佳实践

### 1. 敏感信息管理

- 使用环境变量存储 API 密钥
- 启用配置加密功能
- 定期轮换密钥

### 2. 权限最小化

- 默认拒绝所有操作
- 仅授予必要的权限
- 启用审计日志

### 3. 网络安全

- 限制出站网络连接
- 使用 TLS 加密 IPC
- 启用入侵检测

---

## 配置变更审计

所有配置变更都会被记录到审计日志：

```
${AGENTOS_LOG_DIR}/config-audit.log
```

审计事件包括：
- `config.load` - 配置加载
- `config.reload` - 配置重载
- `config.change` - 配置变更
- `config.rollback` - 配置回滚

---

## 故障排除

### 配置验证失败

```bash
# 检查配置文件语法
python -c "import yaml; yaml.safe_load(open('config/kernel/settings.yaml'))"

# 检查环境变量
echo $AGENTOS_ROOT
echo $AGENTOS_DATA_DIR
```

### 热更新不生效

1. 检查 `hot_reload.enabled` 是否为 `true`
2. 确认配置项在 `supported_paths` 中
3. 检查文件权限

### 权限被拒绝

1. 检查 `security/policy.yaml` 中的规则
2. 确认 Agent 有对应的权限
3. 查看审计日志了解拒绝原因

---

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0.0 | 2026-03-23 | 初始版本，完整的生产级配置 |

---

## 参考文档

- [AgentOS 架构设计](../partdocs/architecture/)
- [AgentOS 编码规范](../partdocs/specifications/coding_standard/)
- [AgentOS 安全策略](../partdocs/specifications/security/)
