# Types — 类型定义模块

**模块路径**: `agentos/commons/utils/types/`
**版本**: v0.1.0

## 概述

Types 模块是 AgentOS 系统的核心类型定义中心，提供系统范围内使用的所有核心数据类型。该模块为 header-only 模块，不包含任何实现代码（`.c` 文件），所有类型定义均为值类型或不可变类型，天然线程安全。类型定义涵盖基础类型、任务类型、记忆类型、会话类型、Agent 类型、可观测性类型、IPC 类型、网络类型等八大类别，并包含辅助宏定义和跨模块规范类型。

## 设计目标

- **单一数据源**：所有核心类型集中定义，避免跨模块类型重复定义
- **接口契约化**：所有类型都有明确的语义和所有权规则，遵循接口契约化原则
- **命名语义化**：类型名称精确表达其用途，遵循命名语义化原则
- **跨模块规范**：提供 `sanitize_level_t`、`cupolas_vault_cred_type_t`、`cupolas_signer_info_t` 等跨模块规范类型，消除类型定义冲突

## 目录结构

```
types/
├── include/
│   ├── types.h                     # 核心类型定义（基础、任务、记忆、会话、Agent、可观测性、IPC、网络）
│   ├── sanitize_level.h            # 输入净化级别规范类型（STRICT / NORMAL / RELAXED）
│   ├── cupolas_vault_cred_type.h   # 凭据类型规范类型（Password / Token / Key / Certificate / Secret / Note）
│   └── cupolas_signer_info.h       # 代码签名者信息规范类型（X.509 证书身份字段）
├── .keep                           # Git 目录占位文件
└── README.md                       # 本文档
```

## 核心数据结构

### 基础类型

#### agentos_result_t — 通用结果类型

| 字段 | 类型 | 说明 |
|------|------|------|
| `code` | `agentos_error_t` | 错误码 |
| `message` | `const char *` | 错误消息 |
| `detail` | `const char *` | 详细信息 |

#### agentos_priority_t — 优先级枚举

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_PRIORITY_LOW` | 低优先级 |
| `AGENTOS_PRIORITY_NORMAL` | 普通优先级 |
| `AGENTOS_PRIORITY_HIGH` | 高优先级 |
| `AGENTOS_PRIORITY_CRITICAL` | 关键优先级 |

#### 基础类型别名

| 类型 | 定义 | 说明 |
|------|------|------|
| `agentos_error_t` | `int32_t` | 错误码类型（负值为错误，0 为成功） |
| `agentos_timestamp_t` | `uint64_t` | 时间戳类型（纳秒精度） |
| `agentos_millis_t` | `uint64_t` | 毫秒时间类型 |
| `agentos_uuid_t` | `char[37]` | 唯一标识符类型 |

#### 通用错误码

| 宏 | 值 | 说明 |
|------|------|------|
| `AGENTOS_SUCCESS` | 0 | 成功 |
| `AGENTOS_EINVAL` | -1 | 参数无效 |
| `AGENTOS_ENOMEM` | -2 | 内存不足 |
| `AGENTOS_EBUSY` | -3 | 资源忙碌 |
| `AGENTOS_ENOENT` | -4 | 资源不存在 |
| `AGENTOS_EPERM` | -5 | 权限不足 |
| `AGENTOS_ETIMEDOUT` | -6 | 操作超时 |
| `AGENTOS_EIO` | -7 | I/O 错误 |
| `AGENTOS_EEXIST` | -8 | 资源已存在 |
| `AGENTOS_ENOTINIT` | -9 | 引擎未初始化 |
| `AGENTOS_ECANCELLED` | -10 | 操作已取消 |
| `AGENTOS_ENOTSUP` | -11 | 操作不支持 |
| `AGENTOS_EOVERFLOW` | -12 | 溢出错误 |
| `AGENTOS_EPROTO` | -13 | 协议错误 |
| `AGENTOS_ENOTCONN` | -14 | 未连接 |
| `AGENTOS_ECONNRESET` | -15 | 连接重置 |
| `AGENTOS_ENOSYS` | -16 | 函数未实现 |
| `AGENTOS_EFAIL` | -17 | 通用失败 |
| `AGENTOS_ENOTFOUND` | -18 | 资源未找到 |
| `AGENTOS_EPLATFORM` | -27 | 平台未初始化 |
| `AGENTOS_EPROTONOSUPPORT` | -28 | 协议/命令不支持 |
| `AGENTOS_ESERVICE` | -29 | 服务不可用 |
| `AGENTOS_EUNKNOWN` | -99 | 未知错误 |

### 任务类型

#### agentos_task_status_t — 任务状态

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_TASK_PENDING` | 等待中 |
| `AGENTOS_TASK_RUNNING` | 运行中 |
| `AGENTOS_TASK_SUCCEEDED` | 已成功 |
| `AGENTOS_TASK_FAILED` | 已失败 |
| `AGENTOS_TASK_CANCELLED` | 已取消 |
| `AGENTOS_TASK_TIMEOUT` | 已超时 |
| `AGENTOS_TASK_RETRYING` | 重试中 |

