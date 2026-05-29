# 性能基准测试

`tests/benchmarks/`

## 概述

`benchmarks/` 目录包含 AgentOS 的性能基准测试框架，共 **14 个文件**，覆盖 C/Python 双语言基准测试、Atoms 层性能、并发压力、检索延迟和 Token 效率等多个维度。

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| tests/benchmarks/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|------------------------|---------------------|----------|
| `c/` | `atoms/`, `commons/` | C 语言性能基准测试（CMocka） |
| `python/` | `daemon/`, `commons/` | Python 性能基准测试（pytest-benchmark） |
| `atoms/` | `agentos/atoms/` | Atoms 层组件性能测试（内核/记忆/系统调用） |
| `concurrency/` | `daemon/`, `gateway/` | 并发压力测试（负载测试/报告输出） |
| `retrieval_latency/` | `atoms/memory/`, `atoms/memoryrovol/` | 记忆检索延迟测试 |
| `token_efficiency/` | `commons/utils/token/` | Token 效率与预算管理测试 |

## 目录结构

```
benchmarks/                        # 共 14 个文件
├── README.md                      # 本文档
├── c/                             # C 语言基准测试（CMocka，2 个文件）
│   ├── CMakeLists.txt             #   C 基准测试构建配置
│   └── test_performance_benchmarks.c
├── python/                        # Python 基准测试（pytest-benchmark，3 个文件）
│   ├── __init__.py
│   ├── benchmark_performance.py
│   └── regression_detector.py     #   性能回归检测器
├── atoms/                         # Atoms 层基准测试（2 个文件）
│   ├── atoms_benchmarks.c
│   └── atoms_benchmarks.h
├── concurrency/                   # 并发性能测试
│   ├── load_test.py               #   并发压力测试
│   └── report/                    #   报告输出目录
├── retrieval_latency/             # 检索延迟测试
│   └── results/                   #   结果输出目录
└── token_efficiency/              # Token 效率测试
    ├── benchmark.py
    └── plots/                     #   图表输出目录
```

## 运行方式

```bash
# C 基准测试
cd build && ctest -R performance_benchmarks

# Python 基准测试
pytest tests/benchmarks/python/ -v --benchmark-only

# 并发测试
python tests/benchmarks/concurrency/load_test.py

# Token 效率测试
python tests/benchmarks/token_efficiency/benchmark.py
```

## 性能测试维度

| 维度 | 对应的 agentos/ 模块 | 关键指标 |
|------|---------------------|----------|
| **内核性能** | `atoms/corekern/` | IPC 延迟、任务调度开销 |
| **认知性能** | `atoms/coreloopthree/` | 三环吞吐量 |
| **记忆检索** | `atoms/memory/`, `atoms/memoryrovol/` | L1-L4 延迟、命中率 |
| **并发处理** | `daemon/`, `gateway/` | QPS、响应时间、错误率 |
| **Token 管理** | `commons/utils/token/` | 预算控制精度、计数效率 |
| **存储性能** | `heapstore/` | SQLite 写入吞吐、内存后端读写延迟 |

---

© 2026 SPHARX Ltd. All Rights Reserved.