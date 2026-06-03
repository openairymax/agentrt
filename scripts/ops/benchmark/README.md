# 性能基准测试框架

`scripts/ops/benchmark/`

## 概述

`benchmark/` 目录提供 AgentOS 的统计驱动性能基准测试框架，包含测试定义、执行监控、统计计算、报告生成和历史对比五大核心能力。该框架专门用于验证 `agentos/atoms/` 层核心组件的性能指标，支持分布拟合、显著性检验和回归分析等高级统计功能，可自动识别性能退化并生成多格式报告。

基准测试框架的设计原则：

- **统计严谨**：集成统计计算引擎，提供分布拟合、显著性检验和回归分析，避免基于直觉的性能判断
- **多格式报告**：支持 HTML、Markdown、PDF、JSON 和 Console 五种输出格式，满足不同使用场景
- **历史可追溯**：历史比较器支持版本对比和趋势分析，可自动检测性能回归
- **可扩展**：通过继承 `benchmark_core.py` 中的基类即可编写自定义基准测试

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| benchmark/ 组件 | 对应的 agentos/ 模块 | 用途 |
|-----------------|---------------------|------|
| `benchmark_core.py` | `atoms/` | 测试框架核心（测试定义/执行/监控/结果收集） |
| `statistics_engine.py` | `atoms/corekern/`, `atoms/coreloopthree/` | 统计计算引擎（分布拟合/显著性检验/回归分析） |
| `report_generator.py` | 全部模块 | 报告生成器（HTML/Markdown/PDF/JSON/Console） |
| `history_comparator.py` | `atoms/` | 历史比较器（版本对比/回归检测/趋势分析） |
| `example_coreloopthree_benchmark.py` | `atoms/coreloopthree/` | CoreLoopThree 三环核心运行时基准测试示例 |

## 目录结构

```
benchmark/
├── README.md                              # 本文档
├── benchmark_core.py                      # 测试框架核心（测试定义/执行/监控/结果收集）
├── statistics_engine.py                   # 统计计算引擎（分布拟合/显著性检验/回归分析）
├── report_generator.py                    # 报告生成器（HTML/Markdown/PDF/JSON/Console）
├── history_comparator.py                  # 历史比较器（版本对比/回归检测/趋势分析）
└── example_coreloopthree_benchmark.py     # CoreLoopThree 基准测试示例
```

## 核心组件说明

### benchmark_core.py — 测试框架核心

基准测试框架的核心模块，提供完整的测试生命周期管理：

- **测试定义**：通过 `BenchmarkCase` 基类定义测试用例，支持 `setup()`、`run()`、`teardown()` 三阶段生命周期
- **执行引擎**：支持迭代轮次控制（`--rounds`）、超时控制（`--timeout`）和预热轮次（`--warmup`）
- **性能监控**：自动采集执行时间、内存占用、CPU 利用率等性能指标
- **结果收集**：将每次迭代的结果汇总为结构化数据，供统计引擎和报告生成器使用

### statistics_engine.py — 统计计算引擎

提供严谨的统计分析能力，避免基于直觉的性能判断：

- **分布拟合**：支持正态分布、对数正态分布和指数分布的参数估计和拟合优度检验
- **显著性检验**：
  - Student's t 检验：比较两组样本均值是否存在显著差异
  - Mann-Whitney U 检验：非参数检验，不要求数据服从正态分布
- **回归分析**：线性回归和多项式回归，用于识别性能随时间的变化趋势
- **描述性统计**：均值、中位数、标准差、百分位数（P50/P90/P95/P99）、变异系数等

### report_generator.py — 报告生成器

将基准测试结果转化为可读的报告，支持五种输出格式：

- **HTML**：交互式网页报告，包含图表和详细数据表格，适合分享和存档
- **Markdown**：纯文本格式报告，适合嵌入文档和代码仓库
- **PDF**：正式文档格式，适合打印和归档
- **JSON**：结构化数据格式，适合程序化处理和持续集成
- **Console**：终端彩色输出，适合开发过程中快速查看

