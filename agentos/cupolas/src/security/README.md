# Security — 安全防护引擎

**模块路径**: `agentos/cupolas/src/security/`
**版本**: v0.1.0

## 概述

Security 模块是 Cupolas 安全穹顶的核心引擎，提供统一错误处理、数字签名验证、密钥保险库、权利管理、运行时保护和网络安全等纵深防御能力。其中签名、保险库、权利和运行时保护模块需要 OpenSSL 支持（通过 `AGENTOS_HAS_OPENSSL` 条件编译启用）。

## 设计目标

- **纵深防御**：多层安全防护，从文件扫描到运行时保护
- **条件编译**：OpenSSL 相关模块可按需启用，基础功能不依赖 OpenSSL
- **统一错误处理**：所有安全模块共享错误码体系和转换函数
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

提供统一错误码和模块特定错误码的双层体系，以及相互转换函数。

**统一错误码**：

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `cupolas_ERR_OK` | 0 | 成功 |
| `cupolas_ERR_UNKNOWN` | -1 | 未知错误 |
| `cupolas_ERR_INVALID_PARAM` | -2 | 无效参数 |
| `cupolas_ERR_NULL_POINTER` | -3 | 空指针 |
| `cupolas_ERR_OUT_OF_MEMORY` | -4 | 内存不足 |
| `cupolas_ERR_BUFFER_TOO_SMALL` | -5 | 缓冲区不足 |
| `cupolas_ERR_NOT_FOUND` | -6 | 资源未找到 |
| `cupolas_ERR_ALREADY_EXISTS` | -7 | 资源已存在 |
| `cupolas_ERR_TIMEOUT` | -8 | 超时 |
| `cupolas_ERR_NOT_SUPPORTED` | -9 | 不支持 |
| `cupolas_ERR_PERMISSION_DENIED` | -10 | 权限不足 |
| `cupolas_ERR_IO` | -11 | I/O 错误 |
| `cupolas_ERR_STATE_ERROR` | -13 | 状态错误 |
| `cupolas_ERR_OVERFLOW` | -14 | 溢出 |
| `cupolas_ERR_AUTH_FAILED` | -16 | 认证失败 |
| `cupolas_ERR_CERT_INVALID` | -17 | 证书无效 |
| `cupolas_ERR_CERT_EXPIRED` | -18 | 证书过期 |
| `cupolas_ERR_SIGNATURE_INVALID` | -19 | 签名无效 |
| `cupolas_ERR_TAMPERED` | -20 | 数据被篡改 |

**模块特定错误码**：`cupolas_sig_error_t`（签名）、`cupolas_ent_error_t`（权利）、`cupolas_vault_error_t`（保险库）、`cupolas_net_error_t`（网络）、`cupolas_runtime_error_t`（运行时保护）。

**转换函数**：

| 函数 | 说明 |
|------|------|
| `cupolas_error_string(error)` | 统一错误码转字符串 |
| `cupolas_error_from_sig(sig_error)` | 签名错误 → 统一错误 |
| `cupolas_error_from_ent(ent_error)` | 权利错误 → 统一错误 |
| `cupolas_error_from_vault(vault_error)` | 保险库错误 → 统一错误 |
| `cupolas_error_from_net(net_error)` | 网络错误 → 统一错误 |
| `cupolas_error_from_runtime(runtime_error)` | 运行时错误 → 统一错误 |

**工具宏**：

| 宏 | 说明 |
|------|------|
| `cupolas_ERROR_IS_SUCCESS(e)` | 检查是否成功 |
| `cupolas_ERROR_IS_FATAL(e)` | 检查是否致命错误 |
| `cupolas_ERROR_IS_PARAM(e)` | 检查是否参数错误 |

### 数字签名（cupolas_signature）— 需 OpenSSL

支持 RSA-SHA256/384/512、ECDSA-P256/P384、Ed25519 算族。

