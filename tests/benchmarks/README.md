# 性能基准测试

`tests/benchmarks/`

## 概述

`benchmarks/` 目录包含 AgentRT 的性能基准测试框架，共 **16 个文件**，覆盖 C/Python 双语言基准测试、Atoms 层性能、Cupolas 安全基准、并发压力、检索延迟和 Token 效率等多个维度。

性能基准测试的核心目标：
- **性能回归检测**：通过 `regression_detector.py` 自动检测性能退化，确保代码变更不会引入性能问题
- **关键指标监控**：追踪 IPC 延迟、任务调度开销、记忆检索命中率、并发 QPS 等核心指标
- **压力测试**：通过 `load_test.py` 模拟高并发场景，验证系统在负载下的稳定性
- **可视化报告**：支持生成性能图表（`plots/`）和测试报告（`report/`）

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| tests/benchmarks/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|------------------------|---------------------|----------|
| `c/` | `atoms/`, `commons/` | C 语言性能基准测试（内核/公共库性能指标） |
| `python/` | `daemons/`, `commons/` | Python 性能基准测试（守护进程/公共库性能指标） |
| `atoms/` | `agentrt/atoms/` | Atoms 层组件性能测试（内核 IPC/记忆/系统调用延迟） |
| `concurrency/` | `daemons/`, `gateway/` | 并发压力测试（负载测试/报告输出） |
| `cupolas/` | `agentrt/cupolas/` | Cupolas 安全基准与压力测试（权限检查/清洗吞吐/并发安全） |
| `retrieval_latency/` | `atoms/memory/`, `atoms/memoryrovol/` | 记忆检索延迟测试（L1-L4 层延迟/命中率） |
| `token_efficiency/` | `commons/utils/token/` | Token 效率与预算管理测试（预算控制精度/计数效率） |

## 目录结构

```
benchmarks/                        # 共 16 个文件
├── README.md                      # 本文档
├── c/                             # C 语言基准测试（2 个文件）
│   ├── CMakeLists.txt             #   C 基准测试构建配置
│   └── test_performance_benchmarks.c # C 性能基准测试套件
│                                  #     内核 IPC 延迟、内存分配开销、任务调度延迟
├── python/                        # Python 基准测试（3 个文件）
│   ├── __init__.py                #   Python 包初始化
│   ├── benchmark_performance.py   #   Python 性能基准测试
│   │                              #     守护进程调用延迟、配置加载时间、数据序列化吞吐
│   └── regression_detector.py     #   性能回归检测器
│                                  #     对比历史基线、自动标记退化、生成报告
├── atoms/                         # Atoms 层基准测试（2 个文件）
│   ├── atoms_benchmarks.c         #   Atoms 性能基准测试
│   │                              #     内核 IPC 延迟、记忆读写延迟、系统调用开销
│   └── atoms_benchmarks.h         #   Atoms 基准测试头文件
│                                  #     基准测试辅助宏与数据结构定义
├── concurrency/                   # 并发性能测试（2 个文件）
│   ├── load_test.py               #   并发压力测试
│   │                              #     模拟多用户并发请求、测量 QPS/响应时间/错误率
│   └── report/                    #   报告输出目录
│       └── .keep                  #     目录占位
├── cupolas/                       # Cupolas 安全基准与压力测试（2 个文件）
│   ├── benchmark_cupolas.c        #   安全穹顶基准测试
│   │                              #     权限检查延迟、清洗吞吐量、审计写入性能
│   └── test_stress_concurrent.c   #   并发压力测试
│                                  #     多线程并发安全验证、竞态条件检测
├── retrieval_latency/             # 检索延迟测试
│   └── results/                   #   结果输出目录
│       └── .keep                  #     目录占位
└── token_efficiency/              # Token 效率测试（2 个文件）
    ├── benchmark.py               #   Token 效率基准测试
    │                              #     预算控制精度、计数效率、Token 分配策略
    └── plots/                     #   图表输出目录
        └── .keep                  #     目录占位
```

## 测试框架说明

### C 语言基准测试

C 语言基准测试使用自定义计时宏实现，通过 `atoms_benchmarks.h` 定义的时间测量工具进行纳秒级精度计时。测试通过 CMake 构建并使用 `ctest` 运行。关键指标包括：

- **IPC 延迟**：Binder 通信的往返时间
- **内存分配开销**：`malloc`/`free` 的平均耗时
- **任务调度延迟**：从提交到执行的等待时间
- **系统调用开销**：从用户态到内核态的切换成本

### Python 基准测试（pytest-benchmark）

