# Domes 配置目录 (config)

## 📁 目录定位

Domes 安全层的配置文件和策略定义目录。

## 📂 文件结构

```
config/
├── audit/                  # 审计日志配置
│   ├── audit_rules.yaml   # 审计规则
│   ├── log_rotation.yaml  # 日志轮转配置
│   └── retention.yaml     # 保留策略
├── permission/             # 权限裁决配置
│   ├── rbac_policies.yaml # RBAC 策略
│   ├── abac_rules.yaml    # ABAC 规则
│   └── capabilities.yaml  # 能力列表
├── sanitizer/              # 输入净化配置
│   ├── filters.yaml       # 过滤器规则
│   ├── patterns.yaml      # 风险模式
│   └── thresholds.yaml    # 阈值配置
├── workbench/              # 虚拟工位配置
│   ├── isolation.yaml     # 隔离策略
│   ├── resources.yaml     # 资源配额
│   └── security.yaml      # 安全配置
└── global/                 # 全局配置
    ├── domes_config.yaml  # 主配置文件
    ├── security_policy.yaml # 安全策略
    └── compliance.yaml    # 合规要求
```

## 🔧 配置示例

### 1. 审计规则配置

**文件**: `audit/audit_rules.yaml`

```yaml
# 审计规则配置
rules:
  # IPC 通信审计
  - name: "ipc_communication"
    enabled: true
    events:
      - binder_transaction
      - service_call
      - data_transfer
    filter:
      min_priority: INFO
      include_data: true
    
  # 文件访问审计
  - name: "file_access"
    enabled: true
    events:
      - file_open
      - file_read
      - file_write
      - file_delete
    paths:
      include:
        - "/data/**"
        - "/etc/**"
      exclude:
        - "/tmp/**"
        
  # 网络访问审计
  - name: "network_access"
    enabled: true
    events:
      - socket_create
      - connect
      - send
      - receive
    ports:
      all: true
```

### 2. 权限策略配置

**文件**: `permission/rbac_policies.yaml`

```yaml
# RBAC 策略定义
roles:
  # 系统管理员
  - name: "sys_admin"
    description: "系统管理员角色"
    permissions:
      - "audit:read"
      - "audit:write"
      - "permission:manage"
      - "sanitizer:configure"
      - "workbench:create"
      - "workbench:destroy"
      
  # 审计员
  - name: "auditor"
    description: "审计员角色"
    permissions:
      - "audit:read"
      - "audit:export"
      - "logs:view"
      
  # 普通用户
  - name: "user"
    description: "普通用户角色"
    permissions:
      - "workbench:use"
      - "tools:execute"
      - "data:read"

# 角色分配
assignments:
  - user: "admin"
    roles: ["sys_admin"]
  - user: "security_team"
    roles: ["auditor"]
  - user: "developer"
    roles: ["user"]
```

### 3. 输入净化配置

**文件**: `sanitizer/filters.yaml`

```yaml
# 输入过滤器配置
filters:
  # SQL 注入过滤
  - name: "sql_injection"
    enabled: true
    action: BLOCK
    patterns:
      - "(?i)(SELECT|INSERT|UPDATE|DELETE|DROP).*(FROM|INTO|TABLE)"
      - "(?i)(UNION|OR|AND).*\\d+=\\d+"
    severity: CRITICAL
    
  # XSS 攻击过滤
  - name: "xss_attack"
    enabled: true
    action: BLOCK
    patterns:
      - "<script[^>]*>.*?</script>"
      - "javascript:"
      - "on\\w+\\s*="
    severity: HIGH
    
  # 路径遍历过滤
  - name: "path_traversal"
    enabled: true
    action: BLOCK
    patterns:
      - "\\.\\./\\.\\."
      - "^/etc/"
      - "%2e%2e%2f"
    severity: HIGH
    
  # 命令注入过滤
  - name: "command_injection"
    enabled: true
    action: BLOCK
    patterns:
      - ";\\s*(rm|cat|wget|curl)"
      - "\\|\\s*(bash|sh)"
      - "`.*`"
    severity: CRITICAL
```

### 4. 虚拟工位资源配置

**文件**: `workbench/resources.yaml`

```yaml
# 资源配额配置
quotas:
  # CPU 配额
  cpu:
    default_limit_us: 100000  # 100ms
    max_limit_us: 500000      # 500ms
    burst_allowed: true
    
  # 内存配额
  memory:
    default_limit_mb: 256
    max_limit_mb: 2048
    oom_kill: true
    
  # 磁盘配额
  disk:
    default_limit_mb: 1024
    max_limit_mb: 10240
    io_limit_mbps: 100
    
  # 网络配额
  network:
    bandwidth_limit_mbps: 100
    connections_max: 100
    packets_per_second: 10000
    
  # 进程配额
  processes:
    max_count: 50
    max_threads: 200
    max_open_files: 1024
```

