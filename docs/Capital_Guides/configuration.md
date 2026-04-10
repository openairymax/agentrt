Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# 配置指南

**版本**: Doc V1.8
**最后更新**: 2026-04-10
**适用范围**: 所有部署环境
**作者**: Team  
**理论基础**: 体系并行论 (MCIS)、五维正交系统、工程两论（控制论与系统工程）

---

## 🎯 理论指导：MCIS视角的配置管理

配置管理是系统工程的基石，在 **体系并行论 (MCIS)** 理论框架中具有重要的理论意义。从 MCIS 视角理解 AgentOS 的配置管理，有助于构建更加灵活、可靠、可维护的系统配置体系。

### 配置在 MCIS 系统中的理论意义

在 MCIS 理论中，配置管理体现了以下几个核心原理：

1. **系统观维度：系统工程层次分解**
   - 分层配置策略对应系统的层次分解结构
   - 不同环境（开发、测试、生产）的配置分离体现系统环境隔离
   - 配置优先级机制（环境变量 > 命令行参数 > 配置文件 > 默认值）体现系统灵活性与确定性平衡

2. **内核观维度：安全与稳定性的保障**
   - 配置验证与安全检查确保系统安全性
   - 配置版本控制支持系统的可追溯性与可回滚性
   - 配置加密与敏感信息保护体现最小特权原则

3. **工程观维度：可维护性与可扩展性**
   - 模块化配置设计支持系统的渐进式扩展
   - 配置热重载机制支持系统的动态调整
   - 配置模板与继承机制提高配置的可重用性

4. **认知观维度：人机协同的接口设计**
   - 人性化的配置格式（YAML/JSON）降低配置复杂度
   - 配置文档与示例提供学习与参考支持
   - 配置验证与错误提示增强配置的可调试性

5. **设计美学维度：简约与优雅的配置体验**
   - 统一的配置风格与命名规范提升配置一致性
   - 合理的默认值设计减少必要的配置项
   - 清晰的配置结构与注释提升配置可读性

### MCIS 多体协同的配置映射

AgentOS 的配置体系与 MCIS 中的不同 **体 (Body)** 紧密相关：

- **基础体配置** → 内核参数、网络设置、资源限制等基础运行时配置
- **认知体配置** → LLM模型参数、推理策略、决策阈值等智能配置
- **执行体配置** → 工具调用参数、执行超时、重试策略等执行配置
- **记忆体配置** → 存储引擎参数、索引配置、缓存策略等记忆配置
- **可观测体配置** → 日志级别、监控指标、追踪采样等可观测性配置

### 配置管理的理论原则

基于 MCIS 理论的配置管理遵循以下原则：

1. **正交分离原则** → 不同组件的配置相互独立，避免配置耦合
2. **渐进式抽象原则** → 从基础配置到高级配置的层次化设计
3. **反馈调节原则** → 配置变更的验证与回滚机制，形成控制论负反馈
4. **安全内生原则** → 配置的安全验证与加密保护，确保系统安全性

通过 MCIS 理论视角理解配置管理，你将能够设计出更加系统、灵活、可靠的配置体系，为 AgentOS 的稳定运行与持续演进提供坚实基础。

---

## 📋 配置文件总览

AgentOS采用**分层配置**策略，支持多环境、多实例配置：

```
优先级（从高到低）:
  环境变量 > 命令行参数 > 配置文件 > 默认值
```

### 配置文件位置

| 环境 | 配置路径 | 用途 |
|------|---------|------|
| **Docker开发** | `docker/.env` | Docker Compose环境变量 |
| **Docker生产** | `docker/.env.production` | 生产环境变量 |
| **源码安装** | `config/kernel.yaml` | 内核主配置 |
| **源码安装** | `config/llm.yaml` | LLM服务配置 |
| **源码安装** | `config/memory.yaml` | 记忆系统配置 |
| **Kubernetes** | ConfigMap/Secret | K8s原生配置 |

---

## 🎛️ 内核核心配置 (kernel.yaml)

