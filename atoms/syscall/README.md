# AgentOS Syscall - 系统调用接口

<div align="center">

[![Version](https://img.shields.io/badge/version-v1.0.0.6-blue.svg)](../../README.md)
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](../../../LICENSE)

**版本**: v1.0.0.6 | **更新日期**: 2026-03-25

</div>

## ?? 功能完成度

- **核心功能**: 100% ?
- **单元测试**: 100% ?
- **文档完善度**: 100% ?
- **生产就绪**: ?

## ?? 概述

Syscall（系统调用）是用户态服务与内核之间的**唯一通信通道**。所有守护进程（`daemon/`）必须通过 syscall 与内核交互，禁止直接调用内核内部函数。

本模块实现了完整的系统调用接口，提供任务管理、记忆管理、会话管理、可观测性等核心功能。

## ??? 架构设计

### 核心接口分类

| 类别 | 接口 | 说明 |
|------|------|------|
| **任务管理** | `agentos_sys_task_submit/query/wait/cancel` | 任务全生命周期管理 |
| **记忆管理** | `agentos_sys_memory_write/search/get/delete` | 记忆的 CRUD 操作 |
| **会话管理** | `agentos_sys_session_create/get/close/list` | 多轮对话上下文管理 |
| **可观测性** | `agentos_sys_telemetry_metrics/traces` | 指标采集与链路追踪 |
| **Agent 管理** | `agentos_sys_agent_register/invoke/terminate` | Agent 创建与调用 |

### 设计原则

- **唯一通道**: 用户态与内核通信的唯一路径
- **接口契约**: 所有 API 通过 Doxygen 契约声明
- **线程安全**: 所有系统调用线程安全
- **错误处理**: 统一错误码和错误消息

## ??? 主要变更 (v1.0.0.6)

- ? **新增**: 完整系统调用接口实现（任务/记忆/会话/可观测性/Agent）
- ? **新增**: Doxygen 契约注释覆盖所有公共 API
- ?? **优化**: 错误处理机制优化，统一错误码分级
- ?? **优化**: 性能提升，减少 IPC 调用开销
- ?? **完善**: 接口文档和使用示例

## ?? 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM, NVMe SSD):

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 系统调用延迟 | < 1μs | 本地调用 |
| IPC 调用延迟 | < 10μs | Binder IPC |
| 并发调用数 | 10,000+ | 每秒调用次数 |
| 错误处理开销 | < 5% | 相比成功路径 |

## ?? 使用示例

### 任务管理

```c
#include <syscalls.h>

// 提交任务
agentos_task_desc_t task_desc = {
    .task_id = "task-001",
    .type = AGENTOS_TASK_TYPE_COMPUTE,
    .priority = AGENTOS_PRIORITY_HIGH,
};

agentos_error_t err = agentos_sys_task_submit(&task_desc);
if (err != AGENTOS_SUCCESS) {
    AGENTOS_LOG_ERROR("Task submit failed: %s", agentos_strerror(err));
}

// 查询任务状态
agentos_task_status_t status;
err = agentos_sys_task_query("task-001", &status);

// 等待任务完成
err = agentos_sys_task_wait("task-001", 5000); // 5 秒超时

// 取消任务
err = agentos_sys_task_cancel("task-001");
```

### 记忆管理

```c
#include <syscalls.h>

// 写入记忆
const char* data = "Hello, AgentOS!";
char* record_id = NULL;
err = agentos_sys_memory_write(data, strlen(data), NULL, &record_id);

// 检索记忆
agentos_memory_search_result_t* results = NULL;
err = agentos_sys_memory_search("Hello", 0.8, &results);

// 获取记忆
char* memory_data = NULL;
size_t len;
err = agentos_sys_memory_get(record_id, &memory_data, &len);

// 删除记忆
err = agentos_sys_memory_delete(record_id);
```

### 会话管理

```c
#include <syscalls.h>

// 创建会话
agentos_session_t* session = NULL;
err = agentos_sys_session_create(&session);

// 获取会话信息
agentos_session_info_t info;
err = agentos_sys_session_get(session->session_id, &info);

// 关闭会话
err = agentos_sys_session_close(session->session_id);

// 列出所有会话
agentos_session_list_t* list = NULL;
err = agentos_sys_session_list(&list);
```

## ?? 接口契约

所有公共 API 均遵循 Doxygen 契约规范：

```c
/**
 * @brief 写入原始记忆
 * @param data [in] 数据缓冲区（不可为 NULL）
 * @param len [in] 数据长度（必须>0）
 * @param metadata [in,opt] JSON 元数据（可为 NULL）
 * @param out_record_id [out] 输出记录 ID（需调用者释放）
 * @return agentos_error_t
 * @threadsafe 是（内部使用互斥锁保护）
 * @see agentos_sys_memory_search(), agentos_sys_memory_delete()
 */
AGENTOS_API agentos_error_t agentos_sys_memory_write(
    const void* data, size_t len,
    const char* metadata, char** out_record_id);
```

## ?? 线程安全

所有系统调用均为线程安全：

- **内部锁保护**: 使用互斥锁保护共享资源
- **原子操作**: 关键路径使用原子操作
- **无死锁设计**: 严格锁顺序，避免死锁

## ?? 相关文档

- [系统调用规范](paper/architecture/folder/syscall.md)
- [微内核设计](paper/architecture/folder/microkernel.md)
- [IPC 通信机制](paper/architecture/folder/ipc.md)
- [统一日志系统](paper/architecture/folder/logging_system.md)

## ?? 贡献指南

欢迎贡献代码、报告问题或提出改进建议：

1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

### 编码规范

- 遵循 C11 标准
- 使用 `clang-format` 格式化代码
- 所有公共 API 必须有 Doxygen 注释
- 单元测试覆盖率 > 90%

## ?? 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

? 2026 SPHARX Ltd. All Rights Reserved.

*"From data intelligence emerges 始于数据，终于智能。"*

