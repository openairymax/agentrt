# Commons — 统一基础库

`agentos/commons/` 目录是 AgentOS 的统一基础库，为整个系统提供跨平台、跨语言、跨模块的通用基础设施。Commons 不依赖任何上层模块，所有原子内核、守护进程、安全组件均可基于它构建。

## 设计目标

- **零依赖抽象**：平台无关的类型系统与接口定义，确保内核与外围代码分离
- **统一错误契约**：全系统共享的错误码体系与异常传播机制
- **高性能基础设施**：内存池、无锁队列、零拷贝数据管道等底层原语
- **可观测性内建**：日志、指标、链路追踪的标准化采集接口
- **安全默认**：所有 I/O 路径默认经过参数校验、边界检查和资源限制

## 核心模块

| 模块 | 路径 | 职责 |
|------|------|------|
| **AgentOS 类型系统** | `agentos_types.h` | 跨语言基础类型定义（Result, Error, UUID, Timestamp 等） |
| **平台抽象层** | `platform/` | OS 抽象（Linux/Windows/MacOS），文件系统、线程、动态库加载 |
| **错误处理框架** | `error/` | 统一错误码（0xxx~9xxx 范围）、错误传播与堆栈追踪 |
| **日志系统** | `logging/` | 三层架构（core→atomic→service），同步/异步/安全模式 |
| **统一配置** | `config_unified/` | 三层配置模型（core→source→service），热重载、Schema 校验 |
| **内存管理** | `memory/` | 内存池、引用计数智能指针、零拷贝缓冲区 |
| **可观测性** | `observability/` | 基于 OpenTelemetry 的指标、追踪、健康检查 |
| **令牌管理** | `token/` | API Key / JWT 令牌的生命周期管理、轮换与校验 |
| **IPC 抽象** | `ipc/` | 跨进程通信抽象层（共享内存、Unix Socket、命名管道） |
| **同步原语** | `sync/` | 互斥锁、读写锁、信号量、屏障、无锁队列 |
| **网络工具** | `network/` | HTTP 客户端、URI 解析、连接池、重试策略 |
| **算法工具集** | `algorithm/` | 常用算法与数据结构（LRU Cache、Bloom Filter、限流器） |

## 架构分层

```
+------------------------------------------------------------------+
|                        上层模块（Daemon / Cupolas / Gateway...）     |
+------------------------------------------------------------------+
|  Utils 工具层  |  strategy  |  cognition  |  execution  |   ...   |
+----------------+------------+-------------+-------------+---------+
|  配置  |  日志  |  错误  |  内存  |  可观测性  |  令牌  |  IPC  |  同步  |
+--------+--------+--------+--------+-----------+-------+-------+-------+
|  平台抽象层（Platform Abstraction Layer）                           |
+------------------------------------------------------------------+
|  操作系统内核（Linux / Windows / MacOS）                            |
+------------------------------------------------------------------+
```

## 模块详解

### AgentOS 类型系统

定义全系统通用的基础类型：

| 类型 | 说明 |
|------|------|
| `agentos_result_t` | 统一返回值，包含错误码与附加信息 |
| `agentos_error_t` | 错误描述结构，含错误码、文件、行号、消息 |
| `agentos_uuid_t` | 128 位 UUID 标识符 |
| `agentos_timestamp_t` | 纳秒精度时间戳 |
| `agentos_buffer_t` | 安全缓冲区（带长度检查和边界保护） |
| `agentos_string_t` | 安全字符串（自动 null-terminate） |

### 平台抽象层

屏蔽 OS 差异，提供统一 API：

- **文件系统**：`platform_file_open/read/write/close`，路径规范化
- **线程与同步**：`platform_thread_create/mutex/semaphore` 抽象
- **动态库加载**：`platform_dl_open/sym/close`，跨平台 FFI 支持
- **系统信息**：CPU 核心数、内存大小、进程 ID、主机名

### 错误处理框架

统一错误码体系，按范围分类：

| 范围 | 分类 | 示例 |
|------|------|------|
| 0xxx | 通用错误 | 参数无效、内存不足、超时 |
| 1xxx | IPC 通信错误 | 连接断开、消息格式错误 |
| 2xxx | 任务调度错误 | 队列满、优先级无效、死锁检测 |
| 3xxx | 内存管理错误 | 页错误、越界访问、碎片过多 |
| 4xxx | 协议层错误 | 序列化失败、校验和无效 |
| 5xxx | 安全错误 | 权限不足、认证失败、注入检测 |
| 6xxx | 配置错误 | Schema 校验失败、键不存在 |
| 7xxx | 存储错误 | I/O 错误、文件损坏、空间不足 |
| 8xxx | 三方集成错误 | 模型调用失败、API 限流 |
| 9xxx | 内部错误 | 断言失败、状态不一致 |