```yaml
# =============================================================================
# AgentOS Kernel Configuration
# 版本: 1.0.0
# =============================================================================

# -----------------------------------------------------------------------------
# 服务器配置 (Server)
# -----------------------------------------------------------------------------
server:
  # IPC API绑定地址
  bind_address: "0.0.0.0"
  bind_port: 8080

  # Prometheus监控端口
  metrics_port: 9090

  # 工作线程数（默认为CPU核心数）
  worker_threads: 8

  # 请求超时时间（毫秒）
  request_timeout_ms: 30000

  # 最大请求体大小（字节）
  max_request_body_size: 104857600  # 100MB

# -----------------------------------------------------------------------------
# IPC配置 (Inter-Process Communication)
# -----------------------------------------------------------------------------
ipc:
  # 消息队列最大长度
  queue_max_length: 10000

  # 单条消息最大大小（字节）
  max_message_size: 1048576  # 1MB

  # 零拷贝阈值（超过此值使用零拷贝）
  zero_copy_threshold: 65536  # 64KB

  # 批量发送大小
  batch_size: 32

# -----------------------------------------------------------------------------
# 内存管理配置 (Memory Management)
# -----------------------------------------------------------------------------
memory:
  # 对象池配置
  pool:
    # 池总大小（MB）
    total_size_mb: 1024

    # 小对象块大小（字节）
    small_block_size: 256

    # 大对象块大小（字节）
    large_block_size: 4096

  # 泄漏检测
  leak_detection:
    enabled: true  # Debug模式自动启用
    threshold_mb: 100  # 超过此值告警
    dump_on_exit: true  # 进程退出时输出泄漏报告

# -----------------------------------------------------------------------------
# 任务调度配置 (Task Scheduler)
# -----------------------------------------------------------------------------
scheduler:
  # 最大并发任务数
  max_concurrent_tasks: 100

  # 任务队列长度
  queue_length: 10000

  # 调度算法: fifo, priority, weighted_fair
  algorithm: "priority"

  # 任务超时时间（秒），0表示无限制
  default_timeout_seconds: 300

  # 失败重试次数
  max_retries: 3

  # 重试退避策略: fixed, exponential, linear
  retry_backoff: "exponential"
  retry_initial_delay_ms: 1000
  retry_max_delay_ms: 30000

# -----------------------------------------------------------------------------
# 时间服务配置 (Time Service)
# -----------------------------------------------------------------------------
time_service:
  # NTP服务器列表
  ntp_servers:
    - "ntp.aliyun.com"
    - "ntp.tencent.com"
    - "pool.ntp.org"

  # 同步间隔（秒）
  sync_interval_seconds: 3600

  # 允许的最大时钟漂移（毫秒）
  max_drift_ms: 100

# -----------------------------------------------------------------------------
# 日志配置 (Logging)
# -----------------------------------------------------------------------------
logging:
  # 日志级别: DEBUG, INFO, WARNING, ERROR, CRITICAL
  level: "INFO"

  # 输出格式: json, text
  format: "json"

  # 输出目标
  outputs:
    console:
      enabled: true
      colorize: false  # 生产环境建议关闭颜色
    file:
      enabled: true
      path: "/app/logs/kernel.log"
      rotation:
        max_size_mb: 100
        max_files: 5
        compress: true

  # 结构化字段
  include_fields:
    - timestamp
    - level
    - module
    - message
    - trace_id
    - context

# -----------------------------------------------------------------------------
# 安全配置 (Security - Cupolas)
# -----------------------------------------------------------------------------
security:
  cupolas:
    enabled: true

    # Layer 1: Workbench Sandboxing
    workbench:
      isolation_type: "process"  # process, container, namespace
      resource_limits:
        cpu_quota: 2.0          # CPU核数限制
        memory_mb: 512           # 内存限制(MB)
        max_file_descriptors: 1024
        max_processes: 50

    # Layer 2: Permission RBAC
    permission:
      mode: "strict"  # strict, relaxed, disabled
      policy_file: "/app/config/policies.yaml"
      cache_ttl_seconds: 300  # 权限缓存TTL

    # Layer 3: Input Sanitization
    sanitizer:
      mode: "strict"  # strict, relaxed, disabled
      rules:
        sql_injection:
          enabled: true
          patterns:
            - "(?i)(\\b(select|insert|update|delete|drop|union)\\b)"
        xss:
          enabled: true
          sanitize_html: true
        command_injection:
          enabled: true
          blocklist:
            - ";"
            - "|"
            - "$("
            - "`"

    # Layer 4: Audit Trail
    audit:
      enabled: true
      log_path: "/app/logs/audit.log"
      format: "json"
      retention_days: 90
      sensitive_fields:  # 自动脱敏的字段
        - api_key
        - password
        - token
        - secret

  # TLS配置（API加密传输）
  tls:
    enabled: false  # 生产环境建议开启
    cert_path: "/etc/ssl/certs/agentos.crt"
    key_path: "/etc/ssl/private/agentos.key"
    ca_path: "/etc/ssl/certs/ca-bundle.crt"
    min_version: "TLS1.2"
    cipher_suites:
      - "ECDHE-ECDSA-AES128-GCM-SHA256"
      - "ECDHE-RSA-AES128-GCM-SHA256"
