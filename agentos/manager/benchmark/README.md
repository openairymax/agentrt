# Manager 性能基准测试

`manager/benchmark/` 包含 Manager 模块的性能基准测试工具，用于评估和监控关键组件的性能指标。

## 测试脚本

| 脚本 | 说明 |
|------|------|
| `benchmark_manager.py` | 主基准测试脚本 |

## 测试场景

| 场景 | 说明 | 指标 |
|------|------|------|
| **TaskManager** | 任务管理器的创建、调度和完成性能 | 吞吐量、响应时间 |
| **SessionManager** | 会话管理的创建、查询和销毁性能 | TPS、延迟 |
| **MemoryManager** | 内存管理的读写和搜索性能 | OPS、延迟 |
| **SkillManager** | 技能管理的加载和执行性能 | TPS、延迟 |

## 使用方式

```bash
# 运行所有基准测试
python manager/benchmark/benchmark_manager.py

# 运行指定场景
python manager/benchmark/benchmark_manager.py --scenario TaskManager

# 指定迭代次数
python manager/benchmark/benchmark_manager.py --iterations 10000

# 输出 JSON 格式结果
python manager/benchmark/benchmark_manager.py --format json
```

## 性能评级

| 评级 | 条件 | 颜色 |
|------|------|------|
| Green | 所有指标在期望范围内 | 🟢 |
| Yellow | 部分指标接近阈值 | 🟡 |
| Red | 指标超出阈值 | 🔴 |

## 自定义测试

```python
# benchmark_custom.py
from benchmark_manager import BenchmarkRunner

runner = BenchmarkRunner()
results = runner.run("CustomTest", iterations=5000)

for metric, value in results.items():
    print(f"{metric}: {value}")
```

---

*AgentOS Manager — Benchmark*
