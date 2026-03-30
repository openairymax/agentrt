﻿# cupolas 审计日志模块 (Audit Logger)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`audit/` 是 cupolas 安全沙箱的**审计日志系统**，负责记录所有安全相关的事件和操作。

核心功能：
- **异步队列写入**: 高性能日志记录，不阻塞主流程
- **日志轮转**: 自动归档旧日志，支持按大小/时间切割
- **结构化格式**: JSON 格式，便于查询和分析
- **风险追踪**: 记录权限裁决、输入净化等安全事件

**重要**: 所有审计日志都是不可篡改的，用于事后追溯和合规审计。

---

## 📁 目录结构

```
audit/
├── audit.h                     # 统一头文件
├── audit_logger.c              # 日志写入器
├── audit_queue.c               # 异步队列
├── audit_queue.h               # 队列接口
├── audit_rotator.c             # 日志轮转器
└── audit_rotator.h             # 轮转器接口
```

---

## 🔧 核心功能详解

### 1. 异步队列 (`audit_queue.c`)

使用无锁队列实现高性能的日志缓冲。

#### 架构设计

```
+-------------+      +-------------+      +-------------+
|  Producers  | ---> |   Queue     | ---> |   Consumer  |
|  (多线程)    |      | (无锁环形)  |      | (写文件)    |
+-------------+      +-------------+      +-------------+
```

#### 使用示例

```c
#include <audit.h>

// 初始化审计队列
audit_queue_t* queue = audit_queue_create(
    1024,           // 队列容量（最多 1024 条）
    "./audit.log",  // 日志文件路径
    NULL            // 配置（NULL=默认）
);

// 记录审计事件（线程安全，非阻塞）
audit_event_t event = {
    .timestamp = get_current_time(),
    .level = AUDIT_LEVEL_WARNING,
    .event_type = "permission.denied",
    .subject = "agent-123",
    .action = "file.write",
    .resource = "/etc/passwd",
    .result = "denied",
    .details = "{\"reason\": \"insufficient_permission\"}"
};

audit_queue_push(queue, &event);

// 后台线程会自动消费队列并写入文件

// 销毁队列
audit_queue_destroy(queue);
```

#### 性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| 队列容量 | 1024~65536 | 可配置 |
| 入队延迟 | < 1μs | 无锁实现 |
| 出队吞吐 | > 100,000/s | 单消费者 |
| 内存占用 | ~8MB | 1024 条日志 |

### 2. 日志写入器 (`audit_logger.c`)

负责将队列中的事件持久化到文件。

#### 使用示例

```c
#include <audit.h>

// 创建日志写入器
audit_logger_t* logger = audit_logger_create(
    "./logs/audit",        // 日志目录
    "audit",               // 日志文件名前缀
    AUDIT_FORMAT_JSON      // JSON 格式
);

// 设置轮转策略
audit_logger_set_rotation(
    logger,
    AUDIT_ROTATE_SIZE,     // 按大小轮转
    100 * 1024 * 1024,     // 100MB
    7                      // 保留 7 个历史文件
);

// 启动写入线程
audit_logger_start(logger, queue);

// ... 运行中 ...

// 停止写入
audit_logger_stop(logger);
audit_logger_destroy(logger);
```

#### 日志格式

```json
{
  "timestamp": "2026-03-25T10:30:45.123Z",
  "level": "WARNING",
  "event_type": "permission.denied",
  "trace_id": "trace-abc123",
  "subject": {
    "type": "agent",
    "id": "agent-123",
    "name": "DataProcessor"
  },
  "action": "file.write",
  "resource": "/etc/passwd",
  "result": "denied",
  "reason": "insufficient_permission",
  "metadata": {
    "ip_address": "192.168.1.100",
    "user_agent": "AgentOS/1.0"
  }
}
```

### 3. 日志轮转器 (`audit_rotator.c`)

自动管理日志文件的生命周期。

#### 轮转策略

