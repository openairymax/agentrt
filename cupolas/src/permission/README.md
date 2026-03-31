# Cupolas 权限裁决模块 (Permission)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-26  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`cupolas/src/permission/` 是 AgentOS 安全穹顶的**核心权限控制系统**，负责基于 YAML 规则的细粒度访问决策。作为系统的"守门人"，本模块确保所有操作都经过严格的权限验证。

### 核心职责

| 职责 | 说明 | 实现方式 |
|------|------|----------|
| **规则解析** | 加载和解析 YAML 权限规则 | libyaml + 自定义解析器 |
| **权限判定** | 实时判断操作是否被允许 | 规则引擎匹配 |
| **角色管理** | 基于角色的访问控制 (RBAC) | 角色 - 权限映射 |
| **审计记录** | 记录所有权限决策日志 | 异步队列写入 |
| **动态更新** | 支持规则热更新无需重启 | 文件监控 + 原子替换 |

### 在 Cupolas 中的位置

```
Cupolas 安全穹顶架构:
┌─────────────────────────────────────┐
│         Workbench (虚拟工位)         │ ← 隔离执行环境
├─────────────────────────────────────┤
│    Permission (权限裁决) ← 本模块    │ ← 访问控制决策
├─────────────────────────────────────┤
│    Sanitizer (输入净化)             │ ← 输入过滤
├─────────────────────────────────────┤
│      Audit (审计日志)               │ ← 行为追踪
└─────────────────────────────────────┘
```

---

## 📁 目录结构

```
permission/
├── README.md                 # 本文档
├── CMakeLists.txt            # 构建配置
│
├── include/                  # 公共头文件
│   ├── permission.h          # 权限接口
│   ├── rule_parser.h         # 规则解析器
│   ├── rbac_manager.h        # RBAC 管理器
│   └── audit_logger.h        # 审计日志
│
├── src/                      # 实现代码
│   ├── permission_core.c     # 权限核心逻辑
│   ├── rule_parser.c         # YAML 规则解析
│   ├── rbac_manager.c        # 角色权限管理
│   ├── audit_logger.c        # 审计记录
│   ├── matcher.c             # 规则匹配引擎
│   └── cache.c               # 权限缓存
│
├── rules/                    # 权限规则示例
│   ├── default_rules.yaml    # 默认规则
│   ├── admin_rules.yaml      # 管理员规则
│   └── custom_rules.yaml     # 自定义规则
│
└── tests/                    # 单元测试
    ├── test_permission.c
    ├── test_rbac.c
    └── test_matcher.c
```

---

## 🔧 核心功能详解

### 1. 规则解析器

#### YAML 规则格式

```yaml
# 基础规则结构
rules:
  - name: "read_data_access"
    description: "允许读取数据"
    enabled: true
    
    # 主体匹配（谁）
    subjects:
      - type: role
        value: analyst
      - type: agent
        value: agent-data-*
    
    # 动作匹配（做什么）
    actions:
      - read
      - query
    
    # 资源匹配（对什么）
    resources:
      - path: "/data/*"
        methods: [GET, POST]
      - path: "/reports/**"
        methods: [GET]
    
    # 条件限制（在什么条件下）
    conditions:
      time_range:
        start: "08:00"
        end: "20:00"
      ip_whitelist:
        - "192.168.1.0/24"
      max_requests_per_minute: 100
    
    # 决策结果
    decision: allow
    priority: 100

  - name: "admin_full_access"
    description: "管理员完全访问"
    subjects:
      - type: role
        value: admin
    actions: ["*"]  # 所有动作
    resources: ["*"]  # 所有资源
    decision: allow
    priority: 1000  # 最高优先级
```

#### 解析器 API

```c
#include <permission/rule_parser.h>

// 加载规则文件
rule_set_t* rules = rule_parser_load("/etc/agentos/permission/rules.yaml");

if (!rules) {
    LOG_ERROR("Failed to load rules");
    return -1;
}

// 获取规则数量
printf("Loaded %d rules\n", rules->count);

// 释放规则
rule_parser_free(rules);
```

