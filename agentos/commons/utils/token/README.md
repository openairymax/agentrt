# Token — 令牌管理模块

**模块路径**: `agentos/commons/utils/token/`
**版本**: v0.1.0

## 概述

Token 模块提供 LLM Token 计数与预算管理功能，是 AgentOS 资源配额管理的核心组件。该模块支持多模型 Token 计算（GPT-4、GPT-3.5、Claude、LLaMA 等），提供 Token 计数器、预算管理、文本截断、资源配额检查等完整功能。通过标准化的 Token 计算算法，确保跨语言实现的一致性，支持资源配额管理和监控集成。

## 设计目标

- **标准化计算**：提供确定性的 Token 计算标准，确保 C/Python 跨语言实现一致
- **多模型支持**：支持 GPT-4、GPT-3.5、Claude、LLaMA 等主流模型的 Token 计算
- **资源配额管理**：支持单次请求、分钟级、小时级、日级等多级配额限制
- **预算控制**：提供 Token 预算创建、消耗追踪、剩余量查询和时间窗口管理

## 目录结构

```
token/
├── include/
│   ├── token.h               # Token 计数器与预算管理 API
│   └── token_standard.h      # 标准化 Token 计算 API（配额、模型类型、语言特征分析）
├── src/
│   ├── counter.c             # Token 计数器实现
│   ├── budget.c              # Token 预算管理实现
│   └── token_standard.c      # 标准化 Token 计算实现
└── README.md                 # 本文档
```

## 核心数据结构

### agentos_token_model_t — Token 计算模型类型

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_TOKEN_MODEL_GENERIC` | 通用模型（默认） |
| `AGENTOS_TOKEN_MODEL_GPT4` | GPT-4 系列模型 |
| `AGENTOS_TOKEN_MODEL_GPT35` | GPT-3.5 系列模型 |
| `AGENTOS_TOKEN_MODEL_CLAUDE` | Claude 系列模型 |
| `AGENTOS_TOKEN_MODEL_LLAMA` | LLaMA 系列模型 |
| `AGENTOS_TOKEN_MODEL_CUSTOM` | 自定义模型 |

### agentos_token_config_t — Token 计算配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `model_type` | `agentos_token_model_t` | 模型类型 |
| `model_name` | `const char *` | 模型名称（可选） |
| `cjk_ratio` | `float` | 中日韩字符比例阈值（默认 0.3） |
| `alpha_ratio` | `float` | 字母字符比例阈值（默认 0.5） |
| `flags` | `uint32_t` | 计算标志位 |

### agentos_token_precision_t — Token 计算精度级别

| 枚举值 | 说明 |
|------|------|
| `AGENTOS_TOKEN_PRECISION_LOW` | 低精度（快速估算） |
| `AGENTOS_TOKEN_PRECISION_MEDIUM` | 中等精度 |
| `AGENTOS_TOKEN_PRECISION_HIGH` | 高精度（准确但较慢） |

### agentos_token_quota_t — 资源配额限制

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `max_tokens_per_request` | `size_t` | 8000 | 单次请求最大 Token 数 |
| `max_tokens_per_minute` | `size_t` | 60000 | 每分钟最大 Token 数 |
| `max_tokens_per_hour` | `size_t` | 360000 | 每小时最大 Token 数 |
| `max_tokens_per_day` | `size_t` | 2000000 | 每天最大 Token 数 |
| `max_requests_per_minute` | `size_t` | 60 | 每分钟最大请求数 |
| `max_requests_per_hour` | `size_t` | 3600 | 每小时最大请求数 |
| `max_requests_per_day` | `size_t` | 10000 | 每天最大请求数 |

### agentos_token_usage_t — 资源使用情况

| 字段 | 类型 | 说明 |
|------|------|------|
| `tokens_used_per_minute` | `size_t` | 当前分钟已使用 Token 数 |
| `tokens_used_per_hour` | `size_t` | 当前小时已使用 Token 数 |
| `tokens_used_per_day` | `size_t` | 当前天已使用 Token 数 |
| `requests_used_per_minute` | `size_t` | 当前分钟已使用请求数 |
| `requests_used_per_hour` | `size_t` | 当前小时已使用请求数 |
| `requests_used_per_day` | `size_t` | 当前天已使用请求数 |

## 接口说明

### Token 计数器（token.h）

| 函数 | 说明 |
|------|------|
| `agentos_token_counter_create(model_name)` | 创建 Token 计数器 |
| `agentos_token_counter_destroy(counter)` | 销毁计数器 |
| `agentos_token_counter_count(counter, text)` | 计算文本的 Token 数量 |
| `agentos_token_counter_count_batch(counter, texts, count, out_counts)` | 批量计算多个文本的 Token 数量 |
| `agentos_token_counter_truncate(counter, text, max_tokens, side)` | 截断文本到指定 Token 数 |

### Token 预算管理（token.h）

| 函数 | 说明 |
|------|------|
| `agentos_token_budget_create(max_tokens)` | 创建 Token 预算 |
| `agentos_token_budget_destroy(budget)` | 销毁预算 |
| `agentos_token_budget_add(budget, input_tokens, output_tokens)` | 添加 Token 消耗（返回 0 成功，-1 超出预算） |
| `agentos_token_budget_remaining(budget)` | 获取剩余 Token 数 |
| `agentos_token_budget_reset(budget)` | 重置预算 |
| `agentos_token_budget_used(budget)` | 获取已使用 Token 数 |
| `agentos_token_budget_input(budget)` | 获取输入 Token 数 |
| `agentos_token_budget_output(budget)` | 获取输出 Token 数 |
| `agentos_token_budget_requests(budget)` | 获取请求计数 |
| `agentos_token_budget_denied(budget)` | 获取拒绝计数 |
| `agentos_token_budget_set_window(budget, window_seconds)` | 设置时间窗口 |
| `agentos_token_budget_check_window(budget)` | 检查并重置时间窗口 |

### 标准化 Token 计算（token_standard.h）

| 函数 | 说明 |
|------|------|
| `agentos_token_standard_count(text, length, config)` | 标准化 Token 计算 |
| `agentos_token_standard_count_batch(texts, lengths, count, out_counts, config)` | 批量 Token 计算 |
| `agentos_token_analyze_text(text, length, out_cjk, out_alpha, out_total)` | 检测文本语言特征 |
| `agentos_token_get_algorithm_info()` | 获取 Token 计算算法信息 |
| `agentos_token_validate_config(config)` | 验证 Token 计算配置 |
| `agentos_token_set_precision(precision, config)` | 设置 Token 计算精度 |
| `agentos_token_check_quota(quota, requested_tokens, current_usage)` | 检查资源配额是否足够 |

## 使用示例

```c
#include "token.h"
#include "token_standard.h"

