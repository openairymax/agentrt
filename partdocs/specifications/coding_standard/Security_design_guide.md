# AgentOS 安全设计指南

**版本**: Doc V1.5  
**发布日期**: 2026-03-24  
**适用范围**: AgentOS 所有组件和模块  
**理论基础**: 工程两论（反馈闭环）、安全穹顶（Domes）、系统工程（层次分解）

---

## 一、概述

### 1.1 编制目的

本指南为 AgentOS 项目提供全面的安全设计标准。基于项目架构设计原则的四维正交体系，本指南聚焦于安全观维度，阐述如何通过 Domes（安全穹顶）实现纵深防御。

### 1.2 与 AgentOS 架构的关系

基于架构设计原则**E-1（安全内生原则）**，AgentOS 的安全穹顶（Domes）采用四层防护体系：

| 层次 | 名称 | 组件路径 | 功能 | 实现机制 |
|------|------|---------|------|---------|
| D1 | **虚拟工位** | `domes/workbench/` | 进程/容器级隔离 | 容器命名空间、WASM 沙箱、资源限额 |
| D2 | **权限裁决** | `domes/permission/` | 动态规则引擎 | YAML 规则、RBAC、ABAC |
| D3 | **输入净化** | `domes/sanitizer/` | 输入验证过滤 | 正则表达式、白名单、类型检查 |
| D4 | **审计追踪** | `domes/audit/` | 全链路追踪 | 异步日志、不可篡改、轮转归档 |

**安全原则**（关联原则 E-1）:

| 原则 | 说明 | 在 Domes 中的体现 | 关联原则 |
|------|------|---------------------|---------|
| 最小权限 | 只授予完成任务所需的最小权限 | D2 权限裁决 | K-1 |
| 纵深防御 | 多层安全检查 | D1-D4 四层防护 | S-2 |
| 默认安全 | 默认配置应是安全的 | 默认拒绝策略 | E-1 |
| 故障安全 | 发生错误时默认拒绝 | 安全机制故障时拒绝访问 | E-6 |
| 开放设计 | 安全性不依赖算法保密 | 公开算法和协议 | K-2 |
| 可观测性 | 所有安全事件必须可追踪 | D4 审计追踪 | E-2 |

本指南详细阐述每一层的设计原则和实现要求。

---

## 二、安全穹顶（Domes）设计

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                     AgentOS 系统边界                         │
│  ┌───────────────────────────────────────────────────────┐  │
│  │              D1: 虚拟工位 (Virtual Workspace)           │  │
│  │  ┌───────────────────────────────────────────────────┐  │  │
│  │  │        D2: 权限裁决 (Permission Arbitration)      │  │  │
│  │  │  ┌─────────────────────────────────────────────┐  │  │  │
│  │  │  │      D3: 输入净化 (Input Sanitization)       │  │  │  │
│  │  │  │  ┌───────────────────────────────────────┐  │  │  │  │
│  │  │  │  │       D4: 审计日志 (Audit Logging)      │  │  │  │  │
│  │  │  │  │                                       │  │  │  │  │
│  │  │  │  └───────────────────────────────────────┘  │  │  │  │
│  │  │  └─────────────────────────────────────────────┘  │  │  │
│  │  └───────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 D1: 虚拟工位

```yaml
# domes/workspace.yaml
workspace:
  # 进程级隔离配置
  isolation:
    type: "container"  # process | container | vm
    resource_limits:
      memory_mb: 512
      cpu_shares: 256
      oom_score_adj: 500
    
  # 网络隔离
  network:
    mode: "none"  # none | bridge | host
    allowed_outbound:
      - "api.agentos.internal"
      - "metrics.internal"
      
  # 文件系统隔离
  filesystem:
    root: "/var/agentos/workspace/{workspace_id}"
    readonly_paths:
      - "/etc"
      - "/usr/bin"
    writable_paths:
      - "/tmp/agentos"
```

