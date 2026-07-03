# Platform — 平台抽象模块

**模块路径**: `agentos/commons/utils/platform/`
**版本**: v0.1.0

## 概述

Platform 模块提供面向应用层的高级跨平台工具集，封装了操作系统差异，为上层业务模块提供统一的文件系统操作、环境变量管理、路径操作和时间工具。该模块位于 `utils/platform/`，与顶层 `platform/` 模块（系统层低级抽象）形成互补，专注于业务逻辑代码所需的跨平台能力。子进程执行统一使用顶层 `platform.h` 的 `agentos_process_run_capture()`（fork+execvp，不经 shell），消除命令注入风险。

## 设计目标

- **统一接口**：所有平台差异通过本模块透明处理，上层代码无需编写 `#ifdef _WIN32`
- **最小惊讶**：API 设计符合直觉，参数语义清晰，返回值语义一致
- **文件系统操作**：递归目录创建、文件复制/移动、路径规范化等
- **环境与路径**：环境变量读写、路径拼接/解析/规范化
- **系统服务**：跨平台时间戳、休眠

## 与顶层 platform/ 模块的区别

| 维度 | 本模块 (utils/platform/) | 顶层模块 (platform/) |
|------|------------------------|---------------------|
| **位置** | utils/platform/ | platform/ |
| **抽象层级** | 应用层 (High-Level) | 系统层 (Low-Level) |
| **核心功能** | 文件系统、环境变量、路径操作 | 线程原语、Socket、时间 |
| **使用场景** | 业务逻辑代码 | 基础设施代码 |
| **性能要求** | 一般 | 关键路径优化 |
| **典型用户** | cognition, strategy 等业务模块 | sync, ipc 等底层模块 |

## 目录结构

```
platform/
├── include/
│   └── platform_adapter.h     # 平台适配器公共接口定义
├── src/
│   └── platform_adapter.c     # 平台适配器实现
└── README.md                  # 本文档
```

## 核心数据结构

### platform_type_t — 平台类型枚举

| 值 | 说明 |
|-----|------|
| `PLATFORM_UNKNOWN` | 未知平台 |
| `PLATFORM_WINDOWS` | Windows |
| `PLATFORM_LINUX` | Linux |
| `PLATFORM_MACOS` | macOS |
| `PLATFORM_UNIX` | 其他 Unix 系统 |

### platform_file_info_t — 文件信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `path` | `const char *` | 文件路径 |
| `size` | `size_t` | 文件大小（字节） |
| `mtime` | `time_t` | 最后修改时间 |
| `is_directory` | `bool` | 是否为目录 |
| `exists` | `bool` | 是否存在 |

## 接口说明

### 平台信息

| 函数 | 说明 |
|------|------|
| `platform_get_type()` | 获取当前平台类型 |
| `platform_get_name()` | 获取平台名称字符串（"Windows"/"Linux"/"macOS"） |

### 文件系统操作

| 函数 | 说明 |
|------|------|
| `platform_get_file_info(path)` | 获取文件信息 |
| `platform_mkdir(path)` | 创建单层目录 |
| `platform_mkdir_recursive(path)` | 递归创建目录 |
| `platform_unlink(path)` | 删除文件 |
| `platform_rmdir(path)` | 删除目录 |
| `platform_copy_file(src, dest)` | 复制文件 |
| `platform_move_file(src, dest)` | 移动文件 |

### 环境与路径

| 函数 | 说明 |
|------|------|
| `platform_get_env(name, default_value)` | 获取环境变量（返回需 `AGENTOS_FREE` 释放） |
| `platform_set_env(name, value)` | 设置环境变量 |
| `platform_get_cwd()` | 获取当前工作目录（返回需 `AGENTOS_FREE` 释放） |
| `platform_chdir(path)` | 改变当前工作目录 |
| `platform_get_temp_dir()` | 获取临时目录（返回需 `AGENTOS_FREE` 释放） |
| `platform_get_temp_file(prefix)` | 生成临时文件路径（返回需 `AGENTOS_FREE` 释放） |
| `platform_path_join(path1, path2)` | 路径连接（自动处理分隔符，返回需 `AGENTOS_FREE` 释放） |
| `platform_path_normalize(path)` | 路径规范化（返回需 `AGENTOS_FREE` 释放） |
| `platform_path_basename(path)` | 获取路径文件名部分（返回需 `AGENTOS_FREE` 释放） |
| `platform_path_dirname(path)` | 获取路径目录部分（返回需 `AGENTOS_FREE` 释放） |
| `platform_path_exists(path)` | 检查路径是否存在 |
| `platform_path_is_directory(path)` | 检查路径是否为目录 |
| `platform_path_is_file(path)` | 检查路径是否为文件 |

### 系统服务

| 函数 | 说明 |
|------|------|
| `platform_get_timestamp_ms()` | 获取毫秒级时间戳 |
| `platform_get_timestamp_us()` | 获取微秒级时间戳 |
| `platform_sleep_ms(ms)` | 休眠指定毫秒数 |
| `platform_adapter_init()` | 初始化平台适配器（Windows 下初始化 Winsock） |
| `platform_adapter_cleanup()` | 清理平台适配器 |

## 使用示例

```c
#include "platform_adapter.h"

// 初始化平台适配器
platform_adapter_init();

// 获取平台信息
printf("Platform: %s\n", platform_get_name());

// 文件系统操作
if (!platform_path_exists("/tmp/agentos_data")) {
    platform_mkdir_recursive("/tmp/agentos_data");
}

platform_file_info_t info = platform_get_file_info("/tmp/agentos_data/config.json");
if (info.exists && !info.is_directory) {
    printf("Config size: %zu bytes, modified: %ld\n", info.size, info.mtime);
}

// 路径操作
char *joined = platform_path_join("/tmp/agentos_data", "output.log");
char *basename = platform_path_basename(joined);
printf("Basename: %s\n", basename);
AGENTOS_FREE(basename);
AGENTOS_FREE(joined);

// 环境变量
char *home = platform_get_env("HOME", "/home/default");
printf("HOME: %s\n", home);
AGENTOS_FREE(home);

// 时间戳
uint64_t start = platform_get_timestamp_ms();
platform_sleep_ms(100);
uint64_t elapsed = platform_get_timestamp_ms() - start;
printf("Slept for %llu ms\n", (unsigned long long)elapsed);

// 清理
platform_adapter_cleanup();
```

## 平台差异

| 特性 | Linux | Windows | macOS |
|------|-------|---------|-------|
| 路径分隔符 | `/` | `\` | `/` |
| 目录创建 | `mkdir(path, 0755)` | `_mkdir(path)` | `mkdir(path, 0755)` |
| 临时目录 | `TMPDIR` 或 `/tmp` | `GetTempPathA()` | `TMPDIR` 或 `/tmp` |
| 临时文件 | `mkstemp()` | `GetTempFileNameA()` | `mkstemp()` |
| 环境变量 | `getenv/setenv` | `GetEnvironmentVariableA/SetEnvironmentVariableA` | `getenv/setenv` |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `memory_compat.h` | 统一内存管理宏（`AGENTOS_MALLOC`、`AGENTOS_FREE` 等） |
| `string_compat.h` | 字符串操作兼容层 |
| `platform.h` | 顶层系统级平台抽象（时间戳等） |
| `error.h` | 统一错误码定义 |

---

© 2026 SPHARX Ltd. All Rights Reserved.