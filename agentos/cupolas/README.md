# Cupolas — 安全穹顶

`agentos/cupolas/` 是 AgentOS 的安全组件集合，提供全方位的安全防护能力。Cupolas（穹顶）寓意全方位、无死角的系统保护。

> Part of AgentOS v0.1.0

## 设计目标

- **纵深防御**：多层安全防护，单层突破不导致系统失守
- **零信任架构**：默认拒绝所有访问，基于身份和上下文逐次授权
- **可审计性**：所有安全事件完整记录，支持溯源和取证
- **最小权限**：按需授权，权限粒度精确到单个操作

## 核心子系统

| 子系统 | 路径 | 职责 |
|--------|------|------|
| **安全工作台** | `src/workbench/` | 安全策略的交互式测试与验证环境 |
| **输入清洗器** | `src/sanitizer/` | 输入校验与注入防护引擎 |
| **权限管理** | `src/permission/` | RBAC + ABAC 权限管理与访问控制 |
| **审计系统** | `src/audit/` | 安全审计日志与事件追踪 |
| **安全防护引擎** | `src/security/` | 文件扫描、API 保护、行为分析、网络防护 |
| **安全守卫框架** | `src/guards/` | 可扩展的安全检测框架（规则/模型/行为分析守卫） |
| **安全平台抽象** | `src/platform/` | 平台级安全适配 |
| **安全工具库** | `src/utils/` | 安全相关的通用工具函数 |
| **安全文档** | `docs/` | 安全策略与配置文档 |
| **测试套件** | `tests/` | 单元/集成/压力/模糊测试（benchmark/fuzz/integration/stress/unit） |

### 独立组件

| 组件 | 源文件 | 职责 |
|------|--------|------|
| **熔断器** | `src/circuit_breaker.c` | 故障快速失败与自动恢复模式 |
| **YAML 解析器** | `src/yaml_minimal.c` | YAML 1.1 配置文件解析（锚点/别名/标签/折叠标量等） |

### OpenSSL 条件编译

当定义 `AGENTOS_HAS_OPENSSL` 时，以下安全模块会被启用：

| 模块 | 源文件 | 职责 |
|------|--------|------|
| **数字签名** | `src/security/cupolas_signature.c` | 代码签名验证（RSA/ECDSA/Ed25519），证书链校验 |
| **密钥保险库** | `src/security/cupolas_vault.c` | 安全凭证存储（AES-256-GCM），类似 iOS Keychain |
| **权利管理** | `src/security/cupolas_entitlements.c` | 声明式权限管理（文件系统/网络/IPC/资源限制） |
| **运行时保护** | `src/security/cupolas_runtime_protection.c` | seccomp、CFI、内存保护、完整性校验 |
| **网络安全** | `src/security/cupolas_network_security.c` | TLS、防火墙、网络访问控制 |
| **TLS 安全** | `src/security/network/tls_security.c` | TLS/SSL 连接管理与证书验证 |

## 架构总览

```
+-----------------------------------------------------------------------+
|                        安全保障体系（Cupolas）                           |
+-----------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+            |
|  |   Workbench    |  |   Sanitizer    |  |  Permission    |            |
|  |  安全策略测试   |  |  输入清洗器     |  |  权限管理      |            |
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

## 安全策略模型

所有 Cupolas 组件遵循统一的安全策略模型：

```yaml
# 安全策略示例
policies:
  - name: "api_access_control"
    effect: "deny"              # allow / deny / audit
    subjects: ["role:guest"]    # 主体
    actions: ["api:write"]      # 操作
    resources: ["/api/v1/*"]    # 资源
    conditions:                  # 条件
      ip_range: ["10.0.0.0/8"]
      time_range: ["09:00-18:00"]
```

## 集成方式

```c
#include "cupolas/cupolas.h"

// 初始化安全穹顶
cupolas_t* cupolas = cupolas_create();

// 注册安全策略
cupolas_policy_t policy = {
    .name = "api_access",
    .effect = CUPOLAS_EFFECT_DENY,
    .subjects = (const char*[]){"role:guest"},
    .actions = (const char*[]){"api:write"},
    .resource_pattern = "/api/v1/*"
};
cupolas_add_policy(cupolas, &policy);

// 执行安全检查
cupolas_result_t result = cupolas_check(
    cupolas,
    "role:guest",
    "api:write",
    "/api/v1/config"
);

if (result == CUPOLAS_DENY) {
    cupolas_audit_log(cupolas, "访问被拒绝",
        "subject", "role:guest",
        "action", "api:write",
        "resource", "/api/v1/config");
}
```

## 依赖

| 依赖 | 必需 | 说明 |
|------|------|------|
| **agentos_common** | 是 | 同步原语、错误框架、类型定义 |
| **OpenSSL** | 否 | 数字签名、密钥保险库、TLS 等（`AGENTOS_HAS_OPENSSL`） |
| **libyaml** | 否 | 完整 YAML 支持（内置 `yaml_minimal.c` 作为后备） |
| **cJSON** | 否 | JSON 配置解析 |

> 依赖检测遵循 BAN-12 规则：所有 `find_package` 在根 CMakeLists.txt 中集中完成，子模块仅引用缓存变量。

## 与其它模块的关系

| 模块 | 关系 |
|------|------|
| **Syscall** | Cupolas 为系统调用层提供安全校验 |
| **Gateway** | Gateway 调用 Cupolas 进行请求鉴权和输入清洗 |
| **Manager** | Manager 管理 Cupolas 的安全策略配置 |
| **Commons** | 使用 Commons 的同步原语和错误框架 |

---

*AgentOS Cupolas — 安全穹顶*
