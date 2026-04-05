# 编码规范

**版本**: 1.0.0  
**最后更新**: 2026-04-05  
**适用范围**: 所有AgentOS贡献者  

---

## 📋 总则

AgentOS代码遵循**五维正交设计原则**中的**美学观**四原则：

- **A-1 简约至上**: 用最少的接口提供最大的价值
- **A-2 极致细节**: 错误消息、日志格式、命名一致
- **A-3 人文关怀**: 开发者体验与用户体验并重
- **A-4 完美主义**: 代码质量、测试覆盖、文档完整

---

## 🌐 多语言规范索引

| 语言 | 规范文件 | 主要用途 |
|------|---------|---------|
| **C/C++** | [#C/C++规范](#cc规范) | 内核层(corekern)、安全穹顶(cupolas) |
| **Python** | [#Python规范](#python规范) | 守护进程(daemon)、SDK(toolkit/python) |
| **Go** | [#Go规范](#go规范) | SDK(toolkit/go)、CLI工具 |
| **Rust** | [#Rust规范](#rust规范) | 高性能组件(toolkit/rust) |
| **TypeScript** | [#TypeScript规范](#typescript规范) | OpenLab前端、Web SDK |

---

## C/C++ 规范

### 文件组织

```c
/**
 * @file ipc_channel.c
 * @brief IPC通道实现 - 进程间通信核心模块
 * @author SPHARX Kernel Team
 * @version 1.0.0
 * @date 2026-04-05
 * @copyright Copyright (c) 2026 SPHARX Ltd.
 *
 * @details 本文件实现了AgentOS微内核的IPC机制，
 *          支持同步/异步消息传递、零拷贝优化。
 *
 * @see ipc_channel.h
 * @see ARCHITECTURAL_PRINCIPLES.md K-2接口契约化原则
 */

/* ==========================================================================
 * 头文件包含（按顺序）
 * ========================================================================== */

// 1. 系统头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 2. 第三方库头文件
#include <cjson/cJSON.h>
#include <openssl/sha.h>

// 3. 项目公共头文件
#include "agentos/common/types.h"
#include "agentos/error_codes.h"

// 4. 模块私有头文件
#include "ipc_channel.h"
#include "ipc_internal.h"

/* ==========================================================================
 * 宏定义常量
 * ========================================================================== */

/** IPC消息最大载荷大小 (1MB) */
#define IPC_MAX_PAYLOAD_SIZE (1 << 20)

/** IPC默认超时时间 (5秒) */
#define IPC_DEFAULT_TIMEOUT_MS 5000

/** 默认重试次数 */
#define IPC_DEFAULT_RETRIES 3

/* ==========================================================================
 * 类型定义
 * ========================================================================== */

/**
 * @brief IPC通道配置结构体
 */
typedef struct ipc_channel_config_t {
    char channel_name[IPC_MAX_NAME_LEN];   /** 通道名称 */
    uint32_t max_message_size;              /** 最大消息大小 */
    uint32_t buffer_size;                   /** 缓冲区大小 */
    uint32_t timeout_ms;                    /** 超时时间 */
    ipc_mode_t mode;                        /** 通信模式 */
} ipc_channel_config_t;

/* ==========================================================================
 * 函数实现
 * ========================================================================== */

/**
 * @brief 创建IPC通道
 *
 * @param config 通道配置（输入）
 * @param[out] channel 输出的通道句柄
 * @return 成功返回AGENTOS_OK，失败返回对应错误码
 *
 * @pre config不为NULL且通过验证
 * @post channel指向有效的通道句柄
 *
 * @par 示例:
 * @code
 * ipc_channel_config_t config = {
 *     .channel_name = "task_queue",
 *     .max_message_size = IPC_MAX_PAYLOAD_SIZE,
 *     .buffer_size = 4096,
 *     .timeout_ms = IPC_DEFAULT_TIMEOUT_MS,
 *     .mode = IPC_MODE_ASYNC
 * };
 *
 * ipc_channel_t *channel;
 * int ret = ipc_channel_create(&config, &channel);
 * if (ret != AGENTOS_OK) {
 *     fprintf(stderr, "[ERROR] 创建通道失败: %s\n",
 *             agentos_error_get_message(ret));
 *     return ret;
 * }
 * @endcode
 *
 * @since 1.0.0
 * @note 线程安全：此函数内部使用互斥锁保护
 * @warning 调用者负责在使用完毕后调用ipc_channel_destroy()释放资源
 *
 * @see ipc_channel_destroy()
 * @see ipc_channel_send()
 * @see ARCHITECTURAL_PRINCIPLES.md K-2接口契约化原则
 */
int ipc_channel_create(const ipc_channel_config_t *config,
                       ipc_channel_t **channel)
{
    // 参数验证（遵循E-6错误可追溯原则）
    if (config == NULL) {
        return AGENTOS_EINVAL;
    }

    if (channel == NULL) {
        return AGENTOS_EINVAL;
    }

    // 验证配置有效性
    if (strlen(config->channel_name) == 0) {
        return AGENTOS_EINVAL;
    }

    if (config->max_message_size > IPC_MAX_PAYLOAD_SIZE ||
        config->max_message_size == 0) {
        return AGENTOS_EINVAL;
    }

    // 分配内存（使用对象池，遵循E-3资源确定性原则）
    ipc_channel_t *ch = memory_pool_alloc(sizeof(ipc_channel_t));
    if (ch == NULL) {
        return AGENTOS_ENOMEM;
    }

    // 初始化结构体
    memset(ch, 0, sizeof(ipc_channel_t));
    strncpy(ch->name, config->channel_name, IPC_MAX_NAME_LEN - 1);
    ch->max_message_size = config->max_message_size;
    ch->buffer_size = config->buffer_size;
    ch->timeout_ms = config->timeout_ms;
    ch->mode = config->mode;

    // 初始化互斥锁
    if (pthread_mutex_init(&ch->mutex, NULL) != 0) {
        memory_pool_free(ch);
        return AGENTOS_EINTERNAL;
    }

    // 初始化条件变量
    if (pthread_cond_init(&ch->cond, NULL) != 0) {
        pthread_mutex_destroy(&ch->mutex);
        memory_pool_free(ch);
        return AGENTOS_EINTERNAL;
    }

    // 初始化消息队列
    ch->queue = queue_create(config->buffer_size);
    if (ch->queue == NULL) {
        pthread_cond_destroy(&ch->cond);
        pthread_mutex_destroy(&ch->mutex);
        memory_pool_free(ch);
        return AGENTOS_ENOMEM;
    }

    // 设置引用计数
    atomic_store(&ch->ref_count, 1);

    // 输出通道句柄
    *channel = ch;

    // 结构化日志输出（遵循E-2可观测性原则）
    LOG_INFO("ipc",
             "channel_created",
             LOG_KV("channel_name", "%s", ch->name),
             LOG_KV("max_msg_size", "%u", ch->max_message_size),
             LOG_KV("buffer_size", "%u", ch->buffer_size));

    return AGENTOS_OK;
}
```

### 命名规范

| 类别 | 规范 | 示例 |
|------|------|------|
| **函数** | `module_action_object()` | `ipc_channel_create()`, `memory_pool_alloc()` |
| **变量** | `snake_case` | `max_message_size`, `buffer_size` |
| **常量** | `UPPER_SNAKE_CASE` | `IPC_MAX_PAYLOAD_SIZE`, `DEFAULT_TIMEOUT_MS` |
| **类型** | `_t` 后缀 | `ipc_channel_t`, `task_handle_t` |
| **宏** | `UPPER_SNAKE_CASE` | `LOG_INFO()`, `RETURN_IF_ERROR()` |
| **枚举值** | `MODULE_` 前缀 | `IPC_MODE_SYNC`, `IPC_MODE_ASYNC` |
| **错误码** | `AGENTOS_E` 前缀 | `AGENTOS_OK`, `AGENTOS_EINVAL`, `AGENTOS_ENOMEM` |

### 错误处理模式

```c
// 使用统一的错误返回模式
int result = some_operation();
if (result != AGENTOS_OK) {
    LOG_ERROR("module",
              "operation_failed",
              LOG_KV("error_code", "%d", result),
              LOG_KV("error_msg", "%s", agentos_error_get_message(result)),
              LOG_KV("context", "key=%s", value));
    return result;  // 错误向上传播
}

// 使用宏简化常见模式
RETURN_IF_ERROR(ipc_channel_create(&config, &channel));
RETURN_IF_NULL(ptr, AGENTOS_ENOMEM);

// 资源获取即初始化(RAII)模式（C语言中使用goto清理）
int complex_operation(void) {
    resource_a_t *a = alloc_resource_a();
    if (!a) goto error_a;

    resource_b_t *b = alloc_resource_b();
    if (!b) goto error_b;

    resource_c_t *c = alloc_resource_c();
    if (!c) goto error_c;

    // ... 使用资源 ...

    free_resource_c(c);
    free_resource_b(b);
    free_resource_a(a);
    return AGENTOS_OK;

error_c:
    free_resource_b(b);
error_b:
    free_resource_a(a);
error_a:
    return AGENTOS_EINTERNAL;
}
```

---

## Python 规范

### 文件结构

```python
"""
AgentOS LLM服务守护进程 (llm_d)

本模块实现了LLM服务的管理与调度功能，
包括模型加载、推理执行、Token管理等。
"""

from __future__ import annotations

# 标准库
import asyncio
import logging
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, Dict, List, Any
from pathlib import Path

# 第三方库
import aiohttp
from pydantic import BaseModel, Field, validator

# 项目内部
from agentos.commons.errors import AgentOSError, ErrorCode
from agentos.commons.logging import StructuredLogger
from agentos.daemon.base import BaseDaemon


class LLMProvider(str, Enum):
    """LLM提供商枚举"""
    OPENAI = "openai"
    DEEPSEEK = "deepseek"
    LOCAL = "local"


@dataclass
class LLMConfig:
    """LLM服务配置"""
    provider: LLMProvider = LLMProvider.OPENAI
    api_key: str = Field(..., min_length=32)
    base_url: str = "https://api.openai.com/v1"
    model: str = "gpt-4-turbo"
    max_tokens: int = Field(default=4096, ge=1, le=128000)
    temperature: float = Field(default=0.7, ge=0.0, le=2.0)

    @validator('api_key')
    def validate_api_key(cls, v):
        """验证API密钥格式"""
        if not v.startswith('sk-'):
            raise ValueError('API密钥必须以sk-开头')
        return v


class LLMDaemon(BaseDaemon):
    """
    LLM服务管理守护进程

    Attributes:
        config (LLMConfig): LLM服务配置
        logger (StructuredLogger): 结构化日志记录器
        _session (aiohttp.ClientSession): HTTP客户端会话
        _model_cache: Dict[str, Any]: 模型缓存

    Example:
        >>> config = LLMConfig(
        ...     provider=LLMProvider.OPENAI,
        ...     api_key='sk-your-key',
        ...     model='gpt-4-turbo'
        ... )
        >>> daemon = LLMDaemon(config)
        >>> await daemon.start()

    See Also:
        - BaseDaemon: 基础守护进程类
        - ARCHITECTURAL_PRINCIPLES.md C-1双系统协同原则
    """

    def __init__(self, config: LLMConfig) -> None:
        """
        初始化LLM守护进程

        Args:
            config: LLM服务配置对象

        Raises:
            ValueError: 配置无效时抛出
        """
        super().__init__(
            name="llm_d",
            version="1.0.0",
            config=config
        )

        self.config = config
        self.logger = StructuredLogger(__name__)
        self._session: Optional[aiohttp.ClientSession] = None
        self._model_cache: Dict[str, Any] = {}

    async def start(self) -> None:
        """启动守护进程"""
        await super().start()

        # 创建HTTP会话
        self._session = aiohttp.ClientSession(
            timeout=aiohttp.ClientTimeout(total=self.config.timeout_seconds)
        )

        self.logger.info(
            "daemon_started",
            extra={
                "provider": self.config.provider.value,
                "model": self.config.model,
                "version": self.version
            }
        )

    async def stop(self) -> None:
        """停止守护进程"""
        if self._session:
            await self._session.close()
            self._session = None

        await super().stop()

        self.logger.info("daemon_stopped")

    async def inference(
        self,
        prompt: str,
        system_prompt: Optional[str] = None,
        max_tokens: Optional[int] = None,
        temperature: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        执行LLM推理

        Args:
            prompt: 用户提示词
            system_prompt: 系统提示词（可选）
            max_tokens: 最大生成Token数（可选，覆盖配置）
            temperature: 温度参数（可选，覆盖配置）

        Returns:
            包含推理结果的字典：
            - content: 生成的文本
            - usage: Token使用统计
            - model: 使用的模型名
            - latency_ms: 推理延迟（毫秒）

        Raises:
            AgentOSError: API调用失败时抛出
            ValueError: 参数无效时抛出

        Example:
            >>> result = await daemon.inference(
            ...     prompt="分析这份财报",
            ...     max_tokens=2000
            ... )
            >>> print(result['content'])
        """
        # 参数验证
        if not prompt or not prompt.strip():
            raise ValueError("prompt不能为空")

        actual_max_tokens = max_tokens or self.config.max_tokens
        actual_temperature = temperature or self.config.temperature

        # 构建请求载荷
        payload = {
            "model": self.config.model,
            "messages": [
                {"role": "system", "content": system_prompt or "You are a helpful assistant."},
                {"role": "user", "content": prompt}
            ],
            "max_tokens": actual_max_tokens,
            "temperature": actual_temperature
        }

        headers = {
            "Authorization": f"Bearer {self.config.api_key}",
            "Content-Type": "application/json"
        }

        try:
            start_time = self._get_timestamp_ms()

            async with self._session.post(
                f"{self.config.base_url}/chat/completions",
                json=payload,
                headers=headers
            ) as response:
                if response.status != 200:
                    error_body = await response.text()
                    raise AgentOSError(
                        code=ErrorCode.LLM_API_ERROR,
                        message=f"LLM API调用失败 (context: status={response.status}, body={error_body[:200]}). "
                               f"Suggestion: 检查API密钥和网络连接。",
                        http_status=response.status
                    )

                data = await response.json()

                end_time = self._get_timestamp_ms()
                latency_ms = end_time - start_time

                result = {
                    "content": data["choices"][0]["message"]["content"],
                    "usage": data["usage"],
                    "model": data["model"],
                    "latency_ms": latency_ms
                }

                # 结构化日志记录（遵循E-2可观测性原则）
                self.logger.info(
                    "inference_completed",
                    extra={
                        "model": result["model"],
                        "prompt_tokens": result["usage"]["prompt_tokens"],
                        "completion_tokens": result["usage"]["completion_tokens"],
                        "latency_ms": latency_ms,
                        "trace_id": self._generate_trace_id()
                    }
                )

                return result

        except aiohttp.ClientError as e:
            raise AgentOSError(
                code=ErrorCode.NETWORK_ERROR,
                message=f"网络请求失败 (context: url={self.config.base_url}, error={str(e)}). "
                       f"Suggestion: 检查网络连接和DNS设置。"
            )

    @staticmethod
    def _get_timestamp_ms() -> int:
        """获取当前时间戳（毫秒）"""
        import time
        return int(time.time() * 1000)

    def _generate_trace_id(self) -> str:
        """生成追踪ID"""
        import uuid
        return f"trace_{uuid.uuid4().hex[:12]}"
```

### Python编码规则

| 规则 | 要求 | 示例 |
|------|------|------|
| **导入顺序** | 标准→第三方→项目内部 | 见上方示例 |
| **类型注解** | 所有公开API必须注解 | `async def inference(self, prompt: str) -> Dict[str, Any]` |
| **Docstring** | Google风格，包含Args/Returns/Raises/Example | 见上方示例 |
| **字符串格式** | f-string（Python 3.6+） | `f"Error: {error_code}"` |
| **异常处理** | 自定义异常类 + 详细上下文 | `AgentOSError(code, message)` |
| **日志** | 结构化JSON格式 | `logger.info("event", extra={"key": value})` |
| **异步编程** | 优先async/await | `async def start(self) -> None` |

---

## Go 规范

### 项目布局

```
agentos/toolkit/go/
├── cmd/
│   └── agentos-cli/           # 主程序入口
│       └── main.go
├── internal/
│   ├── client/                # 内部实现
│   │   ├── client.go
│   │   └── client_test.go
│   └── models/                # 数据模型
│       └── types.go
├── pkg/                       # 可导出包
│   ├── api/                   # API客户端
│   │   ├── kernel.go
│   │   └── tasks.go
│   └── errors/                # 错误处理
│       └── errors.go
├── go.mod
├── go.sum
└── README.md
```

### 代码示例

```go
// Package api provides the AgentOS kernel API client.
//
// This package implements a type-safe, idiomatic Go client for interacting
// with the AgentOS kernel's RESTful API.
package api

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"

	"github.com/spharx/agentos/toolkit-go/pkg/errors"
	"github.com/spharx/agentos/toolkit-go/pkg/models"
)

// Client represents an AgentOS API client.
type Client struct {
	baseURL    string
	apiKey     string
	httpClient *http.Client
	logger     Logger
}

// ClientOption is a functional option for configuring Client.
type ClientOption func(*Client)

// WithBaseURL sets the base URL for the API client.
func WithBaseURL(url string) ClientOption {
	return func(c *Client) {
		c.baseURL = url
	}
}

// WithAPIKey sets the API key for authentication.
func WithAPIKey(key string) ClientOption {
	return func(c *Client) {
		c.apiKey = key
	}
}

// WithHTTPClient sets a custom HTTP client.
func WithHTTPClient(client *http.Client) ClientOption {
	return func(c *Client) {
		c.httpClient = client
	}
}

// NewClient creates a new AgentOS API client with the given options.
//
// Example:
//
//	client, err := api.NewClient(
//	    api.WithBaseURL("http://localhost:8080"),
//	    api.WithAPIKey("your-api-key"),
//	    api.WithHTTPClient(&http.Client{Timeout: 30 * time.Second}),
//	)
//	if err != nil {
//	    log.Fatal(err)
//	}
func NewClient(opts ...ClientOption) (*Client, error) {
	c := &Client{
		baseURL: "http://localhost:8080",
		httpClient: &http.Client{
			Timeout: 30 * time.Second,
		},
		logger: &defaultLogger{},
	}

	for _, opt := range opts {
		opt(c)
	}

	if c.baseURL == "" {
		return nil, errors.New(errors.ErrInvalidConfiguration, "base URL is required")
	}

	return c, nil
}

// CreateTask creates a new task in the AgentOS scheduler.
//
// The method sends a POST request to /api/v1/tasks and returns
// the created task information.
//
// Parameters:
//   - ctx: Context for cancellation and timeout control
//   - req: Task creation request containing description and configuration
//
// Returns:
//   - *models.Task: Created task with ID and status
//   - error: Non-nil if the request fails
//
// Possible errors:
//   - ErrInvalidInput: Request validation fails
//   - ErrUnauthorized: Authentication fails
//   - ErrNetworkError: Network connectivity issues
//   - ErrServerInternal: Server-side errors (5xx)
func (c *Client) CreateTask(ctx context.Context, req *models.TaskCreateRequest) (*models.Task, error) {
	if req == nil {
		return nil, errors.New(errors.ErrInvalidInput, "request cannot be nil")
	}

	if req.Description == "" {
		return nil, errors.New(errors.ErrInvalidInput, "task description is required")
	}

	body, err := json.Marshal(req)
	if err != nil {
		return nil, errors.Wrap(err, "failed to marshal request body")
	}

	httpReq, err := http.NewRequestWithContext(
		ctx,
		http.MethodPost,
		c.baseURL+"/api/v1/tasks",
		io.NewReader(body),
	)
	if err != nil {
		return nil, errors.Wrap(err, "failed to create request")
	}

	httpReq.Header.Set("Content-Type", "application/json")
	httpReq.Header.Set("Authorization", "Bearer "+c.apiKey)
	httpReq.Header.Set("X-Request-ID", generateRequestID())

	resp, err := c.httpClient.Do(httpReq)
	if err != nil {
		return nil, errors.Wrap(err, "failed to send request")
	}
	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, errors.Wrap(err, "failed to read response body")
	}

	if resp.StatusCode != http.StatusCreated {
		var apiErr models.APIErrorResponse
		if json.Unmarshal(respBody, &apiErr) == nil && apiErr.Error != nil {
			return nil, &errors.APIError{
				Code:    apiErr.Error.Code,
				Message: apiErr.Error.Message,
				Status:  resp.StatusCode,
			}
		}
		return nil, errors.Errorf(
			errors.ErrServerInternal,
			"unexpected status code %d: %s",
			resp.StatusCode,
			string(respBody),
		)
	}

	var taskResp models.TaskResponse
	if err := json.Unmarshal(respBody, &taskResp); err != nil {
		return nil, errors.Wrap(err, "failed to unmarshal response")
	}

	c.logger.Info("task_created",
		"task_id", taskResp.Data.ID,
		"status", taskResp.Data.Status,
	)

	return &taskResp.Data, nil
}
```

---

## Rust 规范

### 代码示例

```rust
//! AgentOS 内存管理模块
//!
//! 提供高性能的内存分配和管理功能，
//! 包括对象池、内存统计和泄漏检测。

use std::sync::Arc;
use tokio::sync::RwLock;
use tracing::{info, warn, error};
use thiserror::Error;

/// 内存管理错误类型
#[derive(Error, Debug)]
pub enum MemoryError {
    /// 内存不足
    #[error("内存分配失败 (context: requested={requested} bytes, available={available} bytes). Suggestion: 增加系统内存或减少批量大小。")]
    OutOfMemory { requested: usize, available: usize },

    /// 无效的内存地址
    #[error("无效的内存地址 (context: address={:#x}). Suggestion: 检查内存ID是否正确。")]
    InvalidAddress { address: usize },

    /// 所有权冲突
    #[error("所有权冲突 (context: memory_id={memory_id}, owner={expected_owner}, requester={requester}). Suggestion: 只有所有者才能释放内存。")]
    OwnershipConflict {
        memory_id: String,
        expected_owner: String,
        requester: String,
    },
}

/// 内存池配置
#[derive(Debug, Clone)]
pub struct MemoryPoolConfig {
    /// 池总大小（字节）
    pub pool_size: usize,

    /// 块大小（字节）
    pub block_size: usize,

    /// 是否启用泄漏检测
    pub leak_detection: bool,
}

impl Default for MemoryPoolConfig {
    fn default() -> Self {
        Self {
            pool_size: 1024 * 1024 * 1024, // 1GB
            block_size: 4096,               // 4KB
            leak_detection: cfg!(debug_assertions),
        }
    }
}

/// 高性能内存池
///
/// # Examples
///
/// ```
/// use agentos_memory::MemoryPool;
///
/// let pool = MemoryPool::new(MemoryPoolConfig::default());
/// let mem = pool.allocate(1024, "my_task".to_string())?;
/// println!("Allocated {} bytes at address {:#x}", mem.size, mem.address);
/// pool.deallocate(mem.id)?;
/// # Ok::<(), agentos_memory::MemoryError>(())
/// ```
pub struct MemoryPool {
    config: MemoryPoolConfig,
    state: Arc<RwLock<PoolState>>,
    stats: Arc<RwLock<MemoryStats>>,
}

#[derive(Debug)]
struct PoolState {
    /// 已分配的内存块
    allocations: std::collections::HashMap<String, MemoryBlock>,
    /// 空闲块链表
    free_list: Vec<usize>,
}

#[derive(Debug, Default)]
pub struct MemoryStats {
    /// 总分配字节数
    pub total_allocated: usize,
    /// 总释放字节数
    pub total_freed: usize,
    /// 当前活跃分配数
    pub active_allocations: usize,
    /// 峰值使用量
    pub peak_usage: usize,
}

impl MemoryPool {
    /// 创建新的内存池实例
    ///
    /// # Arguments
    /// * `config` - 内存池配置
    ///
    /// # Returns
    /// * `Ok(MemoryPool)` - 成功创建的内存池
    /// * `Err(MemoryError)` - 配置无效时返回错误
    ///
    /// # Panics
    /// 如果 `config.block_size` 为0，将触发panic
    pub fn new(config: MemoryPoolConfig) -> Result<Self, MemoryError> {
        if config.block_size == 0 {
            panic!("block_size must be greater than 0");
        }

        info!(
            pool_size = config.pool_size,
            block_size = config.block_size,
            leak_detection = config.leak_detection,
            "creating memory pool"
        );

        Ok(Self {
            config,
            state: Arc::new(RwLock::new(PoolState {
                allocations: std::collections::HashMap::new(),
                free_list: Vec::new(),
            })),
            stats: Arc::new(RwLock::new(MemoryStats::default())),
        })
    }

    /// 分配内存块
    ///
    /// 从内存池中分配指定大小的内存块。
    ///
    /// # Arguments
    /// * `size` - 请求的字节数
    /// * `owner` - 所有者标识（用于跟踪和权限控制）
    ///
    /// # Returns
    /// * `Ok(MemoryHandle)` - 内存句柄，包含地址和元数据
    /// * `Err(MemoryError)` - 分配失败时返回错误
    ///
    /// # Errors
    /// * `OutOfMemory` - 池空间不足时返回
    ///
    /// # Example
    /// ```
    /// # use agentos_memory::{MemoryPool, MemoryPoolConfig};
    /// let pool = MemoryPool::new(MemoryPoolConfig::default());
    /// match pool.allocate(1024, "task_123".to_string()) {
    ///     Ok(handle) => println!("Allocated at {:#x}", handle.address),
    ///     Err(e) => eprintln!("Allocation failed: {}", e),
    /// }
    /// ```
    pub async fn allocate(&self, size: usize, owner: String) -> Result<MemoryHandle, MemoryError> {
        let mut state = self.state.write().await;
        let mut stats = self.stats.write().await;

        // 检查是否有足够空间
        let current_usage: usize = state.allocations.values()
            .map(|b| b.size)
            .sum();

        if current_usage + size > self.config.pool_size {
            let available = self.config.pool_size.saturating_sub(current_usage);
            error!(
                requested = size,
                available = available,
                current_usage = current_usage,
                pool_size = self.config.pool_size,
                "memory allocation failed: out of memory"
            );
            return Err(MemoryError::OutOfMemory {
                requested: size,
                available,
            });
        }

        // 分配内存块（简化实现）
        let id = format!("mem_{}", generate_unique_id());
        let address = self.allocate_internal(size);

        let block = MemoryBlock {
            id: id.clone(),
            address,
            size,
            owner: owner.clone(),
            allocated_at: chrono::Utc::now(),
        };

        state.allocations.insert(id.clone(), block);

        // 更新统计信息
        stats.total_allocated += size;
        stats.active_allocations += 1;
        if current_usage + size > stats.peak_usage {
            stats.peak_usage = current_usage + size;
        }

        info!(
            memory_id = %id,
            size = size,
            owner = %owner,
            address = format!("{:#x}", address),
            total_allocated = stats.total_allocated,
            active_allocations = stats.active_allocations,
            "memory allocated successfully"
        );

        Ok(MemoryHandle {
            id,
            address,
            size,
            owner,
        })
    }

    fn allocate_internal(&self, size: usize) -> usize {
        // 实际内存分配逻辑（此处为占位符）
        use std::ptr;
        unsafe {
            let layout = std::alloc::Layout::from_size_align_unchecked(size, 16);
            let ptr = std::alloc::alloc(layout);
            if ptr.is_null() {
                std::alloc::handle_alloc_error(layout);
            }
            ptr as usize
        }
    }
}
```

---

## TypeScript 规范

### 代码示例

```typescript
/**
 * AgentOS OpenLab - 任务管理服务
 *
 * 提供任务的CRUD操作和实时状态更新功能。
 */

import { injectable, inject } from 'inversify';
import { EventEmitter } from 'events';
import type { ILogger } from '../interfaces/logger.interface';
import type { ITaskRepository } from '../interfaces/task.repository.interface';
import type {
  ITaskService,
  TaskCreateDTO,
  TaskDTO,
  TaskStatus,
  TaskPriority,
} from '../interfaces/task.service.interface';
import { TaskNotFoundError } from '../errors/task-not-found.error';
import { ValidationError } from '../errors/validation.error';

/**
 * 任务状态枚举
 */
export enum TaskStatus {
  QUEUED = 'queued',
  RUNNING = 'running',
  COMPLETED = 'completed',
  FAILED = 'failed',
  CANCELLED = 'cancelled',
  TIMEOUT = 'timeout',
}

/**
 * 任务优先级枚举
 */
export enum TaskPriority {
  CRITICAL = 'critical',
  HIGH = 'high',
  NORMAL = 'normal',
  LOW = 'low',
}

/**
 * 任务创建数据传输对象
 */
export interface TaskCreateDTO {
  /** 任务描述 */
  description: string;

  /** 优先级（默认NORMAL） */
  priority?: TaskPriority;

  /** 超时时间（秒，默认300） */
  timeoutSeconds?: number;

  /** 标签列表 */
  tags?: string[];

  /** 元数据 */
  metadata?: Record<string, unknown>;
}

/**
 * 任务数据传输对象
 */
export interface TaskDTO {
  id: string;
  description: string;
  status: TaskStatus;
  priority: TaskPriority;
  progressPercent: number;
  createdAt: Date;
  startedAt?: Date;
  completedAt?: Date;
  result?: unknown;
  error?: string;
}

/**
 * 任务管理服务实现
 *
 * @example
 * ```typescript
 * const taskService = container.get<ITaskService>(TYPES.TaskService);
 *
 * const task = await taskService.create({
 *   description: '分析财报数据',
 *   priority: TaskPriority.HIGH,
 *   tags: ['finance', 'analysis'],
 * });
 *
 * console.log(`Task created: ${task.id}`);
 * ```
 */
@injectable()
export class TaskService extends EventEmitter implements ITaskService {
  constructor(
    @inject(TYPES.Logger) private readonly logger: ILogger,
    @inject(TYPES.TaskRepository) private readonly repository: ITaskRepository,
  ) {
    super();
    this.setMaxListeners(100);  // 允许多个监听器
  }

  /**
   * 创建新任务
   *
   * @param dto - 任务创建数据
   * @returns 创建的任务对象
   * @throws {ValidationError} 当描述为空时抛出
   *
   * @public
   */
  public async create(dto: TaskCreateDTO): Promise<TaskDTO> {
    // 参数验证
    if (!dto.description?.trim()) {
      throw new ValidationError(
        '任务描述不能为空',
        { field: 'description', value: dto.description },
      );
    }

    // 构建任务实体
    const task: Omit<TaskDTO, 'id' | 'createdAt'> = {
      description: dto.description.trim(),
      status: TaskStatus.QUEUED,
      priority: dto.priority ?? TaskPriority.NORMAL,
      progressPercent: 0,
      metadata: dto.metadata ?? {},
    };

    // 持久化到数据库
    const savedTask = await this.repository.create(task);

    // 发布事件
    this.emit('task:created', savedTask);

    // 结构化日志
    this.logger.info('task_created', {
      taskId: savedTask.id,
      description: savedTask.description,
      priority: savedTask.priority,
      traceId: this.generateTraceId(),
    });

    return savedTask;
  }

  /**
   * 根据ID获取任务详情
   *
   * @param taskId - 任务ID
   * @returns 任务对象
   * @throws {TaskNotFoundError} 当任务不存在时抛出
   */
  public async getById(taskId: string): Promise<TaskDTO> {
    const task = await this.repository.findById(taskId);

    if (!task) {
      this.logger.warn('task_not_found', { taskId });
      throw new TaskNotFoundError(taskId);
    }

    return task;
  }

  /**
   * 取消任务
   *
   * @param taskId - 任务ID
   * @returns 更新后的任务
   */
  public async cancel(taskId: string): Promise<TaskDTO> {
    const task = await this.getById(taskId);

    if (task.status !== TaskStatus.QUEUED && task.status !== TaskStatus.RUNNING) {
      throw new ValidationError(
        `无法取消处于${task.status}状态的任务`,
        { currentStatus: task.status },
      );
    }

    const updatedTask = await this.repository.update(taskId, {
      status: TaskStatus.CANCELLED,
      completedAt: new Date(),
    });

    this.emit('task:cancelled', updatedTask);

    this.logger.info('task_cancelled', {
      taskId,
      previousStatus: task.status,
    });

    return updatedTask;
  }

  /**
   * 生成追踪ID（用于请求链路追踪）
   */
  private generateTraceId(): string {
    return `trace_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 10)}`;
  }
}
```

---

## 🧪 测试规范

### 单元测试要求

| 语言 | 框架 | 覆盖率要求 |
|------|------|-----------|
| C/C++ | Unity/CTest | ≥90%（关键路径≥95%） |
| Python | pytest | ≥90% |
| Go | go test | ≥85% |
| Rust | cargo test | ≥85% |
| TypeScript | Jest | ≥85% |

### 测试命名规范

```
[测试场景]_[测试条件]_[预期行为]

示例:
test_ipc_send_valid_message_should_return_success
test_memory_allocate_exceeding_limit_should_throw_out_of_memory
test_task_create_empty_description_should_raise_validation_error
```

---

## 🔍 代码审查清单

提交PR前，请确保通过以下检查：

### 通用检查

- [ ] 代码符合对应语言的编码规范
- [ ] 所有公共API都有完整的Docstring/注释
- [ ] 错误消息遵循统一模板（包含上下文和建议）
- [ ] 日志使用结构化格式（JSON）
- [ ] 没有引入新的编译警告（`-Wall -Wextra -Werror`）

### 安全检查

- [ ] 没有硬编码的密码或密钥
- [ ] 用户输入经过净化处理
- [ ] 敏感数据不记录到日志
- [ ] SQL查询使用参数化查询（防注入）

### 性能检查

- [ ] 没有明显的性能瓶颈（N+1查询等）
- [ ] 大数据处理考虑分页或流式处理
- [ ] 异步操作正确处理并发

### 测试检查

- [ ] 新增功能的单元测试覆盖率≥90%
- [ ] 测试可以独立运行（无外部依赖）
- [ ] 边界条件和异常情况有测试覆盖

---

## 📚 相关文档

- [**架构设计原则**](../../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md) — 五维正交24条原则
- [**测试指南**](testing.md) — 详细的测试策略和工具使用
- [**调试技巧**](debugging.md) — 性能分析和问题定位方法
- [**C安全编程指南**](../../agentos/specifications/coding_standard/C%26Cpp-secure-coding-guide.md) — C语言安全最佳实践

---

> *"代码是写给人看的，顺便能在机器上运行。"*

**© 2026 SPHARX Ltd. All Rights Reserved.**
