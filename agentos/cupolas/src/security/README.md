# Security Module (安全模块)

## 概述

安全模块是 Cupolas 安全穹顶的核心防御层，提供多层次的安全保护机制，确保 AgentOS 系统的完整性和安全性。

## 子模块架构

```
security/
├── cupolas_signature.c/h      # 代码签名验证（RSA/ECDSA/Ed25519）
├── cupolas_vault.c/h          # 安全凭证库（加密存储）
├── cupolas_network_security.c/h  # 网络安全（遗留接口，向后兼容）
├── cupolas_runtime_protection.c/h # 运行时保护（seccomp/CFI）
├── cupolas_entitlements.c/h   # 权限声明系统（YAML-based）
├── cupolas_error.c/h          # 统一错误码定义
└── network/                   # 网络安全子模块（重构后）
    ├── tls_security.c/h       # TLS/SSL 安全配置与验证
    ├── http_security.c/h      # HTTP 安全头与请求校验
    ├── dns_security.c/h       # DNS 安全过滤与解析
    ├── network_filter.c/h     # 网络访问控制规则引擎
    └── network_utils.c/h      # 网络工具函数（URL/CIDR/IP）
```

## 核心功能

### 1. 代码签名验证 (cupolas_signature)
- **功能**: 验证 Agent 代码的数字签名，防止篡改
- **算法支持**: RSA-SHA256, ECDSA-P256, Ed25519
- **证书链验证**: 完整的 X.509 证书链校验
- **使用场景**: Agent 加载前完整性检查

### 2. 安全凭证库 (cupolas_vault)
- **功能**: 加密存储敏感凭证（API密钥、密码、令牌）
- **特性**:
  - AES-256-GCM 加密
  - 自动锁定超时机制
  - 凭证一次性读取后自动删除
  - 支持凭证导出/导入（加密格式）

### 3. 运行时保护 (cupolas_runtime_protection)
- **功能**: 运行时行为监控和防护
- **防御机制**:
  - **Seccomp BPF**: 系统调用过滤（Linux）
  - **CFI (Control Flow Integrity)**: 控制流完整性保护
  - **ASLR**: 地址空间布局随机化
  - **DEP**: 数据执行保护
  - **代码完整性校验**: 定期哈希验证
- **违规记录**: 维护违规事件历史（最多128条）

### 4. 权限声明系统 (cupolas_entitlements)
- **功能**: 基于 YAML 的细粒度权限声明和验证
- **特性**:
  - 声明式权限定义（agent_id, version, capabilities）
  - 路径匹配规则（通配符支持）
  - 主机名/域名访问控制
  - OpenSSL EVP 数字签名验证

### 5. 网络安全子系统 (network/)
详见各子模块 README。

## 关键 API

```c
// 初始化与清理
int cupolas_signature_init(void);
void cupolas_signature_cleanup(void);

int cupolas_vault_init(const char* master_key);
void cupolas_vault_cleanup(void);

int cupolas_runtime_protect_init(const cupolas_runtime_protect_config_t* config);
void cupolas_runtime_protect_cleanup(void);

// 核心操作
int cupolas_verify_signature(const char* file_path, const char* signer_id);
int cupolas_vault_store(const char* id, const void* data, size_t len);
int cupolas_vault_retrieve(const char* id, void* buffer, size_t* len);
int cupolas_seccomp_check(const char* syscall_name);
int cupolas_entitlements_verify(cupolas_entitlements_t* ent);
```

## 线程安全性

所有公开 API 均为线程安全，内部使用 `CUPOLAS_MUTEX_*` 宏进行同步：
- ✅ 多线程并发调用安全
- ✅ 使用统一互斥锁抽象（跨平台兼容 Windows/Linux）
- ⚠️ 回调函数（如 violation_callback）需用户自行保证线程安全

## 内存管理规范

遵循 Cupolas 统一内存管理宏：
- 分配: `CUPOLAS_ALLOC(type, count)`, `CUPOLAS_ALLOC_STRUCT(type)`
- 释放: `CUPOLAS_FREE(ptr)` (自动置 NULL)
- 检查: `CUPOLAS_CHECK_NULL(ptr)`, `CUPOLAS_CHECK_RESULT(expr)`

## 错误处理

所有函数返回标准错误码：
- `0`: 成功
- `-1`: 一般错误（参数无效、未初始化等）
- 特定模块错误码见对应头文件

## 性能指标

| 功能 | 平均延迟 | 吞吐量 | 内存占用 |
|------|---------|--------|----------|
| 签名验证 | <5ms | ~200 ops/s | ~50KB |
| 凭证存储 | <1ms | ~1000 ops/s | ~20KB |
| Seccomp检查 | <0.01ms | ~100K ops/s | ~10KB |
| Entitlements验证 | <2ms | ~500 ops/s | ~30KB |

## 依赖关系

- **OpenSSL**: 签名验证、加密操作 (>=1.1.1)
- **平台库**: pthread (Linux) / CRITICAL_SECTION (Windows)
- **Cupolas Utils**: 统一工具宏 (`../utils/cupolas_utils.h`)

## 测试覆盖

- 单元测试: `test_cupolas_signature.c` (12 cases)
- 单元测试: `test_cupolas_vault.c` (12 cases)
- 目标覆盖率: >90%

## 开发注意事项

1. **新增安全功能** 必须通过安全审计
2. **密钥管理** 严禁硬编码，必须使用 Vault 存储
3. **错误信息** 不得泄露敏感数据（堆栈、内存地址等）
4. **日志记录** 使用 CUPOLAS_LOG_* 宏，避免日志注入

## 相关文档

- [主 README](../../README.md) - Cupolas 整体架构
- [Utils 模块](../utils/README.md) - 统一工具库
- [Architecture Principles](../../../manuals/ARCHITECTURAL_PRINCIPLES.md)

---

**版本**: 1.0.0  
**最后更新**: 2026-04-06  
**维护者**: Spharx Security Team