/* === Token 计数器 === */
agentos_token_counter_t *counter = agentos_token_counter_create("gpt-4");

size_t token_count = agentos_token_counter_count(counter, "Hello, world!");
printf("Token count: %zu\n", token_count);

// 批量计数
const char *texts[] = {"Hello", "World", "How are you?"};
size_t counts[3];
agentos_token_counter_count_batch(counter, texts, 3, counts);

// 截断文本
char *truncated = agentos_token_counter_truncate(counter,
    "This is a very long text that needs to be truncated",
    5, "right");
printf("Truncated: %s\n", truncated);
free(truncated);

agentos_token_counter_destroy(counter);

/* === Token 预算管理 === */
agentos_token_budget_t *budget = agentos_token_budget_create(100000);

// 设置 1 小时的时间窗口
agentos_token_budget_set_window(budget, 3600);

// 消耗 Token
if (agentos_token_budget_add(budget, 500, 200) == 0) {
    printf("Remaining: %zu\n", agentos_token_budget_remaining(budget));
} else {
    printf("Budget exceeded!\n");
}

printf("Used: %zu, Input: %zu, Output: %zu, Requests: %u, Denied: %u\n",
       agentos_token_budget_used(budget),
       agentos_token_budget_input(budget),
       agentos_token_budget_output(budget),
       agentos_token_budget_requests(budget),
       agentos_token_budget_denied(budget));

agentos_token_budget_destroy(budget);

/* === 标准化 Token 计算 === */
agentos_token_config_t config = AGENTOS_TOKEN_CONFIG_DEFAULT;
config.model_type = AGENTOS_TOKEN_MODEL_GPT4;
config.flags = AGENTOS_TOKEN_FLAG_ACCURATE;

size_t std_count = agentos_token_standard_count("Hello, 世界!", 0, &config);
printf("Standard token count: %zu\n", std_count);

// 语言特征分析
size_t cjk_chars, alpha_chars, total_chars;
agentos_token_analyze_text("Hello, 世界!", 0, &cjk_chars, &alpha_chars, &total_chars);
printf("CJK: %zu, Alpha: %zu, Total: %zu\n", cjk_chars, alpha_chars, total_chars);

/* === 资源配额检查 === */
agentos_token_quota_t quota = AGENTOS_TOKEN_QUOTA_DEFAULT;
agentos_token_usage_t usage = {0};

int quota_result = agentos_token_check_quota(&quota, 4000, &usage);
if (quota_result == 0) {
    printf("Quota OK\n");
} else {
    printf("Quota exceeded at level: %d\n", quota_result);
}
```

## 配额检查返回值

| 返回值 | 说明 |
|------|------|
| `0` | 配额足够 |
| `1` | 超出单次请求限制 |
| `2` | 超出分钟 Token 限制 |
| `3` | 超出小时 Token 限制 |
| `4` | 超出日 Token 限制 |
| `5` | 超出分钟请求限制 |
| `6` | 超出小时请求限制 |
| `7` | 超出日请求限制 |

## 计算标志位

| 标志 | 说明 |
|------|------|
| `AGENTOS_TOKEN_FLAG_ACCURATE` | 高精度模式（较慢，更准确） |
| `AGENTOS_TOKEN_FLAG_ESTIMATE` | 估算模式（较快，近似值） |
| `AGENTOS_TOKEN_FLAG_INCLUDE_BOM` | 包含 BOM 字符 |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `stddef.h` | `size_t` 等类型 |
| `stdint.h` | 固定宽度整数类型 |
| `stdlib.h` | 内存分配（`malloc`/`free`） |

---

© 2026 SPHARX Ltd. All Rights Reserved.