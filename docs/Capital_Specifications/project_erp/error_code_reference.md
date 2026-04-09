# AgentOS 错误码参考文档

## 1. 概述

本文档提供了 AgentOS 中所有错误码的统一参考，包括错误码的分类、定义、使用方法和最佳实践。错误码是系统中错误处理的重要组成部分，统一的错误码管理可以提高系统的可维护性和可扩展性。本规范基于 AgentOS 架构设计原则 V1.7，特别是工程观中的 **E-6 错误可追溯原则** 和 **E-8 可测试性原则**。

## 2. 文档信息

| 字段 | 值 |
|------|-----|
| 文档名称 | AgentOS 错误码参考文档 |
| 版本 | Doc V1.7 |
| 作者 | LirenWang |
| 理论基础 | 工程两论（反馈闭环）、五维正交系统（工程观E-6、E-8）、双系统认知理论 |
| 最后更新 | 2026-03-31 |
| 状态 | 🟢 生产就绪 |
| 适用版本 | v1.2.0+ |

## 3. 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| Doc V1.7 | 2026-03-31 | 根据架构设计原则V1.7优化，更新作者信息，统一版本管理 |
| Doc V1.6 | 2026-03-25 | 根据架构设计原则V1.6优化，补充理论基础，更新版本信息 |
| Doc V1.5 | 2026-03-24 | 错误码分类重构，增加更多详细说明 |
| Doc V1.4 | 2026-03-23 | 添加系统调用错误和安全域错误分类 |
| Doc V1.3 | 2026-03-22 | 扩展核心循环错误和认知层错误 |
| Doc V1.2 | 2026-03-21 | 添加通用错误和基本错误码分类 |
| Doc V1.1 | 2026-03-20 | 初始版本，包含通用错误、核心循环错误、认知层错误、执行层错误、记忆层错误 |

## 4. 错误码设计原则

- **统一管理**：所有错误码集中定义和管理
- **分类清晰**：错误码按照功能和模块进行分类
- **扩展性**：预留足够的错误码空间以支持未来扩展
- **可读性**：错误码名称和描述清晰易懂
- **一致性**：在整个系统中保持错误码的一致性

## 3. 错误码分类

AgentOS的错误码分为以下几类：

| 分类 | 范围 | 描述 |
|------|------|------|
| 通用错误 | 0x0000 - 0x0FFF | 适用于所有模块的通用错误 |
| 核心循环错误 | 0x1000 - 0x1FFF | 核心循环模块相关错误 |
| 认知层错误 | 0x2000 - 0x2FFF | 认知引擎相关错误 |
| 执行层错误 | 0x3000 - 0x3FFF | 执行引擎相关错误 |
| 记忆层错误 | 0x4000 - 0x4FFF | 记忆引擎相关错误 |
<!-- From data intelligence emerges. by spharx -->
| 系统调用错误 | 0x5000 - 0x5FFF | 系统调用相关错误 |
| 安全域错误 | 0x6000 - 0x6FFF | 安全域相关错误 |
| 动态模块错误 | 0x7000 - 0x7FFF | 动态模块相关错误 |
| 预留 | 0x8000 - 0xFFFF | 预留用于未来扩展 |

## 4. 通用错误码

| 错误码 | 宏定义 | 描述 | 处理建议 |
|--------|--------|------|----------|
| 0x0000 | AGENTOS_SUCCESS | 操作成功 | 正常流程 |
| 0x0001 | AGENTOS_ERROR_UNKNOWN | 未知错误 | 检查系统日志，排查异常情况 |
| 0x0002 | AGENTOS_ERROR_OUT_OF_MEMORY | 内存不足 | 检查内存使用情况，减少内存分配 |
| 0x0003 | AGENTOS_ERROR_INVALID_PARAMETER | 参数无效 | 检查输入参数的有效性 |
| 0x0004 | AGENTOS_ERROR_NULL_POINTER | 空指针 | 检查指针是否为NULL |
| 0x0005 | AGENTOS_ERROR_TIMEOUT | 操作超时 | 检查超时设置，优化操作性能 |
| 0x0006 | AGENTOS_ERROR_NOT_FOUND | 资源未找到 | 检查资源是否存在 |
| 0x0007 | AGENTOS_ERROR_ALREADY_EXISTS | 资源已存在 | 检查资源是否重复创建 |
| 0x0008 | AGENTOS_ERROR_PERMISSION_DENIED | 权限拒绝 | 检查权限设置 |
| 0x0009 | AGENTOS_ERROR_IO_ERROR | I/O错误 | 检查文件系统和设备状态 |
| 0x000A | AGENTOS_ERROR_NOT_SUPPORTED | 不支持的操作 | 检查操作是否在支持范围内 |
| 0x000B | AGENTOS_ERROR_BUSY | 资源忙 | 稍后重试操作 |
| 0x000C | AGENTOS_ERROR_INTERNAL | 内部错误 | 检查系统内部状态 |
| 0x000D | AGENTOS_ERROR_CORRUPTED_DATA | 数据损坏 | 检查数据完整性 |
| 0x000E | AGENTOS_ERROR_OVERFLOW | 缓冲区溢出 | 检查缓冲区大小 |
| 0x000F | AGENTOS_ERROR_UNDERFLOW | 缓冲区下溢 | 检查缓冲区操作 |

