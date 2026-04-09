# 统一配置模块

## 概述

统一配置模块是AgentOS项目的核心基础设施组件，采用分层架构设计，提供灵活、类型安全、高性能的配置管理功能。本模块旨在消除项目中的配置代码重复，提供统一的配置接口，支持多种配置源和高级功能，同时保持向后兼容性。

## 架构设计

### 三层架构

1. **核心层 (Core Layer)**
   - 位置：`include/core_config.h`, `src/core_config.c`
   - 功能：统一的配置数据模型、类型安全接口、配置上下文管理
   - 特点：平台无关、线程安全、内存高效

2. **源适配层 (Source Adapter Layer)**
   - 位置：`include/config_source.h`, `src/config_source.c`
   - 功能：多种配置源的统一适配（文件、环境变量、命令行参数、内存等）
   - 特点：可扩展、统一接口、支持热加载

3. **服务层 (Service Layer)**
   - 位置：`include/config_service.h`, `src/config_service.c`
   - 功能：高级配置功能（验证、热更新、加密、版本管理、模板展开等）
   - 特点：生产级功能、安全可靠、易于监控

### 向后兼容层

- 位置：`include/config_compat.h`, `src/config_compat.c`
- 功能：为现有代码提供平滑迁移路径，兼容`agentos_config_*` API
- 特点：API兼容、零修改迁移、渐进式替换

## 主要功能

### 配置数据类型
- **字符串类型** (CONFIG_TYPE_STRING) - UTF-8字符串
- **整数类型** (CONFIG_TYPE_INT) - 64位有符号整数
- **浮点类型** (CONFIG_TYPE_DOUBLE) - 双精度浮点数
- **布尔类型** (CONFIG_TYPE_BOOL) - 布尔值
- **数组类型** (CONFIG_TYPE_ARRAY) - 配置值数组
- **对象类型** (CONFIG_TYPE_OBJECT) - 嵌套配置对象
- **空类型** (CONFIG_TYPE_NULL) - 空值

### 核心API

```c
// 创建配置上下文
config_context_t* config_context_create(const char* app_name);

// 创建配置值
config_value_t* config_value_create_string(const char* value);
config_value_t* config_value_create_int(int64_t value);
config_value_t* config_value_create_double(double value);
config_value_t* config_value_create_bool(bool value);
config_value_t* config_value_create_array(size_t capacity);
config_value_t* config_value_create_object(size_t capacity);

// 配置操作
int config_context_set(config_context_t* ctx, const char* key, config_value_t* value);
const config_value_t* config_context_get(const config_context_t* ctx, const char* key);
int config_context_delete(config_context_t* ctx, const char* key);

// 类型安全访问器
const char* config_value_as_string(const config_value_t* value, const char* default_value);
int64_t config_value_as_int(const config_value_t* value, int64_t default_value);
double config_value_as_double(const config_value_t* value, double default_value);
bool config_value_as_bool(const config_value_t* value, bool default_value);

// 资源管理
void config_value_destroy(config_value_t* value);
void config_context_destroy(config_context_t* ctx);
```

### 源适配层API

```c
// 配置源类型
typedef enum {
    CONFIG_SOURCE_FILE,      // 文件配置源
    CONFIG_SOURCE_ENV,       // 环境变量配置源
    CONFIG_SOURCE_CLI,       // 命令行参数配置源
    CONFIG_SOURCE_MEMORY,    // 内存配置源
    CONFIG_SOURCE_DEFAULT    // 默认值配置源
} config_source_type_t;

// 创建配置源
config_source_t* config_source_create_file(const config_file_source_options_t* options);
config_source_t* config_source_create_env(const config_env_source_options_t* options);
config_source_t* config_source_create_cli(const config_cli_source_options_t* options);
config_source_t* config_source_create_memory(const config_memory_source_options_t* options);
config_source_t* config_source_create_default(void);

// 加载配置
int config_source_load(config_source_t* source, config_context_t* ctx);
int config_source_watch(config_source_t* source, config_source_callback_t callback, void* user_data);

// 资源管理
void config_source_destroy(config_source_t* source);
```

### 服务层API

