# Security — 安全防护引擎

`cupolas/src/security/` 提供核心安全防护能力，包括文件扫描、API 防护、行为分析、网络防护和 iOS 级安全模块。

> Part of AgentOS v0.0.5

## 核心能力

| 能力 | 说明 |
|------|------|
| **文件扫描** | 对上传文件和系统文件进行安全扫描 |
| **API 防护** | 检测和阻止 API 层面的攻击行为 |
| **行为分析** | 基于规则的运行时行为异常检测 |
| **环境检测** | 检测运行环境的安全状态 |
| **报告生成** | 生成安全事件报告 |

## 引擎架构

```
输入 → 预处理 → 规则匹配 → 风险评估 → 处置决策 → 输出
       ↓          ↓          ↓           ↓
   规范化    规则引擎    评分模型     放行/阻断/告警
```

## 模块组成

### 基础模块（始终编译）

| 模块 | 源文件 | 职责 |
|------|--------|------|
| **错误处理** | `cupolas_error.c` | 统一错误码与错误信息管理 |

### 网络安全子模块（`network/`）

| 模块 | 源文件 | 职责 |
|------|--------|------|
| **HTTP 安全** | `network/http_security.c` | HTTPS 强制、HSTS、URL 校验、请求方法限制 |
| **DNS 安全** | `network/dns_security.c` | DNSSEC 验证、域名黑白名单、DoH 支持 |
| **网络过滤** | `network/network_filter.c` | 网络访问控制规则、连接管理、流量统计 |
| **网络工具** | `network/network_utils.c` | 网络相关辅助函数 |

### OpenSSL 条件模块（`AGENTOS_HAS_OPENSSL`）

当定义 `AGENTOS_HAS_OPENSSL` 时，以下 iOS 级安全模块会被启用：

| 模块 | 源文件 | 职责 |
|------|--------|------|
| **数字签名** | `cupolas_signature.c` | 代码签名验证（RSA/ECDSA/Ed25519）、证书链校验、完整性检查 |
| **密钥保险库** | `cupolas_vault.c` | 安全凭证存储（AES-256-GCM）、ACL 访问控制、类似 iOS Keychain |
| **权利管理** | `cupolas_entitlements.c` | 声明式权限管理（文件系统/网络/IPC/资源限制/系统调用） |
| **运行时保护** | `cupolas_runtime_protection.c` | seccomp 系统调用过滤、CFI 控制流完整性、内存保护、完整性校验 |
| **网络安全** | `cupolas_network_security.c` | TLS 连接管理、防火墙规则、证书验证 |
| **TLS 安全** | `network/tls_security.c` | TLS/SSL 连接管理、证书链验证、密码套件检查 |

## 扫描规则配置

```json
{
    "file_scan": {
        "max_size": "100MB",
        "allowed_types": ["pdf", "docx", "txt"],
        "scan_engines": ["clamav", "yara"],
        "action_on_threat": "quarantine"
    },
    "api_protection": {
        "rate_limit": 1000,
        "detect_sql_injection": true,
        "detect_xss": true,
        "detect_path_traversal": true
    }
}
```

## 使用示例

### 基础安全检查

```c
#include "cupolas/cupolas_error.h"

// 初始化安全引擎
security_engine_t* engine = security_engine_create();

// 文件扫描
security_scan_result_t result = security_scan_file(engine, "/path/to/file.pdf");
if (result.threat_detected) {
    printf("威胁检测: %s (严重度: %d)", result.threat_name, result.severity);
    security_quarantine_file(engine, "/path/to/file.pdf");
}

// API 请求检查
bool is_safe = security_check_request(engine, request);
if (!is_safe) {
    security_block_request(engine, request);
}
```

### 数字签名验证（需 OpenSSL）

```c
#include "cupolas/cupolas_signature.h"

// 初始化签名验证模块
cupolas_sig_config_t sig_config = {
    .check_cert_chain = true,
    .check_revocation = true,
    .allow_self_signed = false
};
cupolas_signature_init(&sig_config);

// 验证文件签名
cupolas_sig_result_t result;
int ret = cupolas_signature_verify_file("/path/to/agent.wasm", "SPHARX CA", &result);
if (ret == 0 && result == CUPOLAS_SIG_OK) {
    printf("签名验证通过");
}
```

### 密钥保险库（需 OpenSSL）

```c
#include "cupolas/cupolas_vault.h"

// 初始化 Vault
cupolas_vault_config_t vault_config = {
    .storage_path = "/var/lib/agentos/vault",
    .enable_audit = true,
    .enable_auto_lock = true,
    .auto_lock_seconds = 300
};
cupolas_vault_init(&vault_config);

// 存储凭证
cupolas_vault_t* vault;
cupolas_vault_open("default", NULL, &vault);
cupolas_vault_store(vault, "api-key-001", CUPOLAS_VAULT_CRED_API_KEY,
                    (const uint8_t*)"secret-key-data", 15, NULL);
```

### 运行时保护（需 OpenSSL）

```c
#include "cupolas/cupolas_runtime_protection.h"

// 启用运行时保护
cupolas_runtime_protect_config_t config = {
    .level = CUPOLAS_PROTECT_ENHANCED,
    .seccomp = { .enable_seccomp = true, .default_action = 0 },
    .cfi = { .enable_cfi = true, .cfi_level = 2 },
    .integrity = { .enable_code_integrity = true, .check_interval_ms = 5000 }
};
cupolas_runtime_protect_enable(&config);
```

## 相关子系统

| 子系统 | 关系 |
|--------|------|
| [Sanitizer](../sanitizer/README.md) | 安全引擎调用清洗器进行输入预处理 |
| [Permission](../permission/README.md) | Entitlements 提供声明式权限，与 RBAC/ABAC 互补 |
| [Audit](../audit/README.md) | 安全事件（签名验证失败、运行时违规等）写入审计日志 |
| [Guards](#) | 安全守卫框架提供可扩展的检测能力 |
| [Utils](../utils/README.md) | 加密工具为 OpenSSL 模块提供底层支持 |

---

*AgentOS Cupolas — 安全防护*