```c
#include <audit.h>

// === 按大小轮转 ===
audit_rotator_config_t size_cfg = {
    .rotation_type = AUDIT_ROTATE_SIZE,
    .max_size_mb = 100,    // 单个文件最大 100MB
    .max_files = 10        // 保留 10 个文件
};

// === 按时间轮转 ===
audit_rotator_config_t time_cfg = {
    .rotation_type = AUDIT_ROTATE_TIME,
    .interval = AUDIT_ROTATE_DAILY,  // 每天轮转
    .max_days = 30                   // 保留 30 天
};

// === 混合轮转 ===
audit_rotator_config_t hybrid_cfg = {
    .rotation_type = AUDIT_ROTATE_HYBRID,
    .max_size_mb = 500,    // 或达到 500MB
    .interval = AUDIT_ROTATE_DAILY,  // 或每天
    .max_files = 15
};

audit_rotator_t* rotator = audit_rotator_create(&hybrid_cfg);
```

#### 文件命名规则

```
audit.log              # 当前日志文件
audit.log.1            # 最近轮转的文件
audit.log.2            # 次近的文件
...
audit.log.10           # 最老的文件

# 或带时间戳
audit-2026-03-25.log
audit-2026-03-24.log
```

---

## 🚀 快速开始

### 编译

```bash
cd AgentOS/cupolas
mkdir build && cd build
cmake ..
make audit  # 只编译 audit 模块
```

### 测试

```bash
cd build
ctest -R audit           # 运行 audit 测试
./test_audit_logger      # 直接运行测试程序
```

### 集成示例

完整的审计系统集成示例：

```c
#include <cupolas.h>

int main() {
    // 1. 初始化 cupolas
    domes_config_t manager = {
        .audit_enabled = true,
        .audit_log_path = "/var/log/agentos/audit",
        .audit_queue_size = 4096,
        .audit_rotation_type = AUDIT_ROTATE_HYBRID,
        .audit_max_size_mb = 200,
        .audit_max_files = 20
    };
    
    domes_t* cupolas = domes_init(&manager);
    
    // 2. 记录权限裁决事件
    domes_audit_permission(
        cupolas,
        "agent-123",
        "file.read",
        "/etc/passwd",
        PERMISSION_DENIED
    );
    
    // 3. 记录输入净化事件
    domes_audit_sanitizer(
        cupolas,
        "agent-456",
        "user_input",
        "<script>alert('xss')</script>",
        SANITIZER_BLOCKED
    );
    
    // 4. 清理
    domes_destroy(cupolas);
    return 0;
}
```

---

## 📊 性能基准

### 写入性能

| 场景 | 吞吐量 | 延迟 (P99) |
|------|--------|-----------|
| 同步写入 | 5,000/s | ~5ms |
| 异步队列 | > 100,000/s | < 100μs |
| 批量写入 | > 200,000/s | < 50μs |

### 磁盘占用

| 日志级别 | 单条大小 | 日均量 | 月存储 |
|----------|----------|--------|--------|
| INFO | ~500B | 100 万条 | ~50GB |
| WARNING | ~600B | 10 万条 | ~6GB |
| ERROR | ~700B | 1 万条 | ~0.7GB |

---

## 🛠️ 最佳实践

### 1. 日志分级

```c
// ✅ 推荐：根据事件重要性选择级别
AUDIT_LEVEL_INFO     // 常规操作（成功授权）
AUDIT_LEVEL_WARNING  // 异常事件（权限拒绝）
AUDIT_LEVEL_ERROR    // 错误事件（系统异常）
AUDIT_LEVEL_CRITICAL // 严重事件（安全攻击）
```

### 2. 敏感信息处理

```c
// ✅ 推荐：脱敏处理
audit_event_add_field(&event, "user_id", 
    hash_user_id(user_id));  // 哈希处理

// ❌ 不推荐：明文记录
audit_event_add_field(&event, "password", password);
```

### 3. 查询优化

```bash
# 按时间范围查询
grep "2026-03-25" audit.log.* | jq '.'

# 按事件类型查询
jq 'select(.event_type == "permission.denied")' audit.log

# 按风险等级统计
jq -r '.level' audit.log | sort | uniq -c
```

---

## 🔗 相关文档

- [cupolas 总览](../README.md)
- [权限裁决模块](../permission/README.md)
- [输入净化模块](../sanitizer/README.md)
- [虚拟工位模块](../workbench/README.md)

---

## 🤝 贡献指南

欢迎提交 Issue 或 Pull Request！

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- **官方仓库**: https://gitee.com/spharx/agentos

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"安全无小事，审计留痕迹。"*

- **维护者**: AgentOS 安全委员会
- **安全问题**: wangliren@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"Audit everything, trust nothing. 审计一切，零信任。"*