### 2. 权限判定引擎

#### 判定流程

```c
#include <permission/permission.h>

// 构造权限请求
permission_request_t req = {
    .subject = {
        .type = SUBJECT_AGENT,
        .id = "agent-data-001"
    },
    .action = "read",
    .resource = "/data/users/profile",
    .context = {
        .ip_address = "192.168.1.100",
        .timestamp = time(NULL),
        .metadata = NULL
    }
};

// 执行权限判定
permission_result_t result = permission_check(&req, rules);

if (result.allowed) {
    printf("Access granted\n");
} else {
    printf("Access denied: %s\n", result.reason);
}
```

#### 判定算法

```
权限判定伪代码:

function check_permission(request, rules):
    // 1. 按优先级排序规则
    sorted_rules = sort_by_priority(rules)
    
    // 2. 依次匹配规则
    for rule in sorted_rules:
        if match_subject(request.subject, rule.subjects) and
           match_action(request.action, rule.actions) and
           match_resource(request.resource, rule.resources) and
           match_conditions(request.context, rule.conditions):
           
            // 3. 返回第一个匹配规则的决策
            return rule.decision
    
    // 4. 默认拒绝（零信任原则）
    return DENY
```

### 3. RBAC 角色管理

#### 角色定义

```yaml
# roles.yaml
roles:
  - name: admin
    description: "系统管理员"
    permissions:
      - "users:*"
      - "agents:*"
      - "config:*"
      - "logs:read"
    
  - name: developer
    description: "开发者"
    permissions:
      - "agents:create"
      - "agents:update"
      - "agents:delete"
      - "skills:*"
      - "logs:read"
    
  - name: analyst
    description: "数据分析师"
    permissions:
      - "data:read"
      - "reports:read"
      - "metrics:read"
    
  - name: viewer
    description: "只读访客"
    permissions:
      - "public:read"
```

#### 角色分配 API

```c
#include <permission/rbac_manager.h>

// 初始化 RBAC 管理器
rbac_manager_t* rbac = rbac_create();

// 加载角色定义
rbac_load_roles(rbac, "/etc/agentos/permission/roles.yaml");

// 为用户分配角色
rbac_assign_role(rbac, "user-001", "developer");
rbac_assign_role(rbac, "user-002", "analyst");

// 检查用户权限
bool has_perm = rbac_has_permission(
    rbac,
    "user-001",
    "agents:create",
    "/agents/my-agent"
);

if (has_perm) {
    printf("User can create agents\n");
}

// 清理
rbac_destroy(rbac);
```

### 4. 规则匹配引擎

#### 路径通配符匹配

| 模式 | 说明 | 示例 | 匹配结果 |
|------|------|------|---------|
| `/data/*` | 单层通配 | `/data/users` | ✅ 匹配 |
| | | `/data/users/profile` | ❌ 不匹配 |
| `/data/**` | 多层通配 | `/data/users` | ✅ 匹配 |
| | | `/data/users/profile` | ✅ 匹配 |
| `/api/v?` | 单字符通配 | `/api/v1` | ✅ 匹配 |
| | | `/api/v2` | ✅ 匹配 |
| `agent-*` | 前缀通配 | `agent-data-001` | ✅ 匹配 |
| | | `agent-tool-002` | ✅ 匹配 |
| `*.json` | 后缀通配 | `data.json` | ✅ 匹配 |
| | | `config.xml` | ❌ 不匹配 |

#### 匹配器实现

```c
#include <permission/matcher.h>

// 路径匹配
bool match = matcher_path_match("/data/*", "/data/users");
// true

match = matcher_path_match("/data/**", "/data/users/profile");
// true

// 正则表达式匹配
match = matcher_regex_match("^agent-.*$", "agent-data-001");
// true

// CIDR IP 匹配
match = matcher_ip_match("192.168.1.0/24", "192.168.1.100");
// true
```