```c
// atoms/domes/workspace.h
/**
 * @file workspace.h
 * @brief 虚拟工位接口
 *
 * 提供进程/容器级隔离，确保任务在受限环境中执行。
 */

typedef struct agentos_workspace {
    uint64_t workspace_id;
    WorkspaceConfig config;
    WorkspaceState state;
    
    // 资源限制
    struct {
        uint64_t memory_limit_bytes;
        uint32_t cpu_shares;
        int oom_score_adj;
    } limits;
    
    // 隔离状态
    struct {
        bool initialized;
        bool is_isolated;
        pid_t pid;
    } isolation;
} agentos_workspace_t;

/**
 * @brief 创建虚拟工位
 */
int workspace_create(
    const WorkspaceConfig* config,
    agentos_workspace_t** out_workspace
);

/**
 * @brief 销毁虚拟工位
 */
int workspace_destroy(agentos_workspace_t* workspace);

/**
 * @brief 在工位中执行任务
 */
int workspace_execute(
    agentos_workspace_t* workspace,
    const TaskConfig* task_config,
    TaskResult* out_result
);
```

### 2.3 D2: 权限裁决

```yaml
# domes/permission_rules.yaml
permission_rules:
  # 默认策略：拒绝
  default_policy: "deny"
  
  # 角色定义
  roles:
    admin:
      permissions:
        - "task:*"
        - "memory:*"
        - "config:*"
        
    operator:
      permissions:
        - "task:submit"
        - "task:query"
        - "memory:read"
        
    developer:
      permissions:
        - "task:submit"
        - "task:query"
        
    viewer:
      permissions:
        - "task:query"
        
  # 用户绑定
  bindings:
    - user: "admin@agentos.internal"
      roles: ["admin"]
    - user: "operator01@agentos.internal"
      roles: ["operator"]
```

```c
// atoms/domes/permission.h
/**
 * @brief 权限裁决结果
 */
typedef enum {
    PERMISSION_DENIED = 0,
    PERMISSION_GRANTED = 1,
} PermissionResult;

/**
 * @brief 检查权限
 *
 * @param user_id 用户标识
 * @param permission 权限字符串，格式：resource:action
 * @param context 权限上下文（资源所有者等）
 * @return 权限裁决结果
 *
 * @example
 * PermissionResult result = check_permission(
 *     "user123",
 *     "task:submit",
 *     &(PermissionContext){ .owner = "org456" }
 * );
 */
PermissionResult check_permission(
    const char* user_id,
    const char* permission,
    const PermissionContext* context
);

/**
 * @brief 权限裁决日志
 */
void log_permission_check(
    const char* user_id,
    const char* permission,
    PermissionResult result,
    const char* reason
);
```

### 2.4 D3: 输入净化

```yaml
# domes/input_sanitization_rules.yaml
sanitization_rules:
  # SQL 注入防护
  sql_injection:
    enabled: true
    patterns:
      - "'"
      - "\""
      - ";"
      - "--"
      - "/*"
      - "*/"
      - "UNION"
      - "SELECT"
      - "DROP"
      
  # 命令注入防护
  command_injection:
    enabled: true
    patterns:
      - ";"
      - "|"
      - "&"
      - "$"
      - "`"
      - "\n"
      - "\r"
      
  # 路径遍历防护
  path_traversal:
    enabled: true
    blocked_patterns:
      - "../"
      - "..\\"
      - "%2e%2e"
      
  # XSS 防护
  xss:
    enabled: true
    patterns:
      - "<script"
      - "javascript:"
      - "onerror="
```

```c
// atoms/domes/sanitizer.h
/**
 * @brief 输入净化上下文
 */
typedef struct sanitizer_context {
    const char* input;
    size_t input_length;
    SanitizationRuleType rule_type;
    bool is_sanitized;
} sanitizer_context_t;

/**
 * @brief 净化输入
 *
 * @param context 净化上下文
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 净化后的字符串长度，负值表示错误
 */
int sanitize_input(
    const sanitizer_context_t* context,
    char* output,
    size_t output_size
);

/**
 * @brief 预定义的净化规则
 */
typedef enum {
    SANITIZE_SQL_INJECTION = 0x01,
    SANITIZE_COMMAND_INJECTION = 0x02,
    SANITIZE_PATH_TRAVERSAL = 0x04,
    SANITIZE_XSS = 0x08,
    SANITIZE_ALL = 0xFF
} SanitizationRuleType;
```

### 2.5 D4: 审计日志

```yaml
# domes/audit_config.yaml
audit:
  # 审计日志配置
  log:
    path: "/var/agentos/logs/audit"
    rotation:
      max_size_mb: 100
      max_files: 30
      compress: true
      
  # 异步写入
  async:
    enabled: true
    queue_size: 10000
    flush_interval_ms: 1000
    
  # 审计事件类型
  event_types:
    authentication:
      - AUTH_SUCCESS
      - AUTH_FAILURE
      - AUTH_EXPIRED
      
    authorization:
      - PERMISSION_GRANTED
      - PERMISSION_DENIED
      
    data_access:
      - DATA_READ
      - DATA_WRITE
      - DATA_DELETE
      
    config_change:
      - CONFIG_MODIFIED
      - RULE_UPDATED