Python 基准准测试使用 pytest-benchmark 插件，支持以下特性：

- **统计比较**：自动计算均值、中位数、标准差等统计指标
- **回归检测**：`regression_detector.py` 对比历史基线数据，自动标记性能退化
- **结果持久化**：支持将基准结果保存为 JSON 格式，便于跨版本对比
- **可视化**：`token_efficiency/plots/` 目录输出性能图表

### 并发压力测试

并发压力测试使用 Python 的 `asyncio` 或 `threading` 模块模拟高并发场景，测量以下指标：

- **QPS**（每秒查询数）：系统在给定并发度下的吞吐量
- **响应时间**：P50/P95/P99 分位延迟
- **错误率**：高负载下的请求失败比例

## 运行方式

```bash
# C 基准测试
cd build && ctest -R performance_benchmarks -V
cd build && ctest -R atoms_benchmarks -V

# Python 基准测试
pytest tests/benchmarks/python/ -v --benchmark-only
pytest tests/benchmarks/python/benchmark_performance.py -v --benchmark-only

# 性能回归检测
python tests/benchmarks/python/regression_detector.py

# 并发压力测试
python tests/benchmarks/concurrency/load_test.py

# Cupolas 安全基准测试
cd build && ctest -R cupolas_benchmark -V
cd build && ctest -R stress_concurrent -V

# Token 效率测试
python tests/benchmarks/token_efficiency/benchmark.py

# 使用统一入口
python tests/utils/python/run_tests.py --type benchmark

# 生成基准报告
pytest tests/benchmarks/ --benchmark-only --benchmark-json=benchmark_results.json
```

## 性能测试维度

| 维度 | 对应的 agentos/ 模块 | 关键指标 | 测试文件 |
|------|---------------------|----------|---------|
| **内核性能** | `atoms/corekern/` | IPC 延迟、任务调度开销、内存分配耗时 | `atoms/atoms_benchmarks.c`, `c/test_performance_benchmarks.c` |
| **认知性能** | `atoms/coreloopthree/` | 三环吞吐量、认知-执行联动延迟 | `atoms/atoms_benchmarks.c` |
| **记忆检索** | `atoms/memory/`, `atoms/memoryrovol/` | L1-L4 延迟、命中率、缓存效率 | `retrieval_latency/` |
| **并发处理** | `daemons/`, `gateway/` | QPS、响应时间 P50/P95/P99、错误率 | `concurrency/load_test.py` |
| **安全性能** | `cupolas/` | 权限检查延迟、清洗吞吐量、审计写入性能 | `cupolas/benchmark_cupolas.c` |
| **并发安全** | `cupolas/` | 多线程竞态检测、锁争用分析 | `cupolas/test_stress_concurrent.c` |
| **Token 管理** | `commons/utils/token/` | 预算控制精度、计数效率、分配策略 | `token_efficiency/benchmark.py` |
| **存储性能** | `heapstore/` | SQLite 写入吞吐、内存后端读写延迟 | `c/test_performance_benchmarks.c` |
| **Python 性能** | `daemons/`, `commons/` | 服务调用延迟、配置加载时间、序列化吞吐 | `python/benchmark_performance.py` |

## 测试覆盖说明

| agentos/ 模块 | 基准测试文件 | 测试框架 | 覆盖范围 |
|--------------|------------|---------|---------|
| `atoms/corekern/` | `atoms/atoms_benchmarks.c`, `c/test_performance_benchmarks.c` | C (CMocka) | IPC 延迟、内存分配、任务调度 |
| `atoms/coreloopthree/` | `atoms/atoms_benchmarks.c` | C | 三环吞吐量、协调延迟 |
| `atoms/memory/` | `retrieval_latency/` | Python | L1-L4 层检索延迟 |
| `atoms/memoryrovol/` | `retrieval_latency/` | Python | 语义检索延迟、缓存命中率 |
| `cupolas/` | `cupolas/benchmark_cupolas.c`, `cupolas/test_stress_concurrent.c` | C | 权限/清洗/审计性能、并发安全 |
| `daemons/` | `python/benchmark_performance.py`, `concurrency/load_test.py` | pytest-benchmark | 服务调用延迟、并发 QPS |
| `gateway/` | `concurrency/load_test.py` | Python | 网关吞吐量、协议转换延迟 |
| `commons/` | `c/test_performance_benchmarks.c`, `python/benchmark_performance.py` | C + pytest-benchmark | 公共库性能指标 |
| `heapstore/` | `c/test_performance_benchmarks.c` | C | 存储读写延迟 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
