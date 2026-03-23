# AgentOS 迁移指南

## 1. 概述

本文档提供了从 AgentOS 旧版本迁移到新版本的详细指南。随着 AgentOS 的不断发展，我们引入了一些新特性和改进，同时也可能对一些 API 进行了调整。本指南将帮助您理解这些变化并顺利完成迁移。

## 2. 版本兼容性说明

AgentOS 采用语义化版本号（MAJOR.MINOR.PATCH），其中：

- **MAJOR**：表示不兼容的 API 更改
- **MINOR**：表示向后兼容的功能添加
- **PATCH**：表示向后兼容的 bug 修复

### 2.1 版本兼容性保证

- 在相同 MAJOR 版本内，所有 MINOR 和 PATCH 版本都是向后兼容的
- 当 MAJOR 版本变更时，可能会引入不兼容的 API 更改
- 破坏性更改会在本指南中详细说明

## 3. 从 v0.x 迁移到 v1.0

### 3.1 主要变更

#### 3.1.1 API 版本管理
```
# From data intelligence emerges. by spharx -->

```

- **新增**：所有公共头文件现在包含 API 版本宏定义
- **变更**：API 版本宏使用 `MODULE_API_VERSION_MAJOR/MINOR/PATCH` 格式
- **影响**：需要在代码中使用这些宏来检查 API 版本兼容性

#### 3.1.2 符号导出管理

- **新增**：引入了 `AGENTOS_API` 宏用于符号导出
- **变更**：所有公共函数现在使用 `AGENTOS_API` 宏标记
- **影响**：构建系统需要定义 `AGENTOS_BUILDING_SHARED` 宏（CMake 会自动处理）

#### 3.1.3 资源管理

- **新增**：明确的资源释放责任和资源管理表
- **变更**：所有资源都有明确的所有权和释放责任
- **影响**：需要按照资源管理表中的说明正确管理资源生命周期

#### 3.1.4 错误处理

- **新增**：统一的错误码定义和错误处理规范
- **变更**：错误码使用 16 位十六进制格式，按模块分类
- **影响**：需要使用新的错误码宏和错误处理方法

#### 3.1.5 文档改进

- **新增**：完善的 Doxygen 注释，包括 ownership 和线程语义
- **变更**：所有公共函数都有详细的文档注释
- **影响**：代码编辑器和文档生成工具将提供更好的代码提示和文档

### 3.2 迁移步骤

#### 步骤 1：更新包含路径

确保包含路径正确指向新版本的头文件：

```cmake
# 旧版本
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../core/include")

# 新版本
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../atoms/corekern/include")
```

#### 步骤 2：更新 API 版本检查

在代码中添加 API 版本检查，确保与目标版本兼容：

```c
#include "cognition.h"

// 检查 API 版本
#if COGNITION_API_VERSION_MAJOR != 1
    #error "Incompatible cognition API version"
#endif
```

#### 步骤 3：更新资源管理

按照资源管理表中的说明，正确管理资源生命周期：

```c
// 旧版本
agentos_cognition_engine_t* engine;
agentos_cognition_create(NULL, &engine);
// ... 使用引擎 ...
// 可能忘记释放

// 新版本
agentos_cognition_engine_t* engine;
agentos_error_t err = agentos_cognition_create(NULL, &engine);
if (err != AGENTOS_SUCCESS) {
    // 错误处理
    return err;
}
// ... 使用引擎 ...
// 明确释放资源
agentos_cognition_destroy(engine);
```

#### 步骤 4：更新错误处理

使用新的错误码宏和错误处理方法：

```c
// 旧版本
if (result < 0) {
    printf("Error: %d\n", result);
    return result;
}

// 新版本
agentos_error_t err = agentos_some_function();
if (err != AGENTOS_SUCCESS) {
    printf("Error: %s\n", agentos_strerror(err));
    return err;
}
```

#### 步骤 5：更新 CMake 配置

更新 CMakeLists.txt 文件，添加符号导出管理支持：

```cmake
# 添加符号导出管理
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# 定义构建共享库时的宏
if(BUILD_SHARED_LIBS)
    add_definitions(-DAGENTOS_BUILDING_SHARED)
endif()
```

### 3.3 代码示例

#### 3.3.1 核心循环初始化

**旧版本**：

```c
agentos_core_loop_t* loop;
agentos_loop_create(NULL, &loop);
agentos_loop_run(loop);
// ...
agentos_loop_destroy(loop);
```

**新版本**：

``c
agentos_core_loop_t* loop;
agentos_error_t err = agentos_loop_create(NULL, &loop);
if (err != AGENTOS_SUCCESS) {
    printf("Error creating loop: %s\n", agentos_strerror(err));
    return err;
}

// 运行循环
err = agentos_loop_run(loop);
if (err != AGENTOS_SUCCESS) {
    printf("Error running loop: %s\n", agentos_strerror(err));
    agentos_loop_destroy(loop);
    return err;
}

// 停止循环
agentos_loop_stop(loop);

