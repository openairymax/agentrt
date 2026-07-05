# Permission — 权限管理

**模块路径**: `agentrt/cupolas/src/permission/`
**版本**: v0.1.0

## 概述

Permission 模块提供基于 RBAC（基于角色的访问控制）和 ABAC（基于属性的访问控制）的权限管理引擎。遵循最小权限原则，默认拒绝所有访问，基于身份和上下文逐次授权。支持规则优先级排序、动态规则加载和权限缓存，权限评估在微秒级完成。

## 设计目标

- **双模授权**：同时支持 RBAC 和 ABAC，灵活适应不同场景
- **默认拒绝**：未明确授权的请求一律拒绝
- **高效评估**：权限评估在微秒级完成，缓存命中 < 1μs
- **动态加载**：支持运行时添加/移除规则，无需重启
- **引用计数**：引擎支持引用计数，安全共享

## 目录结构

```
permission/
├── permission.h                 # 权限引擎公共接口
├── permission_engine.h          # 权限引擎内部接口
├── permission_engine.c          # 权限引擎实现
├── permission_rule.h            # 规则定义接口
├── permission_rule.c            # 规则管理实现
├── permission_cache.h           # 权限缓存接口
├── permission_cache.c           # 缓存实现
└── README.md                    # 本文档
```

## 核心模型

```
用户 (Agent) ──→ 角色 (Role) ──→ 权限 (Permission) ──→ 资源 (Resource)
   │               │
   │          属性 (Attributes)    ABAC 规则
   └───────── 上下文 (Context) ──→ 策略引擎
```

### RBAC 模式

基于角色的访问控制，适用于组织架构清晰的场景。规则定义格式：

```yaml
rules:
  - agent_id: "admin"
    action: "*"
    resource: "*"
    allow: 1
    priority: 100
  - agent_id: "operator"
    action: "read"
    resource: "/data/*"
    allow: 1
    priority: 50
  - agent_id: "*"
    action: "write"
    resource: "/config/*"
    allow: 0
    priority: 10
```

### ABAC 模式

基于属性的访问控制，适用于上下文敏感的细粒度授权。通过 `context` 参数传递属性信息。

## 接口说明

| 函数 | 说明 |
|------|------|
| `permission_engine_create(rules_path)` | 创建权限引擎（rules_path 可为 NULL） |
| `permission_engine_destroy(engine)` | 销毁权限引擎 |
| `permission_engine_ref(engine)` | 增加引用计数 |
| `permission_engine_unref(engine)` | 减少引用计数 |
| `permission_engine_check(engine, agent_id, action, resource, context)` | 权限检查（1=允许，0=拒绝，负数=错误） |
| `permission_engine_reload(engine)` | 重新加载规则文件 |
| `permission_engine_clear_cache(engine)` | 清除权限缓存 |
| `permission_engine_add_rule(engine, agent_id, action, resource, allow, priority)` | 动态添加规则 |
| `permission_engine_rule_count(engine)` | 获取规则数量 |
| `permission_engine_cache_stats(engine, hit_count, miss_count)` | 获取缓存统计 |

## 使用示例

```c
#include "permission.h"

permission_engine_t *engine = permission_engine_create("rules.yaml");

permission_engine_add_rule(engine, "admin", "*", "*", 1, 100);
permission_engine_add_rule(engine, "viewer", "read", "/data/*", 1, 50);
permission_engine_add_rule(engine, "*", "write", "/config/*", 0, 10);

int result = permission_engine_check(engine, "admin", "write", "/config/app", NULL);
if (result == 1) {
    printf("Access granted\n");
} else {
    printf("Access denied\n");
}

uint64_t hits, misses;
permission_engine_cache_stats(engine, &hits, &misses);
printf("Cache: %lu hits, %lu misses\n", hits, misses);

permission_engine_destroy(engine);
```

## 权限缓存

| 缓存类型 | TTL | 失效策略 |
|----------|-----|----------|
| 权限检查结果 | 动态 | 规则变更时主动失效 |
| 规则索引 | 持久 | `clear_cache` 时失效 |

缓存命中时评估时间 < 1μs，缓存未命中时评估时间取决于规则数量。

## 规则匹配

- **优先级排序**：规则按 `priority` 降序排列，高优先级规则先匹配
- **通配符支持**：`agent_id` 和 `action` 支持 `*` 通配符
- **Glob 模式**：`resource` 支持 glob 模式（如 `/data/*`、`/api/v1/**`）
- **默认拒绝**：无规则匹配时默认拒绝

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `platform.h` | 平台抽象层（互斥锁） |
| `cupolas_utils.h` | 安全内存管理、日志宏 |
| `yaml_minimal.h` | YAML 规则文件解析 |

## 相关子系统

| 子系统 | 关系 |
|--------|------|
| [Sanitizer](../sanitizer/README.md) | 输入清洗后的请求进入权限检查 |
| [Audit](../audit/README.md) | 权限拒绝事件会记录审计日志 |
| [Security](../security/README.md) | Entitlements 提供声明式权限（需 OpenSSL） |
| [Workbench](../workbench/README.md) | 工作台执行前进行权限检查 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
