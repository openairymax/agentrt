# Strategy — 加权评分引擎

**模块路径**: `agentos/commons/utils/strategy/`
**版本**: v0.1.0

## 概述

Strategy 模块提供基于加权评分的智能策略选择引擎，用于在多个候选方案之间做出最优决策。该模块是 AgentRT 调度决策体系的基础，支持多维度评分、权重归一化和运行时动态调整，被 Cognition 模块用于 Agent 选择，也可独立用于负载均衡、A/B 测试等场景。

## 设计目标

- **多维度评分**：支持任意数量的评分维度，每个维度独立加权
- **候选排序**：根据综合评分对候选方案排序，自动选择最优
- **权重归一化**：自动处理权重归一化，确保评分结果一致性
- **运行时调整**：支持动态调整权重和评分函数
- **零成本抽象**：基于宏和内联函数，编译期优化

## 目录结构

```
strategy/
├── include/
│   └── strategy_common.h        # 策略引擎公共接口定义
├── src/
│   └── strategy_common.c        # 策略引擎实现
└── README.md                    # 本文档
```

## 核心数据结构

### weighted_config_t — 加权配置

| 字段 | 类型 | 说明 |
|------|------|------|
| `cost_weight` | `float` | 成本权重 |
| `perf_weight` | `float` | 性能（成功率）权重 |
| `trust_weight` | `float` | 信任度权重 |

### strategy_agent_info_t — 候选代理信息

| 字段 | 类型 | 说明 |
|------|------|------|
| `cost_estimate` | `float` | 成本估计 |
| `success_rate` | `float` | 成功率 |
| `trust_score` | `float` | 信任度评分 |
| `name` | `const char *` | 代理名称 |
| `user_data` | `void *` | 用户自定义数据 |

### strategy_result_t — 策略结果

| 字段 | 类型 | 说明 |
|------|------|------|
| `selected_index` | `int` | 选中的候选索引 |
| `best_score` | `float` | 最佳评分 |
| `success` | `bool` | 选择是否成功 |

## 接口说明

| 函数 | 说明 |
|------|------|
| `strategy_compute_weighted_score(agent, config)` | 计算单个候选的加权评分 |
| `strategy_select_best_agent(agents, count, config, result)` | 从候选数组中选择最优 |
| `strategy_create_default_weighted_config()` | 创建默认加权配置 |
| `strategy_validate_weighted_config(config)` | 验证加权配置有效性 |
| `strategy_normalize_weights(config)` | 归一化权重（确保总和为 1.0） |
| `strategy_cleanup_data(data, free_func)` | 通用数据清理函数 |
| `strategy_generate_name(base_name, suffix)` | 生成策略名称（需 `AGENTOS_FREE` 释放） |

## 评分算法

加权评分公式：

```
score = cost_weight × (1 - cost_estimate) + perf_weight × success_rate + trust_weight × trust_score
```

- 成本维度取反（成本越低越好）
- 权重自动归一化：`w_i' = w_i / Σ(w_j)`
- 评分越高越优

## 使用示例

### C API — Agent 选择

```c
#include "strategy_common.h"

strategy_agent_info_t agents[] = {
    { .cost_estimate = 0.3, .success_rate = 0.95, .trust_score = 0.8, .name = "node-1" },
    { .cost_estimate = 0.5, .success_rate = 0.90, .trust_score = 0.9, .name = "node-2" },
    { .cost_estimate = 0.2, .success_rate = 0.85, .trust_score = 0.7, .name = "node-3" },
};

weighted_config_t config = strategy_create_default_weighted_config();
config.cost_weight  = 0.3f;
config.perf_weight  = 0.4f;
config.trust_weight = 0.3f;

if (!strategy_validate_weighted_config(&config)) {
    config = strategy_normalize_weights(&config);
}

strategy_result_t result;
strategy_select_best_agent(agents, 3, &config, &result);
if (result.success) {
    printf("Selected: %s (score: %.3f)\n",
           agents[result.selected_index].name, result.best_score);
}
```

### 负载均衡场景

```c
strategy_agent_info_t nodes[] = {
    { .cost_estimate = 0.45, .success_rate = 0.12, .trust_score = 0.50, .name = "node-1" },
    { .cost_estimate = 0.60, .success_rate = 0.09, .trust_score = 0.30, .name = "node-2" },
    { .cost_estimate = 0.30, .success_rate = 0.15, .trust_score = 0.70, .name = "node-3" },
};

weighted_config_t lb_config = {
    .cost_weight = 0.3f,
    .perf_weight = 0.4f,
    .trust_weight = 0.3f
};

strategy_result_t lb_result;
strategy_select_best_agent(nodes, 3, &lb_config, &lb_result);
```

## 配置选项

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| cost_weight | float | 0.33 | 成本维度权重 |
| perf_weight | float | 0.34 | 性能维度权重 |
| trust_weight | float | 0.33 | 信任度维度权重 |
| normalization | string | "sum" | 权重归一化方式（sum/max） |
| tie_breaker | string | "first" | 平局处理策略（first/random） |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `memory_compat.h` | 统一内存管理宏（`AGENTOS_MALLOC`/`AGENTOS_FREE`） |
| `string_compat.h` | 字符串操作兼容层 |
| `<string.h>` | 字符串操作 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
