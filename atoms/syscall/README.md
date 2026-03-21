# AgentOS 系统调用层 (Syscall)

**版本**: 1.0.0.5  
**路径**: `atoms/syscall/`  
**最后更新**: 2026-03-18  
**许可证**: Apache License 2.0

---

## 1. 概述

Syscall 层是 AgentOS 内核的系统调用接口，提供稳定、安全的内核功能访问入口。它隐藏内核实现细节，通过统一的 API 暴露核心功能。

### 核心特性

- **统一入口**: `agentos_syscall_invoke()` 作为所有系统调用的总入口
- **分类管理**: 按功能分为任务、记忆、会话、可观测性等类别
- **安全验证**: 所有参数经过严格验证和边界检查
- **错误处理**: 统一的错误码返回机制
- **性能优化**: 最小化上下文切换开销

---

## 2. 目录结构

<!-- From data intelligence emerges. by spharx -->
```
syscall/
├── CMakeLists.txt          # 构建配置
├── README.md               # 本文件
├── include/
│   └── syscalls.h          # 系统调用统一头文件
└── src/
    ├── syscall_entry.c     # 系统调用入口
    ├── syscall_table.c     # 调用表管理
    ├── task_syscalls.c     # 任务系统调用
    ├── memory_syscalls.c   # 记忆系统调用
    ├── session_syscalls.c  # 会话系统调用
    └── telemetry_syscalls.c # 可观测性系统调用
```

---

## 3. 系统调用分类

### 3.1 任务系统调用 (Task Syscalls)

用于任务的提交、查询、等待和取消。

| 调用名 | 说明 | 完成度 |
|--------|------|--------|
| `sys_task_submit()` | 提交自然语言任务 | ✅ 100% |
| `sys_task_query()` | 查询任务状态 | ✅ 100% |
| `sys_task_wait()` | 等待任务完成 | ✅ 100% |
| `sys_task_cancel()` | 取消任务执行 | ✅ 100% |

**使用示例**:
```c
agentos_task_id_t task_id;
const char* nl_description = "分析这份销售数据";

int ret = agentos_sys_task_submit(nl_description, NULL, &task_id);
if (ret == 0) {
    printf("Task submitted: %d\n", task_id);
    
    // 等待任务完成
    agentos_task_result_t result;
    ret = agentos_sys_task_wait(task_id, &result, 30000); // 30 秒超时
}
```

### 3.2 记忆系统调用 (Memory Syscalls)

用于记忆的写入、搜索、获取和删除。

| 调用名 | 说明 | 完成度 |
|--------|------|--------|
| `sys_memory_write()` | 写入记忆数据 | ✅ 100% |
| `sys_memory_search()` | 语义搜索记忆 | ✅ 100% |
| `sys_memory_get()` | 获取指定记忆 | ✅ 100% |
| `sys_memory_delete()` | 删除记忆 | ✅ 100% |

**使用示例**:
```c
// 写入记忆
const char* content = "用户偏好使用 Python 进行数据分析";
uint64_t record_id;
agentos_sys_memory_write(content, AGENTOS_MEMORY_FEATURE, &record_id);

// 搜索记忆
agentos_memory_search_results_t results;
agentos_sys_memory_search("Python 数据分析", 10, &results);
```

### 3.3 会话系统调用 (Session Syscalls)

用于会话的生命周期管理。

| 调用名 | 说明 | 完成度 |
|--------|------|--------|
| `sys_session_create()` | 创建新会话 | ✅ 100% |
| `sys_session_get()` | 获取会话信息 | ✅ 100% |
| `sys_session_close()` | 关闭会话 | ✅ 100% |
| `sys_session_list()` | 列出所有会话 | ✅ 100% |

**使用示例**:
```c
agentos_session_id_t session_id;
agentos_sys_session_create(&session_id);

// 使用会话...

agentos_sys_session_close(session_id);
```

### 3.4 可观测性系统调用 (Telemetry Syscalls)

用于获取系统指标和追踪数据。

| 调用名 | 说明 | 完成度 |
|--------|------|--------|
| `sys_telemetry_metrics()` | 获取性能指标 | ✅ 100% |
| `sys_telemetry_traces()` | 获取追踪数据 | ✅ 100% |

**使用示例**:
```c
agentos_metrics_t metrics;
agentos_sys_telemetry_metrics(&metrics);
printf("Memory usage: %zu MB\n", metrics.memory_used_mb);
```

---

## 4. 编译与集成

### 4.1 编译

```bash
mkdir build && cd build
cmake ../atoms/syscall -DCMAKE_BUILD_TYPE=Release
make
```

### 4.2 依赖

- AgentOS core 模块
- AgentOS utils 模块（日志、错误处理）
- pthread
- cJSON (用于 JSON 格式参数解析)

### 4.3 集成方式

上层应用通过包含 `syscalls.h` 并使用 `agentos_syscall_invoke()` 来调用系统功能：

```c
#include <agentos/syscalls.h>

int main() {
    // 初始化系统调用层
    agentos_syscall_init();
    
    // 使用系统调用...
    agentos_sys_task_submit(...);
    
    // 清理资源
    agentos_syscall_cleanup();
    return 0;
}
```

---

## 5. 设计哲学

### 5.1 稳定性优先

- API 一旦发布，保持向后兼容
- 重大变更通过版本号区分
- 废弃的 API 保留至少一个大版本

### 5.2 安全性保障

- 所有指针参数必须验证
- 数组边界严格检查
- 禁止直接访问内核内存

### 5.3 性能优化

- 减少上下文切换次数
- 使用高效的参数传递方式
- 关键路径无锁设计

### 5.4 错误处理

- 统一的错误码定义
- 详细的错误信息记录
- 支持错误溯源

---

## 6. 与 CoreLoopThree 的关系

Syscall 层为 CoreLoopThree 提供底层系统调用支持：

- **认知层**: 通过 `sys_memory_search()` 查询历史记忆辅助决策
- **行动层**: 通过 `sys_task_submit()` 提交任务执行
- **记忆层**: 封装 MemoryRovol FFI 接口，通过 `sys_memory_*` 系列调用实现记忆操作

---

## 7. 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 指标 | 数值 | 测试条件 |
|------|------|----------|
| **任务提交延迟** | < 1ms | 单次调用 |
| **记忆写入延迟** | < 5ms | L1 层同步写入 |
| **记忆检索延迟** | < 10ms | FAISS IVF, k=10 |
| **会话创建延迟** | < 0.5ms | 不含初始化 |
| **指标收集开销** | < 2% | CPU 占用增加 |

---

## 8. 未来规划

### v1.0.0.4 (2026 Q2)
- [ ] 增加批处理系统调用接口
- [ ] 优化大批量记忆写入性能
- [ ] 支持系统调用追踪和调试

### v1.1.0.0 (2026 Q3)
- [ ] 新增技能系统调用
- [ ] 新增 Agent 管理系统调用
- [ ] 支持异步系统调用模式

---

## 9. 相关文档

- [CoreLoopThree 架构](../coreloopthree/README.md) - 三层核心运行时
- [MemoryRovol 架构](../memoryrovol/README.md) - 记忆卷载系统
- [微内核设计](../core/README.md) - 基础服务
- [系统调用规范](../../partdocs/specifications/syscall_api.md) - API 详细规范

---

**Apache License 2.0 © 2026 SPHARX. "From data intelligence emerges."**

---

© 2026 SPHARX Ltd. All Rights Reserved.