```c
// 配置验证
config_validator_t* config_validator_create(const validator_options_t* options);
int config_validator_validate(config_validator_t* validator, config_context_t* ctx, validation_report_t* report);

// 热更新管理
config_hot_reload_manager_t* config_hot_reload_manager_create(config_context_t* ctx, const hot_reload_options_t* options);
int config_hot_reload_start(config_hot_reload_manager_t* manager, uint32_t check_interval_ms);
int config_hot_reload_stop(config_hot_reload_manager_t* manager);

// 配置加密
int config_encrypt_sensitive_fields(config_context_t* ctx, const char** sensitive_keys, size_t key_count);
int config_decrypt_sensitive_fields(config_context_t* ctx, const char** sensitive_keys, size_t key_count);

// 版本管理
int config_version_snapshot(config_context_t* ctx, const char* snapshot_name);
int config_version_rollback(config_context_t* ctx, const char* snapshot_name);

// 模板展开
int config_expand_templates(config_context_t* ctx, const template_expansion_options_t* options);
```

### 兼容层API

```c
// 向后兼容现有agentos_config_* API
agentos_config_t* agentos_config_create(void);
void agentos_config_destroy(agentos_config_t* manager);
const char* agentos_config_get_string(agentos_config_t* manager, const char* key, const char* default_value);
int agentos_config_get_int(agentos_config_t* manager, const char* key, int default_value);
bool agentos_config_get_bool(agentos_config_t* manager, const char* key, bool default_value);
double agentos_config_get_double(agentos_config_t* manager, const char* key, double default_value);
int agentos_config_set_string(agentos_config_t* manager, const char* key, const char* value);
int agentos_config_set_int(agentos_config_t* manager, const char* key, int value);
int agentos_config_set_bool(agentos_config_t* manager, const char* key, bool value);
int agentos_config_set_double(agentos_config_t* manager, const char* key, double value);
int agentos_config_load_file(agentos_config_t* manager, const char* file_path);
```

### 简化宏

```c
// 创建配置值的简化宏
CONFIG_STRING("value")       // 创建字符串配置值
CONFIG_INT(42)               // 创建整数配置值
CONFIG_DOUBLE(3.14)          // 创建浮点数配置值
CONFIG_BOOL(true)            // 创建布尔配置值

// 安全操作宏
CONFIG_SET_SAFE(ctx, "key", value)  // 安全设置配置值（自动销毁旧值）
CONFIG_GET_STRING_SAFE(ctx, "key", "default")  // 安全获取字符串配置值
```

## 使用示例

### 基础使用

```c
#include "config_unified.h"

int main() {
    // 创建配置上下文
    config_context_t* ctx = config_context_create("myapp");
    
    // 设置配置值
    config_context_set(ctx, "database.host", CONFIG_STRING("localhost"));
    config_context_set(ctx, "database.port", CONFIG_INT(5432));
    config_context_set(ctx, "database.enabled", CONFIG_BOOL(true));
    
    // 获取配置值
    const char* host = config_value_as_string(config_context_get(ctx, "database.host"), "127.0.0.1");
    int64_t port = config_value_as_int(config_context_get(ctx, "database.port"), 3306);
    bool enabled = config_value_as_bool(config_context_get(ctx, "database.enabled"), false);
    
    printf("Host: %s, Port: %lld, Enabled: %s\n", host, port, enabled ? "true" : "false");
    
    // 清理资源
    config_context_destroy(ctx);
    return 0;
}
```

### 使用文件配置源

```c
#include "config_unified.h"

int main() {
    config_context_t* ctx = config_context_create("myapp");
    
    // 创建文件配置源
    config_file_source_options_t file_opts = {
        .file_path = "manager.yaml",
        .format = "yaml",
        .watch_for_changes = true
    };
    config_source_t* file_source = config_source_create_file(&file_opts);
    
    // 加载配置
    config_source_load(file_source, ctx);
    
    // 使用配置值
    const char* log_level = config_value_as_string(
        config_context_get(ctx, "logging.level"), "info");
    
    // 清理资源
    config_source_destroy(file_source);
    config_context_destroy(ctx);
    return 0;
}
```

### 使用配置验证

```c
#include "config_unified.h"

int main() {
    config_context_t* ctx = config_context_create("myapp");
    
    // 加载配置...
    
    // 创建验证器
    validator_options_t validator_opts = {
        .type = VALIDATOR_TYPE_JSON_SCHEMA,
        .schema_path = "config_schema.json"
    };
    config_validator_t* validator = config_validator_create(&validator_opts);
    
    // 验证配置
    validation_report_t report;
    int result = config_validator_validate(validator, ctx, &report);
    
    if (result == 0) {
        printf("配置验证通过\n");
    } else {
        printf("配置验证失败: %s\n", report.error_message);
    }
    
    // 清理资源
    config_validator_destroy(validator);
    config_context_destroy(ctx);
    return 0;
}
```