| 函数 | 说明 |
|------|------|
| `cupolas_signature_init(config)` | 初始化签名模块 |
| `cupolas_signature_cleanup()` | 清理签名模块 |
| `cupolas_signature_verify_file(file_path, expected_signer, result)` | 验证文件签名 |
| `cupolas_signature_verify_data(data, data_len, signature, sig_len, algo, public_key)` | 验证数据签名 |
| `cupolas_signature_verify_integrity(file_path, expected_hash)` | 验证文件完整性 |
| `cupolas_signature_compute_hash(file_path, hash_out)` | 计算文件哈希 |
| `cupolas_signature_get_signer_info(file_path, info)` | 获取签名者信息 |
| `cupolas_signature_sign_file(file_path, private_key, algo, signature_out, sig_len)` | 签名文件 |
| `cupolas_signature_sign_data(data, data_len, private_key, algo, signature_out, sig_len)` | 签名数据 |
| `cupolas_signature_is_trusted_signer(signer_cn)` | 检查签名者是否受信 |
| `cupolas_signature_add_trusted_signer(signer_cn, public_key)` | 添加受信签名者 |

### 密钥保险库（cupolas_vault）— 需 OpenSSL

类似 iOS Keychain 的安全凭证存储，支持 AES-256-GCM 加密、ACL 访问控制和凭证轮换。

| 函数 | 说明 |
|------|------|
| `cupolas_vault_init(config)` | 初始化保险库模块 |
| `cupolas_vault_cleanup()` | 清理保险库模块 |
| `cupolas_vault_open(vault_id, password, vault)` | 打开保险库 |
| `cupolas_vault_close(vault)` | 关闭保险库 |
| `cupolas_vault_lock(vault)` | 锁定保险库 |
| `cupolas_vault_unlock(vault, password)` | 解锁保险库 |
| `cupolas_vault_is_locked(vault)` | 检查是否锁定 |
| `cupolas_vault_store(vault, cred_id, type, data, data_len, acl)` | 存储凭证 |
| `cupolas_vault_retrieve(vault, cred_id, agent_id, data_out, data_len)` | 检索凭证 |
| `cupolas_vault_delete(vault, cred_id, agent_id)` | 删除凭证 |
| `cupolas_vault_update(vault, cred_id, data, data_len, agent_id)` | 更新凭证 |
| `cupolas_vault_exists(vault, cred_id)` | 检查凭证是否存在 |
| `cupolas_vault_list(vault, type, metadata_array, count)` | 列出凭证 |
| `cupolas_vault_check_access(vault, cred_id, agent_id, operation)` | 检查访问权限 |
| `cupolas_vault_grant_access(vault, cred_id, agent_id, operations, expires_at)` | 授权访问 |
| `cupolas_vault_revoke_access(vault, cred_id, agent_id)` | 撤销访问 |
| `cupolas_vault_export(vault, export_path, password, agent_id)` | 导出保险库 |
| `cupolas_vault_import(vault, import_path, password, agent_id)` | 导入保险库 |
| `cupolas_vault_rotate_credential(vault, cred_group, strategy, selected_id, id_buf_size)` | 凭证轮换 |

**凭证轮换策略**：

| 策略 | 说明 |
|------|------|
| `CUPOLAS_VAULT_ROTATE_ROUND_ROBIN` | 轮询轮换 |
| `CUPOLAS_VAULT_ROTATE_LEAST_USED` | 最少使用优先 |
| `CUPOLAS_VAULT_ROTATE_RATE_LIMITED` | 速率限制轮换 |
| `CUPOLAS_VAULT_ROTATE_PRIORITY` | 优先级轮换 |

### 权利管理（cupolas_entitlements）— 需 OpenSSL

声明式权限管理，类似 iOS Entitlements，支持文件系统/网络/IPC/Vault/资源限制/Syscall/Capability 七类权限声明。