#### agentos_task_config_t — 任务配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `input` | `const char *` | 任务输入（自然语言描述） |
| `input_len` | `size_t` | 输入长度 |
| `timeout_ms` | `uint32_t` | 超时时间（毫秒） |
| `priority` | `agentos_priority_t` | 任务优先级 |
| `type` | `agentos_task_type_t` | 任务类型 |
| `agent_id` | `const char *` | 指定执行的 Agent ID（可选） |
| `session_id` | `const char *` | 关联的会话 ID（可选） |
| `parent_task_id` | `const char *` | 父任务 ID（用于子任务） |

#### agentos_task_result_t — 任务结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `task_id` | `char *` | 任务 ID |
| `status` | `agentos_task_status_t` | 任务状态 |
| `output` | `char *` | 输出结果（JSON 字符串） |
| `output_len` | `size_t` | 输出长度 |
| `start_time` | `agentos_timestamp_t` | 开始时间 |
| `end_time` | `agentos_timestamp_t` | 结束时间 |
| `tokens_used` | `uint32_t` | 消耗的 Token 数 |
| `cost_usd` | `double` | 成本（美元） |
| `error_code` | `agentos_error_t` | 错误码 |
| `error_message` | `char *` | 错误消息 |

### 记忆类型

#### agentos_memory_layer_t — 记忆层级（四层记忆卷载结构）

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_MEM_LAYER1_RAW` | Layer1: 原始记忆 |
| `AGENTOS_MEM_LAYER2_WORKING` | Layer2: 工作记忆 |
| `AGENTOS_MEM_LAYER3_EPISODIC` | Layer3: 情景记忆 |
| `AGENTOS_MEM_LAYER4_SEMANTIC` | Layer4: 语义记忆 |

#### agentos_memory_entry_t — 记忆条目

| 字段 | 类型 | 说明 |
|------|------|------|
| `memory_id` | `char *` | 记忆 ID |
| `layer` | `agentos_memory_layer_t` | 记忆层级 |
| `type` | `agentos_memory_type_t` | 记忆类型 |
| `content` | `char *` | 记忆内容 |
| `content_len` | `size_t` | 内容长度 |
| `embedding` | `float *` | 向量嵌入（可选） |
| `embedding_dim` | `size_t` | 嵌入维度 |
| `importance` | `float` | 重要性分数（0-1） |
| `decay_rate` | `float` | 衰减率 |
| `access_count` | `uint32_t` | 访问次数 |
| `created_at` | `agentos_timestamp_t` | 创建时间 |
| `last_access` | `agentos_timestamp_t` | 最后访问时间 |
| `session_id` | `char *` | 关联会话 ID |
| `task_id` | `char *` | 关联任务 ID |
| `tags` | `char **` | 标签列表 |
| `tag_count` | `size_t` | 标签数量 |

### 会话类型

#### agentos_session_status_t — 会话状态

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_SESSION_ACTIVE` | 活跃状态 |
| `AGENTOS_SESSION_IDLE` | 空闲状态 |
| `AGENTOS_SESSION_CLOSED` | 已关闭 |
| `AGENTOS_SESSION_EXPIRED` | 已过期 |

#### agentos_context_t — 执行上下文

| 字段 | 类型 | 说明 |
|------|------|------|
| `agent_id` | `char *` | Agent ID |
| `session_id` | `char *` | 会话 ID |
| `trace_id` | `char *` | 追踪 ID |
| `parent_span_id` | `char *` | 父 Span ID |
| `timestamp` | `agentos_timestamp_t` | 时间戳 |
| `priority` | `agentos_priority_t` | 优先级 |
| `user_id` | `char *` | 用户 ID |
| `project_id` | `char *` | 项目 ID |
| `user_data` | `void *` | 用户自定义数据 |