```

---

## 🤖 LLM服务配置 (llm.yaml)

```yaml
# =============================================================================
# AgentOS LLM Service Configuration
# =============================================================================

# 默认提供商
default_provider: "openai"

# 提供商列表
providers:
  openai:
    type: "openai_compatible"
    api_key: "${AGENTOS_LLM_API_KEY}"  # 从环境变量读取
    base_url: "https://api.openai.com/v1"
    models:
      primary:
        name: "gpt-4-turbo"
        context_window: 128000
        max_output_tokens: 4096
        cost_per_1k_input: 0.01   # 美元
        cost_per_1k_output: 0.03
      secondary:
        name: "gpt-4o-mini"
        context_window: 128000
        max_output_tokens: 16384
        cost_per_1k_input: 0.00015
        cost_per_1k_output: 0.0006

  deepseek:
    type: "openai_compatible"
    api_key: "${DEEPSEEK_API_KEY}"
    base_url: "https://api.deepseek.com/v1"
    models:
      primary:
        name: "deepseek-chat"
        context_window: 64000
        max_output_tokens: 8192
        cost_per_1k_input: 0.0014
        cost_per_1k_output: 0.0028

  local:
    type: "local"
    base_url: "http://localhost:8081/v1"  # 本地模型服务
    models:
      primary:
        name: "qwen2.5-72b-instruct"
        context_window: 32768
        max_output_tokens: 2048
        cost_per_1k_input: 0.0
        cost_per_1k_output: 0.0

# 双系统协同配置 (C-1原则)
dual_system:
  system1_model: "openai/gpt-4o-mini"     # 快速直觉推理
  system2_model: "openai/gpt-4-turbo"       # 深度逻辑验证
  collaboration_mode: "cross_validation"    # parallel, sequential, cross_validation
  disagreement_threshold: 0.3               # 不一致度阈值(0-1)

# 推理参数默认值
inference_defaults:
  temperature: 0.7
  top_p: 0.9
  max_tokens: 4096
  presence_penalty: 0.0
  frequency_penalty: 0.0

# Token管理
token_management:
  # 单次请求最大Token数
  max_tokens_per_request: 16000

  # 上下文窗口利用率上限（避免溢出）
  context_utilization_limit: 0.85

  # 成本控制
  cost_control:
    daily_budget_usd: 50.0
    per_task_budget_usd: 5.0
    alert_threshold: 0.8  # 达到预算80%时告警

# 缓存配置
cache:
  enabled: true
  ttl_seconds: 3600  # 缓存1小时
  max_entries: 10000
  similarity_threshold: 0.95  # 语义相似度>95%时命中缓存

# 重试和容错
retry:
  max_attempts: 3
  backoff_strategy: "exponential"
  initial_delay_ms: 1000
  max_delay_ms: 10000
  retryable_errors:
    - "rate_limit_exceeded"
    - "server_error"
    - "timeout"
```

---

## 🧠 记忆系统配置 (memory.yaml)

```yaml
# =============================================================================
# AgentOS MemoryRovol Configuration (C-3/C-4原则)
# =============================================================================