### 5. 权限缓存

#### 缓存策略

```c
typedef struct {
    char* cache_key;      // 缓存键：subject:action:resource
    bool allowed;         // 决策结果
    int64_t ttl_sec;      // 生存时间（秒）
    time_t created_at;    // 创建时间
} cache_entry_t;

// LRU 缓存配置
cache_config_t config = {
    .max_entries = 10000,     // 最多 1 万条
    .default_ttl_sec = 300,   // 默认 5 分钟过期
    .eviction_policy = LRU    // 最近最少使用淘汰
};

// 创建缓存
permission_cache_t* cache = permission_cache_create(&config);

// 添加到缓存
permission_cache_set(cache, 
    "user-001:read:/data/users",
    true,
    300
);

// 查询缓存
bool result;
if (permission_cache_get(cache, "user-001:read:/data/users", &result)) {
    // 缓存命中，直接返回
    return result;
}

// 未命中，执行实际判定并缓存
```

---

## 🌐 API 接口

### RESTful API

#### 1. 权限检查

```http
POST /api/v1/permission/check
Content-Type: application/json

{
  "subject": {
    "type": "agent",
    "id": "agent-data-001"
  },
  "action": "read",
  "resource": "/data/users",
  "context": {
    "ip_address": "192.168.1.100",
    "timestamp": "2026-03-26T10:30:00Z"
  }
}

Response 200:
{
  "allowed": true,
  "matched_rule": "read_data_access",
  "decision": "allow",
  "audit_id": "audit-12345"
}
```

#### 2. 批量权限查询

```http
POST /api/v1/permission/check_batch
Content-Type: application/json

{
  "subject": {
    "type": "role",
    "id": "developer"
  },
  "requests": [
    {"action": "create", "resource": "/agents"},
    {"action": "delete", "resource": "/agents/test"},
    {"action": "read", "resource": "/logs"}
  ]
}

Response 200:
{
  "results": [
    {"action": "create", "resource": "/agents", "allowed": true},
    {"action": "delete", "resource": "/agents/test", "allowed": false},
    {"action": "read", "resource": "/logs", "allowed": true}
  ]
}
```

#### 3. 获取角色权限

```http
GET /api/v1/roles/{role_name}/permissions

Response 200:
{
  "role": "developer",
  "permissions": [
    "agents:create",
    "agents:update",
    "agents:delete",
    "skills:*",
    "logs:read"
  ],
  "inherited_from": []
}
```

#### 4. 更新规则

```http
PUT /api/v1/rules/{rule_name}
Content-Type: application/json

{
  "description": "Updated description",
  "actions": ["read", "write"],
  "decision": "allow"
}

Response 200:
{
  "status": "updated",
  "version": 2,
  "effective_at": "2026-03-26T10:35:00Z"
}
```

---

## ⚙️ 配置选项

### 服务配置 (config/permission.yaml)

```yaml
# 规则配置
rules:
  # 规则文件路径
  files:
    - /etc/agentos/permission/default_rules.yaml
    - /etc/agentos/permission/custom_rules.yaml
  
  # 自动重载
  auto_reload:
    enabled: true
    interval_sec: 60
  
  # 默认策略
  default_policy: deny  # deny (拒绝) 或 allow (允许)

# RBAC 配置
rbac:
  # 角色定义文件
  roles_file: /etc/agentos/permission/roles.yaml
  
  # 启用继承
  enable_inheritance: true
  
  # 最大角色层级深度
  max_depth: 5

# 缓存配置
cache:
  enabled: true
  backend: memory  # memory 或 redis
  
  memory:
    max_entries: 10000
    default_ttl_sec: 300
  
  redis:
    url: redis://localhost:6379
    key_prefix: "permission:"
    ttl_sec: 300

# 审计配置
audit:
  enabled: true
  
  # 记录级别
  log_level: info  # debug, info, warning, error
  
  # 记录内容
  log_request: true   # 记录请求详情
  log_decision: true  # 记录决策结果
  log_matched_rule: true  # 记录匹配的规则
  
  # 存储后端
  storage:
    type: file  # file, elasticsearch, kafka
    file_path: /var/agentos/logs/permission_audit.log
    max_size_mb: 100
    max_files: 10

# 性能优化
performance:
  # 并行匹配线程数
  parallel_threads: 4
  
  # 规则预编译
  precompile_rules: true
  
  # 批量查询大小
  batch_size: 100
```

