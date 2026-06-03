# Security — 安全防护引擎

**模块路径**: `agentos/cupolas/src/security/`
**版本**: v0.1.0

## 概述

Security 模块是 Cupolas 安全穹顶的核心引擎，提供文件安全扫描、API 防护、行为分析、网络安全等纵深防御能力。包含统一错误处理、数字签名验证、密钥保险库、权利管理、运行时保护和网络安全等子模块。其中签名、保险库、权利和运行时保护模块需要 OpenSSL 支持（通过 `AGENTOS_HAS_OPENSSL` 条件编译启用）。

## 设计目标

- **纵深防御**：多层安全防护，从文件扫描到运行时保护
- **条件编译**：OpenSSL 相关模块可按需启用，基础功能不依赖 OpenSSL
- **统一错误处理**：所有安全模块共享错误码体系和报告机制
- **iOS 级安全**：Entitlements、Vault、Runtime Protection 参照 iOS 安全架构设计

## 目录结构

```
security/
├── cupolas_error.h              # 统一错误处理接口
├── cupolas_error.c              # 错误处理实现
├── cupolas_signature.h          # 数字签名接口（需 OpenSSL）
├── cupolas_signature.c          # 签名实现
├── cupolas_vault.h              # 密钥保险库接口（需 OpenSSL）
├── cupolas_vault.c              # 保险库实现
├── cupolas_entitlements.h       # 权利管理接口（需 OpenSSL）
├── cupolas_entitlements.c       # 权利管理实现
├── cupolas_runtime_protection.h # 运行时保护接口（需 OpenSSL）
├── cupolas_runtime_protection.c # 运行时保护实现
├── cupolas_network_security.h   # 网络安全接口（需 OpenSSL）
├── cupolas_network_security.c   # 网络安全实现
├── network/                     # 网络安全子模块
│   ├── http_security.h          # HTTP 安全接口
│   ├── http_security.c          # HTTP 安全实现
│   ├── dns_security.h           # DNS 安全接口
│   ├── dns_security.c           # DNS 安全实现
│   ├── network_filter.h         # 网络过滤接口
│   ├── network_filter.c         # 过滤实现
│   ├── network_utils.h          # 网络工具接口
│   ├── network_utils.c          # 工具实现
│   ├── tls_security.h           # TLS 安全接口（需 OpenSSL）
│   └── tls_security.c           # TLS 实现
└── README.md                    # 本文档
```

## 子模块说明

### 统一错误处理（cupolas_error）

| 错误码 | 说明 |
|--------|------|
| `CUPOLAS_OK` | 成功 |
| `CUPOLAS_ERR_INVALID_PARAM` | 无效参数 |
| `CUPOLAS_ERR_PERMISSION_DENIED` | 权限不足 |
| `CUPOLAS_ERR_NOT_FOUND` | 资源未找到 |
| `CUPOLAS_ERR_ALREADY_EXISTS` | 资源已存在 |
| `CUPOLAS_ERR_BUFFER_TOO_SMALL` | 缓冲区不足 |
| `CUPOLAS_ERR_CRYPTO_FAILED` | 加密操作失败 |
| `CUPOLAS_ERR_SIGNATURE_INVALID` | 签名无效 |
| `CUPOLAS_ERR_CERT_INVALID` | 证书无效 |
| `CUPOLAS_ERR_VAULT_LOCKED` | 保险库已锁定 |
| `CUPOLAS_ERR_RATE_LIMITED` | 请求频率超限 |
| `CUPOLAS_ERR_INTERNAL` | 内部错误 |

| 函数 | 说明 |
|------|------|
| `cupolas_error_create(code, message)` | 创建错误对象 |
| `cupolas_error_destroy(error)` | 销毁错误对象 |
| `cupolas_error_get_code(error)` | 获取错误码 |
| `cupolas_error_get_message(error)` | 获取错误消息 |

### 数字签名（cupolas_signature）— 需 OpenSSL

| 函数 | 说明 |
|------|------|
| `cupolas_sign_data(data, len, key, sig, sig_len)` | 数据签名 |
| `cupolas_verify_signature(data, len, sig, sig_len, key)` | 签名验证 |
| `cupolas_verify_certificate_chain(cert, chain)` | 证书链校验 |

