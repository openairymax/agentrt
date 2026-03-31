# 统一日志模块

**版本**: v1.0.0.6  
**最后更新**: 2026-03-26  
**许可证**: Apache License 2.0

---

## 🎯 概述

统一日志模块是AgentOS项目的核心基础设施组件，采用分层架构设计，提供高性能、线程安全的日志记录功能。本模块旨在消除项目中的日志代码重复，提供统一的日志接口，支持渐进式迁移。

## 🏗️ 架构设计

### 三层架构

1. **核心层 (Core Layer)**
   - 位置：`include/logging.h`, `src/logging.c`
   - 功能：定义日志级别、基础API、追踪ID管理
   - 特点：平台无关、线程安全、轻量级

2. **原子层 (Atomic Layer)**
   - 位置：`include/atomic_logging.h`, `src/atomic_logging.c`
   - 功能：无锁队列、线程本地缓冲、CAS操作
   - 特点：高性能、低延迟、多线程优化

3. **服务层 (Service Layer)**
   - 位置：`include/service_logging.h`, `src/service_logging.c`
   - 功能：日志轮转、过滤、传输、监控
   - 特点：可扩展、可配置、高级功能

### 向后兼容层

- 位置：`include/logging_compat.h`, `src/logging_compat.c`
- 功能：为现有代码提供平滑迁移路径
- 特点：API兼容、零修改迁移

## 🔧 主要功能

### 日志级别
- `LOG_LEVEL_TRACE` - 追踪级别（最详细）
- `LOG_LEVEL_DEBUG` - 调试级别
- `LOG_LEVEL_INFO` - 信息级别（默认）
- `LOG_LEVEL_WARN` - 警告级别
- `LOG_LEVEL_ERROR` - 错误级别
- `LOG_LEVEL_FATAL` - 致命级别

### 核心API

```c
// 初始化日志系统
int log_init(const log_config_t* manager);

// 写入日志记录
void log_write(log_level_t level, const char* module, int line, 
               const char* fmt, ...);

// 设置日志级别过滤
void log_set_level(log_level_t level);

// 设置日志输出目标
void log_add_outputter(log_outputter_t* outputter);

// 获取当前追踪ID
const char* log_get_trace_id(void);

// 设置追踪ID
void log_set_trace_id(const char* trace_id);
```

### 原子层API

```c
// 提交日志记录到无锁队列
int atomic_logging_submit(const log_record_t* record);

// 处理队列中的日志记录
int atomic_logging_process_batch(size_t max_count);

// 获取原子层统计信息
const atomic_logging_stats_t* atomic_logging_get_stats(void);
```

### 服务层API

```c
// 注册日志过滤器
int service_logging_register_filter(log_filter_t* filter);

// 注册日志输出器
int service_logging_register_outputter(log_outputter_t* outputter);

// 配置日志轮转
int service_logging_config_rotation(const log_rotation_config_t* manager);

// 获取服务层监控数据
const service_logging_monitor_t* service_logging_get_monitor(void);
```

## 使用示例

### 基础使用

```c
#include "logging.h"

int main(void) {
    // 初始化日志系统（使用默认配置）
    log_init(NULL);
    
    // 写入不同级别的日志
    log_write(LOG_LEVEL_INFO, "main", __LINE__, "应用程序启动");
    log_write(LOG_LEVEL_DEBUG, "network", __LINE__, "连接到服务器: %s", "api.example.com");
    log_write(LOG_LEVEL_ERROR, "database", __LINE__, "数据库连接失败: %s", strerror(errno));
    
    // 设置追踪ID
    log_set_trace_id("req-123456");
    
    // 输出带追踪ID的日志
    log_write(LOG_LEVEL_INFO, "request", __LINE__, 
              "处理用户请求: user_id=%d, action=%s", 123, "login");
    
    return 0;
}
```

### 高级配置

```c
#include "logging.h"
#include "service_logging.h"

// 自定义输出器：输出到syslog
static int syslog_outputter(const log_record_t* record, void* user_data) {
    // 实现syslog输出逻辑
    return 0;
}

// 自定义过滤器：过滤特定模块的DEBUG日志
static int debug_filter(const log_record_t* record, void* user_data) {
    if (record->level == LOG_LEVEL_DEBUG && 
        strcmp(record->module, "network") == 0) {
        return 0; // 过滤掉network模块的DEBUG日志
    }
    return 1; // 允许其他日志
}

void configure_logging(void) {
    // 创建配置
    log_config_t manager = {
        .default_level = LOG_LEVEL_INFO,
        .enable_color = true,
        .enable_timestamp = true,
        .enable_thread_id = true,
        .buffer_size = 8192,
        .flush_interval_ms = 1000
    };
    
    // 初始化
    log_init(&manager);
    
    // 注册过滤器
    log_filter_t filter = {
        .filter_fn = debug_filter,
        .user_data = NULL
    };
    service_logging_register_filter(&filter);
    
    // 注册输出器
    log_outputter_t outputter = {
        .output_fn = syslog_outputter,
        .user_data = NULL
    };
    service_logging_register_outputter(&outputter);
}
```

