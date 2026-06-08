# Daemon Examples — 守护进程使用示例

> **模块路径**: `agentos/daemon/examples/` | **版本**: v0.0.5

## 概述

`daemon/examples/` 包含 AgentOS 守护进程服务的使用示例代码，演示如何使用 `svc_common.h` 中定义的服务管理框架 API，包括服务的完整生命周期管理（创建、初始化、启动、暂停、恢复、停止、销毁）。

## 目录结构

```
examples/
├── README.md                  # 本文件
└── example_svc_usage.c        # 服务管理框架使用示例
```

## 核心组件说明

### example_svc_usage.c

服务管理框架完整使用示例，演示以下功能：

| 步骤 | API | 说明 |
|------|-----|------|
| 1 | `agentos_service_create()` | 创建服务实例，配置服务名称、版本、能力等 |
| 2 | `agentos_service_init()` | 初始化服务，分配服务上下文 |
| 3 | `agentos_service_start()` | 启动服务 |
| 4 | `agentos_service_get_state()` | 查询服务状态 |
| 5 | `agentos_service_healthcheck()` | 执行健康检查 |
| 6 | `agentos_service_get_stats()` | 获取服务统计信息 |
| 7 | `agentos_service_pause()` / `agentos_service_resume()` | 暂停/恢复服务 |
| 8 | `agentos_service_stop()` | 停止服务（支持强制停止） |
| 9 | `agentos_service_destroy()` | 销毁服务实例 |
| 10 | `agentos_service_count()` | 查询注册表中的服务数量 |

### 服务接口定义

示例展示了如何实现 `agentos_svc_interface_t` 接口：

```c
agentos_svc_interface_t iface = {
    .init = example_service_init,
    .start = example_service_start,
    .stop = example_service_stop,
    .destroy = example_service_destroy,
    .healthcheck = example_service_healthcheck
};
```

### 服务能力标志

示例中使用了以下服务能力：

| 能力 | 枚举值 | 说明 |
|------|--------|------|
| 异步 | `AGENTOS_SVC_CAP_ASYNC` | 支持异步操作 |
| 可暂停 | `AGENTOS_SVC_CAP_PAUSEABLE` | 支持暂停/恢复 |

## 依赖关系

```
examples
├── common/include (svc_common.h, memory_compat.h)
├── agentos_common (libagentos_common)
└── svc_common (libsvc_common)
```

## 构建说明

```bash
# Linux/macOS
gcc -o example_svc_usage example_svc_usage.c \
    -I./common/include \
    -L./build/daemon/common \
    -lsvc_common -lagentos_common -lpthread

# Windows
cl example_svc_usage.c /I./common/include /link svc_common.lib agentos_common.lib
```

## 使用说明

```bash
# 编译并运行
./example_svc_usage
```

程序将依次演示服务的创建、初始化、启动、状态查询、健康检查、暂停/恢复、停止和销毁全过程。

---

© 2026 SPHARX Ltd. All Rights Reserved.
