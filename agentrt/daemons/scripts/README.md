# Daemon Scripts — CI/CD 与质量保障脚本

> **模块路径**: `agentrt/daemons/scripts/` | **版本**: v0.1.0

## 概述

`daemons/scripts/` 包含 AgentRT 守护进程模块的 CI/CD 构建脚本、静态分析工具和代码覆盖率验证脚本，为持续集成和质量保障提供自动化支持。

## 目录结构

```
scripts/
├── README.md                # 本文件
├── ci.sh                    # CI 流水线构建入口脚本
├── local-ci.sh              # 本地 CI 模拟运行脚本
├── static-analysis.sh       # 静态代码分析脚本（cppcheck）
└── verify-coverage.sh       # 代码覆盖率验证脚本
```

## 核心组件说明

### ci.sh — CI 构建入口脚本

CI 流水线的主入口脚本，执行完整的构建、测试、分析和报告流程。

| 子命令 | 说明 |
|--------|------|
| `all` | 执行完整流程（默认）：依赖检查 → 构建 → 测试 → 静态分析 → 覆盖率 → 报告 |
| `build` | 仅构建所有模块 |
| `test` | 仅运行单元测试 |
| `cppcheck` | 仅运行 cppcheck 静态分析 |
| `coverage` | 仅运行代码覆盖率分析 |
| `clean` | 清理所有构建产物 |

**功能特性**：
- 自动检查构建依赖（cmake、gcc/clang）
- 可选依赖检测（cppcheck、gcov/lcov）
- CMake Debug 模式构建，启用 `--coverage` 编译选项
- 并行构建（自动检测 CPU 核心数）
- ctest + 直接运行测试可执行文件
- 覆盖率目标 ≥ 80%
- 生成构建报告（模块状态、测试状态、静态分析结果）

### local-ci.sh — 本地 CI 模拟脚本

本地开发环境使用的 CI 模拟脚本，支持逐模块构建和灵活配置。

| 子命令 | 说明 |
|--------|------|
| `build` | 构建所有模块（默认） |
| `all` | 构建 + 静态分析 + 覆盖率 + 报告 |
| `analysis` | 仅运行静态分析 |
| `coverage` | 仅运行覆盖率分析（需先构建） |
| `report` | 生成构建报告 |
| `clean` | 清理构建目录 |
| `help` | 显示帮助信息 |

**环境变量**：

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `BUILD_TYPE` | `Release` | 构建类型（Release/Debug） |
| `PARALLEL_JOBS` | CPU 核心数 | 并行作业数 |
| `ENABLE_COVERAGE` | `OFF` | 启用覆盖率编译（ON/OFF） |

**构建模块**：commons → llm_d → tool_d → monit_d → sched_d → market_d

### static-analysis.sh — 静态代码分析脚本

基于 cppcheck 的 C11 代码静态分析脚本。

| 子命令 | 说明 |
|--------|------|
| `all` | 运行完整分析流程（默认）：分析 → HTML 报告 → 摘要 |
| `analyze` | 仅运行分析，生成 XML/TXT 报告 |
| `html` | 仅生成 HTML 报告 |
| `help` | 显示帮助信息 |

**分析配置**：
- 标准：C11
- 平台：unix64
- 级别：exhaustive
- 抑制：`missingIncludeSystem`、`unusedFunction`
- 输出：XML 报告 + TXT 报告 + HTML 报告（可选）

**报告输出**：
- `reports/cppcheck_report.xml` — XML 格式详细报告
- `reports/cppcheck_report.txt` — 文本格式摘要报告
- `reports/cppcheck_html/` — HTML 格式可视化报告

### verify-coverage.sh — 覆盖率验证脚本

代码覆盖率收集、报告生成和目标验证脚本。

**功能流程**：
1. 检查 lcov/genhtml 工具
2. 逐模块收集 gcda 覆盖率数据
3. 合并所有模块覆盖率数据
4. 过滤系统头文件和测试代码
5. 生成 HTML 可视化报告
6. 验证行覆盖率是否达到目标值

**环境变量**：

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `COVERAGE_TARGET` | `80` | 覆盖率目标百分比 |

**报告输出**：
- `reports/coverage/total_coverage.info` — 合并后的覆盖率数据
- `reports/coverage/html/` — HTML 可视化报告

**验证结果**：
- 达标（≥ 目标值）：输出 `COVERAGE VALIDATION PASSED`
- 未达标（< 目标值）：输出 `COVERAGE VALIDATION WARNING`，并列出未充分覆盖的文件

## 依赖关系

```
scripts
├── cmake (必需)       # 构建系统
├── gcc/clang (必需)   # C 编译器
├── cppcheck (可选)    # 静态代码分析
├── lcov/genhtml (可选) # 代码覆盖率
├── gcov (可选)        # 覆盖率数据生成
└── bc (可选)          # 数值比较
```

## 使用说明

### CI 流水线

```bash
# 完整 CI 流程
bash scripts/ci.sh all

# 仅构建
bash scripts/ci.sh build

# 仅测试
bash scripts/ci.sh test
```

### 本地开发

```bash
# 构建所有模块
bash scripts/local-ci.sh build

# 完整流程（含覆盖率和分析）
bash scripts/local-ci.sh all

# 启用覆盖率构建
ENABLE_COVERAGE=ON bash scripts/local-ci.sh build

# Debug 模式构建
BUILD_TYPE=Debug bash scripts/local-ci.sh build
```

### 静态分析

```bash
# 运行完整分析
bash scripts/static-analysis.sh all

# 仅分析，不生成 HTML
bash scripts/static-analysis.sh analyze
```

### 覆盖率验证

```bash
# 验证覆盖率（默认目标 80%）
bash scripts/verify-coverage.sh

# 自定义覆盖率目标
COVERAGE_TARGET=90 bash scripts/verify-coverage.sh
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
