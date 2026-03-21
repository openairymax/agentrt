# AgentOS 配置中心

**版本**: 1.0.0.5  
**最后更新**: 2026-03-18  

---

## 📋 概述

`config/` 目录包含 AgentOS 的所有配置文件，采用 YAML 和 JSON 格式，按功能模块分类组织。

---

## 📁 目录结构

```
config/
├── README.md                    # 本文件
├── agents/                      # Agent 配置
│   ├── registry.yaml           # Agent 注册表
│   └── profiles/               # Agent 配置文件
│       └── *.yaml              # 各 Agent 配置
│
├── kernel/                      # 内核配置
│   └── settings.yaml           # 内核参数（日志、调度器、内存、IPC）
│
├── logging/                     # 日志配置
│   └── config.yaml             # 日志级别、格式、输出目标
│
├── models/                      # 模型配置
│   └── models.yaml             # LLM 模型配置（API 密钥、端点）
│
├── sanitizer/                   # 净化器配置
│   └── sanitizer_rules.json    # 沙箱规则配置
│
├── security/                    # 安全配置
│   ├── policy.yaml             # 安全策略
│   └── permission_rules.yaml   # RBAC 权限模型配置
│
├── services/                    # 服务配置
│   ├── llm_d/                  # LLM 服务配置
│   ├── market_d/               # 市场服务配置
│   ├── monit_d/                # 监控服务配置
│   ├── perm_d/                 # 权限服务配置
│   ├── sched_d/                # 调度服务配置
│   └── tool_d/                 # 工具服务配置
│
└── skills/                      # 技能配置
    └── registry.yaml           # 技能注册表配置
```

---

## ⚙️ 配置说明

### agents/registry.yaml

Agent 注册表配置，定义所有可用 Agent 的元数据：

```yaml
version: "1.0"
agents:
  - id: architect_001
    name: 架构师智能体
    type: planning
    version: "1.0.0"
    capabilities:
      - system_design
      - architecture_review
    config_path: "profiles/architect.yaml"
    
  - id: backend_dev_001
    name: 后端开发智能体
    type: execution
    version: "1.0.0"
    capabilities:
      - api_design
      - database_design
```

### kernel/settings.yaml

内核全局配置：

```yaml
kernel:
  log_level: INFO
  scheduler:
    algorithm: weighted_round_robin
    time_slice_ms: 10
  memory:
    pool_size_mb: 512
    max_allocation_mb: 1024
  ipc:
    max_connections: 1024
    buffer_size_kb: 64
```

### logging/config.yaml

统一日志配置：

```yaml
logging:
  default_level: INFO
  format: "%(asctime)s.%(msecs)03d [%(levelname)s] [%(name)s] %(message)s"
  
  outputs:
    - type: file
      path: "../partdata/logs/agentos.log"
      max_size_mb: 100
      backup_count: 7
      
    - type: console
      enabled: true
      
  modules:
    atoms: DEBUG
    backs: INFO
    domes: WARN
```

### models/models.yaml

LLM 模型配置：

```yaml
models:
  openai:
    provider: openai
    api_key: "${OPENAI_API_KEY}"
    api_base: https://api.openai.com/v1
    default_model: gpt-4
    timeout_sec: 30
    
  deepseek:
    provider: deepseek
    api_key: "${DEEPSEEK_API_KEY}"
    api_base: https://api.deepseek.com
    default_model: deepseek-chat
    timeout_sec: 30
```

### services/llm_d/config.yaml

LLM 服务配置：

```yaml
service:
  name: llm_d
  listen_addr: "0.0.0.0:18791"
  log_level: INFO
  
providers:
  - name: openai
    enabled: true
    api_key: "${OPENAI_API_KEY}"
    rate_limit_per_min: 100
    
  - name: anthropic
    enabled: true
    api_key: "${ANTHROPIC_API_KEY}"
    
cache:
  enabled: true
  ttl_hours: 24
  max_size_mb: 512
```

---

## 🔧 使用方法

### 加载配置

#### C 语言

```c
#include <agentos/config.h>

agentos_config_t* config;
int ret = agentos_config_load("config/kernel/settings.yaml", &config);
if (ret == 0) {
    const char* log_level = agentos_config_get(config, "kernel.log_level");
    // ...
    agentos_config_free(config);
}
```

#### Python

```python
from agentos import Config

config = Config.load("config/services/llm_d/config.yaml")
print(f"Service: {config.service.name}")
print(f"Listen: {config.service.listen_addr}")
```

### 环境变量替换

配置文件支持环境变量替换，使用 `${VAR_NAME}` 语法：

```yaml
database:
  host: ${DB_HOST}
  port: ${DB_PORT:-5432}  # 支持默认值
```

---

## 📖 最佳实践

### 1. 敏感信息管理

- ✅ 使用环境变量存储 API 密钥
- ✅ 生产环境使用加密配置
- ❌ 禁止将密钥直接写入配置文件

### 2. 配置分层

- **基础配置**: 放在 `config/` 目录
- **环境配置**: 使用 `.env` 文件或环境变量
- **运行时配置**: 通过 API 动态调整

### 3. 版本控制

- ✅ 配置文件模板纳入版本控制
- ✅ 提供 `.example` 或 `.template` 文件
- ❌ 不包含真实密钥的配置

---

## 🔍 配置验证

使用提供的验证脚本检查配置文件：

```bash
# 验证所有配置
python scripts/validate_contracts.py

# 验证单个配置
python -c "from agentos import Config; Config.load('config/xxx.yaml')"
```

---

## 📚 相关文档

- [部署指南](../partdocs/guides/deployment.md) - 生产环境配置
- [内核调优](../partdocs/guides/kernel_tuning.md) - 性能优化
- [故障排查](../partdocs/guides/troubleshooting.md) - 常见问题

---

**Apache License 2.0 © 2026 SPHARX**

---

© 2026 SPHARX Ltd. 保留所有权利。
