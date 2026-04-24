# Strategy — 加权评分引擎

`commons/utils/strategy/` 提供基于加权评分的智能策略选择引擎，用于在多个候选方案之间做出最优决策。

## 设计目标

- **多维度评分**：支持任意数量的评分维度，每个维度独立加权
- **候选排序**：根据综合评分对候选方案排序，自动选择最优
- **权重归一化**：自动处理权重归一化，确保评分结果一致性
- **运行时调整**：支持动态调整权重和评分函数

## 核心功能

| 功能 | 说明 |
|------|------|
| 加权评分计算 | 对候选方案在各维度上打分，加权汇总 |
| 最优选择 | 根据综合评分自动选择最优候选 |
| 权重归一化 | 自动将权重归一化到 0-1 范围 |
| 动态调整 | 运行时更新评分维度和权重配置 |
| A/B 测试 | 支持 A/B 对比模式，收集统计结果 |

## 使用场景

### 负载均衡

选择最合适的服务节点处理请求：

```python
strategy = WeightedScoringEngine()
strategy.add_dimension("cpu_usage", weight=0.3, higher_is_better=False)
strategy.add_dimension("response_time", weight=0.4, higher_is_better=False)
strategy.add_dimension("active_connections", weight=0.3, higher_is_better=False)

candidates = [
    {"id": "node-1", "cpu_usage": 45, "response_time": 120, "active_connections": 50},
    {"id": "node-2", "cpu_usage": 60, "response_time": 90, "active_connections": 30},
    {"id": "node-3", "cpu_usage": 30, "response_time": 150, "active_connections": 70},
]

best = strategy.select_best(candidates)
print(f"Selected node: {best['id']}")
```

### A/B 测试

在不同策略间进行对比测试：

```python
engine = WeightedScoringEngine()
engine.add_dimension("conversion_rate", weight=0.5, higher_is_better=True)
engine.add_dimension("user_satisfaction", weight=0.3, higher_is_better=True)
engine.add_dimension("latency", weight=0.2, higher_is_better=False)

results = engine.evaluate([
    {"name": "Strategy-A", "conversion_rate": 0.12, "user_satisfaction": 4.5, "latency": 200},
    {"name": "Strategy-B", "conversion_rate": 0.15, "user_satisfaction": 4.2, "latency": 180},
])
```

## 配置选项

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| dimensions | list | [] | 评分维度定义列表 |
| normalization | string | "sum" | 权重归一化方式（sum/max） |
| tie_breaker | string | "first" | 平局处理策略（first/random） |

---

*AgentOS Commons Utils — Strategy*