# L1: 感觉记忆 (Sensory Memory)
l1:
  storage_backend: "redis"  # redis, memory
  connection:
    host: "${REDIS_HOST:-redis}"
    port: ${REDIS_PORT:-6379}
    password: "${REDIS_PASSWORD}"
    db: 0
  config:
    ttl_seconds: 3600         # 保持时间：1小时
    max_entries: 50000        # 最大条目数
    eviction_policy: "allkeys-lru"  # 淘汰策略

# L2: 短期记忆 (Short-term Memory)
l2:
  storage_backend: "postgresql+faiss"
  database:
    host: "${POSTGRES_HOST:-postgres}"
    port: ${POSTGRES_PORT:-5432}
    user: "${POSTGRES_USER:-agentos}"
    password: "${POSTGRES_PASSWORD}"
    dbname: "${POSTGRES_DB:-agentos}"
  vector_index:
    engine: "faiss"            # faiss, hnswlib, milvus
    index_type: "ivf_flat"     # ivf_flat, ivf_pq, hnsw
    dimension: 768             # 向量维度（与embedding模型匹配）
    nlist: 1024                # IVF分区数
    nprobe: 32                 # 探测数（越大越精确但越慢）
    metric: "inner_product"    # inner_product, l2, cosine
  config:
    ttl_seconds: 86400         # 保持时间：24小时
    max_entries: 1000000
    compression_enabled: true  # 启用向量压缩以节省空间

# L3: 长期记忆 (Long-term Memory)
l3:
  storage_backend: "postgresql"
  database:
    # 使用与L2相同的数据库连接
    # （可通过database_ref复用L2的连接配置）
  config:
    # 结构化知识存储
    knowledge_graph:
      enabled: true
      auto_extract_entities: true
      auto_extract_relations: true
    retention_days: 365        # 保持时间：1年
    max_entries: 10000000
    index_tables:
      - entity_index
      - relation_index
      - temporal_index
      - category_index

# L4: 模式记忆 (Pattern Memory)
l4:
  storage_backend: "postgresql"
  config:
    # 抽象模式和经验法则
    pattern_types:
      - task_pattern           # 任务执行模式
      - error_pattern          # 错误处理模式
      - optimization_pattern   # 优化经验模式
      - decision_pattern       # 决策模式
    min_confidence: 0.7        # 最小置信度阈值
    min_occurrences: 5         # 最小出现次数
    retention: permanent       # 永久保存（除非手动删除）

# 遗忘机制 (C-4原则)
forgetting:
  enabled: true
  curve_type: "ebbinghaus"     # ebbinghaus, linear, exponential
  review_intervals:            # 复习间隔（基于艾宾浩斯曲线）
    - seconds: 300             # 5分钟后第一次复习
    - seconds: 3600            # 1小时后第二次复习
    - seconds: 86400           # 1天后第三次复习
    - seconds: 604800          # 1周后第四次复习
    - seconds: 2592000         # 1月后第五次复习
  decay_rate: 0.1              # 自然衰减率（每天降低10%权重）
  importance_boost:            # 重要程度提升因子
    critical: 5.0              # 关键记忆衰减速度降为1/5
    high: 2.0                  # 高重要性记忆衰减速度降为1/2
    normal: 1.0                # 正常衰减
    low: 0.5                   # 低重要性记忆加速遗忘

# 自动整理
auto_compaction:
  enabled: true
  schedule: "0 3 * * *"       # 每天凌晨3点执行
  actions:
    - expire_expired_records   # 过期记录归档或删除
    - compress_low_weight      # 低权重记录压缩存储
    - migrate_l2_to_l3         # L2重要记录迁移到L3
    - optimize_indices          # 重建索引以提高查询性能
```

---

## 🔌 守护进程通用配置 (daemon-common.yaml)

```yaml
# =============================================================================
# AgentOS Daemon Common Configuration
# =============================================================================

# 所有守护进程共享的基础配置
base:
  # 日志级别
  log_level: "${LOG_LEVEL:-INFO}"

  # 内核连接
  kernel_url: "${AGENTOS_KERNEL_URL:-http://localhost:8080}"

  # 数据库连接
  database_url: "${DATABASE_URL}"

  # Redis连接
  redis_url: "${REDIS_URL:-redis://localhost:6379/0}"