### Agent 类型

#### agentos_agent_contract_t — Agent 契约（完整元数据）

| 字段 | 类型 | 说明 |
|------|------|------|
| `schema_version` | `char *` | 契约版本 |
| `agent_id` | `char *` | Agent ID |
| `agent_name` | `char *` | Agent 名称 |
| `version` | `char *` | Agent 版本 |
| `role` | `char *` | 角色分类 |
| `description` | `char *` | 描述 |
| `capabilities` | `agentos_capability_t *` | 能力列表 |
| `capability_count` | `size_t` | 能力数量 |
| `models` | `agentos_models_t` | 模型配置 |
| `required_permissions` | `char **` | 所需权限 |
| `permission_count` | `size_t` | 权限数量 |
| `cost` | `agentos_cost_profile_t` | 成本概览 |
| `trust` | `agentos_trust_metrics_t` | 信任指标 |
| `extensions` | `char *` | 扩展字段（JSON） |

### 可观测性类型

#### agentos_metric_t — 指标数据

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | `char *` | 指标名称 |
| `type` | `agentos_metric_type_t` | 指标类型 |
| `description` | `char *` | 描述 |
| `unit` | `char *` | 单位 |
| `value` | `double` | 当前值 |
| `labels` | `char **` | 标签键值对 |
| `label_count` | `size_t` | 标签数量 |
| `timestamp` | `agentos_timestamp_t` | 时间戳 |

#### agentos_span_t — Span 数据

| 字段 | 类型 | 说明 |
|------|------|------|
| `trace_id` | `char *` | 追踪 ID |
| `span_id` | `char *` | Span ID |
| `parent_span_id` | `char *` | 父 Span ID |
| `name` | `char *` | Span 名称 |
| `kind` | `agentos_span_kind_t` | Span 类型 |
| `start_time` | `agentos_timestamp_t` | 开始时间 |
| `end_time` | `agentos_timestamp_t` | 结束时间 |
| `status` | `agentos_span_status_t` | Span 状态 |
| `status_message` | `char *` | 状态消息 |
| `attributes` | `char **` | 属性键值对 |
| `attribute_count` | `size_t` | 属性数量 |
| `events` | `char *` | 事件列表（JSON） |

### IPC 类型

#### agentos_ipc_type_t — IPC 通道类型

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_IPC_PIPE` | 管道 |
| `AGENTOS_IPC_SOCKET` | Unix Socket / Named Pipe |
| `AGENTOS_IPC_SHM` | 共享内存 |
| `AGENTOS_IPC_MQ` | 消息队列 |
| `AGENTOS_IPC_RPC` | RPC 调用 |

### 网络类型

#### agentos_protocol_t — 协议类型

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_PROTO_TCP` | TCP 协议 |
| `AGENTOS_PROTO_UDP` | UDP 协议 |
| `AGENTOS_PROTO_HTTP` | HTTP 协议 |
| `AGENTOS_PROTO_HTTPS` | HTTPS 协议 |
| `AGENTOS_PROTO_WS` | WebSocket 协议 |
| `AGENTOS_PROTO_WSS` | WebSocket Secure 协议 |

#### agentos_endpoint_t — 连接端点

| 字段 | 类型 | 说明 |
|------|------|------|
| `host` | `char *` | 主机名或 IP |
| `port` | `uint16_t` | 端口号 |
| `protocol` | `agentos_protocol_t` | 协议类型 |
| `path` | `char *` | 路径（用于 HTTP/WebSocket） |

### 跨模块规范类型

#### sanitize_level_t — 输入净化级别

| 枚举值 | 说明 |
|------|------|
| `SANITIZE_LEVEL_STRICT` | 最大净化，拒绝任何可疑内容 |
| `SANITIZE_LEVEL_NORMAL` | 平衡净化，适用于典型 Agent 交互 |
| `SANITIZE_LEVEL_RELAXED` | 最小净化，适用于受信任的内部通道 |

#### cupolas_vault_cred_type_t — 凭据类型