### history_comparator.py — 历史比较器

对比不同版本的基准测试结果，追踪性能变化趋势：

- **版本对比**：加载两个版本的测试结果，逐项对比性能指标变化
- **回归检测**：自动识别性能退化（如延迟增加、吞吐量下降），标记需要关注的变更
- **趋势分析**：加载多个历史版本的结果，绘制性能趋势图，识别长期性能漂移
- **阈值配置**：支持自定义回归阈值（如延迟增加超过 10% 即标记为回归）

### example_coreloopthree_benchmark.py — 基准测试示例

CoreLoopThree 三环核心运行时的基准测试示例，展示如何编写自定义基准测试：

- 继承 `BenchmarkCase` 基类
- 实现 `setup()`、`run()`、`teardown()` 方法
- 配置测试参数和断言条件
- 展示 CoreLoopThree 认知环/执行环/学习环的吞吐量测试

## 测试维度

| 维度 | 测试对象 | 指标 |
|------|---------|------|
| **内核性能** | `atoms/corekern/` | IPC 延迟、任务调度开销、内存分配效率 |
| **认知环性能** | `atoms/coreloopthree/` | 认知环/执行环/学习环吞吐量 |
| **记忆检索** | `atoms/memory/`, `atoms/memoryrovol/` | L1-L4 检索延迟、缓存命中率 |
| **系统调用** | `atoms/syscall/` | 调用延迟、保护机制开销 |

## 使用方式

### 基础性能基准测试

```bash
# 默认 100 轮迭代
python scripts/ops/benchmark/benchmark_core.py --rounds 100

# 指定预热轮次和超时
python scripts/ops/benchmark/benchmark_core.py --rounds 100 --warmup 10 --timeout 300
```

### 生成报告

```bash
# 生成 HTML 报告
python scripts/ops/benchmark/benchmark_core.py --rounds 100 --report html --output results/

# 生成 Markdown 报告
python scripts/ops/benchmark/benchmark_core.py --rounds 100 --report markdown --output results/

# 生成 JSON 格式结果
python scripts/ops/benchmark/benchmark_core.py --rounds 100 --report json --output results/

# 生成 Console 报告（终端彩色输出）
python scripts/ops/benchmark/benchmark_core.py --rounds 100 --report console

# 同时生成多种格式
python scripts/ops/benchmark/benchmark_core.py --rounds 100 --report html,json --output results/
```

### 运行示例基准测试

```bash
# CoreLoopThree 基准测试示例
python scripts/ops/benchmark/example_coreloopthree_benchmark.py
```

### 历史对比分析

```bash
# 对比两个版本的性能
python scripts/ops/benchmark/history_comparator.py --baseline results/v1.json --current results/v2.json

# 设置回归阈值（延迟增加超过 10% 标记为回归）
python scripts/ops/benchmark/history_comparator.py --baseline results/v1.json --current results/v2.json --threshold 0.10

# 趋势分析（多版本对比）
python scripts/ops/benchmark/history_comparator.py --trend results/v1.json results/v2.json results/v3.json
```

### 编写自定义基准测试

```python
from benchmark_core import BenchmarkCase, BenchmarkRunner

class MyBenchmark(BenchmarkCase):
    def setup(self):
        pass

    def run(self):
        pass

    def teardown(self):
        pass

runner = BenchmarkRunner()
runner.add_case(MyBenchmark())
runner.run(rounds=100)
```

## 依赖说明

| 依赖 | 版本要求 | 用途 |
|------|---------|------|
| Python | 3.8+ | 运行时环境 |
| numpy | 1.20+ | 数值计算和数组操作 |
| scipy | 1.7+ | 统计检验和分布拟合 |
| matplotlib | 3.4+ | 报告中的图表绘制（HTML/PDF 格式） |
| jinja2 | 3.0+ | HTML 报告模板渲染 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