// 释放资源
agentos_loop_destroy(loop);
```

#### 3.3.2 记忆管理

**新版本**：

``c
agentos_memory_engine_t* memory;
agentos_error_t err = agentos_memory_create(NULL, &memory);
if (err != AGENTOS_SUCCESS) {
    printf("Error creating memory engine: %s\\n", agentos_strerror(err));
    return err;
}

// ... 使用内存引擎 ...

// 使用记忆
agentos_memory_record_t record = {
    .memory_record_id = "12345",
    .memory_record_type = MEMORY_TYPE_FEATURE,
    .memory_record_timestamp_ns = 123456789012,
    .memory_record_source_agent = "agent_001",
    .memory_record_trace_id = "trace_123",
    .memory_record_data = "Hello, AgentOS!",
    .memory_record_data_len = 14,
    .memory_record_importance = 0.8,
    .memory_record_access_count = 1
};

char* record_id = NULL;
err = agentos_memory_write(memory, &record, &record_id);
if (err != AGENTOS_SUCCESS) {
    printf("Error writing memory: %s\\n", agentos_strerror(err));
    return err;
}

// 使用记忆
agentos_memory_query_t query = {
    .memory_query_text = "greeting",
    .memory_query_limit = 10,
    .memory_query_threshold = 0.5
};

agentos_memory_result_t* result = NULL;
err = agentos_memory_query(memory, &query, &result);
if (err != AGENTOS_SUCCESS) {
    printf("Error querying memory: %s\\n", agentos_strerror(err));
    agentos_memory_destroy(memory);
    return err;
}

// 释放结果
agentos_memory_result_free(result);
free(record_id);
// 释放内存引擎
agentos_memory_destroy(memory);
```

**新版本**：

``c
agentos_memory_engine_t* memory;
agentos_error_t err = agentos_memory_create(NULL, &memory);
if (err != AGENTOS_SUCCESS) {
    printf("Error creating memory engine: %s\n", agentos_strerror(err));
    return err;
}

// 写入记忆
agentos_memory_record_t record = {
    .content = "Hello, AgentOS!",
    .content_len = 14,
    .metadata = "{\"type\": \"greeting\"}"
};
char* record_id = NULL;
err = agentos_memory_write(memory, &record, &record_id);
if (err != AGENTOS_SUCCESS) {
    printf("Error writing memory: %s\n", agentos_strerror(err));
    agentos_memory_destroy(memory);
    return err;
}

// 使用记忆
agentos_memory_query_t query = {
    .text = "greeting",
    .limit = 10,
    .threshold = 0.5
};
agentos_memory_result_t* result = NULL;
err = agentos_memory_query(memory, &query, &result);
if (err != AGENTOS_SUCCESS) {
    printf("Error querying memory: %s\n", agentos_strerror(err));
    free(record_id);
    agentos_memory_destroy(memory);
    return err;
}

// 释放结果
agentos_memory_result_free(result);
free(record_id);

// 释放内存引擎
agentos_memory_destroy(memory);
```

## 4. 从 v1.x 迁移到 v1.y

### 4.1 版本间变更

#### 4.1.1 v1.0 到 v1.1

- **新增**：添加了 `agentos_memory_evolve()` 函数，用于触发记忆进化
- **改进**：优化了内存查询性能
- **影响**：向后兼容，无需修改现有代码

#### 4.1.2 v1.1 到 v1.2

- **新增**：添加了 `agentos_compensation_get_human_queue()` 函数，用于获取待人工介入的补偿队列
- **改进**：增强了补偿事务的可靠性
- **影响**：向后兼容，无需修改现有代码

### 4.2 迁移注意事项

- 所有 v1.x 版本都是向后兼容的，您可以直接升级到最新版本
- 新功能是可选的，您可以根据需要逐步采用
- 建议定期更新到最新版本，以获得 bug 修复和性能改进

## 5. 常见问题和解决方案

### 5.1 编译错误

**问题**：`AGENTOS_API` 未定义

**解决方案**：确保包含了 `agentos.h` 头文件，并且 CMake 正确设置了 `AGENTOS_BUILDING_SHARED` 宏

### 5.2 链接错误

**问题**：找不到符号 `agentos_some_function`

**解决方案**：确保所有公共函数都使用 `AGENTOS_API` 宏标记，并且 CMake 配置正确

### 5.3 运行时错误

**问题**：资源泄漏

**解决方案**：按照资源管理表中的说明，确保所有资源都被正确释放

### 5.4 性能问题

**问题**：内存查询速度慢

**解决方案**：使用 v1.1+ 版本，它包含了内存查询性能优化

## 6. 最佳实践

### 6.1 代码组织

- 按照模块组织代码，每个模块有明确的职责
- 使用统一的命名规范，遵循 AgentOS 编码风格指南
- 为所有公共函数添加详细的 Doxygen 注释

### 6.2 错误处理

- 始终检查函数返回的错误码
- 使用 `agentos_strerror()` 函数获取错误描述
- 在错误处理路径中正确释放所有已分配的资源

### 6.3 资源管理

- 遵循资源管理表中的说明，正确管理资源生命周期
- 使用 RAII 模式（如果使用 C++）或类似机制管理资源
- 避免跨模块的资源所有权转移，除非有明确的文档说明

### 6.4 版本管理

- 在代码中使用 API 版本宏检查兼容性
- 定期更新到最新版本，以获得 bug 修复和性能改进
- 在 MAJOR 版本变更时，仔细阅读迁移指南

## 7. 结论

AgentOS 的迁移过程设计为尽可能平滑和向后兼容。通过遵循本指南中的步骤和最佳实践，您可以顺利完成从旧版本到新版本的迁移，同时充分利用新特性和改进。

如果您在迁移过程中遇到任何问题，请参考常见问题部分，或联系 AgentOS 开发团队获取支持。

## 8. 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| v1.0.0 | 2026-03-21 | 初始版本，包含核心循环、认知、执行、记忆等模块 |
| v1.0.1 | 2026-04-01 | 修复了内存泄漏问题，改进了错误处理 |
| v1.1.0 | 2026-05-01 | 添加了记忆进化功能，优化了内存查询性能 |
| v1.2.0 | 2026-06-01 | 添加了人工介入补偿队列，增强了补偿事务可靠性 |