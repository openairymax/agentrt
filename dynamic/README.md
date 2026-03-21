# Dynamic – AgentOS 运行时管理

**版本**：1.0.0.5  
**归属**：AgentOS 核心运行时  

---

## 1. 概述

Dynamic 是 AgentOS 的运行时管理模块，作为连接内核与外部世界的桥梁。它负责：

- **网关接入**：提供 HTTP/1.1、HTTP/2、WebSocket 及标准输入输出（stdio）接口，供客户端、工具或本地进程调用系统功能。
- **会话管理**：创建、查询、关闭会话，自动清理过期会话，维持系统状态的一致性。
- **健康检查**：定期执行组件健康检查，记录异常，为集群管理提供基础。
- **可观测性**：集成指标收集和链路追踪，导出 Prometheus 格式的指标及分布式追踪数据。

Dynamic 严格遵循微内核设计，仅通过系统调用（syscall）与内核交互，不直接访问内核内部实现，确保内核纯净。

---

## 2. 目录结构
```
dynamic/
├── CMakeLists.txt          # 构建文件
├── README.md               # 本文档
├── include/
│   └── dynamic.h           # 对外公共接口（启动/停止）
└── src/
    ├── server.c            # 核心控制器
    ├── server.h            # 内部类型定义
    ├── session.c           # 会话管理实现
    ├── session.h
    ├── health.c            # 健康检查实现
    ├── health.h
    ├── telemetry.c         # 可观测性实现
    ├── telemetry.h
    └── gateway/            # 网关实现
        ├── gateway.h       # 网关基类及操作表
        ├── http_gateway.c  # HTTP 网关（libmicrohttpd）
        ├── ws_gateway.c    # WebSocket 网关（libwebsockets）
        └── stdio_gateway.c # stdio 网关
```
---

## 3. 编译

### 3.1 依赖

- CMake 3.20+
- C 编译器（gcc / clang）
- pthread
- libmicrohttpd (≥0.9.70)
- libwebsockets (≥4.3)
- cJSON (≥1.7.15)
- AgentOS 基础库：`agentos_utils`（提供日志、指标、追踪）

### 3.2 构建步骤
```
mkdir build && cd build
cmake ../dynamic
make -j4
```
构建成功后，可执行文件 `agentos_dynamic` 位于 `build/` 下。

---

## 4. 运行

### 4.1 基本启动
```
./agentos_dynamic
```
**默认配置**：

- HTTP 端口：18789
- WebSocket 端口：18790
- stdio 网关启用
- 最大会话数：1000
- 会话超时：3600 秒
- 健康检查间隔：30 秒
- 指标路径：/metrics

### 4.2 自定义配置

当前版本暂不支持命令行参数，可通过修改 `src/server.c` 中的 `set_default_config` 函数或直接修改 `dynamic_config_t` 结构体来调整。

计划在后续版本中支持通过环境变量或配置文件覆盖。

### 4.3 停止

向进程发送 `SIGINT`（Ctrl+C）或 `SIGTERM` 即可优雅关闭。

---

## 5. 设计哲学

### 5.1 工程两论

- **层次分解**：网关、会话、健康、可观测性各层职责分明，相互独立。
- **反馈闭环**：健康检查器定期轮询各组件状态，异常信息通过日志反馈，形成闭环监控。
- **总体优化**：所有组件通过标准接口协作，便于替换和优化。

### 5.2 模块化与美学

- 每个模块有独立的 `.h/.c` 文件，内部结构清晰。
- 网关采用抽象基类 + 操作表，新增网关无需修改核心代码。
- 所有函数均包含完善的错误处理和资源释放，无内存泄漏。
- 命名统一，注释完整，代码如艺术品般整洁。

### 5.3 生产级保证

- **线程安全**：所有共享数据均使用互斥锁保护。
- **优雅关闭**：信号处理 + 条件变量确保所有资源正确释放。
- **可观测性**：内置指标和追踪，便于线上诊断。

---

## 6. 与内核交互

BaseRuntime 通过系统调用接口（`agentos_sys_*`）与内核通信，例如：

- `agentos_sys_task_submit`：提交自然语言任务。
- `agentos_sys_memory_search`：查询记忆。
- `agentos_sys_session_create`：创建会话。

实际调用在网关的回调函数中实现，例如 HTTP 网关的 `/rpc` 端点会解析 JSON-RPC 请求并派发到相应的系统调用。

---

## 7. 扩展性

- **新增网关**：实现 `gateway_t` 的操作表，在 `server.c` 的启动函数中添加即可。
- **新增健康检查组件**：调用 `health_checker_register` 注册检查函数。
- **自定义指标**：通过 `agentos_metrics_increment` 等 API 添加自定义指标。

---

© 2026 SPHARX Ltd. 保留所有权利。
