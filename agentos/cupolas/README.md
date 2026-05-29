# Cupolas — 安全穹顶

**模块路径**: `agentos/cupolas/`
**版本**: v0.0.5

## 概述

Cupolas（穹顶）是 AgentOS 的安全组件集合，提供全方位的安全防护能力。Cupolas 寓意全方位、无死角的系统保护，涵盖输入清洗、权限管理、审计追踪、安全防护引擎、策略工作台和可扩展的安全守卫框架。所有组件遵循纵深防御和零信任架构原则。

## 设计目标

- **纵深防御**：多层安全防护，单层突破不导致系统失守
- **零信任架构**：默认拒绝所有访问，基于身份和上下文逐次授权
- **可审计性**：所有安全事件完整记录，支持溯源和取证
- **最小权限**：按需授权，权限粒度精确到单个操作
- **可扩展**：安全守卫框架支持规则/模型/行为分析守卫的动态加载

## 目录结构

```
cupolas/
├── include/                         # 公共头文件
│   ├── cupolas.h                    # Cupolas 统一入口
│   ├── zero_trust_integration.h     # 零信任集成接口
│   ├── dynamic_policy_engine.h      # 动态策略引擎
│   └── safety_guard.h               # 安全守卫接口
├── src/
│   ├── cupolas.c                    # Cupolas 核心实现
│   ├── cupolas_config.c/h           # 配置管理
│   ├── cupolas_metrics.c/h          # 指标采集
│   ├── cupolas_monitoring.c/h       # 监控系统
│   ├── circuit_breaker.c/h          # 熔断器
│   ├── yaml_minimal.c/h             # YAML 1.1 解析器
│   ├── platform/                    # 平台安全适配
│   │   └── platform.c/h
│   ├── sanitizer/                   # 输入清洗器
│   │   ├── sanitizer.h              # 公共接口
│   │   ├── sanitizer_core.c         # 清洗核心引擎
│   │   ├── sanitizer_rules.c/h      # 规则引擎
│   │   └── sanitizer_cache.c/h      # 清洗缓存
│   ├── permission/                  # 权限管理
│   │   ├── permission.h             # 公共接口
│   │   ├── permission_engine.c/h    # 权限引擎
│   │   ├── permission_rule.c/h      # 规则管理
│   │   └── permission_cache.c/h     # 权限缓存
│   ├── audit/                       # 审计系统
│   │   ├── audit.h                  # 公共接口
│   │   ├── audit_logger.c           # 审计日志器
│   │   ├── audit_queue.c/h          # 线程安全队列
│   │   ├── audit_rotator.c/h        # 日志轮转
│   │   └── audit_overflow.c/h       # 溢出处理
│   ├── security/                    # 安全防护引擎
│   │   ├── cupolas_error.c/h        # 统一错误处理
│   │   ├── cupolas_signature.c/h    # 数字签名（需 OpenSSL）
│   │   ├── cupolas_vault.c/h        # 密钥保险库（需 OpenSSL）
│   │   ├── cupolas_entitlements.c/h # 权利管理（需 OpenSSL）
│   │   ├── cupolas_runtime_protection.c/h  # 运行时保护（需 OpenSSL）
│   │   ├── cupolas_network_security.c/h    # 网络安全（需 OpenSSL）
│   │   └── network/                 # 网络安全子模块
│   │       ├── http_security.c/h    # HTTP 安全
│   │       ├── dns_security.c/h     # DNS 安全
│   │       ├── network_filter.c/h   # 网络过滤
│   │       ├── network_utils.c/h    # 网络工具
│   │       └── tls_security.c/h     # TLS 安全（需 OpenSSL）
│   ├── guards/                      # 安全守卫框架
│   │   ├── guard_core.c/h           # 守卫核心
│   │   └── guard_integration.c/h    # 守卫集成
│   ├── workbench/                   # 安全工作台
│   │   ├── workbench.c/h            # 工作台核心
│   │   ├── workbench_process.h      # 进程管理
│   │   ├── workbench_process_core.c # 进程管理实现
│   │   ├── workbench_container.c/h  # 容器隔离
│   │   └── workbench_limits.c/h     # 资源限制
│   └── utils/                       # 安全工具库
│       └── cupolas_utils.c/h        # 通用工具宏与函数
├── tests/                           # 测试套件
│   ├── unit/                        # 单元测试
│   ├── integration/                 # 集成测试
│   ├── stress/                      # 压力测试
│   ├── fuzz/                        # 模糊测试
│   └── benchmark/                   # 性能基准
├── CMakeLists.txt                   # CMake 构建配置
└── README.md                        # 本文档
```

## 核心子系统

| 子系统 | 路径 | 职责 |
|--------|------|------|
| **输入清洗器** | `src/sanitizer/` | XSS/SQL 注入/命令注入/路径遍历防护 |
| **权限管理** | `src/permission/` | RBAC + ABAC 权限引擎，规则优先级排序 |
| **审计系统** | `src/audit/` | 异步写入、HMAC 签名链、日志轮转 |
| **安全防护引擎** | `src/security/` | 文件扫描、API 防护、行为分析、网络安全 |
| **安全工作台** | `src/workbench/` | 隔离执行环境、资源控制、进程管理 |
| **安全守卫** | `src/guards/` | 可扩展的安全检测框架 |
| **安全工具库** | `src/utils/` | 内存管理、错误处理、日志、编译器提示 |