支持算法：RSA-2048/4096、ECDSA-P256/P384、Ed25519。

### 密钥保险库（cupolas_vault）— 需 OpenSSL

类似 iOS Keychain 的安全凭证存储：

| 函数 | 说明 |
|------|------|
| `cupolas_vault_create(path, master_key)` | 创建保险库 |
| `cupolas_vault_destroy(vault)` | 销毁保险库 |
| `cupolas_vault_store(vault, key, data, len)` | 存储凭证（AES-256-GCM 加密） |
| `cupolas_vault_retrieve(vault, key, data, len)` | 检索凭证 |
| `cupolas_vault_delete(vault, key)` | 删除凭证 |
| `cupolas_vault_list(vault, keys, max)` | 列出所有键 |
| `cupolas_vault_lock(vault)` | 锁定保险库 |
| `cupolas_vault_unlock(vault, master_key)` | 解锁保险库 |

### 权利管理（cupolas_entitlements）— 需 OpenSSL

声明式权限管理，类似 iOS Entitlements：

| 权利类型 | 说明 |
|----------|------|
| 文件系统 | 读/写/执行权限 |
| 网络 | 入站/出站连接权限 |
| IPC | 进程间通信权限 |
| 资源限制 | CPU/内存/时间限制 |
| 设备访问 | 硬件设备访问权限 |

### 运行时保护（cupolas_runtime_protection）— 需 OpenSSL

| 保护类型 | 说明 |
|----------|------|
| seccomp | 系统调用过滤 |
| CFI | 控制流完整性 |
| 内存保护 | W^X、ASLR、栈保护 |
| 完整性校验 | 代码段哈希校验 |
| 反调试 | 检测调试器附加 |

### 网络安全（network/）

| 子模块 | 说明 |
|--------|------|
| **HTTP Security** | 请求头注入防护、CORS 策略、CSRF Token |
| **DNS Security** | DNS 缓存投毒防护、域名黑名单 |
| **Network Filter** | IP/端口黑白名单、协议过滤 |
| **Network Utils** | IP 地址解析、CIDR 匹配、端口扫描检测 |
| **TLS Security** | TLS 1.2/1.3 连接管理、证书验证（需 OpenSSL） |

## 使用示例

```c
#include "cupolas_error.h"

cupolas_error_t *error = cupolas_error_create(CUPOLAS_ERR_PERMISSION_DENIED,
                                              "Agent not authorized for resource");
printf("Error %d: %s\n", cupolas_error_get_code(error),
       cupolas_error_get_message(error));
cupolas_error_destroy(error);
```

```c
#ifdef AGENTOS_HAS_OPENSSL
#include "cupolas_vault.h"

cupolas_vault_t *vault = cupolas_vault_create("/secure/vault.dat", "master-key-256");
cupolas_vault_store(vault, "api-key", secret_data, secret_len);

char retrieved[256];
size_t retrieved_len = sizeof(retrieved);
cupolas_vault_retrieve(vault, "api-key", retrieved, &retrieved_len);

cupolas_vault_lock(vault);
cupolas_vault_destroy(vault);
#endif
```

## 条件编译

| 宏 | 说明 |
|------|------|
| `AGENTOS_HAS_OPENSSL` | 启用签名、保险库、权利、运行时保护、TLS 模块 |
| `AGENTOS_HAS_LIBYAML` | 启用完整 YAML 支持（否则使用内置 `yaml_minimal`） |

未定义 `AGENTOS_HAS_OPENSSL` 时，相关函数返回 `CUPOLAS_ERR_NOT_SUPPORTED`。

## 相关子系统

| 子系统 | 关系 |
|--------|------|
| [Sanitizer](../sanitizer/README.md) | 安全引擎调用清洗器进行输入校验 |
| [Permission](../permission/README.md) | Entitlements 提供声明式权限 |
| [Audit](../audit/README.md) | 安全事件记录审计日志 |
| [Workbench](../workbench/README.md) | 运行时保护限制工作台进程行为 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
