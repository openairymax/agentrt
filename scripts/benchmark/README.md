# 性能基准测试框架

`scripts/benchmark/`

## 概述

`benchmark/` 模块提供 AgentOS 核心组件的性能基准测试框架，支持 CPU/内存/I/O 性能测试，以及自动化的统计分析和报告生成。

## 模块结构

```
benchmark/
├── benchmark_core.py              # 核心基准测试引擎
├── statistics_engine.py           # 统计分析引擎
├── history_comparator.py          # 历史版本对比器
├── report_generator.py            # 报告生成器
└── example_coreloopthree_benchmark.py  # CoreLoop3 示例测试
```

## 快速开始

### 运行核心基准测试

```bash
cd scripts/benchmark

# 运行核心测试
python benchmark_core.py

# 指定测试轮次
python benchmark_core.py --rounds 100

# 输出 JSON 格式
python benchmark_core.py --json
```

### 生成性能报告

```bash
python report_generator.py --input results.json --format markdown
```

## 测试指标

| 指标 | 说明 |
|------|------|
| **Latency** | 请求响应延迟（ms） |
| **Throughput** | 每秒处理请求数（QPS） |
| **Memory** | 内存使用量（MB） |
| **CPU** | CPU 使用率（%） |
| **P50/P90/P99** | 延迟百分位统计 |

## 扩展基准测试

创建新的基准测试文件：

```python
from benchmark_core import BenchmarkRunner

def my_test():
    runner = BenchmarkRunner("MyTest")
    runner.add_test("case1", lambda: ...)
    runner.run()
    return runner.get_results()

if __name__ == "__main__":
    my_test()
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