### 独立组件

| 组件 | 源文件 | 职责 |
|------|--------|------|
| **熔断器** | `src/circuit_breaker.c` | 故障快速失败与自动恢复模式 |
| **YAML 解析器** | `src/yaml_minimal.c` | YAML 1.1 配置文件解析（锚点/别名/标签/折叠标量） |
| **配置管理** | `src/cupolas_config.c` | Cupolas 安全策略配置 |
| **指标采集** | `src/cupolas_metrics.c` | 安全事件指标统计 |
| **监控系统** | `src/cupolas_monitoring.c` | 运行时安全监控 |

### OpenSSL 条件编译

当定义 `AGENTOS_HAS_OPENSSL` 时，以下 iOS 级安全模块会被启用：

| 模块 | 源文件 | 职责 |
|------|--------|------|
| **数字签名** | `cupolas_signature.c` | RSA/ECDSA/Ed25519 签名验证、证书链校验 |
| **密钥保险库** | `cupolas_vault.c` | AES-256-GCM 安全凭证存储，类似 iOS Keychain |
| **权利管理** | `cupolas_entitlements.c` | 声明式权限（文件系统/网络/IPC/资源限制） |
| **运行时保护** | `cupolas_runtime_protection.c` | seccomp、CFI、内存保护、完整性校验 |
| **网络安全** | `cupolas_network_security.c` | TLS 连接管理、防火墙规则 |
| **TLS 安全** | `network/tls_security.c` | TLS/SSL 连接管理与证书验证 |

## 架构总览

```
+-----------------------------------------------------------------------+
|                        安全保障体系（Cupolas）                           |
+-----------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+            |
|  |   Workbench    |  |   Sanitizer    |  |  Permission    |            |
|  |  隔离执行环境   |  |  输入清洗器     |  |  权限管理      |            |
|  +----------------+  +----------------+  +----------------+            |
|  +----------------+  +----------------+  +----------------+            |
|  |    Audit       |  |    Utils       |  |   Security     |            |
|  |  审计系统       |  |  安全工具库     |  |  安全防护引擎   |            |
|  +----------------+  +----------------+  +----------------+            |
|  +----------------+  +----------------+  +----------------+            |
|  |    Guards      |  |Circuit Breaker |  |  YAML Parser   |            |
|  |  安全守卫框架   |  |    熔断器       |  |  配置解析器     |            |
|  +----------------+  +----------------+  +----------------+            |
|  +----------------------------------------------------------------+   |
|  |              OpenSSL 条件模块（AGENTOS_HAS_OPENSSL）              |   |
|  |  Signature | Vault | Entitlements | RuntimeProt | NetSec | TLS  |   |
|  +----------------------------------------------------------------+   |
+-----------------------------------------------------------------------+
|                         系统调用层（Syscall）                            |
+-----------------------------------------------------------------------+
```

## 构建说明

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_WITH_SANITIZERS=OFF
cmake --build .
```

CMake 选项：
- `BUILD_TESTS`（默认 ON）：构建测试套件
- `BUILD_WITH_SANITIZERS`（默认 OFF）：启用 ASAN/MSAN/TSAN
- `BUILD_WITH_LOGGING`（默认 ON）：启用详细日志
- `AGENTOS_HAS_OPENSSL`：由根 CMakeLists 自动检测

编译安全选项（Linux）：
- `-fstack-protector-strong`：栈保护
- `-D_FORTIFY_SOURCE=2`：缓冲区溢出保护
- `-fvisibility=hidden`：符号隐藏
- `-Wl,-z,relro,-z,now`：只读重定位
- `-Wl,-z,noexecstack`：禁止栈执行

## 依赖

| 依赖 | 必需 | 说明 |
|------|------|------|
| **agentos_common** | 是 | 同步原语、错误框架、类型定义、内存管理 |
| **OpenSSL** | 否 | 数字签名、密钥保险库、TLS 等（`AGENTOS_HAS_OPENSSL`） |
| **libyaml** | 否 | 完整 YAML 支持（内置 `yaml_minimal.c` 作为后备） |
| **cJSON** | 否 | JSON 配置解析 |

> **BAN-12**：所有 `find_package` 在根 CMakeLists.txt 中集中完成，子模块仅引用缓存变量。

## 与其它模块的关系

| 模块 | 关系 |
|------|------|
| **Commons** | 使用 Commons 的同步原语、错误框架、类型定义和内存管理宏 |
| **Syscall** | Cupolas 为系统调用层提供安全校验 |
| **Gateway** | Gateway 调用 Cupolas 进行请求鉴权和输入清洗 |
| **Manager** | Manager 管理 Cupolas 的安全策略配置 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