| 函数 | 说明 |
|------|------|
| `cupolas_entitlements_init()` | 初始化权利模块 |
| `cupolas_entitlements_cleanup()` | 清理权利模块 |
| `cupolas_entitlements_load(yaml_path, entitlements)` | 从 YAML 加载 |
| `cupolas_entitlements_load_json(json_path, entitlements)` | 从 JSON 加载 |
| `cupolas_entitlements_load_string(yaml_content, entitlements)` | 从字符串加载 |
| `cupolas_entitlements_free(entitlements)` | 释放权利上下文 |
| `cupolas_entitlements_verify(entitlements, public_key)` | 验证权利签名 |
| `cupolas_entitlements_sign(entitlements, private_key, signature_out, sig_len)` | 签名权利 |
| `cupolas_entitlements_check_fs(entitlements, path, operation)` | 检查文件系统权限 |
| `cupolas_entitlements_check_net(entitlements, host, port, protocol, direction)` | 检查网络权限 |
| `cupolas_entitlements_check_ipc(entitlements, target, operation)` | 检查 IPC 权限 |
| `cupolas_entitlements_check_syscall(entitlements, syscall_name)` | 检查 Syscall 权限 |
| `cupolas_entitlements_check_capability(entitlements, capability)` | 检查 Capability 权限 |
| `cupolas_entitlements_check_vault(entitlements, cred_id, operation)` | 检查 Vault 访问权限 |
| `cupolas_entitlements_get_resource_limits(entitlements, limits)` | 获取资源限制 |
| `cupolas_entitlements_check_resource(entitlements, resource_type, current_value)` | 检查资源是否超限 |
| `cupolas_entitlements_export_yaml(entitlements, yaml_out, len)` | 导出为 YAML |
| `cupolas_entitlements_export_json(entitlements, json_out, len)` | 导出为 JSON |

### 运行时保护（cupolas_runtime_protection）— 需 OpenSSL

多层运行时安全防护，包括 seccomp 系统调用过滤、CFI 控制流完整性、内存保护和完整性校验。

| 函数 | 说明 |
|------|------|
| `cupolas_runtime_protect_init(config)` | 初始化运行时保护 |
| `cupolas_runtime_protect_enable(config)` | 启用运行时保护 |
| `cupolas_runtime_protect_disable()` | 禁用运行时保护 |
| `cupolas_runtime_protect_get_status()` | 获取保护状态 |
| `cupolas_memory_protect_enable(config)` | 启用内存保护 |
| `cupolas_memory_lock(addr, len)` | 锁定内存页 |
| `cupolas_memory_unlock(addr, len)` | 解锁内存页 |
| `cupolas_memory_protect(addr, len, prot)` | 设置内存页保护 |
| `cupolas_cfi_enable(config)` | 启用 CFI |
| `cupolas_cfi_register_target(source, target)` | 注册合法跳转目标 |
| `cupolas_cfi_verify_transfer(source, target)` | 验证控制流转移 |
| `cupolas_seccomp_enable(config)` | 启用 seccomp |
| `cupolas_seccomp_allow(syscall_name)` | 允许系统调用 |
| `cupolas_seccomp_deny(syscall_name)` | 拒绝系统调用 |
| `cupolas_seccomp_check(syscall_name)` | 检查系统调用是否允许 |
| `cupolas_integrity_enable(config)` | 启用完整性校验 |
| `cupolas_integrity_check()` | 执行完整性检查 |

**保护级别**：`PROTECT_NONE` → `PROTECT_BASIC` → `PROTECT_ENHANCED` → `PROTECT_MAXIMUM`

**违规类型**：`VIOLATION_SYSCALL`、`VIOLATION_MEMORY`、`VIOLATION_CONTROL_FLOW`、`VIOLATION_INTEGRITY`、`VIOLATION_RESOURCE`

### 网络安全（cupolas_network_security + network/）

| 子模块 | 说明 |
|--------|------|
| **TLS 管理** | TLS 1.2/1.3 连接管理、证书验证、密码套件控制 |
| **防火墙** | IP/端口/协议黑白名单、速率限制、规则优先级 |
| **HTTP Security** | 请求头注入防护、CORS 策略、HSTS、URL 长度限制 |
| **DNS Security** | DNS 缓存投毒防护、DNSSEC、域名黑名单 |
| **Network Filter** | 连接方向过滤、协议过滤、CIDR 匹配 |
| **Network Utils** | IP 地址解析、端口扫描检测 |
| **TLS Security** | TLS/SSL 连接管理与证书验证（需 OpenSSL） |