---

## 🚀 使用指南

### 快速开始

#### 1. 准备规则文件

创建 `/etc/agentos/permission/rules.yaml`:

```yaml
rules:
  - name: "default_allow"
    description: "默认允许"
    subjects: ["*"]
    actions: ["read"]
    resources: ["/public/*"]
    decision: allow
    priority: 100
  
  - name: "admin_access"
    description: "管理员访问"
    subjects:
      - type: role
        value: admin
    actions: ["*"]
    resources: ["*"]
    decision: allow
    priority: 1000
```

#### 2. 初始化权限系统

```c
#include <permission.h>

int main() {
    // 初始化日志
    logger_init(NULL);
    
    // 加载规则
    rule_set_t* rules = rule_parser_load("/etc/agentos/permission/rules.yaml");
    if (!rules) {
        LOG_ERROR("Failed to load rules");
        return 1;
    }
    
    // 初始化 RBAC
    rbac_manager_t* rbac = rbac_create();
    rbac_load_roles(rbac, "/etc/agentos/permission/roles.yaml");
    
    // 初始化缓存
    permission_cache_t* cache = permission_cache_create(NULL);
    
    LOG_INFO("Permission system initialized");
    
    // ... 使用权限系统
    
    // 清理
    rule_parser_free(rules);
    rbac_destroy(rbac);
    permission_cache_destroy(cache);
    logger_shutdown();
    
    return 0;
}
```

#### 3. 执行权限检查

```c
// 示例：检查 Agent 是否有读取数据的权限
permission_request_t req = {
    .subject = {.type = SUBJECT_AGENT, .id = "agent-001"},
    .action = "read",
    .resource = "/data/reports/sales.pdf",
    .context = {
        .ip_address = "192.168.1.50",
        .timestamp = time(NULL)
    }
};

permission_result_t result = permission_check(&req, rules);

if (result.allowed) {
    // 执行操作
    perform_read_operation();
} else {
    // 拒绝访问
    LOG_ERROR("Access denied: %s", result.reason);
    send_error_response(403, "Forbidden");
}
```

### 高级用法

#### 动态规则更新

```c
// 监控规则文件变化
void on_rules_changed(const char* filepath) {
    LOG_INFO("Rules file changed: %s", filepath);
    
    // 重新加载规则
    rule_set_t* new_rules = rule_parser_load(filepath);
    if (new_rules) {
        // 原子替换旧规则
        rule_set_swap(&g_rules, new_rules);
        
        // 清空缓存（避免脏数据）
        permission_cache_clear(g_cache);
        
        LOG_INFO("Rules updated successfully");
    }
}

// 注册文件监听
fs_watch_register("/etc/agentos/permission/rules.yaml", on_rules_changed);
```

#### 自定义条件函数

```c
// 注册自定义条件匹配函数
matcher_register_condition("is_business_hours", 
    (condition_fn_t)[](context_t* ctx) {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        int hour = tm_info->tm_hour;
        
        // 工作时间：9:00-18:00
        return (hour >= 9 && hour < 18);
    }
);

// 在规则中使用
/*
conditions:
  custom:
    is_business_hours: true
*/
```

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 单次权限检查延迟 | < 50μs | 缓存命中 |
| 单次权限检查延迟 | < 500μs | 缓存未命中 |
| 规则加载时间 | < 10ms | 100 条规则 |
| 缓存命中率 | > 85% | 典型工作负载 |
| 并发检查吞吐 | 20,000+/s | 4 线程并行 |
| 规则匹配复杂度 | O(n) | n=规则数量 |

