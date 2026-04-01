# cupolas – AgentOS 安全穹顶

**版本**: 1.0.0.7  
**最后更新**: 2026-04-01  
**许可证**: Apache License 2.0  
**模块定位**: AgentOS 核心安全隔离与权限控制模块  

---

## 📖 快速索引

- [模块简介](#-模块简介)
- [核心功能](#-核心功能)
- [快速开始](#-快速开始)
- [架构设计](#-架构设计)
- [使用指南](#-使用指南)
- [配置说明](#-配置说明)
- [API 参考](#-api-参考)
- [开发指南](#-开发指南)
- [常见问题](#-常见问题)

---

## 🎯 模块简介

**cupolas**（穹顶）是 AgentOS 的安全核心模块，为所有 Agent 提供生产级的安全隔离、权限控制和审计追踪能力。

### 核心价值

```
┌─────────────────────────────────────────────────┐
│           AgentOS 应用层                          │
├─────────────────────────────────────────────────┤
│            cupolas 安全穹顶                       │  ← 本模块
│  ┌─────────┬─────────┬─────────┬─────────────┐  │
│  │ 权限控制 │ 输入净化 │ 审计日志 │ 安全隔离    │  │
│  └─────────┴─────────┴─────────┴─────────────┘  │
├─────────────────────────────────────────────────┤
│          AgentOS 内核层 (atoms)                   │
└─────────────────────────────────────────────────┘
```

### 设计原则

遵循 [AgentOS 架构设计原则 Doc V1.7](../../manuals/ARCHITECTURAL_PRINCIPLES.md)：

| 原则 | 说明 | 实现方式 |
|------|------|----------|
| **安全内生 (E-1)** | 安全机制内置于架构 | 默认拒绝，最小权限 |
| **纵深防御** | 多层防护体系 | 权限 + 净化 + 审计 + 隔离 |
| **零信任** | 永不信任，始终验证 | 所有请求必须经过权限检查 |
| **可追溯 (E-6)** | 完整审计追踪 | 所有操作记录日志 |
| **跨平台一致 (E-4)** | Windows/Linux/macOS | 统一 API，平台适配 |

### 技术特性

- ✅ **C11 标准**：现代化 C 语言实现
- ✅ **跨平台支持**：Windows/Linux/macOS
- ✅ **高性能**：异步日志、缓存优化
- ✅ **可扩展**：插件化规则引擎
- ✅ **生产就绪**：完整的错误处理和日志
- ✅ **SPDX 合规**：所有源文件包含标准 SPDX 版权头
- ✅ **完整 API 契约**：Doxygen 注释包含 @ownership/@threadsafe/@reentrant

---

## 🔐 核心功能

### 1. 权限裁决引擎 (Permission)

基于 YAML 规则的细粒度访问控制系统。

**功能特性**：
- 📋 **规则引擎**：支持通配符匹配 (`*`, `**`)
- ⚡ **高性能缓存**：LRU 缓存，命中率 >95%
- 🔄 **热重载**：规则更新无需重启
- 🎯 **优先级控制**：支持规则优先级排序
- 📊 **上下文感知**：支持基于上下文的动态决策

**规则示例** (`permission_rules.yaml`)：
```yaml
rules:
  - agent_id: "agent-001"
    action: "read"
    resource: "/data/*"
    allow: true
    priority: 100

  - agent_id: "*"
    action: "write"
    resource: "/tmp/**"
    allow: true
    priority: 50

  - agent_id: "*"
    action: "execute"
    resource: "/bin/*"
    allow: false
    priority: 200
```

**使用示例**：
```c
#include "cupolas.h"

// 初始化模块
cupolas_init("/etc/cupolas/config.yaml", NULL);

// 检查权限
int allowed = cupolas_check_permission(
    "agent-001",      // Agent ID
    "read",           // 操作类型
    "/data/config",   // 资源路径
    NULL              // 上下文（可选）
);

if (allowed) {
    // 执行操作
} else {
    // 权限拒绝
}

// 清理
cupolas_cleanup();
```

**性能指标**：
| 场景 | 延迟 | 说明 |
|------|------|------|
| 首次检查 | ~50μs | 无缓存 |
| 缓存命中 | ~5μs | LRU 缓存 |
| 缓存容量 | 10,000 条 | 可配置 |

---

### 2. 输入净化器 (Sanitizer)

基于规则引擎的输入过滤系统，防止注入攻击。

**功能特性**：
- 🛡️ **多级过滤**：SQL 注入、XSS、路径遍历等
- 📝 **正则匹配**：支持 PCRE 正则表达式
- 🎯 **风险分级**：low/medium/high/critical
- ⚙️ **可配置策略**：替换/拒绝/移除

**规则示例** (`sanitizer_rules.yaml`)：
```yaml
filters:
  - name: "sql_injection"
    pattern: "(SELECT|INSERT|UPDATE|DELETE|DROP|UNION).*"
    action: "replace"
    replacement: "[FILTERED]"
    risk_level: "high"

  - name: "path_traversal"
    pattern: "\\.\\./"
    action: "reject"
    risk_level: "critical"

  - name: "xss_script"
    pattern: "<script.*?>.*?</script>"
    action: "remove"
    risk_level: "high"
```

**使用示例**：
```c
#include "cupolas.h"

char sanitized[1024];
const char* user_input = "<script>alert('xss')</script>Hello";

int ret = cupolas_sanitize_input(
    user_input,        // 原始输入
    sanitized,         // 输出缓冲区
    sizeof(sanitized),  // 缓冲区大小
    SANITIZE_LEVEL_NORMAL
);

if (ret == 0) {
    printf("Sanitized: %s\n", sanitized);
    // 输出：[FILTERED]Hello
}
```

**风险等级说明**：
| 等级 | 处置方式 | 说明 |
|------|----------|------|
| `low` | 记录日志 | 低风险内容 |
| `medium` | 替换内容 | 中等风险 |
| `high` | 拒绝请求 | 高风险攻击 |
| `critical` | 拒绝 + 告警 | 极高风险 |

---

### 3. 审计日志系统 (Audit)

完整的操作追踪与合规记录系统。

**功能特性**：
- 📝 **异步写入**：后台线程批量写入，不阻塞主线程
- 🔄 **自动轮转**：按大小/时间自动轮转
- 📦 **压缩归档**：gzip 压缩，节省 70% 空间
- 🔐 **完整性校验**：SHA-256 哈希校验
- 🔍 **查询接口**：支持过滤和检索

**日志格式** (JSON)：
```json
{
  "timestamp": "2026-04-01T10:30:00Z",
  "agent_id": "agent-001",
  "action": "file.read",
  "resource": "/data/config.yaml",
  "result": "allowed",
  "context": {
    "user": "admin",
    "ip": "192.168.1.100"
  },
  "hash": "sha256:abc123..."
}
```

**使用示例**：
```c
#include "cupolas.h"

// 刷新审计日志（定期调用）
cupolas_flush_audit_log();
```

**配置示例** (`audit_config.yaml`)：
```yaml
audit:
  log_dir: "/var/log/cupolas"
  log_prefix: "audit"
  max_file_size: 104857600  # 100MB
  max_files: 10
  enable_compression: true
  enable_integrity_check: true
```

**性能指标**：
| 指标 | 数值 | 说明 |
|------|------|------|
| 写入延迟 | <1ms | 异步写入 |
| 吞吐量 | 10,000 条/秒 | 并发写入 |
| 压缩率 | 70% | gzip 压缩 |

---

### 4. 虚拟工位 (Workbench)

为 Agent 提供隔离的执行环境。

**功能特性**：
- 🏗️ **两种模式**：进程模式 / 容器模式
- 📊 **资源限制**：CPU/内存/网络配额管理
- 🔒 **隔离执行**：文件系统/网络/进程隔离
- 🛡️ **安全沙箱**：seccomp/CFI 保护

**支持的模式**：
| 模式 | 技术实现 | 隔离级别 | 适用场景 |
|------|----------|----------|----------|
| **进程模式** | namespaces + cgroups | 中 | 轻量级任务 |
| **容器模式** | runc/Docker | 高 | 完整应用 |

**使用示例**：
```c
#include "cupolas.h"

// 执行命令（在隔离环境中）
int exit_code;
char stdout_buf[4096];
char stderr_buf[4096];

char* argv[] = {"/path/to/agent", "--option", NULL};

int ret = cupolas_execute_command(
    "/path/to/agent",  // 命令路径
    argv,              // 参数数组
    &exit_code,        // 退出码
    stdout_buf,        // 标准输出缓冲区
    sizeof(stdout_buf),
    stderr_buf,        // 标准错误缓冲区
    sizeof(stderr_buf)
);

if (ret == 0) {
    printf("Exit code: %d\n", exit_code);
    printf("Output: %s\n", stdout_buf);
}
```

**配置示例** (`workbench_config.yaml`)：
```yaml
workbench:
  mode: "process"  # process | container
  limits:
    max_memory_mb: 512
    max_cpu_percent: 50
    max_processes: 10
    max_open_files: 100
  isolation:
    enable_network: false
    enable_filesystem: true
    readonly_root: true
```

---

### 5. iOS 级安全增强

参考 iOS 沙箱机制实现的高级安全特性。

#### 5.1 代码签名验证 (cupolas_signature)

**功能**：验证 Agent 代码的完整性和可信性

**支持算法**：
- RSA-SHA256/384/512
- ECDSA P-256/P-384
- Ed25519

**使用示例**：
```c
#include "cupolas_signature.h"

// 初始化
cupolas_signature_init(NULL);

// 验证文件签名
cupolas_sig_result_t result;
int ret = cupolas_signature_verify_file(
    "/path/to/agent.bin",
    "Trusted Signer",  // 预期签名者（可选）
    &result
);

if (ret == cupolas_SIG_OK) {
    printf("Signature valid\n");
} else {
    printf("Signature invalid: %d\n", result);
}

// 清理
cupolas_signature_cleanup();
```

#### 5.2 安全凭证库 (cupolas_vault)

**功能**：类似 iOS Keychain 的安全存储

**支持凭证类型**：
- 密码 (Password)
- Token (API Key, OAuth)
- 私钥 (Private Key)
- 证书 (Certificate)
- 机密 (Secret)
- 安全笔记 (Secure Note)

**加密方式**：AES-256-GCM

**使用示例**：
```c
#include "cupolas_vault.h"

// 初始化
cupolas_vault_init(NULL);

// 存储凭证
const char* cred_id = "api_key_001";
const uint8_t* data = (const uint8_t*)"secret_api_key";
size_t data_len = strlen("secret_api_key");

int ret = cupolas_vault_store(
    cred_id,
    data,
    data_len,
    NULL  // 元数据（可选）
);

// 检索凭证
uint8_t buffer[256];
size_t out_len;

ret = cupolas_vault_retrieve(
    cred_id,
    buffer,
    sizeof(buffer),
    &out_len
);

// 清理
cupolas_vault_cleanup();
```

#### 5.3 Entitlements 声明 (cupolas_entitlements)

**功能**：细粒度权限声明机制

**使用示例**：
```c
#include "cupolas_entitlements.h"

// 加载权限声明
int ret = cupolas_entitlements_load("/etc/cupolas/entitlements.yaml");

// 检查权限
int has_access = cupolas_entitlements_check(
    "agent-001",
    "network.access"
);

// 清理
cupolas_entitlements_unload();
```

#### 5.4 运行时保护 (cupolas_runtime_protection)

**功能**：seccomp/CFI 等多层防护

**支持的保护**：
- seccomp-bpf（系统调用过滤）
- CFI（控制流完整性）
- 栈保护（Stack Protector）
- ASLR（地址空间布局随机化）

**使用示例**：
```c
#include "cupolas_runtime_protection.h"

// 启用 seccomp
int ret = cupolas_runtime_protection_enable_seccomp();

// 启用 CFI
ret = cupolas_runtime_protection_enable_cfi();
```

#### 5.5 网络安全 (cupolas_network_security)

**功能**：TLS 强制、网络过滤

**功能特性**：
- TLS 1.2/1.3 强制
- 域名白名单
- 端口过滤
- 协议检查

**使用示例**：
```c
#include "cupolas_network_security.h"

// 启用 TLS 强制
int ret = cupolas_network_security_enable_tls_enforcement();

// 添加过滤规则
ret = cupolas_network_security_add_filter(
    "allow tcp 443",  // 允许 HTTPS
    100               // 优先级
);
```

---

## 🚀 快速开始

### 环境要求

| 组件 | 版本 | 说明 |
|------|------|------|
| **C 编译器** | C11 | GCC 7+/Clang 5+/MSVC 2017+ |
| **CMake** | 3.10+ | 构建系统 |
| **OpenSSL** | 1.1.1+ | 加密库（必需） |
| **libyaml** | 0.2+ | YAML 解析（推荐） |
| **libcjson** | 1.7+ | JSON 处理（推荐） |

### 编译步骤

#### 1. 克隆项目

```bash
git clone https://github.com/spharx/agentos.git
cd AgentOS/cupolas
```

#### 2. 创建构建目录

```bash
mkdir build && cd build
```

#### 3. 配置构建

**Linux/macOS**:
```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_WITH_SANITIZERS=OFF
```

**Windows**:
```bash
cmake .. ^
  -G "Visual Studio 16 2019" ^
  -DBUILD_TESTS=ON
```

#### 4. 编译

```bash
# Linux/macOS
cmake --build . --config Release

# Windows
cmake --build . --config Release
```

#### 5. 运行测试

```bash
ctest --output-on-failure
```

#### 6. 安装（可选）

```bash
sudo cmake --install .
```

### 构建选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_TESTS` | OFF | 编译测试套件 |
| `BUILD_WITH_SANITIZERS` | OFF | 启用地址/线程/未定义行为检测 |
| `BUILD_WITH_LOGGING` | ON | 启用详细日志 |

### 编译输出

```
build/
├── libcupolas.a          # 静态库 (Linux/macOS)
├── cupolas.lib           # 静态库 (Windows)
├── test_cupolas_core     # 核心测试
└── test_cupolas_integration  # 集成测试
```

---

## 🏗️ 架构设计

### 模块架构

```
cupolas/
├── include/
│   └── cupolas.h              # 公共 API
├── src/
│   ├── platform/
│   │   ├── platform.c         # 平台适配层
│   │   └── platform.h
│   ├── permission/
│   │   ├── permission_engine.c  # 权限引擎
│   │   ├── permission_engine.h
│   │   ├── permission_rule.c    # 规则管理
│   │   ├── permission_rule.h
│   │   ├── permission_cache.c   # 权限缓存
│   │   └── permission_cache.h
│   ├── sanitizer/
│   │   ├── sanitizer_core.c     # 净化核心
│   │   ├── sanitizer_core.h
│   │   ├── sanitizer_rules.c    # 规则管理
│   │   ├── sanitizer_rules.h
│   │   ├── sanitizer_cache.c    # 规则缓存
│   │   └── sanitizer_cache.h
│   ├── audit/
│   │   ├── audit.h              # 审计接口
│   │   ├── audit_logger.c       # 日志记录器
│   │   ├── audit_queue.c        # 异步队列
│   │   ├── audit_queue.h
│   │   ├── audit_rotator.c      # 日志轮转
│   │   └── audit_rotator.h
│   ├── workbench/
│   │   ├── workbench.h          # 工位接口
│   │   ├── workbench.c
│   │   ├── workbench_process.h  # 进程管理
│   │   ├── workbench_process_core.c
│   │   ├── workbench_container.c # 容器管理
│   │   ├── workbench_container.h
│   │   ├── workbench_limits.c   # 资源限制
│   │   └── workbench_limits.h
│   ├── security/
│   │   ├── cupolas_error.h      # 统一错误码
│   │   ├── cupolas_error.c
│   │   ├── cupolas_signature.c   # 代码签名
│   │   ├── cupolas_signature.h
│   │   ├── cupolas_vault.c      # 凭证库
│   │   ├── cupolas_vault.h
│   │   ├── cupolas_entitlements.c   # Entitlements
│   │   ├── cupolas_entitlements.h
│   │   ├── cupolas_runtime_protection.c  # 运行时保护
│   │   ├── cupolas_runtime_protection.h
│   │   ├── cupolas_network_security.c    # 网络安全
│   │   └── cupolas_network_security.h
│   ├── cupolas_config.c         # 配置管理
│   ├── cupolas_config.h
│   ├── cupolas_metrics.c        # 指标收集
│   ├── cupolas_metrics.h
│   ├── cupolas_monitoring.c     # 监控
│   └── cupolas_monitoring.h
├── tests/
│   ├── unit/                    # 单元测试
│   │   ├── test_cupolas_core.c
│   │   ├── test_cupolas_config.c
│   │   ├── test_cupolas_metrics.c
│   │   └── test_cupolas_workbench.c
│   ├── integration/             # 集成测试
│   │   └── test_cupolas_integration.c
│   └── fuzz/                    # 模糊测试
│       ├── fuzz_sanitizer.c
│       └── fuzz_permission.c
├── CMakeLists.txt
└── README.md
```

### 数据流

```
用户请求
    ↓
┌─────────────────┐
│  输入净化器      │ ← 规则引擎
│  (Sanitizer)    │
└────────┬────────┘
         ↓
┌─────────────────┐
│  权限裁决引擎    │ ← 规则引擎 + 缓存
│  (Permission)   │
└────────┬────────┘
         ↓
┌─────────────────┐
│  虚拟工位执行    │
│  (Workbench)    │
└────────┬────────┘
         ↓
┌─────────────────┐
│  审计日志记录    │
│  (Audit)        │
└─────────────────┘
```

### 线程模型

```
主线程
├── 权限检查（同步）
├── 输入净化（同步）
└── 命令执行（同步/异步）

后台线程
├── 审计日志写入（异步批量）
├── 日志轮转（定时触发）
└── 缓存清理（定时触发）
```

---

## 📖 使用指南

### 完整示例

```c
/**
 * @file example.c
 * @brief cupolas 完整使用示例
 */

#include <stdio.h>
#include <stdlib.h>
#include "cupolas.h"

int main(int argc, char* argv[]) {
    int ret = 0;
    agentos_error_t error = {0};

    // 1. 初始化模块
    printf("Initializing cupolas...\n");
    ret = cupolas_init("/etc/cupolas/config.yaml", &error);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize cupolas: %s\n", error.message);
        return 1;
    }

    // 2. 检查权限
    printf("Checking permission...\n");
    int allowed = cupolas_check_permission(
        "agent-001",
        "read",
        "/data/config.yaml",
        NULL
    );

    if (!allowed) {
        fprintf(stderr, "Permission denied\n");
        ret = 1;
        goto cleanup;
    }

    // 3. 输入净化
    printf("Sanitizing input...\n");
    char sanitized[1024];
    const char* user_input = "SELECT * FROM users";

    ret = cupolas_sanitize_input(
        user_input,
        sanitized,
        sizeof(sanitized),
        SANITIZE_LEVEL_NORMAL
    );

    if (ret != 0) {
        fprintf(stderr, "Input sanitization failed\n");
        ret = 1;
        goto cleanup;
    }

    printf("Sanitized: %s\n", sanitized);

    // 4. 执行命令（在隔离环境中）
    printf("Executing command...\n");
    int exit_code;
    char stdout_buf[4096];
    char stderr_buf[4096];

    char* cmd_argv[] = {"/bin/echo", "Hello from sandbox", NULL};

    ret = cupolas_execute_command(
        "/bin/echo",
        cmd_argv,
        &exit_code,
        stdout_buf,
        sizeof(stdout_buf),
        stderr_buf,
        sizeof(stderr_buf)
    );

    if (ret == 0) {
        printf("Command output: %s\n", stdout_buf);
    }

    // 5. 刷新审计日志
    printf("Flushing audit log...\n");
    cupolas_flush_audit_log();

cleanup:
    // 6. 清理
    printf("Cleaning up...\n");
    cupolas_cleanup();

    return ret;
}
```

### 编译示例

```bash
gcc -o example example.c \
  -I/path/to/cupolas/include \
  -L/path/to/cupolas/build \
  -lcupolas \
  -lssl -lcrypto \
  -lyaml -lcjson
```

---

## ⚙️ 配置说明

### 主配置文件

**位置**: `/etc/cupolas/config.yaml`

```yaml
# cupolas 主配置文件

# 权限模块配置
permission:
  rules_file: "/etc/cupolas/permission_rules.yaml"
  cache_size: 10000
  cache_ttl_seconds: 300
  enable_logging: true

# 净化器配置
sanitizer:
  rules_file: "/etc/cupolas/sanitizer_rules.yaml"
  default_action: "replace"
  enable_logging: true

# 审计日志配置
audit:
  log_dir: "/var/log/cupolas"
  log_prefix: "audit"
  max_file_size: 104857600  # 100MB
  max_files: 10
  enable_compression: true
  enable_integrity_check: true
  flush_interval_seconds: 5

# 工位配置
workbench:
  mode: "process"  # process | container
  default_limits:
    max_memory_mb: 512
    max_cpu_percent: 50
    max_processes: 10
  enable_network: false
  enable_filesystem_isolation: true

# 安全模块配置
security:
  # 代码签名
  signature:
    enable: true
    trusted_ca_path: "/etc/cupolas/ca"
    check_cert_chain: true
    check_revocation: true

  # 凭证库
  vault:
    enable: true
    storage_path: "/var/lib/cupolas/vault"
    enable_audit: true
    auto_lock_seconds: 300

  # Entitlements
  entitlements:
    enable: true
    rules_file: "/etc/cupolas/entitlements.yaml"

  # 运行时保护
  runtime_protection:
    enable_seccomp: true
    enable_cfi: false  # 仅 Linux

  # 网络安全
  network_security:
    enable_tls_enforcement: true
    allowed_ports: [80, 443]
    blocked_domains: []
```

### 平台特定配置

#### Linux

```yaml
platform:
  name: "linux"
  enable_seccomp: true
  enable_apparmor: false
  cgroup_version: "v2"
```

#### macOS

```yaml
platform:
  name: "macos"
  enable_sandbox: true
  entitlements_required: true
```

#### Windows

```yaml
platform:
  name: "windows"
  enable_job_objects: true
  enable_appcontainer: false
```

---

## 📚 API 参考

### 核心 API

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `cupolas_init(config_path, error)` | 初始化模块 | 0=成功 |
| `cupolas_cleanup()` | 清理模块 | - |
| `cupolas_version()` | 获取版本号 | 版本字符串 |
| `cupolas_execute_command(...)` | 执行命令 | 0=成功 |
| `cupolas_flush_audit_log()` | 刷新审计日志 | - |

### 权限 API

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `cupolas_check_permission(...)` | 检查权限 | 1=允许，0=拒绝 |
| `cupolas_add_permission_rule(...)` | 添加规则 | 0=成功 |
| `cupolas_clear_permission_cache()` | 清除缓存 | - |

### 净化 API

| 函数 | 说明 | 返回值 |
|------|------|--------|
| `cupolas_sanitize_input(...)` | 净化输入 | 0=成功 |

### 错误码

| 错误码 | 值 | 说明 |
|--------|-----|------|
| `cupolas_ERR_OK` | 0 | 成功 |
| `cupolas_ERR_UNKNOWN` | -1 | 未知错误 |
| `cupolas_ERR_INVALID_PARAM` | -2 | 无效参数 |
| `cupolas_ERR_NULL_POINTER` | -3 | 空指针 |
| `cupolas_ERR_OUT_OF_MEMORY` | -4 | 内存不足 |
| `cupolas_ERR_BUFFER_TOO_SMALL` | -5 | 缓冲区太小 |
| `cupolas_ERR_NOT_FOUND` | -6 | 未找到 |
| `cupolas_ERR_ALREADY_EXISTS` | -7 | 已存在 |
| `cupolas_ERR_TIMEOUT` | -8 | 超时 |
| `cupolas_ERR_NOT_SUPPORTED` | -9 | 不支持 |
| `cupolas_ERR_PERMISSION_DENIED` | -10 | 权限拒绝 |
| `cupolas_ERR_IO` | -11 | IO 错误 |
| `cupolas_ERR_STATE_ERROR` | -13 | 状态错误 |
| `cupolas_ERR_OVERFLOW` | -14 | 溢出 |
| `cupolas_ERR_TRY_AGAIN` | -15 | 重试 |

---

## 🛠️ 开发指南

### 代码风格

遵循 [AgentOS C 语言编码规范](../../manuals/specifications/coding_standard/C_coding_style_guide.md)

**关键要求**：
- ✅ 使用 `cupolas_` 前缀命名所有公共 API
- ✅ 所有函数添加 Doxygen 注释
- ✅ 错误处理使用统一错误码
- ✅ 内存分配必须检查返回值
- ✅ 所有资源必须显式释放
- ✅ 所有源文件包含 SPDX 版权头

### API 契约注释示例

```c
/**
 * @brief Initialize cupolas module
 * @param[in] config_path Configuration file path (NULL for default config)
 * @param[out] error Optional error code output
 * @return 0 on success, negative on failure
 * @note Thread-safe: Multiple threads may call init, only first succeeds
 * @ownership config_path string: caller retains ownership, may be NULL
 */
int cupolas_init(const char* config_path, agentos_error_t* error);
```

### 添加新功能

1. **在对应子模块添加实现**
2. **在 `include/cupolas.h` 添加公共 API**
3. **编写单元测试**
4. **更新文档**

### 测试

```bash
# 运行所有测试
ctest --output-on-failure

# 运行特定测试
./test_cupolas_core
./test_cupolas_integration

# 模糊测试（可选）
./fuzz_permission
./fuzz_sanitizer
```

### 调试

**启用详细日志**：
```bash
cmake .. -DBUILD_WITH_LOGGING=ON
```

**启用 Sanitizers**：
```bash
cmake .. -DBUILD_WITH_SANITIZERS=ON
```

**调试日志**：
```c
#ifdef DEBUG
    printf("[DEBUG] %s:%d - Debug message\n", __FILE__, __LINE__);
#endif
```

---

## ❓ 常见问题

### Q1: 如何配置权限规则？

**A**: 创建 `permission_rules.yaml` 文件，参考 [规则示例](#1-权限裁决引擎-permission)。

### Q2: 审计日志存储在哪里？

**A**: 默认存储在 `/var/log/cupolas/`，可通过配置文件修改。

### Q3: 如何启用代码签名验证？

**A**: 在主配置文件中设置：
```yaml
security:
  signature:
    enable: true
    trusted_ca_path: "/etc/cupolas/ca"
```

### Q4: 支持哪些平台？

**A**: Windows 10+/Linux (Kernel 4.0+)/macOS 10.14+

### Q5: 性能如何？

**A**:
- 权限检查：<50μs（缓存命中 <5μs）
- 输入净化：<100μs
- 审计日志写入：<1ms（异步）

### Q6: 如何处理错误？

**A**: 所有 API 返回 0 表示成功，负值表示失败。使用错误码判断具体错误类型。

```c
agentos_error_t error = {0};
int ret = cupolas_init(NULL, &error);
if (ret != 0) {
    switch (ret) {
        case cupolas_ERR_OUT_OF_MEMORY:
            fprintf(stderr, "Out of memory\n");
            break;
        case cupolas_ERR_IO:
            fprintf(stderr, "IO error: %s\n", error.message);
            break;
        default:
            fprintf(stderr, "Unknown error: %s\n", error.message);
    }
}
```

---

## 📊 性能基准

### 测试环境

- **CPU**: Intel i7-10700K
- **内存**: 32GB DDR4
- **系统**: Ubuntu 22.04 LTS
- **编译器**: GCC 11.2.0

### 基准测试结果

| 操作 | 延迟 | 吞吐量 |
|------|------|--------|
| 权限检查（无缓存） | 50μs | 20,000 ops/s |
| 权限检查（缓存命中） | 5μs | 200,000 ops/s |
| 输入净化 | 100μs | 10,000 ops/s |
| 审计日志写入 | <1ms | 10,000 ops/s |
| 命令执行 | ~10ms | 100 ops/s |

---

## 🔒 安全特性

### 纵深防御体系

```
Layer 1: 权限控制  ← 第一道防线
    ↓
Layer 2: 输入净化  ← 第二道防线
    ↓
Layer 3: 隔离执行  ← 第三道防线
    ↓
Layer 4: 审计追踪  ← 事后追溯
```

### 安全最佳实践

1. **最小权限原则**：只授予必要的权限
2. **默认拒绝**：未明确允许的操作一律拒绝
3. **深度防御**：多层防护，不依赖单一机制
4. **完整审计**：所有操作必须记录
5. **定期更新**：及时更新规则和签名

---

## 🤝 贡献指南

### 提交代码

1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

### 报告问题

在 [GitHub Issues](https://github.com/spharx/agentos/issues) 报告问题。

### 代码审查要求

- ✅ 遵循编码规范
- ✅ 添加单元测试
- ✅ 更新文档
- ✅ 通过 CI/CD 流水线

---

## 📄 许可证

Copyright © 2026 SPHARX Ltd.

采用 [Apache License 2.0](../../LICENSE) 许可证。

---

## 🔗 相关链接

- [AgentOS 官方文档](https://agentos.dev)
- [架构设计原则](../../manuals/ARCHITECTURAL_PRINCIPLES.md)
- [C 语言编码规范](../../manuals/specifications/coding_standard/C_coding_style_guide.md)
- [问题反馈](https://github.com/spharx/agentos/issues)

---

**最后更新**: 2026-04-01  
**维护者**: SPHARX AgentOS Team  
**联系方式**: support@spharx.com