```

```c
// atoms/domes/audit.h
/**
 * @brief 审计事件类型
 */
typedef enum {
    // 认证事件 (1xxx)
    AUDIT_AUTH_SUCCESS = 1001,
    AUDIT_AUTH_FAILURE = 1002,
    AUDIT_AUTH_EXPIRED = 1003,
    
    // 授权事件 (2xxx)
    AUDIT_PERMISSION_GRANTED = 2001,
    AUDIT_PERMISSION_DENIED = 2002,
    
    // 数据访问事件 (3xxx)
    AUDIT_DATA_READ = 3001,
    AUDIT_DATA_WRITE = 3002,
    AUDIT_DATA_DELETE = 3003,
    
    // 配置变更事件 (4xxx)
    AUDIT_CONFIG_MODIFIED = 4001,
    AUDIT_RULE_UPDATED = 4002
} AuditEventType;

/**
 * @brief 审计记录
 */
typedef struct audit_record {
    uint64_t record_id;
    AuditEventType event_type;
    uint64_t timestamp_us;
    const char* user_id;
    const char* resource;
    const char* result;
    const char* ip_address;
    const char* session_id;
    KeyValuePair* metadata;
} audit_record_t;

/**
 * @brief 记录审计事件
 */
int audit_log(const audit_record_t* record);

/**
 * @brief 查询审计日志
 */
int audit_query(
    const AuditQuery* query,
    AuditRecordList* out_records
);
```

---

## 三、加密设计

### 3.1 密钥管理架构

```
┌─────────────────────────────────────────────────────────────┐
│                    密钥管理架构                              │
│  ┌─────────────────┐    ┌─────────────────┐                │
│  │   KEK (密钥加密密钥)  │ ←→  │   密钥存储服务   │                │
│  └────────┬──────────┘    └─────────────────┘                │
│           │                                                  │
│           ▼                                                  │
│  ┌─────────────────┐    ┌─────────────────┐                │
│  │   DEK (数据加密密钥)  │ ←→  │   envelope encryption │                │
│  └────────┬──────────┘    └─────────────────┘                │
│           │                                                  │
│           ▼                                                  │
│  ┌─────────────────┐                                        │
│  │      加密数据     │                                        │
│  └─────────────────┘                                        │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 加密算法标准

| 用途 | 算法 | 密钥长度 | 模式 |
|------|------|----------|------|
| 对称加密 | AES | 256 位 | GCM |
| 非对称加密 | RSA | 3072 位 | OAEP |
| 哈希 | SHA-256/SHA-384 | - | - |
| HMAC | SHA-256 | - | - |
| 密钥交换 | ECDH | 256 位 | - |
| 签名 | ECDSA | 256 位 | - |

```c
// atoms/security/crypto.h
/**
 * @brief 加密配置
 */
typedef struct crypto_config {
    /** 对称加密算法 */
    SymmetricAlgorithm symmetric_algo;
    
    /** 非对称加密算法 */
    AsymmetricAlgorithm asymmetric_algo;
    
    /** 哈希算法 */
    HashAlgorithm hash_algo;
    
    /** 密钥长度（位） */
    uint32_t key_length;
    
    /** 是否启用硬件加速 */
    bool hw_acceleration;
} crypto_config_t;

/**
 * @brief 对称加密
 */
int crypto_encrypt_symmetric(
    const uint8_t* plaintext,
    size_t plaintext_len,
    const uint8_t* key,
    size_t key_len,
    uint8_t** out_ciphertext,
    size_t* out_ciphertext_len
);

/**
 * @brief 对称解密
 */
int crypto_decrypt_symmetric(
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* key,
    size_t key_len,
    uint8_t** out_plaintext,
    size_t* out_plaintext_len
);
```

---

## 四、安全通信

### 4.1 传输层安全