---

## 🔍 故障排查

### 问题 1: 权限检查总是失败

**症状**: 所有请求都被拒绝

**解决方案**:
```bash
# 1. 检查规则文件语法
yamllint /etc/agentos/permission/rules.yaml

# 2. 查看规则加载日志
tail -f /var/agentos/logs/permission.log | grep "loaded"

# 3. 验证默认策略
cat /etc/agentos/permission/rules.yaml | grep default_policy

# 4. 测试单条规则
curl -X POST http://localhost:9090/api/v1/permission/check \
  -H "Content-Type: application/json" \
  -d '{"subject":{"type":"role","id":"admin"},"action":"read","resource":"/test"}'
```

### 问题 2: 规则更新不生效

**症状**: 修改规则后权限判断未改变

**解决方案**:
```bash
# 1. 检查自动重载是否启用
grep auto_reload /etc/agentos/permission/config.yaml

# 2. 手动触发重载
curl -X POST http://localhost:9090/api/v1/rules/reload

# 3. 清空缓存
curl -X DELETE http://localhost:9090/api/v1/cache/clear

# 4. 查看规则版本
curl http://localhost:9090/api/v1/rules/version
```

### 问题 3: 性能下降严重

**症状**: 权限检查延迟显著增加

**解决方案**:
```bash
# 1. 检查缓存命中率
curl http://localhost:9090/api/v1/metrics/cache_hit_rate

# 2. 查看规则数量
curl http://localhost:9090/api/v1/rules/count

# 3. 优化建议
# - 如果缓存命中率低：增加 TTL 或缓存大小
# - 如果规则数量过多：合并相似规则
# - 如果并发量大：增加 parallel_threads

# 4. 调整配置
cat >> /etc/agentos/permission/config.yaml << EOF
cache:
  memory:
    max_entries: 20000  # 增大缓存
    default_ttl_sec: 600  # 延长 TTL

performance:
  parallel_threads: 8  # 增加线程数
EOF
```

---

## 🧪 测试

### 单元测试

```bash
cd cupolas/build
ctest -R permission --output-on-failure

# 运行特定测试
ctest -R test_permission_basic --verbose
ctest -R test_rbac_manager --verbose
```

### 集成测试

```python
import requests

def test_permission_chain():
    """测试权限链"""
    # 1. 测试允许情况
    response = requests.post(
        'http://localhost:9090/api/v1/permission/check',
        json={
            'subject': {'type': 'role', 'id': 'admin'},
            'action': 'delete',
            'resource': '/agents/test'
        }
    )
    assert response.status_code == 200
    assert response.json()['allowed'] is True
    
    # 2. 测试拒绝情况
    response = requests.post(
        'http://localhost:9090/api/v1/permission/check',
        json={
            'subject': {'type': 'role', 'id': 'viewer'},
            'action': 'delete',
            'resource': '/agents/test'
        }
    )
    assert response.json()['allowed'] is False

test_permission_chain()
print("✅ All permission tests passed!")
```

---

## 📄 许可证

Permission 模块采用 **Apache License 2.0** 开源协议。

详见项目根目录的 [LICENSE](../../LICENSE) 文件。

---

## 🔗 相关文档

- [Cupolas 总览](../README.md) - 安全穹顶架构
- [Workbench 虚拟工位](workbench/README.md) - 隔离执行环境
- [Sanitizer 输入净化](sanitizer/README.md) - 输入过滤
- [Audit 审计日志](audit/README.md) - 行为追踪
- [RBAC 最佳实践](../../paper/guides/security/rbac.md) - 角色权限设计

---

<div align="center">

**Permission - AgentOS 的智能权限守门人**

[返回顶部](#cupolas 权限裁决模块 -permission)

© 2026 SPHARX Ltd. All Rights Reserved.

</div>
