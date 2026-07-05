# Tests — Commons 测试套件

**模块路径**: `agentrt/commons/tests/`
**版本**: v0.1.0

## 概述

Tests 是 Commons 统一基础库的测试套件，包含单元测试和集成测试，覆盖平台抽象、错误处理、日志系统、令牌管理、成本控制、输入校验等核心模块。测试套件使用 CMake + CTest 框架，支持自动化构建和回归检测。

## 设计目标

- **全面覆盖**：覆盖 Commons 所有核心模块的基础功能路径
- **快速执行**：单元测试设计为轻量级，整体执行时间控制在分钟级
- **自动化集成**：通过 CTest 集成到 CI 流水线，每次提交自动执行
- **隔离性**：每个测试用例独立运行，不依赖外部资源和特定环境

## 目录结构

```
tests/
├── CMakeLists.txt               # 测试构建配置
├── README.md                    # 本文档
├── utils/                       # 测试工具框架
│   ├── test_framework.h         # 通用测试框架（断言宏、测试注册）
│   ├── test_macros.h            # 测试辅助宏（EXPECT_EQ、ASSERT_TRUE 等）
│   └── cmocka_stub.h            # cmocka 模拟框架兼容适配
├── unit/                        # 单元测试
│   ├── core_test.c              # 核心功能测试
│   ├── io_test.c                # I/O 工具测试
│   ├── test_platform.c          # 平台抽象层测试
│   ├── test_error.c             # 错误处理框架测试
│   ├── test_logger.c            # 日志系统测试
│   ├── test_token.c             # 令牌管理测试
│   ├── test_cost.c              # 成本估算与控制测试
│   ├── test_config.c            # 配置系统测试（当前禁用）
│   ├── test_types.c             # 类型系统测试（当前禁用）
│   ├── test_ipc.c               # IPC 测试（当前禁用）
│   ├── test_network.c           # 网络工具测试（当前禁用）
│   ├── test_string_utils.c      # 字符串工具测试
│   ├── test_observability.c     # 可观测性测试
│   ├── test_resource_guard.c    # 资源保护测试
│   └── test_input_validator.c   # 输入校验测试
└── integration/                  # 集成测试
    ├── test_common_integration.c # 公共模块集成测试（当前禁用）
    └── test_unified_modules.c   # 统一模块集成测试（当前禁用）
```

## 测试框架

### 测试辅助宏

`test_macros.h` 提供类 xUnit 风格的断言宏：

| 宏 | 说明 |
|-----|------|
| `TEST_ASSERT(cond)` | 断言条件为真，失败则终止测试 |
| `TEST_ASSERT_EQUAL(expected, actual)` | 断言值相等 |
| `TEST_ASSERT_NOT_NULL(ptr)` | 断言指针非空 |
| `TEST_ASSERT_NULL(ptr)` | 断言指针为空 |
| `TEST_ASSERT_STR_EQUAL(expected, actual)` | 断言字符串相等 |

### 通用测试框架

`test_framework.h` 提供测试注册和执行基础设施：

| 功能 | 说明 |
|------|------|
| 测试注册 | 通过宏自动注册测试函数到测试套件 |
| 结果汇总 | 自动统计通过/失败/跳过的测试数量 |
| 输出格式化 | 统一的测试输出格式，CI 友好 |
| 内存泄漏检测 | 可选的内存追踪模式 |

### cmocka 适配

`cmocka_stub.h` 为尚未迁移到 cmocka 框架的测试提供兼容适配层，确保测试代码在无 cmocka 环境下也能编译执行。

## 构建与运行

### 启用测试

```bash
# 在项目根目录构建并启用测试
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build
```

### 运行所有测试

```bash
cd build && ctest --output-on-failure
```

### 运行特定测试

```bash
# 运行单个测试
ctest -R test_platform

# 运行所有单元测试
ctest -R "^commons_test_"

# 并行运行
ctest -j $(nproc) --output-on-failure
```

### 运行指定测试可执行文件

```bash
cd build/agentrt/commons/tests
./test_platform
./test_error
```

## 当前状态

### 活跃测试

| 测试 | 状态 | 覆盖模块 |
|------|------|----------|
| `test_platform` | 活跃 | 平台抽象层 |
| `test_error` | 活跃 | 错误处理框架 |
| `test_logger` | 活跃 | 日志系统 |
| `test_token` | 活跃 | 令牌管理 |
| `test_cost` | 活跃 | 成本估算与控制 |
| `test_string_utils` | 活跃 | 字符串操作 |
| `test_observability` | 活跃 | 可观测性 |
| `test_resource_guard` | 活跃 | 资源保护 |
| `test_input_validator` | 活跃 | 输入校验 |
| `core_test` | 活跃 | 核心功能 |
| `io_test` | 活跃 | I/O 工具 |

### 待启用测试

以下测试当前因依赖未就绪或框架迁移而被禁用：

| 测试 | 禁用原因 |
|------|----------|
| `test_config` | config_unified 模块实现与头文件不匹配 |
| `test_types` | 需要 cmocka 测试框架 |
| `test_ipc` | 需要 cmocka 测试框架 |
| `test_network` | 需要 cmocka 测试框架 |
| `test_common_integration` | 缺少 manager.h 头文件 |
| `test_unified_modules` | 依赖 test_common_integration |

## 依赖关系

| 依赖 | 说明 |
|------|------|
| `agentrt_common` | Commons 聚合库，所有测试的链接目标 |
| `agentrt_core` | 核心库，提供基础功能 |
| CMake 3.20+ | 构建系统 |
| CTest | 测试执行框架 |

---

© 2026 SPHARX Ltd. All Rights Reserved.