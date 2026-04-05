# AgentOS Manager 性能基准测试

本目录包含 Manager 模块的性能基准测试工具。

## 目录结构

```
benchmark/
├── benchmark_manager.py    # 主基准测试脚本
└── README.md              # 本文件
```

## benchmark_manager.py

性能基准测试脚本，用于测试 Manager 模块各组件的性能指标。

### 功能特性

- **TaskManager 性能测试**: 任务提交、查询、列表操作
- **SessionManager 性能测试**: 会话创建、获取、关闭操作
- **MemoryManager 性能测试**: 记忆写入、搜索、列表操作
- **SkillManager 性能测试**: 技能加载、列表、获取操作
- **统计指标**: 平均耗时、最小/最大耗时、吞吐量、OPS、标准差
- **性能评级**: 优秀(<1ms)、良好(<10ms)、一般(<100ms)、需优化(≥100ms)

### 使用方法

#### 基本用法

```bash
# 进入 benchmark 目录
cd agentos/manager/benchmark

# 运行基准测试（默认100次迭代）
python3 benchmark_manager.py --verbose
```

#### 高级用法

```bash
# 指定迭代次数
python3 benchmark_manager.py --iterations 1000 --verbose

# 输出JSON报告
python3 benchmark_manager.py --output performance_report.json

# 仅测试框架（不导入实际模块）
python3 benchmark_manager.py --skip-modules
```

### 输出示例

```
======================================================================
AgentOS Manager 模块性能基准测试报告
======================================================================
测试时间: 2026-04-04T10:30:00
总耗时: 12.34秒
测试项目: 12
----------------------------------------------------------------------
测试名称                         平均耗时      吞吐量           标准差
----------------------------------------------------------------------
TaskManager.submit               0.1234ms     8105.67 ops/s     0.0234ms
TaskManager.query                 0.0876ms     11415.22 ops/s    0.0156ms
SessionManager.create             0.1567ms     6382.15 ops/s     0.0345ms
MemoryManager.write               0.2012ms     4970.15 ops/s     0.0456ms
SkillManager.load                0.3245ms     3081.78 ops/s     0.0678ms

性能评级:
  TaskManager.submit: 🟢 优秀 (平均 0.1234ms)
  TaskManager.query: 🟢 优秀 (平均 0.0876ms)
  SessionManager.create: 🟢 优秀 (平均 0.1567ms)
======================================================================
```

### 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--verbose` | 显示详细日志 | False |
| `--iterations` | 每个测试的迭代次数 | 100 |
| `--output` | 输出JSON报告文件路径 | None |
| `--skip-modules` | 跳过实际模块测试 | False |

### 集成到 CI/CD

```yaml
# .github/workflows/performance-test.yml
name: Performance Tests

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]

jobs:
  performance-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.10'

      - name: Install dependencies
        run: |
          pip install pytest pytest-cov pyyaml jsonschema

      - name: Run performance benchmarks
        run: |
          cd agentos/manager/benchmark
          python3 benchmark_manager.py --output report.json

      - name: Upload benchmark results
        uses: actions/upload-artifact@v4
        with:
          name: benchmark-report
          path: agentos/manager/benchmark/report.json
```

## 性能基线

| 组件 | 操作 | 基线 (ms) | 目标 (ms) | 阈值 (ms) |
|------|------|-----------|-----------|-----------|
| TaskManager | submit | 0.15 | <1.0 | <10.0 |
| TaskManager | query | 0.10 | <0.5 | <5.0 |
| SessionManager | create | 0.20 | <1.0 | <10.0 |
| MemoryManager | write | 0.25 | <1.0 | <10.0 |
| MemoryManager | search | 0.30 | <2.0 | <20.0 |
| SkillManager | load | 0.40 | <2.0 | <20.0 |

## 故障排查

### 模块导入失败

如果看到 `MODULES_AVAILABLE = False`，请确保：

1. 在正确的目录下运行
2. Python path 设置正确
3. 所有依赖已安装

### 性能不达标

如果测试结果不达标（显示为🔴），请检查：

1. 系统负载情况
2. 内存是否充足
3. 是否有其他进程占用资源
4. 是否在虚拟机或容器环境中运行

---

Copyright (c) 2026 SPHARX. All Rights Reserved.