### 使用热更新功能

```c
#include "config_unified.h"

// 配置变化回调函数
void on_config_changed(config_context_t* ctx, const char* changed_key, void* user_data) {
    printf("配置已更新: %s\n", changed_key);
    
    // 重新读取相关配置
    const char* new_value = config_value_as_string(
        config_context_get(ctx, changed_key), NULL);
    printf("新值: %s\n", new_value ? new_value : "(null)");
}

int main() {
    config_context_t* ctx = config_context_create("myapp");
    
    // 创建热更新管理器
    hot_reload_options_t hot_reload_opts = {
        .callback = on_config_changed,
        .user_data = NULL,
        .debounce_ms = 1000
    };
    config_hot_reload_manager_t* hot_reload = 
        config_hot_reload_manager_create(ctx, &hot_reload_opts);
    
    // 启动热更新监控（每5秒检查一次）
    config_hot_reload_start(hot_reload, 5000);
    
    printf("热更新已启动，按Enter键停止...\n");
    getchar();
    
    // 停止热更新
    config_hot_reload_stop(hot_reload);
    config_hot_reload_manager_destroy(hot_reload);
    config_context_destroy(ctx);
    return 0;
}
```

## 迁移指南

### 从现有配置模块迁移

1. **渐进式迁移策略**
   - 新代码使用统一配置模块API
   - 现有代码继续使用兼容层API (`agentos_config_*`)
   - 逐步将模块迁移到新API

2. **第一步：包含头文件**
   ```c
   // 新代码使用
   #include "config_unified.h"
   
   // 现有代码保持
   #include "manager.h"  // 自动重定向到兼容层
   ```

3. **第二步：替换API调用**
   ```c
   // 旧代码
   agentos_config_t* manager = agentos_config_create();
   const char* value = agentos_config_get_string(manager, "key", "default");
   
   // 新代码
   config_context_t* ctx = config_context_create("appname");
   const char* value = config_value_as_string(
       config_context_get(ctx, "key"), "default");
   ```

4. **第三步：利用高级功能**
   - 添加配置验证
   - 启用热更新
   - 使用多种配置源

5. **第四步：移除兼容层依赖**
   - 当所有模块都迁移完成后
   - 移除对`agentos_config_*` API的调用
   - 删除旧的配置模块实现

### 向后兼容性保证

统一配置模块的兼容层保证：
1. `agentos_config_*` API 100%兼容
2. 现有代码无需修改即可编译通过
3. 运行时行为保持一致
4. 内存管理语义相同

## 性能特性

### 内存效率
- **对象池管理**: 配置值和上下文使用对象池减少内存碎片
- **字符串内部化**: 相同字符串值共享内存
- **惰性加载**: 配置源支持惰性加载，减少启动时间

### 访问性能
- **哈希索引**: 配置键使用高效哈希表，O(1)访问时间
- **缓存友好**: 数据结构设计考虑CPU缓存局部性
- **线程安全**: 读写操作使用细粒度锁，最小化竞争

### 扩展性
- **插件化架构**: 可轻松添加新的配置源类型
- **水平扩展**: 支持分布式配置管理
- **批量操作**: 提供批量加载和更新接口

## 配置选项

### 构建时配置
通过CMake选项控制功能：

```cmake
# 启用/禁用功能
option(CONFIG_ENABLE_VALIDATION "启用配置验证" ON)
option(CONFIG_ENABLE_ENCRYPTION "启用配置加密" OFF)
option(CONFIG_ENABLE_HOT_RELOAD "启用热更新" ON)

# 选择支持的配置格式
option(CONFIG_SUPPORT_YAML "支持YAML格式" ON)
option(CONFIG_SUPPORT_JSON "支持JSON格式" ON)
option(CONFIG_SUPPORT_TOML "支持TOML格式" OFF)
```

### 运行时配置
通过环境变量控制行为：

```bash
# 配置调试模式
export CONFIG_DEBUG=1

# 设置默认配置路径
export CONFIG_DEFAULT_PATH=/etc/myapp/manager.yaml

# 启用详细日志
export CONFIG_LOG_LEVEL=debug
```

## 监控和诊断

### 内置监控
- **配置访问统计**: 记录每个配置键的访问次数
- **源加载性能**: 监控配置源加载时间和成功率
- **内存使用情况**: 跟踪配置对象的内存占用

