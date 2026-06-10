# Workbench — 安全工作台

**模块路径**: `agentos/cupolas/src/workbench/`
**版本**: v0.1.0

## 概述

Workbench 模块提供隔离的安全执行环境，用于在受控条件下运行不可信代码和命令。支持进程级隔离、容器隔离和资源限制，确保工作台内的操作不会影响宿主系统。所有执行操作均经过权限检查和输入清洗，执行结果记录审计日志。

## 设计目标

- **隔离执行**：进程级和容器级双重隔离，防止逃逸
- **资源限制**：CPU、内存、时间、文件系统等维度的精细控制
- **安全校验**：执行前自动进行权限检查和输入清洗
- **可观测**：执行过程完整记录，支持实时监控
- **可配置**：灵活的隔离策略和资源配额配置

## 目录结构

```
workbench/
├── workbench.h                  # 工作台公共接口
├── workbench.c                  # 工作台核心实现
├── workbench_process.h          # 进程管理接口
├── workbench_process_core.c     # 进程管理实现
├── workbench_container.h        # 容器隔离接口
├── workbench_container.c        # 容器隔离实现
├── workbench_limits.h           # 资源限制接口
├── workbench_limits.c           # 资源限制实现
└── README.md                    # 本文档
```

## 核心数据结构

### workbench_config_t — 工作台配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `working_dir` | `const char *` | 工作目录 |
| `env_vars` | `const char **` | 环境变量数组 |
| `env_count` | `size_t` | 环境变量数量 |
| `timeout_ms` | `uint32_t` | 执行超时（毫秒） |
| `max_output_size` | `size_t` | 最大输出大小 |
| `redirect_stdin` | `bool` | 重定向标准输入 |
| `redirect_stdout` | `bool` | 重定向标准输出 |
| `redirect_stderr` | `bool` | 重定向标准错误 |
| `limits` | `workbench_limits_t` | 资源限制配置 |
| `enable_limits` | `bool` | 是否启用资源控制 |

### workbench_limits_t — 资源限制配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `max_memory_bytes` | `size_t` | 最大内存限制（字节），0=无限制 |
| `max_cpu_time_ms` | `uint32_t` | 最大 CPU 时间（毫秒），0=无限制 |
| `max_output_bytes` | `size_t` | 最大输出大小（字节），0=默认 |
| `max_processes` | `uint32_t` | 最大子进程数，0=无限制 |
| `max_threads` | `uint32_t` | 最大线程数，0=无限制 |
| `max_file_size_bytes` | `size_t` | 最大文件大小（字节），0=无限制 |

### workbench_result_t — 执行结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `exit_code` | `int` | 退出码 |
| `timed_out` | `bool` | 是否超时 |
| `signaled` | `bool` | 是否被信号终止 |
| `signal` | `int` | 终止信号编号 |
| `stdout_data` | `char *` | 标准输出 |
| `stdout_size` | `size_t` | 标准输出大小 |
| `stderr_data` | `char *` | 标准错误 |
| `stderr_size` | `size_t` | 标准错误大小 |
| `start_time_ms` | `uint64_t` | 开始时间 |
| `end_time_ms` | `uint64_t` | 结束时间 |

### workbench_state_t — 工作台状态

| 枚举值 | 说明 |
|--------|------|
| `WORKBENCH_STATE_IDLE` | 空闲 |
| `WORKBENCH_STATE_RUNNING` | 运行中 |
| `WORKBENCH_STATE_STOPPED` | 已停止 |
| `WORKBENCH_STATE_ERROR` | 错误 |

## 接口说明

### 工作台核心 API

| 函数 | 说明 |
|------|------|
| `workbench_create(config)` | 创建工作台实例（config 可为 NULL 使用默认配置） |
| `workbench_destroy(wb)` | 销毁工作台实例（终止子进程） |
| `workbench_execute(wb, command, argv, result)` | 同步执行命令 |
| `workbench_execute_async(wb, command, argv)` | 异步执行命令 |
| `workbench_wait(wb, result, timeout_ms)` | 等待异步执行完成 |
| `workbench_terminate(wb)` | 终止执行 |
| `workbench_get_state(wb)` | 获取工作台状态 |
| `workbench_get_pid(wb)` | 获取进程 ID |
| `workbench_write_stdin(wb, data, size, written)` | 写入标准输入 |
| `workbench_read_stdout(wb, buf, size, read_size)` | 读取标准输出 |
| `workbench_read_stderr(wb, buf, size, read_size)` | 读取标准错误 |
| `workbench_result_free(result)` | 释放执行结果 |
| `workbench_default_config(config)` | 获取默认配置 |
| `workbench_set_limits(wb, limits)` | 设置资源限制 |
| `workbench_get_limits(wb, limits)` | 获取资源限制 |
| `workbench_get_usage(wb, memory_usage, cpu_usage)` | 获取资源使用情况 |

