# Workbench — 安全工作台

**模块路径**: `agentos/cupolas/src/workbench/`
**版本**: v0.0.5

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

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `max_cpu_time_ms` | `uint32_t` | 30000 | 最大 CPU 时间（毫秒） |
| `max_memory_mb` | `uint32_t` | 512 | 最大内存（MB） |
| `max_file_size_kb` | `uint32_t` | 10240 | 最大文件大小（KB） |
| `max_processes` | `uint32_t` | 10 | 最大子进程数 |
| `max_output_bytes` | `uint32_t` | 1048576 | 最大输出大小（字节） |
| `network_enabled` | `bool` | false | 是否允许网络访问 |
| `filesystem_readonly` | `bool` | true | 文件系统是否只读 |
| `isolation_level` | `workbench_isolation_t` | PROCESS | 隔离级别 |

### workbench_result_t — 执行结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `exit_code` | `int` | 退出码 |
| `stdout_output` | `char *` | 标准输出 |
| `stderr_output` | `char *` | 标准错误 |
| `cpu_time_ms` | `uint64_t` | 实际 CPU 时间 |
| `memory_peak_kb` | `uint64_t` | 峰值内存使用 |
| `timed_out` | `bool` | 是否超时 |
| `oom_killed` | `bool` | 是否因内存超限被杀 |

### workbench_isolation_t — 隔离级别

| 枚举值 | 说明 |
|--------|------|
| `WORKBENCH_ISOLATION_NONE` | 无隔离（仅用于可信代码） |
| `WORKBENCH_ISOLATION_PROCESS` | 进程级隔离（fork + 资源限制） |
| `WORKBENCH_ISOLATION_CONTAINER` | 容器级隔离（Linux namespace + cgroup） |

## 接口说明

### 工作台核心 API

| 函数 | 说明 |
|------|------|
| `workbench_create(config)` | 创建工作台实例 |
| `workbench_destroy(wb)` | 销毁工作台实例 |
| `workbench_execute(wb, command, agent_id, result)` | 执行命令 |
| `workbench_execute_script(wb, script, language, agent_id, result)` | 执行脚本 |
| `workbench_cancel(wb, execution_id)` | 取消执行 |
| `workbench_get_status(wb, execution_id)` | 获取执行状态 |
| `workbench_list_executions(wb)` | 列出所有执行 |

### 进程管理 API

| 函数 | 说明 |
|------|------|
| `workbench_process_create(config)` | 创建进程管理器 |
| `workbench_process_destroy(pm)` | 销毁进程管理器 |
| `workbench_process_spawn(pm, command, result)` | 启动子进程 |
| `workbench_process_kill(pm, pid)` | 终止子进程 |
| `workbench_process_wait(pm, pid, timeout_ms)` | 等待子进程结束 |
| `workbench_process_get_stats(pm, pid, stats)` | 获取进程统计 |

### 容器隔离 API

| 函数 | 说明 |
|------|------|
| `workbench_container_create(config)` | 创建容器 |
| `workbench_container_destroy(container)` | 销毁容器 |
| `workbench_container_run(container, command, result)` | 在容器中执行命令 |
| `workbench_container_pause(container)` | 暂停容器 |
| `workbench_container_resume(container)` | 恢复容器 |
| `workbench_container_get_info(container, info)` | 获取容器信息 |

### 资源限制 API

| 函数 | 说明 |
|------|------|
| `workbench_limits_apply(pid, config)` | 对进程应用资源限制 |
| `workbench_limits_check(pid, config)` | 检查进程是否超限 |
| `workbench_limits_get_usage(pid, usage)` | 获取资源使用情况 |

## 使用示例

```c
#include "workbench.h"

workbench_config_t config = {
    .max_cpu_time_ms = 10000,
    .max_memory_mb = 256,
    .max_file_size_kb = 5120,
    .max_processes = 5,
    .max_output_bytes = 65536,
    .network_enabled = false,
    .filesystem_readonly = true,
    .isolation_level = WORKBENCH_ISOLATION_PROCESS
};

workbench_t *wb = workbench_create(&config);

workbench_result_t result;
int rc = workbench_execute(wb, "python3 analyze.py", "agent-001", &result);

if (rc == 0) {
    printf("Exit code: %d\n", result.exit_code);
    printf("CPU time: %lu ms\n", result.cpu_time_ms);
    printf("Memory peak: %lu KB\n", result.memory_peak_kb);
    if (result.timed_out) {
        printf("Execution timed out!\n");
    }
} else {
    printf("Execution failed\n");
}

workbench_destroy(wb);
```

## 隔离级别对比

| 特性 | NONE | PROCESS | CONTAINER |
|------|------|---------|-----------|
| 文件系统隔离 | 无 | chroot | Mount namespace |
| 网络隔离 | 无 | 可选 | Network namespace |
| PID 隔离 | 无 | 无 | PID namespace |
| 用户隔离 | 无 | setuid | User namespace |
| 资源限制 | 无 | setrlimit | cgroup v2 |
| IPC 隔离 | 无 | 无 | IPC namespace |
| 适用场景 | 可信代码 | 一般不可信代码 | 高风险不可信代码 |

## 安全流程

```
请求执行 → 输入清洗(Sanitizer) → 权限检查(Permission) → 资源分配(Limits)
    → 进程/容器创建 → 执行监控 → 结果收集 → 审计记录(Audit)
```

## 相关子系统

| 子系统 | 关系 |
|--------|------|
| [Sanitizer](../sanitizer/README.md) | 执行前对命令进行输入清洗 |
| [Permission](../permission/README.md) | 执行前进行权限检查 |
| [Audit](../audit/README.md) | 执行结果记录审计日志 |
| [Security](../security/README.md) | 运行时保护限制工作台进程行为 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
