# 测试架构说明

**版本**: v2.2.0  
**最后更新**: 2026-04-22

---

## 📋 概述

AgentOS 采用**双层测试架构**，结合了 C/C++ 和 Python 项目的最佳实践：

1. **模块自测层** - `agentos/*/tests/`（C/C++ 单元测试，与源码相邻）
2. **集中测试层** - `tests/`（统一测试入口，Python 集成/契约/性能测试）

---

## 🏗️ 双层测试架构

### 第一层：模块自测（agentos/*/tests/）

**位置**: 各子模块内的 `tests/` 目录

**特点**:
- 与源码相邻，构建便利
- CMake 直接引用同级头文件
- 模块级单元测试
- 编译时即可运行

**示例**:
```
agentos/atoms/corekern/
├── src/              # 内核源码
├── include/          # 头文件
├── tests/            # 模块自测
│   ├── CMakeLists.txt
│   └── test_*.c      # C 单元测试
└── CMakeLists.txt
```

**运行方式**:
```bash
cd agentos/atoms/corekern/tests
make test
# 或使用 CTest
ctest --verbose
```

### 第二层：集中测试（tests/）

**位置**: 项目根目录 `tests/`

**特点**:
- 统一测试入口
- Python 集成测试为主
- 跨模块端到端测试
- 契约/安全/性能测试
- CI/CD 集成

**运行方式**:
```bash
cd tests
python run_tests.py              # 统一运行器
pytest unit/                     # 仅运行单元测试
pytest integration/              # 仅运行集成测试
pytest -v --cov=agentos          # 带覆盖率
```

---

## 🔗 测试映射关系

### 模块级测试映射

| AgentOS 模块 | agentos/*/tests/ | tests/unit/ | 测试类型 |
|--------------|------------------|-------------|----------|
| **CoreKern** | `atoms/corekern/tests/` (7 文件) | `unit/atoms/corekern/` (6 文件) | C 单元 + Python 集成 |
| **CoreLoopThree** | `atoms/coreloopthree/tests/` (8 文件) | `unit/atoms/coreloopthree/` (9 文件) | C 单元 + Python 集成 |
| **MemoryRovol** | `atoms/memoryrovol/tests/` (5 文件) | `unit/atoms/memoryrovol/` (5 文件) | C 单元 + Python 集成 |
| **Syscall** | `atoms/syscall/tests/` | `unit/atoms/syscall/` | C 单元 + Python 集成 |
| **Commons** | `commons/tests/` (15+ 文件) | `unit/commons/` (15+ 文件) | C 单元 + Python 集成 |
| **Cupolas** | `cupolas/tests/` (15+ 文件) | `unit/cupolas/` (10+ 文件) | C 单元 + Python 集成 |
| **Gateway** | `gateway/tests/` (2 文件) | `unit/gateway/` | C 单元 + Python 集成 |
| **Heapstore** | `heapstore/tests/` (1 文件) | `unit/heapstore/` | C 单元 + Python 集成 |
| **LLM Daemon** | `daemon/llm_d/tests/` (6 文件) | `unit/daemon/llm_d/` | C 单元 + Python 集成 |
| **Market Daemon** | `daemon/market_d/tests/` (5 文件) | `unit/daemon/market_d/` | C 单元 + Python 集成 |
| **Monitor Daemon** | `daemon/monit_d/tests/` (4 文件) | `unit/daemon/monit_d/` | C 单元 + Python 集成 |
| **Scheduler Daemon** | `daemon/sched_d/tests/` (2 文件) | `unit/daemon/sched_d/` | C 单元 + Python 集成 |
| **Tool Daemon** | `daemon/tool_d/tests/` (6 文件) | `unit/daemon/tool_d/` | C 单元 + Python 集成 |
| **Manager** | `manager/tests/` | `unit/manager/` | Python 集成 |

### 测试类型分布

| 测试类型 | 位置 | 框架 | 文件数 |
|----------|------|------|--------|
| **C 单元测试** | `agentos/*/tests/` | CMockery2/CTest | 100+ |
| **Python 单元测试** | `tests/unit/` | pytest | 80+ |
| **Python 集成测试** | `tests/integration/` | pytest | 15+ |
| **契约测试** | `tests/contract/` | pytest | 2 |
| **安全测试** | `tests/security/` | pytest | 5 |
| **性能基准测试** | `tests/benchmarks/` | pytest-benchmark | 8+ |
| **模糊测试** | `tests/fuzz/` | 自定义 | 3 |
| **端到端测试** | `tests/e2e/` | pytest | 1 |

---

## 🚀 测试运行指南

### 快速开始

```bash
# 1. 统一运行所有测试
cd tests
python run_tests.py

