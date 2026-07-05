# Channel Daemon — IPC 通道守护进程

> **模块路径**: `agentrt/daemons/channel_d/` | **版本**: v0.1.0

## 概述

`daemons/channel_d/` 是 AgentRT 的进程间通信通道管理守护进程，负责管理 IPC 通道的创建、销毁、数据收发和状态监控。它支持 Unix Socket、共享内存和命名管道三种通道类型，为 AgentRT 各守护进程间的高效通信提供统一的通道抽象层（IMP-08），是 IPC Service Bus 的底层传输基础设施。

### 架构定位

```
channel_d/ → IPC Service Bus → 各守护进程间通信
    ↑
 通道管理层（Socket/SHM/Pipe）
```

### 核心职责

- **多通道类型支持**：Unix Socket、共享内存（SHM）、命名管道（Pipe）三种通道类型
- **通道生命周期管理**：通道的创建、销毁、启动、停止、打开、关闭
- **数据收发**：统一的 send/receive 接口，屏蔽不同通道类型的传输细节
- **状态监控**：通道状态跟踪（CLOSED/OPEN/ERROR/DRAINING）与健康检查
- **回调机制**：支持设置通道事件回调，实现事件驱动通信
- **延迟测量**：Ping/Pong 机制测量通道往返延迟

## 目录结构

```
channel_d/
├── CMakeLists.txt                    # 构建配置
├── README.md                         # 本文件
├── include/                          # 公共头文件
│   └── channel_service.h             # 通道服务统一接口（IMP-08）
├── src/                              # 实现文件
│   ├── main.c                        # 守护进程入口
│   └── channel_service.c             # 通道服务核心实现
└── tests/                            # 测试代码
    └── test_channel_e2e.c            # E2E 测试（P3-B01）
```

## 核心组件说明

### 通道类型

| 通道类型 | 枚举值 | 说明 |
|----------|--------|------|
| Unix Socket | `CHANNEL_TYPE_SOCKET` (0) | AF_UNIX SOCK_STREAM，非阻塞模式，backlog=128 |
| 共享内存 | `CHANNEL_TYPE_SHM` (1) | shm_open + mmap，消息格式 `[4字节长度][4字节标志][数据]`，atomic_thread_fence 同步 |
| 命名管道 | `CHANNEL_TYPE_PIPE` (2) | mkfifo 命名管道，非阻塞读写 |

### 通道状态

| 状态 | 说明 |
|------|------|
| CLOSED | 通道已关闭 |
| OPEN | 通道已打开，可正常通信 |
| ERROR | 通道处于错误状态 |
| DRAINING | 通道正在排空，等待剩余消息处理完毕 |

### 传输协议

- **Socket 传输**：`[4字节网络序长度][数据]`
- **SHM 传输**：写入共享内存 + 标志位，对端通过标志位检测并读取数据，使用 atomic_thread_fence 保证内存可见性
- **Pipe 传输**：非阻塞写入，直接传输原始数据

## 接口说明

### 通道服务生命周期（channel_service.h）

```c
agentrt_error_t channel_service_create(channel_service_t *service,
                                       const channel_service_config_t *config);
void channel_service_destroy(channel_service_t service);
agentrt_error_t channel_service_start(channel_service_t service);
agentrt_error_t channel_service_stop(channel_service_t service);
```

### 通道操作接口

```c
agentrt_error_t channel_open(channel_service_t service, channel_type_t type,
                             const char *name, channel_handle_t *handle);
agentrt_error_t channel_close(channel_service_t service, channel_handle_t handle);
agentrt_error_t channel_send(channel_service_t service, channel_handle_t handle,
                             const void *data, size_t len);
agentrt_error_t channel_receive(channel_service_t service, channel_handle_t handle,
                                void *buffer, size_t buf_size, size_t *out_len);
```

### 查询与回调接口

```c
agentrt_error_t channel_list(channel_service_t service, char **out_json);
agentrt_error_t channel_get_info(channel_service_t service, channel_handle_t handle,
                                 channel_info_t *info);
agentrt_error_t channel_set_callback(channel_service_t service, channel_handle_t handle,
                                     channel_event_cb_t callback, void *user_data);
agentrt_error_t channel_ping(channel_service_t service, channel_handle_t handle,
                             uint64_t *out_latency_ms);
bool channel_is_healthy(channel_service_t service, channel_handle_t handle);
```

### 配置结构体

```c
typedef struct {
    size_t max_channels;          // 最大通道数，默认 256
    size_t default_buffer_size;   // 默认缓冲区大小，默认 65536
    int socket_backlog;           // Socket 监听 backlog，默认 128
    uint32_t idle_timeout_ms;     // 空闲超时时间，默认 30000ms
} channel_service_config_t;
```

## 通信方式

| 方向 | 协议 | 说明 |
|------|------|------|
| 入站 | JSON-RPC 2.0 | 通过 IPC Service Bus 接收请求 |
| 通道传输 | Unix Socket | AF_UNIX SOCK_STREAM 非阻塞通信 |
| 通道传输 | 共享内存 | shm_open + mmap 高速通信 |
| 通道传输 | 命名管道 | mkfifo 进程间通信 |

### JSON-RPC 2.0 方法

| 方法 | 说明 |
|------|------|
| `channel.ping` | 测量通道往返延迟 |
| `channel.list` | 列出所有通道信息 |
| `channel.open` | 打开指定通道 |
| `channel.close` | 关闭指定通道 |
| `channel.send` | 通过通道发送数据 |
| `channel.health` | 查询通道健康状态 |

## 健康检查机制

- **Ping 延迟检测**：通过 `channel.ping` 测量往返延迟，E2E 验收标准 < 10ms
- **通道状态检查**：`channel_is_healthy` 检查通道是否处于 OPEN 状态
- **E2E 测试覆盖**（P3-B01）：6 个测试组
  - 生命周期测试
  - 健康检查测试
  - 打开/关闭/列表测试
  - 发送/接收 + 延迟测试（验收：往返 < 10ms）
  - 回调机制测试
  - 错误处理测试

## 跨平台支持

| 平台 | Unix Socket | 共享内存 | 命名管道 |
|------|-------------|----------|----------|
| Linux | ✅ AF_UNIX | ✅ shm_open + mmap | ✅ mkfifo |
| Windows | ⚠️ 有限支持 | ⚠️ 有限支持 | ⚠️ 有限支持 |

## 依赖关系

```
channel_d
├── common (svc_common, agentrt_common)
└── Threads::Threads
```

## 构建说明

```bash
# 构建通道守护进程
cmake -B build -DBUILD_TESTS=ON
cmake --build build --target channel_d

# 运行 E2E 测试
ctest --test-dir build -R "test_channel_e2e" -V
```

## 使用示例

### 启动通道守护进程

```bash
# 默认启动
./channel_d

# 指定配置文件
./channel_d -c channel_config.json

# 指定服务名称
./channel_d -n my-channel-service

# 查看帮助
./channel_d -h
```

### 命令行参数

| 参数 | 说明 |
|------|------|
| `-c <path>` | 指定配置文件路径 |
| `-s <path>` | 指定 Socket 路径 |
| `-n <name>` | 指定服务名称 |
| `-h` | 显示帮助信息 |

### 信号处理

| 信号 | 行为 |
|------|------|
| SIGINT | 优雅关闭 |
| SIGTERM | 优雅关闭 |
| SIGUSR1 | 动态调整日志级别 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
