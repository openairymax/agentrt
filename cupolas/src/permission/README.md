# cupolas 权限裁决模块 (Permission Engine)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`permission/` 是 cupolas 安全沙箱的**访问控制核心**，基于规则引擎实现细粒度的权限管理。

核心功能：
- **规则引擎**: 基于 YAML 的动态权限规则解析
- **缓存机制**: LRU 缓存加速高频权限检查
- **热重载**: 支持规则动态更新，无需重启
- **通配符匹配**: 支持 glob 风格的路径和资源匹配

**重要**: 所有对敏感资源的访问都必须经过权限裁决模块的检查。

---

## 📁 目录结构

```
permission/
├── permission.h              # 统一头文件
├── permission_engine.c       # 规则引擎核心
├── permission_engine.h       # 引擎接口
├── permission_rule.c         # 规则解析和管理
├── permission_rule.h         # 规则数据结构
├── permission_cache.c        # LRU 缓存
└── permission_cache.h        # 缓存接口
```

---

## 🔧 核心功能详解

### 1. 规则引擎 (`permission_engine.c`)

解析和执行权限规则的决策引擎。

#### 架构设计

```
+-------------+      +-------------+      +-------------+
|  Rule File  | ---> |   Parser    | ---> | Rule Tree   |
|  (YAML)     |      |  (libyaml)  |      | (内存结构)  |
+-------------+      +-------------+      +-------------+
                              ↓
                    +-------------+
                    |   Matcher   |
                    | (通配符匹配) |
                    +-------------+
                              ↓
                    +-------------+
                    |   Decision  |
                    | Allow/Deny  |
                    +-------------+
```

#### 使用示例

```c
#include <permission.h>

// 初始化权限引擎
permission_engine_t* engine = permission_engine_create(
    "./manager/permission_rules.yaml"  // 规则文件路径
);

// 权限检查
permission_result_t result = permission_engine_check(
    engine,
    "agent-123",           // 主体 ID
    "file.read",           // 操作类型
    "/etc/passwd",         // 资源路径
    NULL                   // 上下文（可选）
);

if (result == PERMISSION_ALLOWED) {
    // 允许执行
    read_file("/etc/passwd");
} else if (result == PERMISSION_DENIED) {
    // 拒绝并记录审计日志
    audit_log_permission_denied("agent-123", 
        "file.read", "/etc/passwd");
}

// 销毁引擎
permission_engine_destroy(engine);
```

#### 规则文件格式

```yaml
# permission_rules.yaml
version: "1.0"

# 默认策略
default_policy: deny  # 或 allow

# 规则列表
rules:
  # 规则 1: 允许读取 /tmp 目录
  - rule_id: "read_tmp"
    subjects: ["agent-*"]  # 通配符匹配所有 agent
    actions: ["file.read", "file.list"]
    resources: ["/tmp/*", "/tmp/**"]  # * 单层，**多层
    effect: allow
    priority: 100
    
  # 规则 2: 禁止访问敏感文件
  - rule_id: "deny_sensitive"
    subjects: ["*"]
    actions: ["file.*"]
    resources: 
      - "/etc/passwd"
      - "/etc/shadow"
      - "/root/**"
    effect: deny
    priority: 200  # 优先级更高
    
  # 规则 3: 特定 Agent 的特权
  - rule_id: "admin_privilege"
    subjects: ["agent-admin"]
    actions: ["*"]  # 所有操作
    resources: ["*"]
    effect: allow
    priority: 300
```

### 2. 规则管理 (`permission_rule.c`)

规则的加载、解析和匹配逻辑。

#### 规则数据结构

```c
typedef struct {
    char* rule_id;           // 规则 ID
    char** subjects;         // 主体列表（支持通配符）
    size_t subject_count;
    char** actions;          // 操作列表
    size_t action_count;
    char** resources;        // 资源列表
    size_t resource_count;
    permission_effect_t effect;  // allow/deny
    int priority;            // 优先级（数字越大优先级越高）
} permission_rule_t;
```

#### 通配符匹配

```c
#include <permission_rule.h>

// 通配符匹配函数
bool match = permission_rule_match_pattern(
    "/tmp/file.txt",      // 待匹配路径
    "/tmp/*"              // 模式（* 匹配单层）
);
// ✅ true

bool match2 = permission_rule_match_pattern(
    "/tmp/subdir/file.txt",  // 待匹配路径
    "/tmp/**"                // 模式（** 匹配多层）
);
// ✅ true

bool match3 = permission_rule_match_pattern(
    "agent-123",         // 待匹配字符串
    "agent-*"            // 模式
);
// ✅ true
```

### 3. 缓存机制 (`permission_cache.c`)

LRU 缓存加速高频权限检查。

#### 缓存架构

```
+------------------+
|  Permission Cache| (LRU, 容量可配置)
+------------------+
| Key:             |
|  subject:action: |
|  resource        |
+------------------+
| Value:           |
|  ALLOW/DENY      |
|  TTL             |
+------------------+
```

#### 使用示例

```c
#include <permission_cache.h>

// 创建缓存
permission_cache_t* cache = permission_cache_create(
    1024,   // 最多 1024 条记录
    3600    // 默认 TTL 1 小时
);

// 查询缓存
permission_result_t cached;
if (permission_cache_get(cache, 
    "agent-123:file.read:/tmp/test.txt", &cached)) {
    // 缓存命中，直接返回结果
    return cached;
}

// 缓存未命中，执行规则引擎检查
permission_result_t result = permission_engine_check(...);

// 写入缓存
permission_cache_set(cache,
    "agent-123:file.read:/tmp/test.txt",
    result,
    1800  // 30 分钟 TTL
);
```

