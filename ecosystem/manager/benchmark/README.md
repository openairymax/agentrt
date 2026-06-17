# Manager 性能基准测试

**模块路径**: `ecosystem/manager/benchmark/`
**版本**: v0.1.0

## 概述

`manager/benchmark/` 包含 Manager 模块的性能基准测试工具，用于评估和监控关键组件的性能指标。基准测试覆盖 TaskManager、SessionManager、MemoryManager、SkillManager 四大核心管理器，提供吞吐量、响应时间、OPS 等多维度性能数据，并自动生成性能评级报告。

## 目录结构

```
benchmark/
├── benchmark_manager.py    # 主基准测试脚本
└── README.md               # 本文件
```

## 测试场景

| 场景 | 测试项 | 指标 |
|------|--------|------|
| **TaskManager** | submit / query / list | 吞吐量（ops/s）、平均响应时间（ms） |
| **SessionManager** | create / get / close | TPS、延迟分布 |
| **MemoryManager** | write / search / list | OPS、读写延迟 |
| **SkillManager** | load / list / get | TPS、加载延迟 |

每个测试场景均包含 **预热阶段**（默认 10 次）和 **正式测量阶段**，确保 JIT 优化和缓存预热不影响结果。

## 测试结果结构

```python
@dataclass
class BenchmarkResult:
    name: str              # 测试名称
    iterations: int        # 迭代次数
    total_time_ms: float   # 总耗时（毫秒）
    avg_time_ms: float     # 平均耗时（毫秒）
    min_time_ms: float     # 最小耗时（毫秒）
    max_time_ms: float     # 最大耗时（毫秒）
    ops_per_second: float  # 每秒操作数
    std_dev_ms: float      # 标准差（毫秒）
```

## 使用方式

```bash
# 运行所有基准测试（默认 100 次迭代）
python benchmark_manager.py

# 详细模式
python benchmark_manager.py --verbose

# 指定迭代次数
python benchmark_manager.py --iterations 10000

# 输出 JSON 格式结果
python benchmark_manager.py --output report.json

# 跳过实际模块测试（仅测试框架）
python benchmark_manager.py --skip-modules
```

### 命令行参数

| 参数 | 缩写 | 默认值 | 说明 |
|------|------|--------|------|
| `--verbose` | `-v` | false | 显示详细日志 |
| `--iterations` | `-i` | 100 | 每个测试的迭代次数 |
| `--output` | `-o` | None | 输出 JSON 结果文件路径 |
| `--skip-modules` | - | false | 跳过实际模块测试（仅测试框架） |

## 性能评级

| 评级 | 条件 | 含义 |
|------|------|------|
| 🟢 优秀 | 平均耗时 < 1ms | 性能优异，无需优化 |
| 🟡 良好 | 平均耗时 < 10ms | 性能良好，可接受 |
| 🟠 一般 | 平均耗时 < 100ms | 性能一般，建议关注 |
| 🔴 需优化 | 平均耗时 ≥ 100ms | 性能瓶颈，需要优化 |

## 输出示例

```
======================================================================
AgentRT Manager 模块性能基准测试报告
======================================================================
测试时间: 2026-05-29T10:00:00
总耗时: 5.23秒
测试项目: 12
----------------------------------------------------------------------
测试名称                          平均耗时        吞吐量           标准差
----------------------------------------------------------------------
TaskManager.submit              0.1234ms       8103.24 ops/s    0.0567ms
TaskManager.query               0.0891ms      11223.34 ops/s    0.0345ms
SessionManager.create           0.2345ms        4264.39 ops/s    0.0789ms
MemoryManager.write             0.1567ms        6381.62 ops/s    0.0456ms
----------------------------------------------------------------------

性能评级:
  TaskManager.submit: 🟢 优秀 (平均 0.1234ms)
  TaskManager.query: 🟢 优秀 (平均 0.0891ms)
  SessionManager.create: 🟢 优秀 (平均 0.2345ms)
======================================================================
```

## 自定义测试

```python
from benchmark_manager import PerformanceBenchmark

benchmark = PerformanceBenchmark(verbose=True)

result = benchmark.measure(
    "CustomOperation",
    lambda: my_custom_function(),
    iterations=5000
)

print(f"Average: {result.avg_time_ms:.4f}ms, OPS: {result.ops_per_second:.2f}")

benchmark.print_summary()
benchmark.save_results("custom_report.json")
```

## 依赖关系

| 组件 | 用途 |
|------|------|
| Python ≥ 3.10 | 运行环境 |
| agentos.modules.* | 被测模块（可选，缺失时使用 Mock API） |

---

© 2026 SPHARX Ltd. All Rights Reserved.
