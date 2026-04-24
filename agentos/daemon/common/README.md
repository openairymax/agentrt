# Common — 公共服务库

`daemon/common/` 是所有守护进程共享的基础库，提供兼容层、工具函数和通用服务组件。

## 核心职责

- **兼容层**：操作系统差异屏蔽，提供统一 API
- **服务基础设施**：服务发现、配置管理、日志、告警
- **安全工具**：认证、输入校验、日志清洗
- **IPC 通信**：服务间通信总线
- **容错机制**：熔断器、检查点、告警管理

## 组件列表

| 组件 | 说明 |
|------|------|
| `svc_common` | 守护进程通用基础设施 |
| `svc_logger` | 统一日志接口 |
| `svc_config` | 配置管理接口 |
| `svc_auth` | 服务间认证 |
| `svc_cache` | 本地缓存 |
| `ipc_service_bus` | 服务间通信总线 |
| `service_discovery` | 服务发现 |
| `circuit_breaker` | 熔断器 |
| `checkpoint` | 检查点/状态持久化 |
| `alert_manager` | 告警管理器 |
| `daemon_security` | 安全工具函数 |
| `input_validator` | 输入校验器 |
| `param_validator` | 参数校验器 |
| `log_sanitizer` | 日志清洗器 |
| `safe_string_utils` | 安全字符串操作 |
| `method_dispatcher` | 方法分发器 |
| `jsonrpc_helpers` | JSON-RPC 辅助函数 |
| `unified_metrics` | 统一指标采集 |
| `platform` | 平台兼容层 |

## 使用方式

```c
#include "daemon/common/svc_common.h"
#include "daemon/common/svc_logger.h"
#include "daemon/common/ipc_service_bus.h"

// 初始化服务
svc_common_init("my_daemon", "1.0.0");

// 获取日志器
svc_logger_t* log = svc_logger_get("my_daemon");
svc_log_info(log, "守护进程启动");

// 创建 IPC 客户端
ipc_client_t* client = ipc_client_create("llm_d");
ipc_client_connect(client, IPC_TYPE_UNIX, "/tmp/llm_d.sock");

// 发送 JSON-RPC 请求
jsonrpc_request_t req = {
    .method = "llm.generate",
    .params = "{\"prompt\": \"hello\"}"
};
jsonrpc_response_t* resp = method_dispatcher_call(client, &req);
```

---

*AgentOS Daemon — Common*