```yaml
# security/tls_config.yaml
tls:
  # 最低 TLS 版本
  min_version: "1.3"
  
  # 密码套件
  cipher_suites:
    - "TLS_AES_256_GCM_SHA384"
    - "TLS_CHACHA20_POLY1305_SHA256"
    - "TLS_AES_128_GCM_SHA256"
    
  # 证书配置
  certificate:
    path: "/etc/agentos/tls/server.crt"
    private_key_path: "/etc/agentos/tls/server.key"
    ca_path: "/etc/agentos/tls/ca.crt"
    
  # 客户端认证
  client_auth:
    enabled: true
    cert_path: "/etc/agentos/tls/client.crt"
```

### 4.2 mTLS 配置

```c
// atoms/security/tls.h
/**
 * @brief mTLS 连接配置
 */
typedef struct mtls_config {
    /** 服务器证书 */
    const char* server_cert_path;
    
    /** 服务器私钥 */
    const char* server_key_path;
    
    /** CA 证书（用于验证客户端） */
    const char* ca_cert_path;
    
    /** 是否验证客户端证书 */
    bool verify_client;
    
    /** CRL 列表路径 */
    const char* crl_paths;
} mtls_config_t;

/**
 * @brief 创建 mTLS 连接
 */
TLSSession* mtls_connect(const mtls_config_t* config);
```

---

## 五、身份认证

### 5.1 认证架构

```yaml
# security/auth_config.yaml
authentication:
  # 默认认证方式
  default_method: "jwt"
  
  # JWT 配置
  jwt:
    issuer: "agentos.auth"
    audience: "agentos.api"
    access_token_ttl_seconds: 3600
    refresh_token_ttl_days: 30
    algorithm: "ES256"
    
  # 外部认证集成
  external_providers:
    - name: "oauth2"
      enabled: true
      providers:
        - "google"
        - "github"
        
    - name: "saml"
      enabled: false
```

### 5.2 会话管理

```c
// atoms/security/session.h
/**
 * @brief 会话状态
 */
typedef enum {
    SESSION_ACTIVE = 0,
    SESSION_EXPIRED = 1,
    SESSION_REVOKED = 2,
    SESSION_INVALID = 3
} SessionState;

/**
 * @brief 会话信息
 */
typedef struct session {
    uint64_t session_id;
    const char* user_id;
    SessionState state;
    uint64_t created_at_us;
    uint64_t expires_at_us;
    const char* ip_address;
    const char* user_agent;
} session_t;

/**
 * @brief 创建会话
 */
int session_create(
    const char* user_id,
    const SessionConfig* config,
    session_t** out_session
);

/**
 * @brief 验证会话
 */
int session_validate(
    const char* session_token,
    session_t** out_session
);

/**
 * @brief 撤销会话
 */
int session_revoke(uint64_t session_id);
```

---

## 六、隐私保护

### 6.1 数据脱敏

```yaml
# security/data_masking.yaml
data_masking:
  # 敏感字段配置
  sensitive_fields:
    - name: "password"
      mask_type: "always"  # always | partial | none
      mask_char: "*"
      visible_chars: 0
      
    - name: "email"
      mask_type: "partial"
      mask_char: "*"
      visible_start: 2
      visible_end: 2
      
    - name: "phone"
      mask_type: "partial"
      mask_char: "*"
      visible_start: 3
      visible_end: 4
      
    - name: "credit_card"
      mask_type: "partial"
      mask_char: "*"
      visible_start: 0
      visible_end: 4
```

### 6.2 数据最小化

```c
// atoms/security/privacy.h
/**
 * @brief 数据最小化配置
 */
typedef struct privacy_config {
    /** 是否记录个人数据 */
    bool log_personal_data;
    
    /** 是否记录查询参数 */
    bool log_query_params;
    
    /** 敏感字段列表 */
    const char** sensitive_fields;
    size_t num_sensitive_fields;
    
    /** 数据保留期限（天） */
    uint32_t retention_days;
} privacy_config_t;

/**
 * @brief 脱敏数据
 */
int privacy_mask(
    const char* field_name,
    const uint8_t* data,
    size_t data_len,
    uint8_t* out_masked,
    size_t* out_masked_len
);

/**
 * @brief 检查是否为敏感数据
 */
bool privacy_is_sensitive(const char* field_name);
```

---

## 七、安全监控

### 7.1 健康检查