### 日志系统

三层架构：

1. **Core 层**：日志核心，格式化、级别过滤、缓冲区管理
2. **Atomic 层**：原子写入、环形缓冲区、无锁队列
3. **Service 层**：远程日志聚合、结构化日志（JSON）、实时告警

支持同步/异步/安全三种写入模式，可动态切换。

### 统一配置

三层配置模型：

1. **Core 层**：Schema 定义、默认值、类型系统
2. **Source 层**：配置来源管理（文件/环境变量/远程/内存）
3. **Service 层**：运行时访问接口、热重载通知、变更监听

配置格式支持 JSON / YAML / TOML，所有配置变更均记录审计日志。

### 内存管理

- **内存池**：固定大小块分配，无碎片，O(1) 分配/释放
- **智能指针**：引用计数的共享所有权，自动释放
- **零拷贝缓冲区**：基于 region 的内存管理，避免大数据拷贝
- **内存屏障**：确保多线程环境下的可见性与顺序一致性

### Observability

基于 OpenTelemetry 的可观测性基础设施：

- **指标**：计数器、直方图、仪表盘，支持 Prometheus 导出
- **追踪**：分布式链路追踪，Span 上下文传播
- **健康检查**：组件健康状态聚合、存活/就绪探针

### 令牌管理

API Key 和 JWT 令牌的完整生命周期管理：

- **生成**：加密安全随机数 + 可配置前缀
- **存储**：哈希存储（bcrypt/argon2），不保存明文
- **校验**：快速验证缓存 + 定期同步黑名单
- **轮换**：自动过期、平滑轮换、旧令牌宽限期

### IPC 抽象

跨进程通信的统一抽象层：

- **共享内存**：基于 mmap 的零拷贝数据传输
- **Unix Socket**：基于 SOCK_SEQPACKET 的可靠消息传递
- **命名管道**：Windows 命名管道支持
- **消息协议**：内置长度前缀 + CRC32 校验的消息帧

### 同步原语

- **互斥锁**：支持普通/递归/超时模式
- **读写锁**：偏向读/偏向写可配置
- **信号量**：计数信号量，支持超时
- **屏障**：多线程同步屏障
- **无锁队列**：基于 CAS 的 MPSC / SPMC / MPMC 队列

### 网络工具

- **HTTP 客户端**：连接池、自动重试、断路器
- **URI 解析**：RFC 3986 兼容解析器
- **DNS 缓存**：带 TTL 的本地 DNS 缓存
- **速率限制**：令牌桶算法，支持 burst

### 算法工具集

- **LRU Cache**：O(1) 查找/插入/淘汰
- **Bloom Filter**：可配置假阳性率
- **限流器**：滑动窗口 / 令牌桶 / 漏桶
- **一致性哈希**：虚拟节点 + 加权分布

## 代码示例

```c
#include "commons/agentos_types.h"
#include "commons/logging/logging.h"
#include "commons/config_unified/config_unified.h"

// 初始化配置
agentos_config_t* config = config_unified_create("agentos_config.json");
config_unified_set_default(config, "logging.level", "info");

// 创建日志器
agentos_logger_t* logger = logging_create("my_module", LOG_LEVEL_INFO);
logging_info(logger, "系统初始化完成, config: %s", config_unified_path(config));

// 使用错误处理
agentos_result_t result = some_operation();
if (agentos_result_is_error(result)) {
    agentos_error_t* err = agentos_result_get_error(result);
    logging_error(logger, "操作失败: [%d] %s", err->code, err->message);
    return result;
}

// 使用内存池
agentos_mempool_t* pool = memory_pool_create(1024, 64); // 64个 1KB 块
void* block = memory_pool_alloc(pool);
memory_pool_free(pool, block);
memory_pool_destroy(pool);
```

## 与其它模块的关系

| 模块 | 关系 |
|------|------|
| **Atoms** | 直接使用 Commons 的平台抽象、内存管理、错误框架 |
| **Daemon** | 依赖 Commons 的日志、配置、网络工具 |
| **Cupolas** | 使用 Commons 的类型系统、同步原语 |
| **Gateway** | 使用 Commons 的网络工具、令牌管理 |
| **Manager** | 构建于 Commons 的配置系统之上 |

---

*AgentOS Commons — 统一基础库*