# 各守护进程特定配置
daemons:

  llm_d:
    port: 8001
    workers: 4
    config_file: "/app/config/llm.yaml"

  market_d:
    port: 8002
    workers: 2
    tool_registry:
      auto_discovery: true
      scan_interval_seconds: 300
      max_tools: 10000

  monit_d:
    port: 8003
    workers: 1
    alerts:
      email:
        enabled: false
        smtp_host: "smtp.example.com"
        smtp_port: 587
        from_addr: "alerts@spharx.cn"
        to_addrs:
          - "ops-team@spharx.cn"
      webhook:
        enabled: false
        url: "${ALERT_WEBHOOK_URL}"

  sched_d:
    port: 8004
    workers: 4
    queues:
      - name: "critical"
        priority: 100
        concurrency: 10
      - name: "high"
        priority: 75
        concurrency: 20
      - name: "normal"
        priority: 50
        concurrency: 30
      - name: "low"
        priority: 25
        concurrency: 10

  tool_d:
    port: 8005
    workers: 8
    sandbox:
      enabled: true
      timeout_seconds: 120
      max_memory_mb: 512
      allowed_network: false  # 是否允许网络访问

  gateway_d:
    port: 8006
    workers: 4
    rate_limiting:
      enabled: true
      requests_per_minute: 60
      burst_size: 10
    cors:
      allowed_origins:
        - "https://openlab.spharx.cn"
        - "http://localhost:5173"
      allowed_methods:
        - GET
        - POST
        - PUT
        - DELETE
      allowed_headers:
        - Content-Type
        - Authorization
        - X-Request-ID
```

---

## 🛡️ 环境变量速查表

| 变量名 | 说明 | 默认值 | 必填 |
|--------|------|--------|------|
| `AGENTOS_IPC_BIND_ADDR` | IPC绑定地址 | 0.0.0.0 | ❌ |
| `AGENTOS_IPC_BIND_PORT` | IPC绑定端口 | 8080 | ❌ |
| `LOG_LEVEL` | 日志级别 | INFO | ❌ |
| `TZ` | 时区 | Asia/Shanghai | ❌ |
| `AGENTOS_CUPOLAS_ENABLED` | 启用安全穹顶 | true | ❌ |
| `AGENTOS_PERMISSION_MODE` | 权限检查模式 | strict | ❌ |
| `AGENTOS_SANITIZER_MODE` | 输入净化模式 | strict | ❌ |
| `AGENTOS_AUDIT_ENABLED` | 启用审计日志 | true | ❌ |
| `AGENTOS_LLM_PROVIDER` | LLM提供商 | openai | ❌ |
| `AGENTOS_LLM_API_KEY` | LLM API密钥 | - | ✅ |
| `AGENTOS_LLM_BASE_URL` | LLM API地址 | https://api.openai.com/v1 | ❌ |
| `AGENTOS_LLM_MODEL` | 默认LLM模型 | gpt-4-turbo | ❌ |
| `DATABASE_URL` | 数据库连接字符串 | - | ✅ |
| `POSTGRES_PASSWORD` | PostgreSQL密码 | - | ✅ |
| `REDIS_PASSWORD` | Redis密码 | - | ✅ |

完整环境变量示例请参考：[docker/.env.example](../docker/.env.example)

---

## ✅ 配置验证

启动前验证配置是否有效：

```bash
# Docker环境
docker compose -f docker/docker-compose.yml config  # 验证Compose语法

# 源码环境
./build/bin/agentos-kernel --validate-config --config config/kernel.yaml

# 检查必需的环境变量
./scripts/check-env.sh --required AGENTOS_LLM_API_KEY,DATABASE_URL
```

---

## 📚 相关文档

- [**快速开始**](getting-started.md) — 5分钟上手体验
- [**安装指南**](installation.md) — 详细的环境搭建步骤
- [**架构概览**](../architecture/overview.md) — 理解各模块关系
- [**故障排查**](../troubleshooting/common-issues.md) — 配置问题诊断

---

> *"好的配置是可读、可维护、可验证的。"*

**© 2026 SPHARX Ltd. All Rights Reserved.**