```yaml
# security/health_check.yaml
health_check:
  # 安全组件健康检查
  security:
    check_interval_seconds: 60
    
    # 检查项
    checks:
      - name: "encryption_module"
        enabled: true
        critical: true
        
      - name: "permission_cache"
        enabled: true
        critical: false
        
      - name: "audit_queue"
        enabled: true
        critical: true
        
      - name: "tls_cert_expiry"
        enabled: true
        critical: true
        warning_days: 30
```

```c
// atoms/security/health.h
/**
 * @brief 健康检查结果
 */
typedef struct health_check_result {
    const char* check_name;
    bool healthy;
    bool critical;
    const char* message;
    uint64_t last_check_us;
} health_check_result_t;

/**
 * @brief 执行安全健康检查
 */
int security_health_check(
    health_check_result_t** out_results,
    size_t* out_num_results
);

/**
 * @brief 获取安全指标
 */
int security_get_metrics(SecurityMetrics* out_metrics);
```

### 7.2 威胁检测

```yaml
# security/threat_detection.yaml
threat_detection:
  # 启用威胁检测
  enabled: true
  
  # 速率限制
  rate_limiting:
    - name: "auth_attempts"
      window_seconds: 300
      max_attempts: 5
      action: "block"
      
    - name: "api_requests"
      window_seconds: 60
      max_attempts: 1000
      action: "throttle"
      
  # 异常检测
  anomaly_detection:
    # 失败登录检测
    failed_login:
      threshold: 5
      window_minutes: 15
      action: "alert"
      
    # 异常访问模式
    access_pattern:
      enabled: true
      baseline_hours: 72
      deviation_threshold: 2.5
```

---

## 八、漏洞防护

### 8.1 常见漏洞防护

| 漏洞类型 | 防护措施 | 实现层 |
|----------|----------|--------|
| SQL 注入 | 参数化查询 | D3 输入净化 |
| XSS | 输出编码 | D3 输入净化 |
| CSRF | Token 验证 | D2 权限裁决 |
| 命令注入 | 白名单验证 | D3 输入净化 |
| 路径遍历 | 路径规范化 | D3 输入净化 |
| 敏感信息泄露 | 加密存储 | 加密模块 |

### 8.2 内存安全

```c
// atoms/security/memory.h
/**
 * @brief 安全内存操作
 */

/**
 * @brief 安全内存复制
 */
void* secure_memcpy(void* dest, const void* src, size_t n);

/**
 * @brief 安全内存设置（防编译器优化）
 */
void secure_memset(void* ptr, uint8_t value, size_t n);

/**
 * @brief 安全内存释放
 */
void secure_free(void* ptr, size_t size);

/**
 * @brief 分配安全内存
 */
void* secure_malloc(size_t size);

/**
 * @brief 分配并初始化安全内存
 */
void* secure_calloc(size_t nmemb, size_t size);
```

---

## 九、合规性

### 9.1 数据保护合规

| 法规 | 要求 | AgentOS 实现 |
|------|------|--------------|
| GDPR | 数据主体权利 | D4 审计、D3 输入控制 |
| 最小化收集 | 只收集必要数据 | 数据最小化配置 |
| 数据保留 | 定期删除过期数据 | 保留策略配置 |
| 数据可携 | 支持数据导出 | 数据导出 API |

### 9.2 审计合规

```yaml
# compliance/audit_requirements.yaml
compliance:
  # 审计日志保留期
  retention:
    standard: 365  # 天
    financial: 2555  # 天 (7年)
    healthcare: 1825  # 天 (5年)
    
  # 审计日志格式
  format:
    type: "json"
    encoding: "utf-8"
    include_fields:
      - timestamp
      - user_id
      - action
      - resource
      - result
      - ip_address
      
  # 审计日志签名
  integrity:
    enabled: true
    algorithm: "HMAC-SHA256"
    key_rotation_days: 90
```

---

## 十、参考文献

1. **AgentOS 架构设计原则**: [architectural_design_principles.md](../../architecture/folder/architectural_design_principles.md)
2. **AgentOS 微内核设计**: [microkernel.md](../../architecture/folder/microkernel.md)
3. **AgentOS 统一术语表**: [TERMINOLOGY.md](../TERMINOLOGY.md)
4. **OWASP Top 10**: https://owasp.org/www-project-top-ten/
5. **NIST Cybersecurity Framework**: https://www.nist.gov/cyberframework
6. **ISO 27001**: https://www.iso.org/isoiec-27001-information-security.html

---

© 2026 SPHARX Ltd. All Rights Reserved.  
"From data intelligence emerges."