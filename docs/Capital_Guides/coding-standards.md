# 缂栫爜瑙勮寖

**鐗堟湰**: 1.0.0  
**鏈€鍚庢洿鏂?*: 2026-04-05  
**閫傜敤鑼冨洿**: 鎵€鏈堿gentOS璐＄尞鑰? 

---

## 馃搵 鎬诲垯

AgentOS浠ｇ爜閬靛惊**浜旂淮姝ｄ氦璁捐鍘熷垯**涓殑**缇庡瑙?*鍥涘師鍒欙細

- **A-1 绠€绾﹁嚦涓?*: 鐢ㄦ渶灏戠殑鎺ュ彛鎻愪緵鏈€澶х殑浠峰€?- **A-2 鏋佽嚧缁嗚妭**: 閿欒娑堟伅銆佹棩蹇楁牸寮忋€佸懡鍚嶄竴鑷?- **A-3 浜烘枃鍏虫€€**: 寮€鍙戣€呬綋楠屼笌鐢ㄦ埛浣撻獙骞堕噸
- **A-4 瀹岀編涓讳箟**: 浠ｇ爜璐ㄩ噺銆佹祴璇曡鐩栥€佹枃妗ｅ畬鏁?
---

## 馃寪 澶氳瑷€瑙勮寖绱㈠紩