| 枚举值 | 说明 |
|------|------|
| `CUPOLAS_VAULT_CRED_PASSWORD` | 密码 |
| `CUPOLAS_VAULT_CRED_TOKEN` | 令牌（API Key、OAuth Token） |
| `CUPOLAS_VAULT_CRED_KEY` | 密钥（私钥） |
| `CUPOLAS_VAULT_CRED_CERTIFICATE` | 证书 |
| `CUPOLAS_VAULT_CRED_SECRET` | 通用密钥 |
| `CUPOLAS_VAULT_CRED_NOTE` | 安全笔记 |

#### cupolas_signer_info_t — 代码签名者信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `subject_cn` | `char *` | 主体通用名称 |
| `subject_org` | `char *` | 主体组织 |
| `subject_ou` | `char *` | 主体组织单元 |
| `issuer_cn` | `char *` | 签发者通用名称 |
| `serial_number` | `char *` | 序列号 |
| `key_id` | `char *` | 密钥标识 |
| `algorithm` | `char *` | 算法 |
| `not_before` | `uint64_t` | 有效期起始 |
| `not_after` | `uint64_t` | 有效期截止 |
| `is_ca` | `bool` | 是否为 CA |
| `key_usage` | `uint32_t` | 密钥用途 |

## 辅助宏定义

| 宏 | 说明 |
|------|------|
| `AGENTOS_ARRAY_SIZE(arr)` | 数组元素数量计算 |
| `AGENTOS_MIN(a, b)` | 取最小值 |
| `AGENTOS_MAX(a, b)` | 取最大值 |
| `AGENTOS_ALIGN_UP(x, align)` | 向上对齐 |
| `AGENTOS_STRINGIFY(x)` | 字符串化 |
| `AGENTOS_TOSTRING(x)` | 转换为字符串 |
| `AGENTOS_CONCAT(a, b)` | 符号连接 |
| `AGENTOS_MAKE_VERSION(maj, min, pat)` | 版本号打包 |
| `AGENTOS_VERSION_MAJOR(v)` | 提取主版本号 |
| `AGENTOS_VERSION_MINOR(v)` | 提取次版本号 |
| `AGENTOS_VERSION_PATCH(v)` | 提取补丁版本号 |
| `AGENTOS_MS_TO_NS(ms)` | 毫秒转纳秒 |
| `AGENTOS_SEC_TO_MS(s)` | 秒转毫秒 |
| `AGENTOS_SEC_TO_NS(s)` | 秒转纳秒 |

## 使用示例

```c
#include "types.h"

/* === 使用通用结果类型 === */
agentos_result_t result = { .code = AGENTOS_SUCCESS, .message = "OK" };
if (result.code == AGENTOS_SUCCESS) {
    printf("Operation succeeded: %s\n", result.message);
}

/* === 定义任务配置 === */
agentos_task_config_t config = {
    .input = "Analyze the codebase",
    .input_len = 20,
    .timeout_ms = 30000,
    .priority = AGENTOS_PRIORITY_HIGH,
    .type = AGENTOS_TASKTYPE_ONESHOT,
    .agent_id = NULL,  // 自动选择
};

/* === 使用执行上下文 === */
agentos_context_t ctx = {
    .agent_id = "agent-001",
    .session_id = "session-abc",
    .trace_id = "trace-xyz",
    .priority = AGENTOS_PRIORITY_NORMAL,
};

/* === 使用辅助宏 === */
#define BUFFER_SIZE 256
char buffer[BUFFER_SIZE];
int items[] = {1, 2, 3, 4, 5};
size_t count = AGENTOS_ARRAY_SIZE(items);  // 5

uint32_t version = AGENTOS_MAKE_VERSION(1, 2, 3);
printf("v%d.%d.%d\n",
       AGENTOS_VERSION_MAJOR(version),
       AGENTOS_VERSION_MINOR(version),
       AGENTOS_VERSION_PATCH(version));

/* === 使用规范类型 === */
#include "sanitize_level.h"
sanitize_level_t level = SANITIZE_LEVEL_NORMAL;
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `agentos_types.h` | 上层类型定义（IPC 消息等） |
| `platform.h` | 平台类型定义（socket 句柄等） |
| `stdbool.h` | 布尔类型支持 |
| `stddef.h` | `size_t` 等类型 |
| `stdint.h` | 固定宽度整数类型 |

---

© 2026 SPHARX Ltd. All Rights Reserved.