# Gateway Daemon (gateway_d)

AgentOS 网关守护进程，负责管理 HTTP/WebSocket/Stdio 网关实例。

## 架构定位

```
gateway_d/ → agentos/gateway/ → agentos/atoms/syscall/
     ↑           ↑
  守护进程    协议转换层
```

**职责边界**：
- `gateway_d`: 守护进程管理、配置加载、服务生命周期
- `agentos/gateway/`: 协议转换（HTTP/WebSocket/Stdio → JSON-RPC）
- `agentos/atoms/syscall/`: 系统调用接口

## 功能特性

- 多协议支持：HTTP、WebSocket、Stdio
- 统一配置管理
- 服务生命周期管理
- 信号处理与优雅关闭
- 跨平台支持（Windows/Linux）

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 使用

```bash
# 默认配置启动
./gateway_d

# 指定HTTP端口
./gateway_d -p 8080

# 启用WebSocket
./gateway_d -w 8081

# 启用Stdio模式（CLI）
./gateway_d -s

# 从配置文件加载
./gateway_d -c /etc/agentos/gateway.conf

# 后台运行（Unix）
./gateway_d -d
```

## 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-c <path>` | 配置文件路径 | - |
| `-h <host>` | HTTP监听地址 | 0.0.0.0 |
| `-p <port>` | HTTP监听端口 | 8080 |
| `-w <port>` | WebSocket端口 | 8081 |
| `-s` | 启用Stdio模式 | false |
| `-d` | 后台运行 | false |
| `-v` | 详细输出 | false |
| `--help` | 显示帮助 | - |

## 配置文件示例

```ini
[gateway.http]
host = 0.0.0.0
port = 8080
enabled = true

[gateway.ws]
host = 0.0.0.0
port = 8081
enabled = false

[gateway.stdio]
enabled = false

[gateway.metrics]
enabled = true

[gateway.tracing]
enabled = false
```

## API 端点

### HTTP Gateway

- `POST /` - JSON-RPC 2.0 请求
- `GET /health` - 健康检查
- `GET /metrics` - Prometheus 指标

### JSON-RPC 方法

| 方法 | 说明 |
|------|------|
| `agentos_sys_task_submit` | 提交任务 |
| `agentos_sys_task_query` | 查询任务状态 |
| `agentos_sys_task_wait` | 等待任务完成 |
| `agentos_sys_task_cancel` | 取消任务 |
| `agentos_sys_memory_write` | 写入记忆 |
| `agentos_sys_memory_search` | 搜索记忆 |
| `agentos_sys_memory_get` | 获取记忆 |
| `agentos_sys_memory_delete` | 删除记忆 |
| `agentos_sys_session_create` | 创建会话 |
| `agentos_sys_session_get` | 获取会话信息 |
| `agentos_sys_session_close` | 关闭会话 |
| `agentos_sys_session_list` | 列出会话 |
| `agentos_sys_telemetry_metrics` | 获取指标 |
| `agentos_sys_telemetry_traces` | 获取追踪 |

## 依赖

- `gateway` - 协议转换模块
- `syscall` - 系统调用接口
- `svc_common` - 服务公共模块
- `libmicrohttpd` - HTTP 服务器库
- `libwebsockets` - WebSocket 库
- `cJSON` - JSON 解析库

## 许可证

Apache-2.0