## 🚀 配置加载

### 程序化加载

```c
#include <domes/config.h>

// 加载配置文件
int result = domes_config_load("/etc/domes/global/domes_config.yaml");
if (result != 0) {
    LOG_ERROR("Failed to load config");
    return -1;
}

// 获取审计配置
audit_config_t* audit_cfg = domes_config_get_audit();
LOG_INFO("Audit enabled: %d", audit_cfg->enabled);

// 获取权限配置
permission_config_t* perm_cfg = domes_config_get_permission();
LOG_INFO("RBAC policies loaded: %d", perm_cfg->policy_count);

// 释放配置
domes_config_cleanup();
```

### 热重载配置

```bash
# 重新加载配置
kill -HUP $(pidof domes_service)

# 或者使用 CLI
domes-ctl config reload
```

## 📊 配置验证

### 使用 CLI 验证

```bash
# 验证所有配置
domes-ctl config validate

# 验证特定配置
domes-ctl config validate audit
domes-ctl config validate permission
domes-ctl config validate sanitizer

# 验证语法
domes-ctl config lint /path/to/config.yaml
```

### 编程验证

```c
#include <domes/config_validator.h>

// 验证配置文件
validation_result_t result = config_validate_file(
    "/etc/domes/audit/audit_rules.yaml"
);

if (!result.valid) {
    LOG_ERROR("Config validation failed:");
    for (int i = 0; i < result.error_count; i++) {
        LOG_ERROR("  - %s", result.errors[i]);
    }
    return -1;
}

LOG_INFO("Config validated successfully");
```

## 🔐 安全配置

### 加密敏感配置

```yaml
# 使用加密值
database:
  password: ENC[AES256,aGVsbG93b3JsZA==]
  api_key: ENC[AES256,c2VjcmV0a2V5MTIz]
  
# 解密在运行时进行
decryption:
  provider: openssl
  key_source: env:DOME_CONFIG_KEY
  key_file: /etc/domes/.key
```

### 配置完整性校验

```bash
# 生成配置哈希
sha256sum /etc/domes/**/*.yaml > /etc/domes/checksums.sha256

# 验证配置完整性
sha256sum -c /etc/domes/checksums.sha256
```

## 📝 最佳实践

### 1. 配置分层

```
/etc/domes/
├── base/           # 基础配置（只读）
├── override/       # 覆盖配置（环境特定）
└── runtime/        # 运行时配置（动态生成）
```

**优势**：
- 基础配置保持不变
- 环境配置独立管理
- 运行时配置可动态调整

### 2. 版本控制

```bash
# 将配置纳入版本控制
git add config/*.yaml

# 使用 Git tags 标记配置版本
git tag -a config-v1.0.0 -m "Initial config version"
```

### 3. 配置模板化

```yaml
# 使用环境变量
database:
  host: ${DB_HOST:-localhost}
  port: ${DB_PORT:-5432}
  user: ${DB_USER:-domes}
  password: ${DB_PASSWORD}
```

## 🔍 故障排查

### 常见问题

#### 1. 配置加载失败

```bash
# 检查文件权限
ls -l /etc/domes/config.yaml

# 验证 YAML 语法
yamllint /etc/domes/config.yaml

# 查看详细日志
journalctl -u domes -f
```

#### 2. 配置不生效

```bash
# 检查配置是否被覆盖
domes-ctl config show --source

# 查看配置加载顺序
domes-ctl config debug-load
```

## 📚 相关文档

- [Domes 架构](../README.md) - Domes 层整体介绍
- [审计日志模块](src/audit/README.md) - 审计日志配置详解
- [权限裁决模块](src/permission/README.md) - 权限策略配置
- [输入净化模块](src/sanitizer/README.md) - 过滤器配置
- [虚拟工位模块](src/workbench/README.md) - 资源配额配置
- [配置管理指南](../../partdocs/configuration/README.md) - 全局配置管理

## 🤝 贡献指南

### 提交配置变更

1. **修改配置文件**
2. **验证语法**: `yamllint config/*.yaml`
3. **测试效果**: 在测试环境验证
4. **提交 PR**: 说明变更原因和影响

## 📞 支持

- **问题反馈**: https://github.com/AgentOS/AgentOS/issues
- **讨论区**: https://github.com/AgentOS/AgentOS/discussions
- **文档**: https://agentos.dev/domes/config

---

*最后更新：2026-03-25*  
*维护者：AgentOS Domes Team*
