# 性能基准测试框架

`scripts/ops/benchmark/`

## 概述

`benchmark/` 目录提供 AgentOS 的性能基准测试框架，共 **5 个文件**，包含测试定义、执行监控、统计计算、报告生成和历史对比功能，用于验证 `agentos/atoms/` 层核心组件的性能指标。

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| benchmark/ 组件 | 对应的 agentos/ 模块 | 用途 |
|-----------------|---------------------|------|
| `benchmark_core.py` | `atoms/` | 测试框架核心（测试定义/执行/监控/结果收集） |
| `statistics_engine.py` | `atoms/corekern/`, `atoms/coreloopthree/` | 统计计算引擎（分布拟合/显著性检验/回归分析） |
| `report_generator.py` | 全部模块 | 报告生成器（HTML/Markdown/PDF/JSON/Console） |
| `history_comparator.py` | `atoms/` | 历史比较器（版本对比/回归检测/趋势分析） |
| `example_coreloopthree_benchmark.py` | `atoms/coreloopthree/` | CoreLoopThree 三环核心运行时基准测试示例 |

## 文件清单

| 文件 | 说明 | 主要功能 |
|------|------|----------|
| `benchmark_core.py` | 测试框架核心 | 测试定义、执行引擎、性能监控、结果收集 |
| `statistics_engine.py` | 统计计算引擎 | 分布拟合、显著性检验、回归分析 |
| `report_generator.py` | 报告生成器 | 支持 HTML/Markdown/PDF/JSON/Console 输出 |
| `history_comparator.py` | 历史比较器 | 版本对比、回归检测、趋势分析 |
| `example_coreloopthree_benchmark.py` | 基准测试示例 | CoreLoopThree 双思考系统性能测试用例 |

## 运行方式

```bash
# 基础性能基准测试
python scripts/ops/benchmark/benchmark_core.py --rounds 100

# CoreLoopThree 基准测试示例
python scripts/ops/benchmark/example_coreloopthree_benchmark.py

# 生成 HTML 报告
python scripts/ops/benchmark/benchmark_core.py --rounds 100 --report html --output results/

# 历史对比分析
python scripts/ops/benchmark/history_comparator.py --baseline results/v1.json --current results/v2.json
```

## 测试维度

| 维度 | 测试对象 | 指标 |
|------|---------|------|
| **内核性能** | `atoms/corekern/` | IPC 延迟、任务调度开销、内存分配效率 |
| **认知环性能** | `atoms/coreloopthree/` | 认知环/执行环/学习环吞吐量 |
| **记忆检索** | `atoms/memory/`, `atoms/memoryrovol/` | L1-L4 检索延迟、缓存命中率 |
| **系统调用** | `atoms/syscall/` | 调用延迟、保护机制开销 |

---

© 2026 SPHARX Ltd. All Rights Reserved.