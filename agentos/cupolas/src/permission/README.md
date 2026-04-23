# Permission — 权限管理

`cupolas/src/permission/` 提供基于 RBAC（基于角色的访问控制）和 ABAC（基于属性的访问控制）的权限管理引擎。

## 设计目标

- **双模授权**：同时支持 RBAC 和 ABAC，灵活适应不同场景
- **权限继承**：支持角色继承和资源层级继承
- **默认拒绝**：未明确授权的请求一律拒绝
- **高效评估**：权限评估在微秒级完成，最小化性能开销

## 核心模型

```
用户 (User) ──→ 角色 (Role) ──→ 权限 (Permission) ──→ 资源 (Resource)
   │               │
   │          属性 (Attributes)    ABAC 规则
   └───────── 上下文 (Context) ──→ 策略引擎
```

### RBAC

基于角色的访问控制，适用于组织架构清晰的场景：

```json
{
    "roles": {
        "admin": {
            "permissions": ["*:*"],
            "inherits": []
        },
        "operator": {
            "permissions": ["task:read", "task:write", "log:read"],
            "inherits": ["viewer"]
        },
        "viewer": {
            "permissions": ["task:read", "log:read"],
            "inherits": []
        }
    }
}
```

### ABAC

基于属性的访问控制，适用于上下文敏感的细粒度授权：

```json
{
    "abac_rules": [
        {
            "name": "restrict_sensitive_data",
            "effect": "deny",
            "conditions": {
                "user.department": {"ne": "finance"},
                "resource.classification": {"eq": "sensitive"}
            }
        }
    ]
}
```

## 使用示例

```c
#include "cupolas/src/permission/permission.h"

// 初始化权限引擎
permission_engine_t* engine = permission_engine_create();

// 配置角色
permission_role_t admin_role = {
    .name = "admin",
    .permissions = (const char*[]){"*:*"},
    .permission_count = 1
};
permission_add_role(engine, &admin_role);

// 为用户分配角色
permission_assign_role(engine, "user-001", "admin");

// 权限检查
permission_request_t request = {
    .user_id = "user-001",
    .action = "task:write",
    .resource = "task:123"
};

permission_result_t result = permission_check(engine, &request);
if (result == PERMISSION_GRANTED) {
    // 允许操作
} else {
    // 拒绝操作
    printf("权限不足: %s", result.reason);
}
```

## 权限缓存

权限评估结果会被缓存，缓存命中时评估时间 < 1μs：

| 缓存类型 | TTL | 失效策略 |
|----------|-----|----------|
| 用户角色 | 300s | 角色变更时主动失效 |
| 角色权限 | 600s | 权限变更时主动失效 |
| ABAC 策略 | 60s | 策略变更时主动失效 |

---

*AgentOS Cupolas — Permission*
