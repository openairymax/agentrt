# Cost — 成本估算与控制

**模块路径**: `agentos/commons/utils/cost/`
**版本**: v0.1.0

## 概述

Cost 模块提供 LLM 调用成本估算和预算控制能力，支持基于模型定价的 Token 成本预估、多周期预算管理以及成本消耗追踪。该模块是 AgentRT 成本管理体系的核心，确保 Agent 在执行 LLM 任务时不会超出预设的财务预算。

## 设计目标

- **精确估算**：基于模型名称、输入/输出 Token 数量精确计算每次调用的成本（美元）
- **预算控制**：多周期预算管理，支持按时间窗口限制总成本支出
- **模型定价表**：内置主流 LLM 模型的定价信息，支持通过 YAML 配置文件自定义
- **实时追踪**：提供剩余预算、已消耗成本的实时查询接口

## 目录结构

```
cost/
├── include/
│   └── cost.h                   # 成本估算与控制公共接口
├── src/
│   ├── estimator.c              # 成本预估器实现（模型定价表、Token 计费）
│   └── controller.c             # 预算控制器实现（周期管理、消耗追踪）
└── README.md                    # 本文档
```

## 核心数据结构

### agentos_cost_estimator_t — 成本预估器

管理模型定价表，提供基于 Token 数量的成本计算。

**定价表结构**（YAML 配置）：

```yaml
models:
  gpt-4o:
    input_price_per_1k: 0.005     # $0.005 / 1K input tokens
    output_price_per_1k: 0.015    # $0.015 / 1K output tokens
  gpt-4o-mini:
    input_price_per_1k: 0.00015
    output_price_per_1k: 0.0006
  claude-3.5-sonnet:
    input_price_per_1k: 0.003
    output_price_per_1k: 0.015
```

### agentos_budget_controller_t — 预算控制器

管理时间窗口内的预算分配和消耗追踪。

| 参数 | 类型 | 说明 |
|------|------|------|
| `max_cost_usd` | `double` | 周期内最大成本（美元） |
| `period_seconds` | `uint32_t` | 预算周期（秒），0 表示无周期限制 |
| `consumed` | `double` | 当前周期已消耗成本 |
| `period_start` | `uint64_t` | 当前周期开始时间戳 |

## 接口说明

### 成本预估器 API

| 函数 | 说明 |
|------|------|
| `agentos_cost_estimator_create(config_path)` | 创建成本预估器（可传入 YAML 定价配置文件） |
| `agentos_cost_estimator_destroy(estimator)` | 销毁预估器，释放资源 |
| `agentos_cost_estimator_estimate(estimator, model_name, input_tokens, output_tokens)` | 根据模型和 Token 数计算成本（美元），失败返回 -1.0 |

### 预算控制器 API

| 函数 | 说明 |
|------|------|
| `agentos_budget_controller_create(max_cost_usd, period_seconds)` | 创建预算控制器，指定周期内最大成本和周期时长 |
| `agentos_budget_controller_destroy(controller)` | 销毁控制器，释放资源 |
| `agentos_budget_controller_consume(controller, cost_usd)` | 消耗指定成本，返回 0 成功，-1 超出预算 |
| `agentos_budget_controller_remaining(controller)` | 获取当前周期剩余预算（美元） |
| `agentos_budget_controller_consumed(controller)` | 获取当前周期已消耗成本（美元） |

## 使用示例

```c
#include "cost.h"

/* 创建成本预估器 */
agentos_cost_estimator_t *estimator = agentos_cost_estimator_create("models.yaml");
if (!estimator) {
    /* 失败则使用内置默认定价表 */
    estimator = agentos_cost_estimator_create(NULL);
}

/* 预估 GPT-4o 的一次调用成本 */
double cost = agentos_cost_estimator_estimate(
    estimator,
    "gpt-4o",
    500,   /* 500 input tokens */
    200    /* 200 output tokens */
);
printf("Estimated cost: $%.6f\n", cost);
/* $0.005 * 500/1000 + $0.015 * 200/1000 = $0.0025 + $0.003 = $0.0055 */

/* 创建预算控制器：每天 $10 预算 */
agentos_budget_controller_t *budget = agentos_budget_controller_create(10.0, 86400);

/* 消耗成本 */
int rc = agentos_budget_controller_consume(budget, cost);
if (rc != 0) {
    printf("Budget exceeded! Remaining: $%.4f\n",
           agentos_budget_controller_remaining(budget));
} else {
    printf("Cost consumed: $%.6f, Remaining: $%.4f\n",
           cost, agentos_budget_controller_remaining(budget));
}

/* 清理 */
agentos_budget_controller_destroy(budget);
agentos_cost_estimator_destroy(estimator);
```

## 内置定价表

当未提供 YAML 配置文件时，预估器使用以下内置默认定价（以美元 / 1K tokens 计）：

| 模型 | Input 价格 | Output 价格 |
|------|-----------|-------------|
| gpt-4o | $0.005 | $0.015 |
| gpt-4o-mini | $0.00015 | $0.0006 |
| gpt-4-turbo | $0.01 | $0.03 |
| gpt-3.5-turbo | $0.0005 | $0.0015 |
| claude-3.5-sonnet | $0.003 | $0.015 |
| claude-3-opus | $0.015 | $0.075 |
| claude-3-haiku | $0.00025 | $0.00125 |

## 预算策略

| 策略 | 说明 |
|------|------|
| **每日预算** | `period_seconds=86400`，每天重置预算 |
| **每月预算** | `period_seconds=2592000`，每月重置预算 |
| **无限预算** | `period_seconds=0` 或 `max_cost_usd=DBL_MAX` |
| **累计预算** | 不设置周期重置，仅限制总额 |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `libyaml`（可选） | YAML 定价配置文件解析 |
| `memory_compat.h` | 统一内存管理 |

---

© 2026 SPHARX Ltd. All Rights Reserved.