## 5. 核心循环错误码

| 错误码 | 宏定义 | 描述 | 处理建议 |
|--------|--------|------|----------|
| 0x1001 | AGENTOS_ERROR_LOOP_CREATE_FAILED | 核心循环创建失败 | 检查系统资源和配置 |
| 0x1002 | AGENTOS_ERROR_LOOP_RUN_FAILED | 核心循环运行失败 | 检查循环状态和依赖 |
| 0x1003 | AGENTOS_ERROR_LOOP_STOP_FAILED | 核心循环停止失败 | 检查循环状态 |
| 0x1004 | AGENTOS_ERROR_TASK_SUBMIT_FAILED | 任务提交失败 | 检查任务参数和系统状态 |
| 0x1005 | AGENTOS_ERROR_TASK_WAIT_FAILED | 任务等待失败 | 检查任务ID和超时设置 |
| 0x1006 | AGENTOS_ERROR_ENGINE_CREATE_FAILED | 引擎创建失败 | 检查引擎配置和依赖 |
| 0x1007 | AGENTOS_ERROR_ENGINE_DESTROY_FAILED | 引擎销毁失败 | 检查引擎状态 |
| 0x1008 | AGENTOS_ERROR_ENGINE_NOT_INITIALIZED | 引擎未初始化 | 确保引擎已正确初始化 |

## 6. 认知层错误码

| 错误码 | 宏定义 | 描述 | 处理建议 |
|--------|--------|------|----------|
| 0x2001 | AGENTOS_ERROR_COGNITION_CREATE_FAILED | 认知引擎创建失败 | 检查配置和资源 |
| 0x2002 | AGENTOS_ERROR_COGNITION_PROCESS_FAILED | 认知处理失败 | 检查输入数据和模型状态 |
| 0x2003 | AGENTOS_ERROR_COGNITION_PLAN_FAILED | 规划失败 | 检查规划策略和约束条件 |
| 0x2004 | AGENTOS_ERROR_COGNITION_REASON_FAILED | 推理失败 | 检查推理规则和数据 |
| 0x2005 | AGENTOS_ERROR_COGNITION_LEARN_FAILED | 学习失败 | 检查学习数据和模型 |
| 0x2006 | AGENTOS_ERROR_COGNITION_MEMORY_FAILED | 记忆访问失败 | 检查记忆引擎状态 |
| 0x2007 | AGENTOS_ERROR_COGNITION_CONTEXT_FAILED | 上下文处理失败 | 检查上下文数据 |
| 0x2008 | AGENTOS_ERROR_COGNITION_PRIORITY_FAILED | 优先级处理失败 | 检查优先级设置 |

## 7. 执行层错误码

| 错误码 | 宏定义 | 描述 | 处理建议 |
|--------|--------|------|----------|
| 0x3001 | AGENTOS_ERROR_EXECUTION_CREATE_FAILED | 执行引擎创建失败 | 检查配置和资源 |
| 0x3002 | AGENTOS_ERROR_EXECUTION_SUBMIT_FAILED | 任务提交失败 | 检查任务参数和系统状态 |
| 0x3003 | AGENTOS_ERROR_EXECUTION_EXECUTE_FAILED | 任务执行失败 | 检查任务逻辑和依赖 |
| 0x3004 | AGENTOS_ERROR_EXECUTION_MONITOR_FAILED | 任务监控失败 | 检查监控配置和状态 |
| 0x3005 | AGENTOS_ERROR_EXECUTION_RECOVER_FAILED | 任务恢复失败 | 检查恢复策略和资源 |
| 0x3006 | AGENTOS_ERROR_COMPENSATION_CREATE_FAILED | 补偿事务创建失败 | 检查补偿配置 |
| 0x3007 | AGENTOS_ERROR_COMPENSATION_EXECUTE_FAILED | 补偿执行失败 | 检查补偿逻辑和资源 |
| 0x3008 | AGENTOS_ERROR_COMPENSATION_QUEUE_FULL | 补偿队列已满 | 增加队列大小或处理队列中的任务 |

