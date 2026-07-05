# Commons — 统一基础库

**模块路径**: `agentrt/commons/`
**版本**: v0.1.0

## 概述

Commons 是 AgentRT 的统一基础库，为整个系统提供跨平台、跨语言、跨模块的通用基础设施。Commons 不依赖任何上层模块，所有原子核心、守护进程、安全组件均可基于它构建。作为全项目的类型定义权威源（Authoritative Source），Commons 确保了模块间类型一致性，消除了跨模块类型冲突。

## 设计目标

- **零依赖抽象**：平台无关的类型系统与接口定义，确保内核与外围代码分离
- **统一错误契约**：全系统共享的错误码体系（`agentrt_error_t`）与异常传播机制
- **高性能基础设施**：内存池、无锁队列、零拷贝数据管道等底层原语
- **可观测性内建**：日志、指标、链路追踪的标准化采集接口
- **安全默认**：所有 I/O 路径默认经过参数校验、边界检查和资源限制

## 目录结构

```
commons/
├── CMakeLists.txt               # CMake 构建配置
├── README.md                    # 本文档
├── platform/                    # 平台抽象层
│   ├── include/
│   │   ├── platform.h           # 平台检测与基础定义
│   │   └── export.h             # 符号导出控制
│   ├── compat/                  # 平台兼容头文件
│   │   ├── stdbool.h
│   │   └── stdint.h
│   └── platform.c               # 平台抽象实现
├── include/                     # 全局公共头文件
│   └── agentrt_types.h          # 统一类型与错误码定义
├── utils/                       # 工具模块集合
│   ├── include/                 # 跨模块共享头文件
│   │   ├── atomic_compat.h      # 跨平台原子操作兼容层
│   │   └── check.h              # 通用检查宏
│   ├── cognition/               # 认知管理（Agent 信息、任务调度、计划生成）
│   ├── config_unified/          # 统一配置管理（三层：Core→Source→Service）
│   ├── execution/               # 命令执行引擎（安全校验、跨平台）
│   ├── logging/                 # 日志系统（三层：Core→Atomic→Service）
│   ├── strategy/                # 加权评分策略引擎
│   ├── memory/                  # 内存管理（内存池、智能指针、零拷贝）
│   ├── sync/                    # 同步原语（8+ 种锁与队列）
│   ├── error/                   # 错误处理框架
│   ├── observability/           # 可观测性（OpenTelemetry 指标/追踪）
│   ├── token/                   # 令牌管理（API Key/JWT 生命周期）
│   ├── cost/                    # 成本估算与控制
│   ├── resource/                # 资源保护与配额
│   ├── security/                # 输入校验与安全过滤
│   ├── config/                  # 配置解析（JSON/YAML/TOML）[注：已合并至 config_unified]
│   ├── cache/                   # 缓存管理（LRU/TTL）
│   ├── compat/                  # 跨版本/跨平台兼容性适配
│   ├── compliance/              # 合规性校验与策略执行
│   ├── quality/                 # 代码质量检查
│   ├── types/                   # 通用类型定义与转换
│   ├── string/                  # 字符串操作与格式化
│   ├── io/                      # 文件 I/O 工具
│   ├── uuid/                    # UUID 生成与解析
│   ├── network/                 # 网络工具（HTTP/URI/DNS）
│   ├── ipc/                     # IPC 抽象层
│   └── platform/                # 平台相关工具函数
└── tests/                       # 测试套件
    ├── utils/                   # 测试工具框架
    ├── unit/                    # 单元测试
    └── integration/             # 集成测试
```

## 核心组件

### AgentRT 类型系统（`agentrt_types.h`）

作为全项目唯一的类型定义权威源，定义了：

| 类型 | 说明 |
|------|------|
| `agentrt_error_t` | 统一错误码类型（`int32_t`，负值为错误，0 为成功） |
| `agentrt_ipc_header_t` | 应用级 IPC 消息头（magic/version/type/flags/msg_id 等） |
| `agentrt_ipc_message_t` | 应用级 IPC 消息结构（header + payload） |
| `agentrt_task_id_t` | 任务 ID 类型（`uint64_t`） |
| `agentrt_message_id_t` | 消息 ID 类型（`uint64_t`） |

统一错误码体系（`AGENTRT_E*`）覆盖：参数无效、内存不足、权限不足、超时、I/O 错误、协议错误、配额超限等 29 种标准错误。

### 平台抽象层（`platform/`）

屏蔽 OS 差异，提供统一 API：

- **平台检测**：自动识别 Linux / Windows / macOS
- **文件系统**：路径规范化、文件操作抽象
- **线程与同步**：`agentrt_thread_t`、`agentrt_mutex_t`、`agentrt_cond_t` 等
- **动态库加载**：跨平台 FFI 支持
- **系统信息**：CPU 核心数、内存大小、进程 ID