### 健康检查
```c
// 获取配置系统健康状态
config_health_status_t status;
config_get_health_status(&status);

if (status.overall == HEALTH_STATUS_OK) {
    printf("配置系统健康\n");
} else {
    printf("配置系统问题: %s\n", status.error_message);
}
```

### 日志集成
配置模块与统一日志系统集成，提供结构化日志：
  - 配置变更日志
  - 验证错误日志
  - 性能警告日志

## 最佳实践

### 配置组织
1. **分层结构**: 使用点分隔键名创建层次结构
   ```
   database.host
   database.port
   logging.level
   logging.format
   ```

2. **环境分离**: 为不同环境使用不同配置源
   ```c
   #ifdef PRODUCTION
   config_source_t* source = config_source_create_file(&prod_opts);
   #else
   config_source_t* source = config_source_create_file(&dev_opts);
   #endif
   ```

3. **敏感数据保护**: 使用加密功能保护敏感配置
   ```c
   const char* sensitive_keys[] = {"database.password", "api.key"};
   config_encrypt_sensitive_fields(ctx, sensitive_keys, 2);
   ```

### 错误处理
1. **防御性编程**: 总是检查API返回值
   ```c
   config_context_t* ctx = config_context_create("app");
   if (!ctx) {
       // 处理创建失败
   }
   ```

2. **提供默认值**: 使用类型安全访问器并指定默认值
   ```c
   const char* value = config_value_as_string(
       config_context_get(ctx, "key"), "default_value");
   ```

3. **优雅降级**: 当配置源不可用时使用默认值
   ```c
   if (config_source_load(source, ctx) != 0) {
       // 加载失败，使用默认配置
       config_context_set(ctx, "key", CONFIG_STRING("default"));
   }
   ```

## 扩展开发

### 添加新的配置源
1. **实现源适配器接口**
   ```c
   typedef struct {
       int (*load)(config_source_adapter_t* adapter, config_context_t* ctx);
       int (*watch)(config_source_adapter_t* adapter, 
                    config_source_callback_t callback, void* user_data);
       void (*destroy)(config_source_adapter_t* adapter);
   } config_source_adapter_ops_t;
   ```

2. **注册源适配器**
   ```c
   config_source_adapter_t* my_adapter = my_source_adapter_create(options);
   config_source_t* source = config_source_create_custom(my_adapter, CONFIG_SOURCE_CUSTOM);
   ```

### 添加新的验证器
1. **实现验证器接口**
2. **注册验证器工厂**
3. **集成到验证框架中**

## 故障排除

### 常见问题

**Q: 配置值访问返回空指针**
A: 检查键名是否正确，使用`config_value_as_*`函数并提供默认值。

**Q: 热更新不工作**
A: 确保文件系统支持inotify（Linux）或ReadDirectoryChangesW（Windows），检查文件权限。

**Q: 配置验证失败**
A: 检查验证模式设置，确保配置值符合模式要求。

**Q: 内存泄漏**
A: 确保每个`config_value_create_*`都有对应的`config_value_destroy`，每个`config_context_create`都有对应的`config_context_destroy`。

### 调试技巧

1. **启用调试日志**
   ```c
   config_set_debug_level(CONFIG_DEBUG_VERBOSE);
   ```

2. **检查配置状态**
   ```c
   config_dump_context(ctx, stdout);
   ```

3. **性能分析**
   ```c
   config_perf_stats_t stats;
   config_get_performance_stats(&stats);
   printf("平均访问时间: %lld ns\n", stats.avg_access_time_ns);
   ```

## 版本历史

### v1.0.0 (2026-03-26)
- 初始版本发布
- 三层架构实现：核心层、源适配层、服务层
- 向后兼容层支持现有API
- 支持文件、环境变量、命令行参数、内存配置源
- 提供配置验证、热更新、加密、版本管理功能
- 性能基准测试框架
- 完整的API文档和使用示例

### 未来计划
- 分布式配置管理支持
- 配置变更审计日志
- 机器学习驱动的配置优化
- 更丰富的配置格式支持（XML、INI等）
- 配置模板语言支持

---

## 许可证

Copyright (c) 2026 SPHARX. All Rights Reserved.

## 作者

AgentOS 开发团队

## 反馈和贡献

欢迎通过GitHub Issues提交问题和建议，或通过Pull Request贡献代码。