## 8. 记忆层错误码

| 错误码 | 宏定义 | 描述 | 处理建议 |
|--------|--------|------|----------|
| 0x4001 | AGENTOS_ERROR_MEMORY_CREATE_FAILED | 记忆引擎创建失败 | 检查配置和资源 |
| 0x4002 | AGENTOS_ERROR_MEMORY_WRITE_FAILED | 记忆写入失败 | 检查存储介质和权限 |
| 0x4003 | AGENTOS_ERROR_MEMORY_QUERY_FAILED | 记忆查询失败 | 检查查询参数和索引状态 |
| 0x4004 | AGENTOS_ERROR_MEMORY_GET_FAILED | 记忆获取失败 | 检查记录ID和存储状态 |
| 0x4005 | AGENTOS_ERROR_MEMORY_MOUNT_FAILED | 记忆挂载失败 | 检查上下文和记录状态 |
| 0x4006 | AGENTOS_ERROR_MEMORY_EVOLVE_FAILED | 记忆进化失败 | 检查进化配置和数据 |
| 0x4007 | AGENTOS_ERROR_MEMORY_CORRUPTED | 记忆数据损坏 | 检查存储介质和数据完整性 |
| 0x4008 | AGENTOS_ERROR_MEMORY_FULL | 记忆存储已满 | 清理旧数据或增加存储容量 |

## 9. 系统调用错误码

| 错误码 | 宏定义 | 描述 | 处理建议 |
|--------|--------|------|----------|
| 0x5001 | AGENTOS_ERROR_SYSCALL_TABLE_CREATE_FAILED | 系统调用表创建失败 | 检查配置和资源 |
| 0x5002 | AGENTOS_ERROR_SYSCALL_ENTRY_CREATE_FAILED | 系统调用条目创建失败 | 检查条目参数 |
| 0x5003 | AGENTOS_ERROR_SYSCALL_REGISTER_FAILED | 系统调用注册失败 | 检查调用参数和权限 |
| 0x5004 | AGENTOS_ERROR_SYSCALL_EXECUTE_FAILED | 系统调用执行失败 | 检查调用参数和实现 |
| 0x5005 | AGENTOS_ERROR_SYSCALL_NOT_FOUND | 系统调用未找到 | 检查调用ID是否存在 |
| 0x5006 | AGENTOS_ERROR_SYSCALL_PERMISSION_DENIED | 系统调用权限拒绝 | 检查权限设置 |
| 0x5007 | AGENTOS_ERROR_SYSCALL_TIMEOUT | 系统调用超时 | 检查调用实现和资源状态 |
| 0x5008 | AGENTOS_ERROR_SYSCALL_INVALID_PARAMETER | 系统调用参数无效 | 检查调用参数 |

## 10. 安全域错误码

| 错误码 | 宏定义 | 描述 | 处理建议 |
|--------|--------|------|----------|
| 0x6001 | AGENTOS_ERROR_CUPOLAS_CREATE_FAILED | 安全穹顶创建失败 | 检查配置和资源 |
| 0x6002 | AGENTOS_ERROR_CUPOLA_CREATE_FAILED | 单个安全穹顶创建失败 | 检查穹顶配置 |
| 0x6003 | AGENTOS_ERROR_CUPOLA_ACCESS_DENIED | 穹顶访问拒绝 | 检查访问权限 |
| 0x6004 | AGENTOS_ERROR_CUPOLA_SECURITY_VIOLATION | 安全违反 | 检查安全策略和访问模式 |
| 0x6005 | AGENTOS_ERROR_CUPOLA_ISOLATION_FAILED | 隔离失败 | 检查隔离机制和资源 |
| 0x6006 | AGENTOS_ERROR_CUPOLA_MONITOR_FAILED | 监控失败 | 检查监控配置和状态 |
| 0x6007 | AGENTOS_ERROR_CUPOLA_POLICY_VIOLATION | 策略违反 | 检查安全策略设置 |
| 0x6008 | AGENTOS_ERROR_CUPOLA_RESOURCE_EXHAUSTED | 资源耗尽 | 检查资源使用情况 |