**网络安全核心 API**：

| 函数 | 说明 |
|------|------|
| `cupolas_network_security_init(config)` | 初始化网络安全模块 |
| `cupolas_network_security_cleanup()` | 清理网络安全模块 |
| `cupolas_tls_context_create(config)` | 创建 TLS 上下文 |
| `cupolas_tls_client_connect(ctx, host, port, error)` | TLS 客户端连接 |
| `cupolas_tls_server_accept(ctx, socket_fd, error)` | TLS 服务端接受 |
| `cupolas_tls_close(conn)` | 关闭 TLS 连接 |
| `cupolas_tls_read(conn, buf, len)` | TLS 读取 |
| `cupolas_tls_write(conn, buf, len)` | TLS 写入 |
| `cupolas_tls_verify_peer(conn, hostname)` | 验证对端证书 |
| `cupolas_firewall_enable(config)` | 启用防火墙 |
| `cupolas_firewall_add_rule(rule)` | 添加防火墙规则 |
| `cupolas_firewall_check(protocol, direction, src_ip, src_port, dst_ip, dst_port)` | 检查连接是否允许 |
| `cupolas_cert_validate(cert_pem, ca_file, error)` | 验证证书 |
| `cupolas_cert_verify_hostname(cert_pem, hostname)` | 验证证书主机名 |

## 使用示例

```c
#include "cupolas_error.h"

const char *msg = cupolas_error_string(cupolas_ERR_PERMISSION_DENIED);
printf("Error: %s\n", msg);
```

```c
#ifdef AGENTOS_HAS_OPENSSL
#include "cupolas_vault.h"

cupolas_vault_config_t config = {
    .storage_path = "/secure/vault.dat",
    .master_key_path = "/secure/master.key",
    .enable_audit = true,
    .enable_auto_lock = true,
    .auto_lock_seconds = 300
};
cupolas_vault_init(&config);

cupolas_vault_t *vault = NULL;
cupolas_vault_open("default", "master-password", &vault);

/* 存储凭证 */
const uint8_t secret[] = {0x01, 0x02, 0x03};
cupolas_vault_store(vault, "api-key", CUPOLAS_VAULT_CRED_GENERIC,
                    secret, sizeof(secret), NULL);

/* 检索凭证 */
uint8_t retrieved[256];
size_t retrieved_len = sizeof(retrieved);
cupolas_vault_retrieve(vault, "api-key", "agent-001", retrieved, &retrieved_len);

cupolas_vault_lock(vault);
cupolas_vault_close(vault);
cupolas_vault_cleanup();
#endif
```

## 条件编译

| 宏 | 说明 |
|------|------|
| `AGENTOS_HAS_OPENSSL` | 启用签名、保险库、权利、运行时保护、TLS 模块 |
| `AGENTOS_HAS_LIBYAML` | 启用完整 YAML 支持（否则使用内置 `yaml_minimal`） |

未定义 `AGENTOS_HAS_OPENSSL` 时，相关函数返回 `cupolas_ERR_NOT_SUPPORTED`。

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `platform.h` | 平台抽象层 |
| `cupolas_utils.h` | 安全内存管理、日志宏 |
| `OpenSSL` | 签名、保险库、TLS 等（条件编译） |

## 相关子系统

| 子系统 | 关系 |
|--------|------|
| [Sanitizer](../sanitizer/README.md) | 安全引擎调用清洗器进行输入校验 |
| [Permission](../permission/README.md) | Entitlements 提供声明式权限 |
| [Audit](../audit/README.md) | 安全事件记录审计日志 |
| [Workbench](../workbench/README.md) | 运行时保护限制工作台进程行为 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