## 📦 迁移指南

### 从旧日志系统迁移

现有代码通常使用`atoms/utils/observability/logger.c`中的函数。统一日志模块提供了完整的向后兼容层。

#### 步骤1：包含兼容头文件

```c
// 原代码
#include "../../../commons/utils/observability/logger.h"

// 新代码（兼容）
#include "logging_compat.h"
// 或保持原包含路径不变（通过构建系统重定向）
```

#### 步骤2：修改构建配置

更新CMakeLists.txt，添加统一日志模块源文件：

```cmake
# 添加统一日志模块
set(LOGGING_SOURCES
    commons/utils/logging/src/logging.c
    commons/utils/logging/src/atomic_logging.c
    commons/utils/logging/src/service_logging.c
    commons/utils/logging/src/logging_compat.c
)

# 添加包含目录
include_directories(
    commons/utils/logging/include
)
```

#### 步骤3：渐进式迁移

1. **第一阶段**：使用兼容层，现有代码无需修改
2. **第二阶段**：逐步将新代码使用新API
3. **第三阶段**：迁移旧代码到新API（可选）

### API映射表

| 旧API | 新API | 备注 |
|-------|-------|------|
| `log_message(level, module, fmt, ...)` | `log_write(level, module, line, fmt, ...)` | 添加了line参数 |
| `log_set_level(level)` | `log_set_level(level)` | 完全兼容 |
| `log_get_level()` | `log_get_level()` | 完全兼容 |
| `log_set_output_callback(cb)` | `log_add_outputter(outputter)` | 新API更灵活 |

## ⚡ 性能特性

### 原子层优化
- **无锁队列**：多线程写入无锁竞争
- **线程本地缓冲**：减少线程间同步
- **批量处理**：提高吞吐量
- **内存池**：减少内存分配开销

### 性能指标
- 单线程吞吐量：>100k 记录/秒
- 多线程扩展性：接近线性扩展
- 平均延迟：<10微秒/记录
- 内存使用：固定大小缓冲，无内存泄漏

### 基准测试

运行性能基准测试：

```bash
cd commons/utils/logging
./bench_atomic_logging 1000000 4
```

输出示例：
```
单线程性能: 125,000 记录/秒
多线程性能（4线程）: 480,000 记录/秒
并发加速比: 3.84x
```

## ⚙️ 配置选项

### 日志级别过滤
支持运行时动态调整日志级别，支持按模块过滤。

### 输出目标
- 标准输出（控制台）
- 文件（支持轮转）
- 系统日志（syslog）
- 网络传输（TCP/UDP）
- 自定义输出器

### 格式定制
- 时间戳格式
- 日志级别显示
- 模块名称
- 线程ID
- 追踪ID
- 自定义字段

## 📊 监控和诊断

### 内置统计
- 日志记录总数
- 按级别计数
- 吞吐量统计
- 队列使用情况
- 错误统计

### 健康检查
```c
// 获取日志系统健康状态
log_health_t health = log_get_health_status();
if (health.status != LOG_HEALTH_OK) {
    // 处理异常情况
}
```

## 📝 最佳实践

### 1. 合理使用日志级别
- TRACE：详细的调试信息，生产环境关闭
- DEBUG：开发调试信息
- INFO：重要的运行时信息
- WARN：潜在问题
- ERROR：错误情况，需要关注
- FATAL：致命错误，程序可能终止

### 2. 模块化日志
为每个模块使用不同的模块名称，便于过滤和分析。

### 3. 避免日志性能问题
- 避免在热路径中进行复杂字符串格式化
- 使用条件判断避免不必要的日志调用
- 合理设置日志级别，减少生产环境日志量

### 4. 追踪ID传递
在分布式系统中传递追踪ID，便于问题排查。

## 🔌 扩展开发

### 自定义输出器
实现`log_outputter_t`接口，支持自定义输出目标。

### 自定义过滤器
实现`log_filter_t`接口，支持自定义过滤逻辑。

### 自定义格式器
实现`log_formatter_t`接口，支持自定义日志格式。

## ❓ 故障排除

### 常见问题

1. **日志不输出**
   - 检查日志级别设置
   - 检查输出器配置
   - 检查初始化状态

2. **性能问题**
   - 检查原子层队列大小
   - 检查批量处理设置
   - 检查内存分配情况

3. **内存泄漏**
   - 使用内存检测工具
   - 检查输出器资源释放
   - 检查过滤器资源管理

### 调试模式
启用调试模式获取详细内部状态：

```c
log_enable_debug(true);
```

## 版本历史

### v1.0.0 (2026-03-26)
- 初始版本发布
- 三层架构实现
- 向后兼容支持
- 性能基准测试框架

### v1.1.0 (计划中)
- 异步日志支持
- 结构化日志（JSON格式）
- 分布式追踪集成
- 配置热更新

## 贡献指南

欢迎贡献代码、文档或提出建议。请遵循项目编码规范，确保代码质量。

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- **官方仓库**: https://gitee.com/spharx/agentos

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"统一日志，追踪每一行代码的足迹。"*