### 原子操作兼容层（`atomic_compat.h`）

| 后端 | 适用环境 | 实现方式 |
|------|----------|----------|
| C11 stdatomic | Linux / macOS（C11+ 编译器） | `<stdatomic.h>` |
| Windows Interlocked | Windows（MSVC / MinGW） | `<intrin.h>` Interlocked API |
| POSIX fallback | 旧版 GCC/Clang 环境 | `__atomic` builtins |

覆盖类型：`atomic_bool`、`atomic_int`、`atomic_uint`、`atomic_int64_t`、`atomic_uint64_t`、`atomic_size_t` 等 11 种。

### 统一内存管理（`memory_compat.h`）

| 宏 | 替代 | 说明 |
|------|------|------|
| `AGENTRT_MALLOC(size)` | `malloc(size)` | 统一内存分配 |
| `AGENTRT_CALLOC(num, size)` | `calloc(num, size)` | 统一零初始化分配 |
| `AGENTRT_FREE(ptr)` | `free(ptr)` | 统一内存释放 |

## 接口说明

### 日志系统

三层架构：Core 层（格式化/级别过滤）→ Atomic 层（无锁队列/批量写入）→ Service 层（远程聚合/告警/查询）

详见 [logging/README.md](utils/logging/README.md)

### 统一配置

三层模型：Core 层（Schema/类型系统）→ Source 层（文件/环境变量/远程/内存）→ Service 层（热重载/加密/版本管理）

详见 [config_unified/README.md](utils/config_unified/README.md)

### 认知管理

Agent 信息管理、任务调度、计划生成与协调

详见 [cognition/README.md](utils/cognition/README.md)

### 命令执行引擎

安全校验、跨平台命令执行、结果结构化

详见 [execution/README.md](utils/execution/README.md)

### 加权评分策略

多维度加权评分、最优选择、权重归一化

详见 [strategy/README.md](utils/strategy/README.md)

### 同步原语（`sync/`）

提供 8+ 种同步原语：互斥锁（普通/递归/超时）、读写锁、信号量、屏障、自旋锁、条件变量、事件、无锁队列。

### 令牌管理（`token/`）

- `counter.c`：Token 计数器
- `budget.c`：预算控制
- `token_standard.c`：标准定义

### 可观测性（`observability/`）

基于 OpenTelemetry：指标（`metrics.c`）、追踪（`trace.c`）、日志桥接（`logger.c`）。

## 依赖关系

| 依赖 | 必需 | 说明 |
|------|------|------|
| C11 编译器 | 是 | 支持 `<stdatomic.h>` |
| pthreads / Win32 threads | 是 | 线程与同步原语 |
| libyaml | 否 | 完整 YAML 支持 |
| cJSON | 否 | JSON 配置解析 |

> **BAN-12**：外部依赖由根 `CMakeLists.txt` 集中检测，子模块不得独立调用 `find_package`。检测结果通过 `AGENTRT_HAS_CJSON` 等 CMake 缓存变量传递。

## 构建说明

```bash
# 在项目根目录
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .
```

CMake 选项：
- `BUILD_TESTS`（默认 ON）：构建单元测试和集成测试
- `AGENTRT_HAS_CJSON`：由根 CMakeLists 自动检测
- `AGENTRT_HAS_YAML`：由根 CMakeLists 自动检测

## 使用示例

```c
#include "agentrt_types.h"
#include "logging.h"
#include "config_unified.h"

log_config_t log_cfg = {
    .level = LOG_LEVEL_INFO,
    .output = LOG_OUTPUT_CONSOLE,
    .format = LOG_FORMAT_JSON
};
log_init(&log_cfg);

config_context_t *ctx = config_context_create("myapp");
config_value_t *val = CONFIG_STRING("0.0.0.0");
config_context_set(ctx, "server.host", val);

LOG_INFO("系统初始化完成, host: %s",
         CONFIG_GET_STRING_SAFE(ctx, "server.host", "localhost"));

log_cleanup();
config_context_destroy(ctx);
```

## 与其它模块的关系

| 模块 | 关系 |
|------|------|
| **Atoms** | 直接使用 Commons 的平台抽象、内存管理、错误框架 |
| **Daemon** | 依赖 Commons 的日志、配置、网络工具 |
| **Cupolas** | 使用 Commons 的类型系统、同步原语、内存管理宏 |
| **Gateway** | 使用 Commons 的网络工具、令牌管理 |
| **Manager** | 构建于 Commons 的配置系统之上 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
