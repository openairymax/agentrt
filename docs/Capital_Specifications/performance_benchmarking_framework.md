# AgentOS 性能基准测试框架设计 (Phase 3)

<div align="center">

**AgentOS 统一性能基准测试框架设计文档**  
*第三阶段"系统完善"核心设计 - 性能基准建立*

[![Phase](https://img.shields.io/badge/Phase-3%3A系统完善-blue.svg)](https://gitee.com/spharx/agentos)  
[![Status](https://img.shields.io/badge/Status-设计完成-green.svg)](https://gitee.com/spharx/agentos)  
[![License](https://img.shields.io/badge/license-Apache--2.0-green.svg)](https://gitee.com/spharx/agentos/blob/main/LICENSE)

*"度量改进" - 为 AgentOS 核心模块提供统一、可重复、可比较的性能基准测试框架*

</div>

---

## 📋 文档导航

- [设计概述](#-设计概述) - 设计目标与原则
- [现有基准测试分析](#-现有基准测试分析) - 当前实现评估
- [架构设计方案](#-架构设计方案) - 统一框架设计
- [核心接口定义](#-核心接口定义) - API 接口规范
- [实施路线图](#-实施路线图) - 分阶段实现策略
- [性能指标定义](#-性能指标定义) - 标准性能指标集
- [附录](#-附录) - 相关文档与参考资料

---

## 🎯 设计概述

### 1.1 设计背景

AgentOS 当前有多个独立的性能基准测试实现：
- `coreloopthree/tests/benchmark.c` - 核心循环性能测试
- `cupolas/tests/benchmark/benchmark_cupolas.c` - 协程池性能测试
- `heapstore/tests/benchmark_heapstore.c` - 堆存储性能测试

这些实现存在以下问题：
1. **测试方法不统一**：计时方法、统计计算、输出格式各不相同
2. **缺乏标准化指标**：各模块定义自己的性能指标，难以横向比较
3. **重复代码**：每个基准测试都重新实现计时、统计、报告功能
4. **缺乏自动化**：需要手动运行和结果收集
5. **历史数据缺失**：没有性能趋势跟踪和回归检测

### 1.2 设计目标

| 目标 | 描述 | 对应原则 |
|------|------|----------|
| **统一测试框架** | 标准化的基准测试开发接口和运行环境 | K-2 接口契约化 |
| **可比较结果** | 跨模块、跨版本的性能结果可比性 | E-2 可观测性 |
| **自动化测试** | CI/CD 集成的自动化性能测试流水线 | E-7 配置管理 |
| **历史跟踪** | 性能趋势分析和回归检测 | E-6 错误可追溯 |
| **真实场景模拟** | 模拟真实负载和压力测试 | K-1 真实场景原则 |

### 1.3 设计原则

1. **统一性**：所有基准测试使用相同的 API 和工具
2. **可重复性**：测试结果在不同环境和时间可重复
3. **可比较性**：支持跨版本、跨配置的性能比较
4. **低开销**：测试框架自身开销最小化（< 1%）
5. **易用性**：简单直观的 API，易于添加新基准测试
6. **可扩展性**：支持自定义指标、报告格式和测试场景

---

## 📊 现有基准测试分析

### 2.1 coreloopthree/benchmark.c

**优点**：
- 简单直接，易于理解
- 使用标准 C 库时钟函数
- 基本的操作速率计算（ops/sec）

**局限性**：
- 仅计算平均时间，缺乏统计分布
- 没有预热机制
- 输出格式简单，难以自动化解析
- 缺乏错误处理和资源清理

### 2.2 cupolas/benchmark_cupolas.c

**优点**：
- 完善的统计计算（平均值、最小值、最大值、p50、p99）
- 使用高精度单调时钟（`clock_gettime`）
- 预热机制减少冷启动影响
- 结构化的结果输出

**局限性**：
- 实现复杂，代码重复
- 与特定模块耦合
- 缺乏统一的报告格式
- 没有历史数据比较

### 2.3 heapstore/benchmark_heapstore.c

**待分析**：需要查看具体实现

### 2.4 总结

现有实现展示了两种主要模式：
1. **简单模式**：平均时间计算，适合快速验证
2. **统计模式**：完整统计分布，适合生产性能分析

需要统一的框架同时支持这两种模式。

---

## 🏗️ 架构设计方案

### 3.1 整体架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                  统一性能基准测试框架 (Unified Benchmark Framework)      │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    基准测试运行器 (Benchmark Runner)             │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐               │   │
│  │  │ 环境准备     │ │ 预热执行     │ │ 主测试执行   │               │   │
│  │  └─────────────┘ └─────────────┘ └─────────────┘               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    统计计算引擎 (Statistics Engine)              │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐               │   │
│  │  │ 时间测量     │ │ 分布计算     │ │ 百分位计算   │               │   │
│  │  └─────────────┘ └─────────────┘ └─────────────┘               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    结果报告器 (Result Reporter)                  │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐               │   │
│  │  │ 文本报告     │ │ JSON 报告    │ │ CSV 报告     │               │   │
│  │  └─────────────┘ └─────────────┘ └─────────────┘               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    历史比较器 (History Comparator)               │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐               │   │
│  │  │ 数据存储     │ │ 趋势分析     │ │ 回归检测     │               │   │
│  │  └─────────────┘ └─────────────┘ └─────────────┘               │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 核心组件设计

#### 3.2.1 基准测试运行器

**功能**：
- 管理测试生命周期（准备、预热、执行、清理）
- 控制并发和迭代次数
- 处理错误和超时
- 资源管理和清理

**设计要点**：
- 支持同步和异步测试模式
- 可配置的预热迭代次数
- 自适应迭代次数（基于统计稳定性）

#### 3.2.2 统计计算引擎

**功能**：
- 高精度时间测量（纳秒级）
- 统计分布计算（平均值、标准差、百分位数）
- 内存和CPU使用率测量
- 结果验证和异常检测

**设计要点**：
- 使用平台最优的计时器（`clock_gettime`, `QueryPerformanceCounter`）
- 支持多种统计方法（简单平均、指数移动平均）
- 内存安全的统计计算

#### 3.2.3 结果报告器

**功能**：
- 多格式输出（文本、JSON、CSV、HTML）
- 可定制的报告模板
- 结果比较和可视化
- 自动化结果上传

**设计要点**：
- 可插拔的报告格式
- 支持 CI/CD 集成
- 人类可读和机器可读格式

#### 3.2.4 历史比较器

**功能**：
- 性能结果存储和版本管理
- 趋势分析和预测
- 回归检测和告警
- 基线管理和比较

**设计要点**：
- 轻量级数据库存储（SQLite）
- 可配置的回归阈值
- 自动基线更新策略

### 3.3 测试类型支持

#### 3.3.1 微基准测试（Micro-benchmarks）
- **目标**：测量单一操作的性能
- **用例**：函数调用、内存分配、锁操作
- **特点**：高频率迭代，短时间运行

#### 3.3.2 宏基准测试（Macro-benchmarks）
- **目标**：测量完整工作流的性能
- **用例**：任务处理、请求响应、数据处理
- **特点**：真实场景模拟，长时间运行

#### 3.3.3 压力测试（Stress Tests）
- **目标**：测量系统在极限负载下的表现
- **用例**：高并发、大数据量、资源竞争
- **特点**：资源耗尽测试，稳定性验证

#### 3.3.4 回归测试（Regression Tests）
- **目标**：检测性能退步
- **用例**：版本比较、配置变更、代码修改
- **特点**：历史数据比较，阈值告警

---

## 🔌 核心接口定义

### 4.1 基准测试定义接口

```c
/**
 * @brief 基准测试配置
 */
typedef struct {
    const char* name;           /* 测试名称 */
    const char* description;    /* 测试描述 */
    uint32_t warmup_iterations; /* 预热迭代次数 */
    uint32_t min_iterations;    /* 最小迭代次数 */
    uint32_t max_iterations;    /* 最大迭代次数 */
    uint64_t max_duration_ms;   /* 最大运行时间（毫秒） */
    uint32_t threads;           /* 并发线程数 */
    bool verbose;               /* 详细输出 */
} benchmark_config_t;

/**
 * @brief 基准测试结果
 */
typedef struct {
    char name[128];             /* 测试名称 */
    uint64_t total_iterations;  /* 总迭代次数 */
    uint64_t total_time_ns;     /* 总时间（纳秒） */
    double avg_time_ns;         /* 平均时间（纳秒） */
    double min_time_ns;         /* 最小时间（纳秒） */
    double max_time_ns;         /* 最大时间（纳秒） */
    double stddev_ns;           /* 标准差（纳秒） */
    double p50_time_ns;         /* 50% 分位时间（纳秒） */
    double p90_time_ns;         /* 90% 分位时间（纳秒） */
    double p99_time_ns;         /* 99% 分位时间（纳秒） */
    double throughput;          /* 吞吐量（操作/秒） */
    uint64_t memory_usage_kb;   /* 内存使用（KB） */
    uint32_t error_count;       /* 错误次数 */
} benchmark_result_t;

/**
 * @brief 基准测试函数类型
 */
typedef agentos_error_t (*benchmark_fn_t)(
    void* context,              /* 测试上下文 */
    uint64_t iteration,         /* 当前迭代序号 */
    benchmark_result_t* result  /* 结果输出 */
);

/**
 * @brief 注册基准测试
 */
AGENTOS_API agentos_error_t agentos_benchmark_register(
    const char* suite_name,     /* 测试套名称 */
    const char* test_name,      /* 测试名称 */
    benchmark_fn_t test_fn,     /* 测试函数 */
    void* context,              /* 测试上下文 */
    const benchmark_config_t* config /* 测试配置 */
);

/**
 * @brief 运行基准测试套
 */
AGENTOS_API agentos_error_t agentos_benchmark_run_suite(
    const char* suite_name,     /* 测试套名称 */
    benchmark_result_t** results, /* 结果数组 */
    size_t* result_count        /* 结果数量 */
);
```

### 4.2 统计计算接口

```c
/**
 * @brief 时间测量句柄
 */
typedef struct agentos_timer* agentos_timer_t;

/**
 * @brief 创建高精度计时器
 */
AGENTOS_API agentos_error_t agentos_timer_create(agentos_timer_t* timer);

/**
 * @brief 开始计时
 */
AGENTOS_API agentos_error_t agentos_timer_start(agentos_timer_t timer);

/**
 * @brief 停止计时并返回经过时间（纳秒）
 */
AGENTOS_API agentos_error_t agentos_timer_stop(
    agentos_timer_t timer,
    uint64_t* elapsed_ns
);

/**
 * @brief 统计计算器
 */
typedef struct agentos_stats* agentos_stats_t;

/**
 * @brief 创建统计计算器
 */
AGENTOS_API agentos_error_t agentos_stats_create(agentos_stats_t* stats);

/**
 * @brief 添加样本值
 */
AGENTOS_API agentos_error_t agentos_stats_add_sample(
    agentos_stats_t stats,
    uint64_t value_ns
);

/**
 * @brief 计算统计结果
 */
AGENTOS_API agentos_error_t agentos_stats_compute(
    agentos_stats_t stats,
    benchmark_result_t* result
);
```

### 4.3 报告生成接口

```c
/**
 * @brief 报告格式枚举
 */
typedef enum {
    BENCHMARK_FORMAT_TEXT = 0,  /* 文本格式 */
    BENCHMARK_FORMAT_JSON,      /* JSON 格式 */
    BENCHMARK_FORMAT_CSV,       /* CSV 格式 */
    BENCHMARK_FORMAT_HTML       /* HTML 格式 */
} benchmark_format_t;

/**
 * @brief 生成测试报告
 */
AGENTOS_API agentos_error_t agentos_benchmark_generate_report(
    const benchmark_result_t* results,
    size_t result_count,
    benchmark_format_t format,
    const char* output_path
);

/**
 * @brief 比较测试结果
 */
AGENTOS_API agentos_error_t agentos_benchmark_compare(
    const benchmark_result_t* baseline,
    const benchmark_result_t* current,
    double* improvement_pct,    /* 改进百分比（正数表示改进） */
    bool* regression           /* 是否回归 */
);
```

### 4.4 历史管理接口

```c
/**
 * @brief 存储测试结果到历史数据库
 */
AGENTOS_API agentos_error_t agentos_benchmark_store_result(
    const char* test_name,
    const benchmark_result_t* result,
    const char* version,       /* 版本标识 */
    const char* git_commit,    /* Git 提交哈希 */
    const char* environment    /* 环境标识 */
);

/**
 * @brief 查询历史测试结果
 */
AGENTOS_API agentos_error_t agentos_benchmark_query_history(
    const char* test_name,
    const char* version_range,
    time_t start_time,
    time_t end_time,
    benchmark_result_t** results,
    size_t* result_count
);

/**
 * @brief 检测性能回归
 */
AGENTOS_API agentos_error_t agentos_benchmark_detect_regression(
    const char* test_name,
    double threshold_pct,      /* 回归阈值（百分比） */
    bool* has_regression,
    char** regression_report
);
```

---

## 🗓️ 实施路线图

### 5.1 Phase 3.2.1：基础框架实现（1周）

**目标**：实现核心计时器和统计计算功能

**任务**：
1. **高精度计时器**：跨平台计时器抽象（Linux/macOS/Windows）
2. **统计计算库**：平均值、标准差、百分位数计算
3. **内存测量工具**：进程内存使用测量
4. **基础测试运行器**：简单的测试生命周期管理

**交付物**：
- `commons/utils/benchmark/include/benchmark.h` 头文件
- `commons/utils/benchmark/src/benchmark.c` 基础实现
- 单元测试和验证程序

### 5.2 Phase 3.2.2：测试框架完善（1周）

**目标**：完善测试注册、运行和报告功能

**任务**：
1. **测试注册表**：动态测试注册和发现
2. **配置管理**：灵活的测试配置系统
3. **多格式报告**：文本、JSON、CSV 报告生成
4. **错误处理**：健壮的错误处理和资源清理

**交付物**：
- 完整的测试框架 API 实现
- 多格式报告生成器
- 配置管理系统

### 5.3 Phase 3.2.3：历史跟踪和比较（1周）

**目标**：实现性能历史跟踪和比较功能

**任务**：
1. **结果存储**：SQLite 数据库存储
2. **历史查询**：时间范围和版本查询
3. **回归检测**：自动回归检测和告警
4. **趋势分析**：性能趋势可视化和预测

**交付物**：
- 历史数据库管理模块
- 回归检测算法
- 趋势分析工具

### 5.4 Phase 3.2.4：集成和优化（1周）

**目标**：集成到现有模块和优化性能

**任务**：
1. **模块集成**：为 coreloopthree、cupolas、heapstore 等模块创建基准测试
2. **CI/CD 集成**：集成到 GitHub Actions 或 Jenkins
3. **性能优化**：框架自身性能优化
4. **文档完善**：用户指南和 API 文档

**交付物**：
- 各模块的基准测试示例
- CI/CD 集成脚本
- 完整的文档体系

---

## 📈 性能指标定义

### 6.1 核心性能指标

| 指标 | 单位 | 描述 | 适用场景 |
|------|------|------|----------|
| **平均响应时间** | 纳秒 (ns) | 请求处理平均时间 | 所有测试 |
| **吞吐量** | 操作/秒 (ops/sec) | 单位时间处理能力 | 高并发测试 |
| **百分位数** | 纳秒 (ns) | P50/P90/P99 响应时间 | 服务质量评估 |
| **内存使用** | 千字节 (KB) | 测试期间内存占用 | 内存敏感测试 |
| **CPU 使用率** | 百分比 (%) | 测试期间 CPU 占用 | CPU 敏感测试 |

### 6.2 扩展性能指标

| 指标 | 单位 | 描述 | 适用场景 |
|------|------|------|----------|
| **延迟抖动** | 纳秒 (ns) | 响应时间标准差 | 稳定性测试 |
| **错误率** | 百分比 (%) | 失败请求比例 | 可靠性测试 |
| **资源泄漏** | 字节/迭代 (bytes/iter) | 每次迭代内存增长 | 内存泄漏测试 |
| **可扩展性** | 线性系数 | 并发增加时的性能变化 | 扩展性测试 |

### 6.3 测试场景定义

#### 6.3.1 核心循环性能测试
- **测试目标**：任务提交、执行、查询性能
- **关键指标**：任务处理延迟、系统吞吐量
- **测试配置**：不同任务大小、并发级别

#### 6.3.2 内存系统性能测试
- **测试目标**：缓存命中率、存储操作性能
- **关键指标**：访问延迟、缓存效率、内存使用
- **测试配置**：不同数据大小、访问模式

#### 6.3.3 网络通信性能测试
- **测试目标**：RPC 调用、消息传递性能
- **关键指标**：往返延迟、消息吞吐量
- **测试配置**：不同消息大小、并发连接数

#### 6.3.4 安全沙箱性能测试
- **测试目标**：安全检查和隔离开销
- **关键指标**：安全检查延迟、系统调用开销
- **测试配置**：不同安全级别、检查深度

---

## 📚 附录

### A.1 相关文档

- [service_management_framework.md](../Capital_Architecture/service_management_framework.md) - 服务管理框架设计
- [performance-tuning.md](../Capital_Guides/performance-tuning.md) - 性能调优指南
- [testing.md](../Capital_Guides/testing.md) - 测试指南
- [ARCHITECTURAL_PRINCIPLES.md](../ARCHITECTURAL_PRINCIPLES.md) - 架构原则

### A.2 参考实现

- [Google Benchmark](https://github.com/google/benchmark) - C++ 微基准测试库
- [Criterion](https://github.com/Snaipe/Criterion) - C 单元测试和基准测试框架
- [nanobench](https://github.com/martinus/nanobench) - 简单快速的 C++ 基准测试库

### A.3 设计决策记录

**决策 1：使用独立库而非现有测试框架**
- **理由**：现有测试框架（如 CMocka）专注于单元测试，缺乏性能测试特性
- **影响**：需要实现专用性能测试框架，但能更好满足需求
- **风险**：增加维护负担，但收益大于成本

**决策 2：支持多种报告格式**
- **理由**：不同场景需要不同格式（开发调试用文本，CI/CD 用 JSON）
- **影响**：增加实现复杂度，但提高实用性
- **风险**：格式兼容性和维护成本

**决策 3：集成历史跟踪**
- **理由**：性能回归检测需要历史数据
- **影响**：需要数据库支持，增加依赖
- **风险**：SQLite 是轻量级选择，风险可控

### A.4 待解决问题

1. **平台计时器差异**：不同平台的计时精度和开销差异
2. **测试干扰**：如何减少其他进程对测试结果的干扰
3. **结果稳定性**：如何确保测试结果在不同运行中的稳定性
4. **基准线管理**：如何管理和更新性能基准线

### A.5 修订历史

| 版本 | 日期 | 修改内容 | 修改人 |
|------|------|----------|--------|
| 1.0 | 2026-04-11 | 初始版本，基于 Phase 3.2 设计 | AgentOS 开发组 |
| 1.1 | 2026-04-11 | 完善接口定义和实施路线图 | AgentOS 开发组 |

---

<div align="center">

**AgentOS 性能基准测试框架设计**  
*度量改进 - 构建统一、可重复、可比较的性能测试体系*

© 2026 SPHARX Ltd. All Rights Reserved.

</div>