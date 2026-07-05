# Platform — 平台抽象层

**模块路径**: `agentrt/commons/platform/`
**版本**: v0.1.0

## 概述

Platform 是 AgentRT 的跨平台抽象层，屏蔽 Linux、Windows、macOS 三大操作系统的底层差异，为上层模块提供统一的系统调用 API。该模块是 Commons 基础库的底层基石，所有涉及文件 I/O、线程同步、网络通信、动态库加载的模块均通过此抽象层访问操作系统能力。

## 设计目标

- **平台无关 API**：统一接口隐藏 `#ifdef _WIN32` / `#ifdef __linux__` 等平台条件编译
- **零开销抽象**：内联函数 + 宏定义，编译期优化，无运行时性能损失
- **类型安全**：强类型封装，避免 `void*` 和裸指针的平台差异
- **最小依赖**：仅依赖 C 标准库和 POSIX/Win32 API，不引入第三方库

## 目录结构

```
platform/
├── include/
│   ├── platform.h               # 平台检测与基础定义（线程、互斥锁、条件变量、Socket、进程、时间等）
│   └── export.h                 # 符号导出控制（DLL/SO 可见性）
├── compat/
│   ├── stdbool.h                # C99 stdbool 兼容头文件（旧编译器）
│   └── stdint.h                 # C99 stdint 兼容头文件（旧编译器）
├── platform.c                   # 平台抽象实现（线程、文件系统、网络、随机数）
└── README.md                    # 本文档
```

## 核心组件

### 1. 平台检测 (`platform.h`)

自动识别目标平台并定义对应宏：

| 宏 | 平台 |
|-----|------|
| `AGENTRT_PLATFORM_LINUX` | Linux |
| `AGENTRT_PLATFORM_WINDOWS` | Windows (Win32/Win64) |
| `AGENTRT_PLATFORM_MACOS` | macOS (Darwin) |
| `AGENTRT_PLATFORM_POSIX` | 任意 POSIX 兼容系统 |

### 2. 线程与同步原语

| 类型 | 说明 |
|------|------|
| `agentrt_thread_t` | 跨平台线程句柄，封装 `pthread_t` / `HANDLE` |
| `agentrt_mutex_t` | 跨平台互斥锁，封装 `pthread_mutex_t` / `CRITICAL_SECTION` |
| `agentrt_cond_t` | 跨平台条件变量，封装 `pthread_cond_t` / `CONDITION_VARIABLE` |
| `agentrt_rwlock_t` | 跨平台读写锁，封装 `pthread_rwlock_t` / `SRWLOCK` |

### 3. 平台抽象实现 (`platform.c`)

提供统一的跨平台实现：

- **线程管理**：`agentrt_thread_create` / `agentrt_thread_join` / `agentrt_thread_detach`
- **互斥锁**：`agentrt_mutex_init` / `agentrt_mutex_lock` / `agentrt_mutex_unlock` / `agentrt_mutex_destroy`
- **条件变量**：`agentrt_cond_init` / `agentrt_cond_wait` / `agentrt_cond_signal` / `agentrt_cond_broadcast` / `agentrt_cond_destroy`
- **Socket 网络**：`agentrt_socket_create` / `agentrt_socket_bind` / `agentrt_socket_listen` / `agentrt_socket_accept` / `agentrt_socket_connect` / `agentrt_socket_send` / `agentrt_socket_recv` / `agentrt_socket_close`
- **进程管理**：`agentrt_process_spawn` / `agentrt_process_wait` / `agentrt_process_kill`
- **时间与休眠**：`agentrt_sleep_ms` / `agentrt_get_time_ms` / `agentrt_get_monotonic_time`
- **文件系统**：`agentrt_file_exists` / `agentrt_mkdir` / `agentrt_path_join` / `agentrt_get_temp_dir`
- **随机数**：`agentrt_random_bytes`（Windows 使用 `BCryptGenRandom`，POSIX 使用 `/dev/urandom`）
- **动态库加载**：`agentrt_dlopen` / `agentrt_dlsym` / `agentrt_dlclose`

### 4. 符号导出控制 (`export.h`)

跨平台 DLL/SO 符号可见性控制：

| 宏 | 说明 |
|-----|------|
| `AGENTRT_EXPORT` | 导出符号（`__declspec(dllexport)` / `__attribute__((visibility("default")))`） |
| `AGENTRT_IMPORT` | 导入符号（`__declspec(dllimport)`） |
| `AGENTRT_LOCAL` | 隐藏符号（`__attribute__((visibility("hidden")))`） |

### 5. 兼容头文件 (`compat/`)

为旧编译器或不完整 C 标准库提供兼容性头文件：

| 文件 | 说明 |
|------|------|
| `stdbool.h` | 提供 `bool`、`true`、`false` 定义（非 C99 编译器） |
| `stdint.h` | 提供 `int8_t`、`uint32_t` 等定宽整数类型 |

## 使用示例

```c
#include "platform.h"

/* 线程创建 */
agentrt_thread_t thread;
agentrt_thread_create(&thread, my_thread_func, my_arg);

/* 互斥锁 */
agentrt_mutex_t mutex;
agentrt_mutex_init(&mutex);
agentrt_mutex_lock(&mutex);
/* ... 临界区 ... */
agentrt_mutex_unlock(&mutex);
agentrt_mutex_destroy(&mutex);

/* 休眠 */
agentrt_sleep_ms(100);

/* 文件系统 */
if (!agentrt_file_exists("/tmp/myapp")) {
    agentrt_mkdir("/tmp/myapp");
}

/* 等待线程完成 */
agentrt_thread_join(thread, NULL);
```

## 平台差异适配表

| 特性 | Linux | Windows | macOS |
|------|-------|---------|-------|
| 线程 API | pthread | Win32 Thread | pthread |
| 互斥锁 | pthread_mutex_t | CRITICAL_SECTION | pthread_mutex_t |
| Socket | BSD socket | Winsock2 | BSD socket |
| 动态库 | dlopen | LoadLibrary | dlopen |
| 路径分隔符 | `/` | `\\` | `/` |
| 随机数 | /dev/urandom | BCryptGenRandom | /dev/urandom |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| C 标准库 | `<stdint.h>`、`<stdbool.h>`、`<stddef.h>` |
| POSIX 线程（Linux/macOS） | `libpthread` |
| Win32 API（Windows） | `kernel32.lib`、`ws2_32.lib` |

> Platform 模块不依赖任何 AgentRT 内部模块，是 Commons 的零依赖基础层。

---

© 2026 SPHARX Ltd. All Rights Reserved.