### 资源限制 API（workbench_limits）

| 函数 | 说明 |
|------|------|
| `limits_create(memory_limit_bytes, cpu_time_limit_ms, processes_limit)` | 创建资源限制上下文 |
| `limits_destroy(ctx)` | 销毁资源限制上下文 |
| `limits_attach(ctx)` | 将当前进程/线程附加到限制上下文 |
| `limits_detach(ctx)` | 从限制上下文分离 |
| `limits_set_memory(ctx, limit_bytes, mode)` | 设置内存限制 |
| `limits_set_cpu_time(ctx, limit_ms, mode)` | 设置 CPU 时间限制 |
| `limits_set_cpu_weight(ctx, weight, mode)` | 设置 CPU 权重（1-10000） |
| `limits_set_processes(ctx, limit, mode)` | 设置进程数限制 |
| `limits_set_threads(ctx, limit, mode)` | 设置线程数限制 |
| `limits_set_file_size(ctx, limit_bytes, mode)` | 设置文件大小限制 |
| `limits_set_file_descriptors(ctx, limit, mode)` | 设置文件描述符限制 |
| `limits_get_stats(ctx, stats)` | 获取资源统计 |
| `limits_check(ctx, type, status)` | 检查是否超限 |
| `limits_enforce(ctx)` | 强制执行限制（终止超限进程） |
| `limits_set_exceeded_callback(ctx, callback, user_data)` | 设置超限回调 |
| `limits_is_available()` | 检查平台是否支持资源限制 |

**限制执行模式**：

| 模式 | 说明 |
|------|------|
| `LIMIT_MODE_SOFT` | 软限制（仅警告） |
| `LIMIT_MODE_HARD` | 硬限制（阻止分配） |
| `LIMIT_MODE_ENFORCED` | 强制限制（终止进程） |

**平台实现**：
- Linux：cgroups v2（memory, cpu, pids）
- Windows：Job Objects with CPU/Memory limits
- macOS：Mach task with resource limits

## 使用示例

```c
#include "workbench.h"

workbench_config_t config;
workbench_default_config(&config);

config.timeout_ms = 10000;
config.enable_limits = true;
config.limits.max_memory_bytes = 256 * 1024 * 1024;  /* 256 MB */
config.limits.max_cpu_time_ms = 5000;                  /* 5 秒 */
config.limits.max_processes = 5;

workbench_t *wb = workbench_create(&config);

workbench_result_t result = {0};
char *argv[] = {"python3", "analyze.py", NULL};
int rc = workbench_execute(wb, "python3", argv, &result);

if (rc == 0) {
    printf("Exit code: %d\n", result.exit_code);
    printf("Duration: %lu ms\n", result.end_time_ms - result.start_time_ms);
    if (result.timed_out) {
        printf("Execution timed out!\n");
    }
    if (result.signaled) {
        printf("Killed by signal: %d\n", result.signal);
    }
    workbench_result_free(&result);
} else {
    printf("Execution failed: %d\n", rc);
}

workbench_destroy(wb);
```

## 安全流程

```
请求执行 → 输入清洗(Sanitizer) → 权限检查(Permission) → 资源分配(Limits)
    → 进程/容器创建 → 执行监控 → 结果收集 → 审计记录(Audit)
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `platform.h` | 平台抽象层 |
| `cupolas_utils.h` | 安全内存管理、日志宏 |

## 相关子系统

| 子系统 | 关系 |
|--------|------|
| [Sanitizer](../sanitizer/README.md) | 执行前对命令进行输入清洗 |
| [Permission](../permission/README.md) | 执行前进行权限检查 |
| [Audit](../audit/README.md) | 执行结果记录审计日志 |
| [Security](../security/README.md) | 运行时保护限制工作台进程行为 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