# 2. 运行 C 单元测试
cd agentos
mkdir -p build && cd build
cmake -DBUILD_TESTS=ON ..
make test

# 3. 运行 Python 测试
cd tests
pytest unit/ -v

# 4. 运行集成测试
pytest integration/ -v

# 5. 运行完整测试套件（含覆盖率）
pytest --cov=agentos --cov-report=html
```

### CI/CD 运行

```bash
# GitHub Actions 示例
- name: Run Tests
  run: |
    # 编译 C 测试
    cd agentos && mkdir -p build && cd build
    cmake -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug ..
    make -j$(nproc)
    ctest --output-on-failure
    
    # 运行 Python 测试
    cd ../../../tests
    pip install -r requirements.txt
    pytest --cov=agentos --cov-report=xml -v
```

---

## 📊 测试覆盖率目标

| 模块 | 目标覆盖率 | 当前状态 |
|------|------------|----------|
| CoreKern | 90%+ | ✅ 95% |
| CoreLoopThree | 85%+ | ✅ 88% |
| MemoryRovol | 85%+ | ✅ 87% |
| Commons | 80%+ | ✅ 85% |
| Cupolas | 90%+ | ✅ 92% |
| Gateway | 85%+ | ⚠️ 78% |
| Daemon | 85%+ | ✅ 86% |
| **总体** | **85%+** | **✅ 88%** |

---

## 📁 目录结构

```
tests/
├── README.md                    # 本文档
├── TESTING_GUIDELINES.md        # 测试指南
├── run_tests.py                 # 统一测试运行器
├── conftest.py                  # pytest 全局配置
├── Makefile                     # Make 运行入口
├── .coveragerc                  # 覆盖率配置
├── fixtures/                    # 测试夹具
│   └── data/                    #   测试数据
├── unit/                        # 单元测试
│   ├── atoms/                   #   内核层
│   ├── commons/                 #   支持层
│   ├── cupolas/                 #   安全层
│   ├── daemon/                  #   服务层
│   ├── gateway/                 #   网关层
│   ├── heapstore/               #   存储层
│   ├── manager/                 #   管理层
│   ├── openlab/                 #   应用层
│   └── sdk/                     #   SDK层
├── integration/                 # 集成测试
├── contract/                    # 契约测试
├── security/                    # 安全测试
├── benchmarks/                  # 性能基准测试
├── fuzz/                        # 模糊测试
├── e2e/                         # 端到端测试
└── utils/                       # 测试工具
```

---

## 🔧 维护规范

### 测试文件命名

- **C 测试**: `test_<模块名>.c`
- **Python 测试**: `test_<模块名>.py`
- **基准测试**: `<模块名>_benchmark.py`

### 提交前检查

1. ✅ 所有测试通过 (`python run_tests.py`)
2. ✅ 覆盖率不低于 85%
3. ✅ 无废弃测试 (`pytest.mark.skip` 需注释说明)
4. ✅ 测试数据放在 `fixtures/data/`

### 新增模块测试

当新增 agentos 子模块时：

1. 在 `agentos/<module>/tests/` 创建 C 单元测试
2. 在 `tests/unit/<module>/` 创建 Python 集成测试
3. 更新 `ARCHITECTURE.md` 中的映射表

---

© 2026 SPHARX Ltd. All Rights Reserved.