#### 性能提升

| 场景 | 无缓存 | 有缓存 | 提升倍数 |
|------|--------|--------|----------|
| 单次检查 | ~50μs | < 1μs | 50x |
| 缓存命中率 | - | 90%+ | - |
| 平均延迟 | 50μs | ~5μs | 10x |

---

## 🚀 快速开始

### 编译

```bash
cd AgentOS/cupolas
mkdir build && cd build
cmake ..
make permission  # 只编译 permission 模块
```

### 测试

```bash
cd build
ctest -R permission        # 运行 permission 测试
./test_permission_engine   # 直接运行测试
```

### 完整集成示例

```c
#include <cupolas.h>

int main() {
    // 1. 初始化 cupolas（包含 permission）
    domes_config_t manager = {
        .permission_enabled = true,
        .permission_rules_path = "./manager/permission_rules.yaml",
        .permission_cache_size = 2048,
        .permission_cache_ttl = 1800,
        .default_policy = PERMISSION_DENY
    };
    
    domes_t* cupolas = domes_init(&manager);
    
    // 2. 权限检查示例
    const char* resource = "/data/sensitive.txt";
    
    permission_result_t result = domes_permission_check(
        cupolas,
        "agent-data-processor",
        "file.read",
        resource
    );
    
    if (result == PERMISSION_ALLOWED) {
        // 允许读取
        FILE* f = fopen(resource, "r");
        // ... 处理文件 ...
        fclose(f);
        
        // 记录成功审计
        domes_audit(cupolas, AUDIT_LEVEL_INFO,
            "permission.allowed", "agent-data-processor",
            "file.read", resource);
            
    } else {
        // 拒绝访问
        LOG_WARNING("Permission denied for %s", resource);
        
        // 记录拒绝审计
        domes_audit(cupolas, AUDIT_LEVEL_WARNING,
            "permission.denied", "agent-data-processor",
            "file.read", resource);
    }
    
    // 3. 动态添加规则（运行时）
    permission_rule_t new_rule = {
        .rule_id = "custom_rule_1",
        .subjects = (char*[]){"agent-special"},
        .subject_count = 1,
        .actions = (char*[]){"file.write"},
        .action_count = 1,
        .resources = (char*[])={"/data/special/*"},
        .resource_count = 1,
        .effect = PERMISSION_ALLOW,
        .priority = 150
    };
    
    domes_permission_add_rule(cupolas, &new_rule);
    
    // 4. 清理
    domes_destroy(cupolas);
    return 0;
}
```

---

## 📊 性能基准

### 规则引擎性能

| 指标 | 数值 | 测试条件 |
|------|------|----------|
| 单次检查延迟 | ~50μs | 无缓存，100 条规则 |
| 规则加载时间 | < 10ms | 1000 条规则 |
| 热重载时间 | < 50ms | 全量规则更新 |
| 内存占用 | ~2MB | 1000 条规则 |

### 缓存性能

| 指标 | 数值 |
|------|------|
| 缓存命中率 | 90%~95% |
| 缓存查找延迟 | < 1μs |
| 缓存写入延迟 | < 500ns |
| 缓存淘汰率 | < 5%/min |

---

## 🛠️ 最佳实践

### 1. 规则设计原则

```yaml
# ✅ 推荐：具体规则优先，通用规则兜底
rules:
  # 具体规则（高优先级）
  - rule_id: "allow_admin"
    subjects: ["agent-admin"]
    resources: ["*"]
    effect: allow
    priority: 300
    
  # 通用规则（低优先级）
  - rule_id: "deny_default"
    subjects: ["*"]
    resources: ["*"]
    effect: deny
    priority: 1
```

### 2. 通配符使用技巧

```yaml
# ✅ 推荐：精确匹配优先
resources:
  - "/data/public/*.txt"     # 只允许 .txt 文件
  - "/data/private/**"       # 允许所有子目录
  
# ❌ 不推荐：过度宽泛
resources:
  - "*"                      # 匹配所有资源（危险）
  - "/**"                    # 匹配整个文件系统
```

### 3. 缓存调优

```c
// 根据实际场景调整缓存大小
size_t cache_size = calculate_cache_size(
    requests_per_second,    // QPS
    avg_check_latency,      // 平均检查延迟
    target_hit_rate         // 目标命中率
);

// 典型配置
cache_size = 2048;  // 2K 条目，覆盖大部分场景
```

### 4. 规则热重载

```c
// 监听配置文件变化
inotify_fd = inotify_init();
inotify_add_watch(inotify_fd, 
    "./manager/permission_rules.yaml",
    IN_MODIFY);

// 文件变化时重新加载
if (event_received) {
    permission_engine_reload(engine, 
        "./manager/permission_rules.yaml");
    LOG_INFO("Permission rules reloaded");
}
```

---

## 🔗 相关文档

- [cupolas 总览](../README.md)
- [审计日志模块](../audit/README.md)
- [输入净化模块](../sanitizer/README.md)
- [虚拟工位模块](../workbench/README.md)
- [安全策略配置](../../../manager/security/README.md)

---

## 🤝 贡献指南

欢迎提交 Issue 或 Pull Request！

## 📞 联系方式

- **维护者**: AgentOS 安全委员会
- **安全问题**: wangliren@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"Permission is everything. 权限即一切。"*