## 11. 动态模块错误码

| 错误码 | 宏定义 | 描述 | 处理建议 |
|--------|--------|------|----------|
| 0x7001 | AGENTOS_ERROR_DYNAMIC_CREATE_FAILED | 动态模块创建失败 | 检查配置和资源 |
| 0x7002 | AGENTOS_ERROR_DYNAMIC_LOAD_FAILED | 模块加载失败 | 检查模块路径和依赖 |
| 0x7003 | AGENTOS_ERROR_DYNAMIC_UNLOAD_FAILED | 模块卸载失败 | 检查模块状态和依赖 |
| 0x7004 | AGENTOS_ERROR_DYNAMIC_SYMBOL_NOT_FOUND | 符号未找到 | 检查模块导出的符号 |
| 0x7005 | AGENTOS_ERROR_DYNAMIC_VERSION_MISMATCH | 版本不匹配 | 检查模块版本兼容性 |
| 0x7006 | AGENTOS_ERROR_DYNAMIC_INIT_FAILED | 模块初始化失败 | 检查模块初始化函数 |
| 0x7007 | AGENTOS_ERROR_DYNAMIC_DEINIT_FAILED | 模块反初始化失败 | 检查模块反初始化函数 |
| 0x7008 | AGENTOS_ERROR_DYNAMIC_RESOURCE_LEAK | 资源泄漏 | 检查模块资源管理 |

## 12. 错误码使用方法

### 12.1 错误码定义

错误码在 `agentos_error.h` 文件中定义，使用以下格式：

```c
#define AGENTOS_SUCCESS                    0x0000
#define AGENTOS_ERROR_UNKNOWN              0x0001
#define AGENTOS_ERROR_OUT_OF_MEMORY        0x0002
// 其他错误码...
```

### 12.2 错误码返回

函数应返回 `agentos_error_t` 类型的错误码：

```c
agentos_error_t agentos_function(void) {
    // 操作成功
    if (success) {
        return AGENTOS_SUCCESS;
    }
    // 操作失败
    else {
        return AGENTOS_ERROR_SOME_ERROR;
    }
}
```

### 12.3 错误码检查

调用函数时应检查返回的错误码：

```c
agentos_error_t err = agentos_function();
if (err != AGENTOS_SUCCESS) {
    // 错误处理
    printf("Error: %x\n", err);
    return err;
}
```

### 12.4 错误码转换

可以使用 `agentos_error_to_string()` 函数将错误码转换为字符串描述：

```c
agentos_error_t err = agentos_function();
if (err != AGENTOS_SUCCESS) {
    const char* error_str = agentos_error_to_string(err);
    printf("Error: %s\n", error_str);
    return err;
}
```

## 13. 错误处理最佳实践

### 13.1 错误传播

- 底层函数应返回具体的错误码
- 上层函数可以根据需要转换或包装错误码
- 避免在错误处理中丢失原始错误信息

### 13.2 错误日志

- 在关键操作失败时记录详细的错误信息
- 包含错误码、错误描述、操作上下文等信息
- 使用适当的日志级别（error、warning、info等）

### 13.3 错误恢复

- 对于可恢复的错误，实现适当的恢复机制
- 对于不可恢复的错误，优雅地终止操作并释放资源
- 提供错误恢复的策略和方法

### 13.4 错误预防

- 进行输入参数验证，防止错误发生
- 使用防御性编程技术，处理边界情况
- 定期检查系统状态，预防潜在错误

## 14. 错误码扩展

当需要添加新的错误码时，应遵循以下步骤：

1. 在相应的错误码范围内选择一个未使用的错误码
2. 在 `agentos_error.h` 文件中添加错误码定义
3. 在本文档中添加错误码的描述和处理建议
4. 确保错误码的使用符合本文档中的规范

## 15. 结论

统一的错误码管理是AgentOS系统稳定性和可靠性的重要组成部分。通过本文档提供的错误码参考，开发者可以更加规范地处理错误，提高系统的可维护性和可扩展性。所有开发者必须严格遵循本文档中的错误码规范，确保错误处理的一致性和有效性。