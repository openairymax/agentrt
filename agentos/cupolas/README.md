# Cupolas — 安全穹顶

`agentos/cupolas/` 是 AgentOS 的安全组件集合，提供全方位的安全防护能力。Cupolas（穹顶）寓意全方位、无死角的系统保护。

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
| **安全工具库** | `src/utils/` | 安全相关的通用工具函数 |

## 架构总览

```
+-------------------------------------------------------------------+
|                      安全保障体系（Cupolas）                         |
+-------------------------------------------------------------------+
|  +----------------+  +----------------+  +----------------+        |
|  |   Workbench    |  |   Sanitizer    |  |  Permission    |        |
|  |  安全策略测试   |  |  输入清洗器     |  |  权限管理      |        |
|  +----------------+  +----------------+  +----------------+        |
|  +----------------+  +----------------+  +----------------+        |
|  |    Audit       |  |    Utils       |  |  External I/F  |        |
|  |  审计系统       |  |  安全工具库     |  |  外部安全接口   |        |
|  +----------------+  +----------------+  +----------------+        |
+-------------------------------------------------------------------+
|                       系统调用层（Syscall）                          |
+-------------------------------------------------------------------+
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

## 与其它模块的关系

| 模块 | 关系 |
|------|------|
| **Syscall** | Cupolas 为系统调用层提供安全校验 |
| **Gateway** | Gateway 调用 Cupolas 进行请求鉴权和输入清洗 |
| **Manager** | Manager 管理 Cupolas 的安全策略配置 |
| **Commons** | 使用 Commons 的同步原语和错误框架 |

---

*AgentOS Cupolas — 安全穹顶*