| 璇█ | 瑙勮寖鏂囦欢 | 涓昏鐢ㄩ€?|
|------|---------|---------|
| **C/C++** | [#C/C++瑙勮寖](#cc瑙勮寖) | 鍐呮牳灞?corekern)銆佸畨鍏ㄧ┕椤?cupolas) |
| **Python** | [#Python瑙勮寖](#python瑙勮寖) | 瀹堟姢杩涚▼(daemon)銆丼DK(toolkit/python) |
| **Go** | [#Go瑙勮寖](#go瑙勮寖) | SDK(toolkit/go)銆丆LI宸ュ叿 |
| **Rust** | [#Rust瑙勮寖](#rust瑙勮寖) | 楂樻€ц兘缁勪欢(toolkit/rust) |
| **TypeScript** | [#TypeScript瑙勮寖](#typescript瑙勮寖) | OpenLab鍓嶇銆乄eb SDK |

---

## C/C++ 瑙勮寖

### 鏂囦欢缁勭粐

```c
/**
 * @file ipc_channel.c
 * @brief IPC閫氶亾瀹炵幇 - 杩涚▼闂撮€氫俊鏍稿績妯″潡
 * @author SPHARX Kernel Team
 * @version 1.0.0
 * @date 2026-04-05
 * @copyright Copyright (c) 2026 SPHARX Ltd.
 *
 * @details 鏈枃浠跺疄鐜颁簡AgentOS寰唴鏍哥殑IPC鏈哄埗锛? *          鏀寔鍚屾/寮傛娑堟伅浼犻€掋€侀浂鎷疯礉浼樺寲銆? *
 * @see ipc_channel.h
 * @see ARCHITECTURAL_PRINCIPLES.md K-2鎺ュ彛濂戠害鍖栧師鍒? */

/* ==========================================================================
 * 澶存枃浠跺寘鍚紙鎸夐『搴忥級
 * ========================================================================== */

// 1. 绯荤粺澶存枃浠?#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 2. 绗笁鏂瑰簱澶存枃浠?#include <cjson/cJSON.h>
#include <openssl/sha.h>

// 3. 椤圭洰鍏叡澶存枃浠?#include "agentos/common/types.h"
#include "agentos/error_codes.h"

// 4. 妯″潡绉佹湁澶存枃浠?#include "ipc_channel.h"
#include "ipc_internal.h"

/* ==========================================================================
 * 瀹忓畾涔夊父閲? * ========================================================================== */

/** IPC娑堟伅鏈€澶ц浇鑽峰ぇ灏?(1MB) */
#define IPC_MAX_PAYLOAD_SIZE (1 << 20)

/** IPC榛樿瓒呮椂鏃堕棿 (5绉? */
#define IPC_DEFAULT_TIMEOUT_MS 5000

/** 榛樿閲嶈瘯娆℃暟 */
#define IPC_DEFAULT_RETRIES 3

/* ==========================================================================
 * 绫诲瀷瀹氫箟
 * ========================================================================== */

/**
 * @brief IPC閫氶亾閰嶇疆缁撴瀯浣? */
typedef struct ipc_channel_config_t {
    char channel_name[IPC_MAX_NAME_LEN];   /** 閫氶亾鍚嶇О */
    uint32_t max_message_size;              /** 鏈€澶ф秷鎭ぇ灏?*/
    uint32_t buffer_size;                   /** 缂撳啿鍖哄ぇ灏?*/
    uint32_t timeout_ms;                    /** 瓒呮椂鏃堕棿 */
    ipc_mode_t mode;                        /** 閫氫俊妯″紡 */
} ipc_channel_config_t;

/* ==========================================================================
 * 鍑芥暟瀹炵幇
 * ========================================================================== */

/**
 * @brief 鍒涘缓IPC閫氶亾
 *
 * @param config 閫氶亾閰嶇疆锛堣緭鍏ワ級
 * @param[out] channel 杈撳嚭鐨勯€氶亾鍙ユ焺
 * @return 鎴愬姛杩斿洖AGENTOS_OK锛屽け璐ヨ繑鍥炲搴旈敊璇爜
 *
 * @pre config涓嶄负NULL涓旈€氳繃楠岃瘉
 * @post channel鎸囧悜鏈夋晥鐨勯€氶亾鍙ユ焺
 *
 * @par 绀轰緥:
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
 *     fprintf(stderr, "[ERROR] 鍒涘缓閫氶亾澶辫触: %s\n",
 *             agentos_error_get_message(ret));
 *     return ret;
 * }
 * @endcode
 *
 * @since 1.0.0
 * @note 绾跨▼瀹夊叏锛氭鍑芥暟鍐呴儴浣跨敤浜掓枼閿佷繚鎶? * @warning 璋冪敤鑰呰礋璐ｅ湪浣跨敤瀹屾瘯鍚庤皟鐢╥pc_channel_destroy()閲婃斁璧勬簮
 *
 * @see ipc_channel_destroy()
 * @see ipc_channel_send()
 * @see ARCHITECTURAL_PRINCIPLES.md K-2鎺ュ彛濂戠害鍖栧師鍒? */
int ipc_channel_create(const ipc_channel_config_t *config,
                       ipc_channel_t **channel)
{
    // 鍙傛暟楠岃瘉锛堥伒寰狤-6閿欒鍙拷婧師鍒欙級
    if (config == NULL) {
        return AGENTOS_EINVAL;
    }

    if (channel == NULL) {
        return AGENTOS_EINVAL;
    }

    // 楠岃瘉閰嶇疆鏈夋晥鎬?    if (strlen(config->channel_name) == 0) {
        return AGENTOS_EINVAL;
    }

    if (config->max_message_size > IPC_MAX_PAYLOAD_SIZE ||
        config->max_message_size == 0) {
        return AGENTOS_EINVAL;
    }

    // 鍒嗛厤鍐呭瓨锛堜娇鐢ㄥ璞℃睜锛岄伒寰狤-3璧勬簮纭畾鎬у師鍒欙級
    ipc_channel_t *ch = memory_pool_alloc(sizeof(ipc_channel_t));
    if (ch == NULL) {
        return AGENTOS_ENOMEM;
    }

    // 鍒濆鍖栫粨鏋勪綋
    memset(ch, 0, sizeof(ipc_channel_t));
    strncpy(ch->name, config->channel_name, IPC_MAX_NAME_LEN - 1);
    ch->max_message_size = config->max_message_size;
    ch->buffer_size = config->buffer_size;
    ch->timeout_ms = config->timeout_ms;
    ch->mode = config->mode;

    // 鍒濆鍖栦簰鏂ラ攣
    if (pthread_mutex_init(&ch->mutex, NULL) != 0) {
        memory_pool_free(ch);
        return AGENTOS_EINTERNAL;
    }

    // 鍒濆鍖栨潯浠跺彉閲?    if (pthread_cond_init(&ch->cond, NULL) != 0) {
        pthread_mutex_destroy(&ch->mutex);
        memory_pool_free(ch);
        return AGENTOS_EINTERNAL;
    }

    // 鍒濆鍖栨秷鎭槦鍒?    ch->queue = queue_create(config->buffer_size);
    if (ch->queue == NULL) {
        pthread_cond_destroy(&ch->cond);
        pthread_mutex_destroy(&ch->mutex);
        memory_pool_free(ch);
        return AGENTOS_ENOMEM;
    }

    // 璁剧疆寮曠敤璁℃暟
    atomic_store(&ch->ref_count, 1);

    // 杈撳嚭閫氶亾鍙ユ焺
    *channel = ch;

    // 缁撴瀯鍖栨棩蹇楄緭鍑猴紙閬靛惊E-2鍙娴嬫€у師鍒欙級
    LOG_INFO("ipc",
             "channel_created",
             LOG_KV("channel_name", "%s", ch->name),
             LOG_KV("max_msg_size", "%u", ch->max_message_size),
             LOG_KV("buffer_size", "%u", ch->buffer_size));

    return AGENTOS_OK;
}
```

### 鍛藉悕瑙勮寖

| 绫诲埆 | 瑙勮寖 | 绀轰緥 |
|------|------|------|
| **鍑芥暟** | `module_action_object()` | `ipc_channel_create()`, `memory_pool_alloc()` |
| **鍙橀噺** | `snake_case` | `max_message_size`, `buffer_size` |
| **甯搁噺** | `UPPER_SNAKE_CASE` | `IPC_MAX_PAYLOAD_SIZE`, `DEFAULT_TIMEOUT_MS` |
| **绫诲瀷** | `_t` 鍚庣紑 | `ipc_channel_t`, `task_handle_t` |
| **瀹?* | `UPPER_SNAKE_CASE` | `LOG_INFO()`, `RETURN_IF_ERROR()` |
| **鏋氫妇鍊?* | `MODULE_` 鍓嶇紑 | `IPC_MODE_SYNC`, `IPC_MODE_ASYNC` |
| **閿欒鐮?* | `AGENTOS_E` 鍓嶇紑 | `AGENTOS_OK`, `AGENTOS_EINVAL`, `AGENTOS_ENOMEM` |

### 閿欒澶勭悊妯″紡

```c
// 浣跨敤缁熶竴鐨勯敊璇繑鍥炴ā寮?int result = some_operation();
if (result != AGENTOS_OK) {
    LOG_ERROR("module",
              "operation_failed",
              LOG_KV("error_code", "%d", result),
              LOG_KV("error_msg", "%s", agentos_error_get_message(result)),
              LOG_KV("context", "key=%s", value));
    return result;  // 閿欒鍚戜笂浼犳挱
}

// 浣跨敤瀹忕畝鍖栧父瑙佹ā寮?RETURN_IF_ERROR(ipc_channel_create(&config, &channel));
RETURN_IF_NULL(ptr, AGENTOS_ENOMEM);

// 璧勬簮鑾峰彇鍗冲垵濮嬪寲(RAII)妯″紡锛圕璇█涓娇鐢╣oto娓呯悊锛?int complex_operation(void) {
    resource_a_t *a = alloc_resource_a();
    if (!a) goto error_a;

    resource_b_t *b = alloc_resource_b();
    if (!b) goto error_b;

    resource_c_t *c = alloc_resource_c();
    if (!c) goto error_c;

    // ... 浣跨敤璧勬簮 ...

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

## Python 瑙勮寖

### 鏂囦欢缁撴瀯

```python
"""
AgentOS LLM鏈嶅姟瀹堟姢杩涚▼ (llm_d)

鏈ā鍧楀疄鐜颁簡LLM鏈嶅姟鐨勭鐞嗕笌璋冨害鍔熻兘锛?鍖呮嫭妯″瀷鍔犺浇銆佹帹鐞嗘墽琛屻€乀oken绠＄悊绛夈€?"""

from __future__ import annotations

# 鏍囧噯搴?import asyncio
import logging
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, Dict, List, Any
from pathlib import Path

# 绗笁鏂瑰簱
import aiohttp
from pydantic import BaseModel, Field, validator

# 椤圭洰鍐呴儴
from agentos.commons.errors import AgentOSError, ErrorCode
from agentos.commons.logging import StructuredLogger
from agentos.daemon.base import BaseDaemon


class LLMProvider(str, Enum):
    """LLM鎻愪緵鍟嗘灇涓?""
    OPENAI = "openai"
    DEEPSEEK = "deepseek"
    LOCAL = "local"


@dataclass
class LLMConfig:
    """LLM鏈嶅姟閰嶇疆"""
    provider: LLMProvider = LLMProvider.OPENAI
    api_key: str = Field(..., min_length=32)
    base_url: str = "https://api.openai.com/v1"
    model: str = "gpt-4-turbo"
    max_tokens: int = Field(default=4096, ge=1, le=128000)
    temperature: float = Field(default=0.7, ge=0.0, le=2.0)

    @validator('api_key')
    def validate_api_key(cls, v):
        """楠岃瘉API瀵嗛挜鏍煎紡"""
        if not v.startswith('sk-'):
            raise ValueError('API瀵嗛挜蹇呴』浠k-寮€澶?)
        return v


class LLMDaemon(BaseDaemon):
    """
    LLM鏈嶅姟绠＄悊瀹堟姢杩涚▼

    Attributes:
        config (LLMConfig): LLM鏈嶅姟閰嶇疆
        logger (StructuredLogger): 缁撴瀯鍖栨棩蹇楄褰曞櫒
        _session (aiohttp.ClientSession): HTTP瀹㈡埛绔細璇?        _model_cache: Dict[str, Any]: 妯″瀷缂撳瓨

    Example:
        >>> config = LLMConfig(
        ...     provider=LLMProvider.OPENAI,
        ...     api_key='sk-your-key',
        ...     model='gpt-4-turbo'
        ... )
        >>> daemon = LLMDaemon(config)
        >>> await daemon.start()

    See Also:
        - BaseDaemon: 鍩虹瀹堟姢杩涚▼绫?        - ARCHITECTURAL_PRINCIPLES.md C-1鍙岀郴缁熷崗鍚屽師鍒?    """

    def __init__(self, config: LLMConfig) -> None:
        """
        鍒濆鍖朙LM瀹堟姢杩涚▼

        Args:
            config: LLM鏈嶅姟閰嶇疆瀵硅薄

        Raises:
            ValueError: 閰嶇疆鏃犳晥鏃舵姏鍑?        """
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
        """鍚姩瀹堟姢杩涚▼"""
        await super().start()

        # 鍒涘缓HTTP浼氳瘽
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
        """鍋滄瀹堟姢杩涚▼"""
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
        鎵цLLM鎺ㄧ悊

        Args:
            prompt: 鐢ㄦ埛鎻愮ず璇?            system_prompt: 绯荤粺鎻愮ず璇嶏紙鍙€夛級
            max_tokens: 鏈€澶х敓鎴怲oken鏁帮紙鍙€夛紝瑕嗙洊閰嶇疆锛?            temperature: 娓╁害鍙傛暟锛堝彲閫夛紝瑕嗙洊閰嶇疆锛?
        Returns:
            鍖呭惈鎺ㄧ悊缁撴灉鐨勫瓧鍏革細
            - content: 鐢熸垚鐨勬枃鏈?            - usage: Token浣跨敤缁熻
            - model: 浣跨敤鐨勬ā鍨嬪悕
            - latency_ms: 鎺ㄧ悊寤惰繜锛堟绉掞級

        Raises:
            AgentOSError: API璋冪敤澶辫触鏃舵姏鍑?            ValueError: 鍙傛暟鏃犳晥鏃舵姏鍑?
        Example:
            >>> result = await daemon.inference(
            ...     prompt="鍒嗘瀽杩欎唤璐㈡姤",
            ...     max_tokens=2000
            ... )
            >>> print(result['content'])
        """
        # 鍙傛暟楠岃瘉
        if not prompt or not prompt.strip():
            raise ValueError("prompt涓嶈兘涓虹┖")

        actual_max_tokens = max_tokens or self.config.max_tokens
        actual_temperature = temperature or self.config.temperature

        # 鏋勫缓璇锋眰杞借嵎
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
                        message=f"LLM API璋冪敤澶辫触 (context: status={response.status}, body={error_body[:200]}). "
                               f"Suggestion: 妫€鏌PI瀵嗛挜鍜岀綉缁滆繛鎺ャ€?,
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

                # 缁撴瀯鍖栨棩蹇楄褰曪紙閬靛惊E-2鍙娴嬫€у師鍒欙級
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
                message=f"缃戠粶璇锋眰澶辫触 (context: url={self.config.base_url}, error={str(e)}). "
                       f"Suggestion: 妫€鏌ョ綉缁滆繛鎺ュ拰DNS璁剧疆銆?
            )

    @staticmethod
    def _get_timestamp_ms() -> int:
        """鑾峰彇褰撳墠鏃堕棿鎴筹紙姣锛?""
        import time
        return int(time.time() * 1000)

    def _generate_trace_id(self) -> str:
        """鐢熸垚杩借釜ID"""
        import uuid
        return f"trace_{uuid.uuid4().hex[:12]}"
```

### Python缂栫爜瑙勫垯

| 瑙勫垯 | 瑕佹眰 | 绀轰緥 |
|------|------|------|
| **瀵煎叆椤哄簭** | 鏍囧噯鈫掔涓夋柟鈫掗」鐩唴閮?| 瑙佷笂鏂圭ず渚?|
| **绫诲瀷娉ㄨВ** | 鎵€鏈夊叕寮€API蹇呴』娉ㄨВ | `async def inference(self, prompt: str) -> Dict[str, Any]` |
| **Docstring** | Google椋庢牸锛屽寘鍚獳rgs/Returns/Raises/Example | 瑙佷笂鏂圭ず渚?|
| **瀛楃涓叉牸寮?* | f-string锛圥ython 3.6+锛?| `f"Error: {error_code}"` |
| **寮傚父澶勭悊** | 鑷畾涔夊紓甯哥被 + 璇︾粏涓婁笅鏂?| `AgentOSError(code, message)` |
| **鏃ュ織** | 缁撴瀯鍖朖SON鏍煎紡 | `logger.info("event", extra={"key": value})` |
| **寮傛缂栫▼** | 浼樺厛async/await | `async def start(self) -> None` |

---

## Go 瑙勮寖

### 椤圭洰甯冨眬

```
agentos/toolkit/go/
鈹溾攢鈹€ cmd/
鈹?  鈹斺攢鈹€ agentos-cli/           # 涓荤▼搴忓叆鍙?鈹?      鈹斺攢鈹€ main.go
鈹溾攢鈹€ internal/
鈹?  鈹溾攢鈹€ client/                # 鍐呴儴瀹炵幇
鈹?  鈹?  鈹溾攢鈹€ client.go
鈹?  鈹?  鈹斺攢鈹€ client_test.go
鈹?  鈹斺攢鈹€ models/                # 鏁版嵁妯″瀷
鈹?      鈹斺攢鈹€ types.go
鈹溾攢鈹€ pkg/                       # 鍙鍑哄寘
鈹?  鈹溾攢鈹€ api/                   # API瀹㈡埛绔?鈹?  鈹?  鈹溾攢鈹€ kernel.go
鈹?  鈹?  鈹斺攢鈹€ tasks.go
鈹?  鈹斺攢鈹€ errors/                # 閿欒澶勭悊
鈹?      鈹斺攢鈹€ errors.go
鈹溾攢鈹€ go.mod
鈹溾攢鈹€ go.sum
鈹斺攢鈹€ README.md
```

### 浠ｇ爜绀轰緥

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

## Rust 瑙勮寖

### 浠ｇ爜绀轰緥

```rust
//! AgentOS 鍐呭瓨绠＄悊妯″潡
//!
//! 鎻愪緵楂樻€ц兘鐨勫唴瀛樺垎閰嶅拰绠＄悊鍔熻兘锛?//! 鍖呮嫭瀵硅薄姹犮€佸唴瀛樼粺璁″拰娉勬紡妫€娴嬨€?
use std::sync::Arc;
use tokio::sync::RwLock;
use tracing::{info, warn, error};
use thiserror::Error;

/// 鍐呭瓨绠＄悊閿欒绫诲瀷
#[derive(Error, Debug)]
pub enum MemoryError {
    /// 鍐呭瓨涓嶈冻
    #[error("鍐呭瓨鍒嗛厤澶辫触 (context: requested={requested} bytes, available={available} bytes). Suggestion: 澧炲姞绯荤粺鍐呭瓨鎴栧噺灏戞壒閲忓ぇ灏忋€?)]
    OutOfMemory { requested: usize, available: usize },

    /// 鏃犳晥鐨勫唴瀛樺湴鍧€
    #[error("鏃犳晥鐨勫唴瀛樺湴鍧€ (context: address={:#x}). Suggestion: 妫€鏌ュ唴瀛業D鏄惁姝ｇ‘銆?)]
    InvalidAddress { address: usize },

    /// 鎵€鏈夋潈鍐茬獊
    #[error("鎵€鏈夋潈鍐茬獊 (context: memory_id={memory_id}, owner={expected_owner}, requester={requester}). Suggestion: 鍙湁鎵€鏈夎€呮墠鑳介噴鏀惧唴瀛樸€?)]
    OwnershipConflict {
        memory_id: String,
        expected_owner: String,
        requester: String,
    },
}

/// 鍐呭瓨姹犻厤缃?#[derive(Debug, Clone)]
pub struct MemoryPoolConfig {
    /// 姹犳€诲ぇ灏忥紙瀛楄妭锛?    pub pool_size: usize,

    /// 鍧楀ぇ灏忥紙瀛楄妭锛?    pub block_size: usize,

    /// 鏄惁鍚敤娉勬紡妫€娴?    pub leak_detection: bool,
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

/// 楂樻€ц兘鍐呭瓨姹?///
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
    /// 宸插垎閰嶇殑鍐呭瓨鍧?    allocations: std::collections::HashMap<String, MemoryBlock>,
    /// 绌洪棽鍧楅摼琛?    free_list: Vec<usize>,
}

#[derive(Debug, Default)]
pub struct MemoryStats {
    /// 鎬诲垎閰嶅瓧鑺傛暟
    pub total_allocated: usize,
    /// 鎬婚噴鏀惧瓧鑺傛暟
    pub total_freed: usize,
    /// 褰撳墠娲昏穬鍒嗛厤鏁?    pub active_allocations: usize,
    /// 宄板€间娇鐢ㄩ噺
    pub peak_usage: usize,
}

impl MemoryPool {
    /// 鍒涘缓鏂扮殑鍐呭瓨姹犲疄渚?    ///
    /// # Arguments
    /// * `config` - 鍐呭瓨姹犻厤缃?    ///
    /// # Returns
    /// * `Ok(MemoryPool)` - 鎴愬姛鍒涘缓鐨勫唴瀛樻睜
    /// * `Err(MemoryError)` - 閰嶇疆鏃犳晥鏃惰繑鍥為敊璇?    ///
    /// # Panics
    /// 濡傛灉 `config.block_size` 涓?锛屽皢瑙﹀彂panic
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

    /// 鍒嗛厤鍐呭瓨鍧?    ///
    /// 浠庡唴瀛樻睜涓垎閰嶆寚瀹氬ぇ灏忕殑鍐呭瓨鍧椼€?    ///
    /// # Arguments
    /// * `size` - 璇锋眰鐨勫瓧鑺傛暟
    /// * `owner` - 鎵€鏈夎€呮爣璇嗭紙鐢ㄤ簬璺熻釜鍜屾潈闄愭帶鍒讹級
    ///
    /// # Returns
    /// * `Ok(MemoryHandle)` - 鍐呭瓨鍙ユ焺锛屽寘鍚湴鍧€鍜屽厓鏁版嵁
    /// * `Err(MemoryError)` - 鍒嗛厤澶辫触鏃惰繑鍥為敊璇?    ///
    /// # Errors
    /// * `OutOfMemory` - 姹犵┖闂翠笉瓒虫椂杩斿洖
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

        // 妫€鏌ユ槸鍚︽湁瓒冲绌洪棿
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

        // 鍒嗛厤鍐呭瓨鍧楋紙绠€鍖栧疄鐜帮級
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

        // 鏇存柊缁熻淇℃伅
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
        // 瀹為檯鍐呭瓨鍒嗛厤閫昏緫锛堟澶勪负鍗犱綅绗︼級
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

## TypeScript 瑙勮寖

### 浠ｇ爜绀轰緥

```typescript
/**
 * AgentOS OpenLab - 浠诲姟绠＄悊鏈嶅姟
 *
 * 鎻愪緵浠诲姟鐨凜RUD鎿嶄綔鍜屽疄鏃剁姸鎬佹洿鏂板姛鑳姐€? */

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
 * 浠诲姟鐘舵€佹灇涓? */
export enum TaskStatus {
  QUEUED = 'queued',
  RUNNING = 'running',
  COMPLETED = 'completed',
  FAILED = 'failed',
  CANCELLED = 'cancelled',
  TIMEOUT = 'timeout',
}

/**
 * 浠诲姟浼樺厛绾ф灇涓? */
export enum TaskPriority {
  CRITICAL = 'critical',
  HIGH = 'high',
  NORMAL = 'normal',
  LOW = 'low',
}

/**
 * 浠诲姟鍒涘缓鏁版嵁浼犺緭瀵硅薄
 */
export interface TaskCreateDTO {
  /** 浠诲姟鎻忚堪 */
  description: string;

  /** 浼樺厛绾э紙榛樿NORMAL锛?*/
  priority?: TaskPriority;

  /** 瓒呮椂鏃堕棿锛堢锛岄粯璁?00锛?*/
  timeoutSeconds?: number;

  /** 鏍囩鍒楄〃 */
  tags?: string[];

  /** 鍏冩暟鎹?*/
  metadata?: Record<string, unknown>;
}

/**
 * 浠诲姟鏁版嵁浼犺緭瀵硅薄
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
 * 浠诲姟绠＄悊鏈嶅姟瀹炵幇
 *
 * @example
 * ```typescript
 * const taskService = container.get<ITaskService>(TYPES.TaskService);
 *
 * const task = await taskService.create({
 *   description: '鍒嗘瀽璐㈡姤鏁版嵁',
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
    this.setMaxListeners(100);  // 鍏佽澶氫釜鐩戝惉鍣?  }

  /**
   * 鍒涘缓鏂颁换鍔?   *
   * @param dto - 浠诲姟鍒涘缓鏁版嵁
   * @returns 鍒涘缓鐨勪换鍔″璞?   * @throws {ValidationError} 褰撴弿杩颁负绌烘椂鎶涘嚭
   *
   * @public
   */
  public async create(dto: TaskCreateDTO): Promise<TaskDTO> {
    // 鍙傛暟楠岃瘉
    if (!dto.description?.trim()) {
      throw new ValidationError(
        '浠诲姟鎻忚堪涓嶈兘涓虹┖',
        { field: 'description', value: dto.description },
      );
    }

    // 鏋勫缓浠诲姟瀹炰綋
    const task: Omit<TaskDTO, 'id' | 'createdAt'> = {
      description: dto.description.trim(),
      status: TaskStatus.QUEUED,
      priority: dto.priority ?? TaskPriority.NORMAL,
      progressPercent: 0,
      metadata: dto.metadata ?? {},
    };

    // 鎸佷箙鍖栧埌鏁版嵁搴?    const savedTask = await this.repository.create(task);

    // 鍙戝竷浜嬩欢
    this.emit('task:created', savedTask);

    // 缁撴瀯鍖栨棩蹇?    this.logger.info('task_created', {
      taskId: savedTask.id,
      description: savedTask.description,
      priority: savedTask.priority,
      traceId: this.generateTraceId(),
    });

    return savedTask;
  }

  /**
   * 鏍规嵁ID鑾峰彇浠诲姟璇︽儏
   *
   * @param taskId - 浠诲姟ID
   * @returns 浠诲姟瀵硅薄
   * @throws {TaskNotFoundError} 褰撲换鍔′笉瀛樺湪鏃舵姏鍑?   */
  public async getById(taskId: string): Promise<TaskDTO> {
    const task = await this.repository.findById(taskId);

    if (!task) {
      this.logger.warn('task_not_found', { taskId });
      throw new TaskNotFoundError(taskId);
    }

    return task;
  }

  /**
   * 鍙栨秷浠诲姟
   *
   * @param taskId - 浠诲姟ID
   * @returns 鏇存柊鍚庣殑浠诲姟
   */
  public async cancel(taskId: string): Promise<TaskDTO> {
    const task = await this.getById(taskId);

    if (task.status !== TaskStatus.QUEUED && task.status !== TaskStatus.RUNNING) {
      throw new ValidationError(
        `鏃犳硶鍙栨秷澶勪簬${task.status}鐘舵€佺殑浠诲姟`,
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
   * 鐢熸垚杩借釜ID锛堢敤浜庤姹傞摼璺拷韪級
   */
  private generateTraceId(): string {
    return `trace_${Date.now().toString(36)}_${Math.random().toString(36).slice(2, 10)}`;
  }
}
```

---

## 馃И 娴嬭瘯瑙勮寖

### 鍗曞厓娴嬭瘯瑕佹眰

| 璇█ | 妗嗘灦 | 瑕嗙洊鐜囪姹?|
|------|------|-----------|
| C/C++ | Unity/CTest | 鈮?0%锛堝叧閿矾寰勨墺95%锛?|
| Python | pytest | 鈮?0% |
| Go | go test | 鈮?5% |
| Rust | cargo test | 鈮?5% |
| TypeScript | Jest | 鈮?5% |

### 娴嬭瘯鍛藉悕瑙勮寖

```
[娴嬭瘯鍦烘櫙]_[娴嬭瘯鏉′欢]_[棰勬湡琛屼负]

绀轰緥:
test_ipc_send_valid_message_should_return_success
test_memory_allocate_exceeding_limit_should_throw_out_of_memory
test_task_create_empty_description_should_raise_validation_error
```

---

## 馃攳 浠ｇ爜瀹℃煡娓呭崟

鎻愪氦PR鍓嶏紝璇风‘淇濋€氳繃浠ヤ笅妫€鏌ワ細

### 閫氱敤妫€鏌?
- [ ] 浠ｇ爜绗﹀悎瀵瑰簲璇█鐨勭紪鐮佽鑼?- [ ] 鎵€鏈夊叕鍏盇PI閮芥湁瀹屾暣鐨凞ocstring/娉ㄩ噴
- [ ] 閿欒娑堟伅閬靛惊缁熶竴妯℃澘锛堝寘鍚笂涓嬫枃鍜屽缓璁級
- [ ] 鏃ュ織浣跨敤缁撴瀯鍖栨牸寮忥紙JSON锛?- [ ] 娌℃湁寮曞叆鏂扮殑缂栬瘧璀﹀憡锛坄-Wall -Wextra -Werror`锛?
### 瀹夊叏妫€鏌?
- [ ] 娌℃湁纭紪鐮佺殑瀵嗙爜鎴栧瘑閽?- [ ] 鐢ㄦ埛杈撳叆缁忚繃鍑€鍖栧鐞?- [ ] 鏁忔劅鏁版嵁涓嶈褰曞埌鏃ュ織
- [ ] SQL鏌ヨ浣跨敤鍙傛暟鍖栨煡璇紙闃叉敞鍏ワ級

### 鎬ц兘妫€鏌?
- [ ] 娌℃湁鏄庢樉鐨勬€ц兘鐡堕锛圢+1鏌ヨ绛夛級
- [ ] 澶ф暟鎹鐞嗚€冭檻鍒嗛〉鎴栨祦寮忓鐞?- [ ] 寮傛鎿嶄綔姝ｇ‘澶勭悊骞跺彂

### 娴嬭瘯妫€鏌?
- [ ] 鏂板鍔熻兘鐨勫崟鍏冩祴璇曡鐩栫巼鈮?0%
- [ ] 娴嬭瘯鍙互鐙珛杩愯锛堟棤澶栭儴渚濊禆锛?- [ ] 杈圭晫鏉′欢鍜屽紓甯告儏鍐垫湁娴嬭瘯瑕嗙洊

---

## 馃摎 鐩稿叧鏂囨。

- [**鏋舵瀯璁捐鍘熷垯**](../../agentos/docs/ARCHITECTURAL_PRINCIPLES.md) 鈥?浜旂淮姝ｄ氦24鏉″師鍒?- [**娴嬭瘯鎸囧崡**](testing.md) 鈥?璇︾粏鐨勬祴璇曠瓥鐣ュ拰宸ュ叿浣跨敤
- [**璋冭瘯鎶€宸?*](debugging.md) 鈥?鎬ц兘鍒嗘瀽鍜岄棶棰樺畾浣嶆柟娉?- [**C瀹夊叏缂栫▼鎸囧崡**](../../agentos/specifications/coding_standard/C%26Cpp-secure-coding-guide.md) 鈥?C璇█瀹夊叏鏈€浣冲疄璺?
---

> *"浠ｇ爜鏄啓缁欎汉鐪嬬殑锛岄『渚胯兘鍦ㄦ満鍣ㄤ笂杩愯銆?*

**漏 2026 SPHARX Ltd. All Rights Reserved.**
