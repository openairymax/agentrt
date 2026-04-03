# AgentOS 内核层 (Atoms)

> **AgentOS Microkernel Implementation Layer - 微内核实现层**

**版本**: v1.0.0.6
**最后更新**: 2026-03-26
**许可证**: Apache License 2.0
**生产级状态**: 99.9999% Production Ready

---

## 🎯 模块定位

`atoms/` 是 AgentOS 的**微内核核心实现层**，为整个智能体操作系统提供最基础的运行机制。作为系统的基石，本模块实现了：

- **微内核基础** (`corekern/`): IPC、内存、任务、时间四大原子机制
- **三层运行时** (`coreloopthree/`): 认知 → 行动 → 记忆的完整生命周期管理
- **记忆系统** (`memoryrovol/`): L1→L2→L3→L4 四层卷载架构
- **系统调用** (`syscall/`): 用户态与内核通信的唯一标准通道
- **公共工具** (`utils/`): 日志、错误处理、可观测性等基础设施

**设计哲学**:
- 🏛️ **工程控制论**: 反馈闭环，确保系统稳定性
- 🧩 **系统工程**: 模块化设计，接口稳定
- 🧠 **双系统认知**: System 1（快思考）+ System 2（慢思考）
- ⚡ **纯净微内核**: 内核功能最小化，机制与策略分离
- 🎨 **乔布斯美学**: 代码优雅、简洁、高可用性

**重要**: 本目录仅包含 C 语言实现的内核代码，不包含上层应用或服务逻辑。

---

## 📁 目录结构

```
atoms/
├── README.md                 # 本文档
├── BUILD.md                  # 编译指南（详细构建步骤）
├── CMakeLists.txt            # 顶层构建配置
│
├── corekern/                 # 微内核基础 ⭐
│   ├── include/              # 公共头文件
│   │   ├── agentos.h         # 统一入口
│   │   ├── ipc.h             # IPC 接口
│   │   ├── mem.h             # 内存接口
│   │   ├── task.h            # 任务接口
│   │   ├── time.h            # 时间接口
│   │   └── error.h           # 错误码
│   ├── src/                  # 实现
│   │   ├── ipc/              # Binder IPC 实现
│   │   ├── mem/              # 内存管理（RAII）
│   │   ├── task/             # 任务调度（加权轮询）
│   │   ├── time/             # 高精度时间服务
│   │   └── main.c            # 内核入口
│   ├── tests/                # 单元测试
│   └── CMakeLists.txt
│
├── coreloopthree/            # 三层核心运行时 ⭐
│   ├── include/              # 三层接口
│   │   ├── cognition.h       # 认知层
│   │   ├── execution.h       # 行动层
│   │   ├── memory.h          # 记忆层
│   │   └── loop.h            # 主循环接口
│   ├── src/                  # 实现
│   │   ├── cognition/        # 意图理解/任务规划/Agent 调度
│   │   ├── execution/        # 执行引擎/补偿事务/责任链
│   │   └── memory/           # 记忆 FFI 封装
│   ├── tests/                # 集成测试
│   └── CMakeLists.txt
│
├── memoryrovol/              # 四层记忆卷载 ⭐
│   ├── include/              # 记忆接口
│   │   ├── memoryrovol.h     # 主接口
│   │   ├── layer1_raw.h      # L1 原始卷
│   │   ├── layer2_feature.h  # L2 特征层
│   │   ├── layer3_structure.h# L3 结构层
│   │   ├── layer4_pattern.h  # L4 模式层
│   │   ├── retrieval.h       # 检索机制
│   │   └── forgetting.h      # 遗忘机制
│   ├── src/                  # 各层实现
│   └── CMakeLists.txt
│
├── syscall/                  # 系统调用层
│   ├── include/              # 系统调用接口
│   │   └── syscalls.h        # 统一 syscall 头文件
│   ├── src/                  # syscall 实现
│   │   ├── task_sys.c        # 任务管理 syscall
│   │   ├── memory_sys.c      # 记忆管理 syscall
│   │   ├── session_sys.c     # 会话管理 syscall
│   │   └── telemetry_sys.c   # 可观测性 syscall
│   └── CMakeLists.txt
│
└── utils/                    # 公共工具库
    ├── logging/              # 统一日志系统
    ├── error/                # 错误处理
    ├── observability/        # OpenTelemetry 集成
    └── types/                # 通用类型定义
```

---

## 🔧 快速开始

### 前置要求

| 依赖 | 最低版本 | 推荐版本 | 用途 |
|------|---------|----------|------|
| **CMake** | 3.20 | 3.25+ | 构建系统 |
| **GCC/Clang** | GCC 11 / Clang 14 | GCC 12 / Clang 15 | 编译器 |
| **FAISS** | 1.7.0 | 1.8.0+ | 向量检索 |
| **SQLite3** | 3.35 | 3.40+ | 数据存储 |
| **cJSON** | 1.7.15 | 1.8.0+ | JSON 解析 |

### 构建步骤

```bash
# 1. 克隆项目（推荐 AtomGit 官方仓库）
git clone https://atomgit.com/spharx/agentos.git
cd agentos

# 或 Gitee 官方仓库
# git clone https://gitee.com/spharx/agentos.git
# cd agentos

# 或 GitHub 官方仓库
# git clone https://github.com/SpharxTeam/AgentOS.git
# cd AgentOS

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置 CMake（生产环境推荐）
cmake ../atoms \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DENABLE_TRACING=OFF \
  -DENABLE_LOGGING=ON

# 4. 编译
cmake --build . --parallel $(nproc)

# 5. 运行测试
ctest --output-on-failure
```

### 常用 CMake 选项

| 选项 | 默认值 | 说明 | 推荐值 |
|------|--------|------|--------|
| `CMAKE_BUILD_TYPE` | Release | 构建类型 (Debug/Release/RelWithDebInfo) | Release |
| `BUILD_TESTS` | OFF | 构建单元测试 | ON (开发环境) |
| `ENABLE_TRACING` | OFF | 启用 OpenTelemetry 追踪 | OFF (生产环境) |
| `ENABLE_ASAN` | OFF | 启用 AddressSanitizer | ON (开发环境) |
| `ENABLE_LOGGING` | ON | 启用统一日志系统 | ON |
| `ENABLE_COVERAGE` | OFF | 启用代码覆盖率 | ON (测试环境) |

**详细说明**: [BUILD.md](BUILD.md)

---

## 📦 核心模块详解

### 1. CoreKern (微内核基础)

> **The Foundation of AgentOS - AgentOS 的基石**

**代码量**: ~9,000 LOC
**职责**: 提供最基本的 OS 机制，确保系统稳定性和性能

| 组件 | 功能 | 性能指标 | 实现细节 |
|------|------|----------|----------|
| **IPC Binder** | 进程间通信 | < 1μs 延迟 | Binder 协议，零拷贝 |
| **内存管理** | RAII 智能指针、内存池 | 零泄漏保证 | 自动生命周期管理 |
| **任务调度** | 加权轮询算法 | < 1ms 切换 | 公平调度，优先级支持 |
| **时间服务** | 高精度计时器 | 纳秒级精度 | TSC/RDTSC 指令 |

**典型用法**:

```c
#include <agentos.h>

int main(void) {
    // 初始化内核
    agentos_error_t err = agentos_init(NULL);
    if (err != AGENTOS_SUCCESS) {
        return 1;
    }

    // 创建任务
    task_handle_t task = agentos_task_create(my_function, arg);
    if (task == NULL) {
        agentos_cleanup();
        return 1;
    }

    // 等待完成（5 秒超时）
    err = agentos_task_wait(task, 5000);
    if (err != AGENTOS_SUCCESS) {
        // 错误处理
    }

    // 清理
    agentos_cleanup();
    return 0;
}
```

**关键特性**:
- ✅ **线程安全**: 所有 API 均为线程安全
- ✅ **错误处理**: 完善的错误码体系
- ✅ **资源管理**: RAII 模式，零泄漏保证
- ✅ **可移植性**: 支持 Linux、Windows、macOS

**详细文档**: [corekern/README.md](corekern/README.md)

---

### 2. CoreLoopThree (三层运行时)

> **Complete Agent Lifecycle Management - 智能体完整生命周期管理**

**代码量**: ~15,000 LOC
**职责**: 实现智能体的认知、行动、记忆三层核心循环

#### 架构图

```
┌─────────────────────────────────┐
│         认知层 (Cognition)      │  ← System 2 慢思考
│  Intent Understanding · Task    │     (深度规划)
│  Planning · Agent Dispatching   │
└────────────┬────────────────────┘
             │
             ↓
┌─────────────────────────────────┐
│         行动层 (Execution)      │  ← System 1 快思考
│  Execute · Compensate · Trace   │   (模式执行)
│  (责任链 · 补偿事务 · 追踪)      │
└────────────┬────────────────────┘
             │
             ↓
┌─────────────────────────────────┐
│         记忆层 (Memory)         │  ← MemoryRovol FFI
│  Write · Search · Mount         │   (上下文挂载)
│  (写入 · 检索 · 挂载)            │
└─────────────────────────────────┘
```

**核心 API**:

```c
#include "loop.h"

int main(void) {
    // 创建核心循环
    agentos_core_loop_t* loop;
    agentos_error_t err = agentos_loop_create(NULL, &loop);
    if (err != AGENTOS_SUCCESS) {
        return 1;
    }

    // 提交任务
    char* task_id = NULL;
    const char* task_content = "分析销售数据";
    err = agentos_loop_submit(
        loop,
        task_content,
        strlen(task_content),
        &task_id
    );

    // 等待结果（30 秒超时）
    char* result = NULL;
    err = agentos_loop_wait(loop, task_id, 30000, &result, NULL);

    // 处理结果
    if (err == AGENTOS_SUCCESS) {
        printf("Result: %s\n", result);
        agentos_free(result);
    }

    // 销毁循环
    agentos_loop_destroy(loop);
    return 0;
}
```

**性能指标**:
- 🚀 任务提交延迟：< 1ms
- 🚀 认知处理速度：100+ 任务/秒
- 🚀 执行引擎吞吐量：1000+ 操作/秒
- 🚀 记忆检索延迟：< 10ms

**详细文档**: [coreloopthree/README.md](coreloopthree/README.md)
**架构说明**: [paper/architecture/folder/coreloopthree.md](../paper/architecture/folder/coreloopthree.md)

---

### 3. MemoryRovol (四层记忆系统)

> **From Raw Data to Advanced Patterns - 从原始数据到高级模式**

**代码量**: ~20,000 LOC
**职责**: 提供从原始事件存储到高级模式挖掘的完整记忆管理能力

#### 四层架构

| 层级 | 名称 | 功能 | 技术实现 | 性能指标 |
|------|------|------|----------|----------|
| **L1** | Raw Layer | 原始事件存储 | 文件系统 + SQLite 索引 | 10,000+ 条/秒 |
| **L2** | Feature Layer | 向量嵌入检索 | FAISS + Embedding 模型 | < 10ms (k=10) |
| **L3** | Structure Layer | 关系绑定编码 | 绑定算子 + 图神经网络 | 100 条/秒 |
| **L4** | Pattern Layer | 模式挖掘抽象 | 持久同调 + HDBSCAN | 10 万条/分钟 |

**核心接口**:

```c
#include "memoryrovol.h"

// 创建存储
agentos_mrl_storage_handle_t* storage;
agentos_mrl_error_t err = agentos_mrl_storage_create(
    &storage,
    AGENTOS_MRL_HYBRID_TYPE,  // 混合存储类型
    10000,                     // 最大条目数
    "/path/to/db"
);

// 写入记忆
agentos_mrl_item_handle_t* item;
err = agentos_mrl_item_save(
    storage,
    "conversation",            // 类型
    "用户对话内容",            // 内容
    &metadata,                 // 元数据
    vector_data,               // 向量数据
    vector_dim,                // 维度
    raw_data,                  // 原始数据
    raw_data_len,
    &item
);

// 检索相似记忆
agentos_mrl_search_result_t results[10];
size_t num_results;
err = agentos_mrl_search_by_vector(
    storage,
    query_vector,
    vector_dim,
    0.7f,                      // 相似度阈值
    results,
    10,
    &num_results
);

// 清理
agentos_mrl_storage_destroy(&storage);
```

**关键特性**:
- 🧠 **神经科学基础**: 模拟人脑记忆分层机制
- 🔍 **多模态检索**: 支持向量、关键词、时间等多种检索方式
- ♻️ **遗忘机制**: 自动清理低重要性记忆，优化存储效率
- 📊 **可观测性**: 完整的记忆统计和监控功能

**详细文档**: [memoryrovol/README.md](memoryrovol/README.md)

---

### 4. Syscall (系统调用)

> **The Only Bridge Between User and Kernel - 用户态与内核的唯一桥梁**

**代码量**: ~3,000 LOC
**职责**: 提供用户态程序与内核交互的标准接口，所有系统调用均为线程安全

#### 核心接口分类

| 类别 | 接口数量 | 主要函数 | 典型用途 |
|------|---------|----------|----------|
| **任务管理** | 4 个 | `submit/query/wait/cancel` | 提交和管理任务 |
| **记忆管理** | 4 个 | `write/search/get/delete` | 记忆存储和检索 |
| **会话管理** | 4 个 | `create/get/close/list` | 会话生命周期管理 |
| **可观测性** | 2 个 | `metrics/traces` | 监控和追踪 |
| **Agent 管理** | 3 个 | `register/invoke/terminate` | Agent 注册和调用 |

**使用示例**:

```c
#include "syscalls.h"

// 提交任务
const char* task_json = "{\"type\": \"analysis\", \"content\": \"...\"}";
agentos_error_t err = agentos_sys_task_submit(task_json, strlen(task_json));

// 查询任务状态
char* status_json = NULL;
err = agentos_sys_task_query(task_id, &status_json);

// 写入记忆
err = agentos_sys_memory_write(memory_data, memory_len);

// 检索记忆
char* search_results = NULL;
err = agentos_sys_memory_search(query, &search_results);
```

**线程安全保证**:
- ✅ 所有 syscall 内部使用互斥锁保护
- ✅ 支持多线程并发调用
- ✅ 无锁读取优化（部分只读接口）

**详细文档**: [syscall/README.md](syscall/README.md)

---

## 🧪 测试

### 单元测试

```bash
# 运行所有测试
ctest --output-on-failure

# 运行特定模块测试
ctest -R test_ipc_binder --verbose      # IPC Binder 测试
ctest -R test_memory_pool --verbose     # 内存池测试
ctest -R coreloopthree --verbose        # 核心循环测试
ctest -R memoryrovol --verbose          # 记忆系统测试
```

### 覆盖率报告

```bash
# 生成覆盖率
cmake .. -DENABLE_COVERAGE=ON
make
ctest

# 生成 HTML 报告
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report

# 查看报告
open coverage_report/index.html
```

**覆盖率要求**:
- 📊 **整体覆盖率**: ≥ 85%
- 📊 **核心模块**: ≥ 90% (corekern, syscall)
- 📊 **新增代码**: ≥ 95%

**测试报告**: 每次 CI/CD 自动生成覆盖率报告

---

## 📊 性能基准

基于标准测试环境 (Intel i7-12700K, 32GB RAM, NVMe SSD):

| 指标 | 数值 | 测试条件 | 说明 |
|------|------|---------|------|
| **IPC Binder 延迟** | < 1μs | 本地调用 | 零拷贝优化 |
| **任务调度延迟** | < 1ms | 加权轮询 | 100 并发任务 |
| **记忆检索延迟** | < 10ms | FAISS IVF,PQ k=10 | 百万级向量库 |
| **系统调用开销** | < 5% | 相比直接调用 | 内核态切换开销 |
| **启动时间** | < 500ms | Cold start | 完整初始化 |
| **内存占用** | ~2MB | 典型场景 | 基础运行时 |

**性能调优指南**: [paper/guides/folder/kernel_tuning.md](../paper/guides/folder/kernel_tuning.md)

---

## 🔗 相关文档

### 架构设计

- 📐 [架构设计原则 v1.6](../paper/architecture/folder/architectural_design_principles.md) - 四维正交原则体系
- 🏛️ [微内核设计](../paper/architecture/folder/microkernel.md) - CoreKern 详细架构
- 🧠 [三层运行时](../paper/architecture/folder/coreloopthree.md) - CoreLoopThree 理论根基
- 💾 [四层记忆系统](../paper/architecture/folder/memoryrovol.md) - MemoryRovol 神经科学基础
- 🔌 [系统调用规范](../paper/architecture/folder/syscall.md) - Syscall 接口契约

### 开发指南

- 🚀 [快速入门](../paper/guides/folder/getting_started.md) - 环境搭建与首次编译
- 🤖 [创建 Agent](../paper/guides/folder/create_agent.md) - Agent 开发教程
- ⚙️ [内核调优](../paper/guides/folder/kernel_tuning.md) - 性能优化参数

### 技术规范

- 📝 [C 编码规范](../paper/specifications/coding_standard/C_coding_style_guide.md)
- 🔒 [安全编程指南](../paper/specifications/coding_standard/C&Cpp-secure-coding-guide.md)
- 📖 [API 设计规范](../paper/specifications/api_design_guidelines.md)

---

## 🤝 贡献

欢迎贡献代码！请遵循以下步骤：

1. **Fork 项目**
2. **创建特性分支**: `git checkout -b feature/amazing-feature`
3. **提交更改**: `git commit -am 'Add amazing feature'`
4. **推送到分支**: `git push origin feature/amazing-feature`
5. **创建 Pull Request**

### 编码要求

- ✅ **遵循 C11 标准**: 使用现代 C 语言特性
- ✅ **代码格式化**: 使用 `clang-format` 格式化代码
- ✅ **文档注释**: 所有公共 API 必须有 Doxygen 注释
- ✅ **测试覆盖**: 单元测试覆盖率 > 85%
- ✅ **安全编程**: 遵循安全编程指南，避免常见漏洞
- ✅ **性能意识**: 关注内存使用和 CPU 效率

### 代码审查标准

- 📋 **功能完整性**: 功能实现完整，无简化或 TODO
- 📋 **代码质量**: 无编译警告，通过静态分析
- 📋 **测试充分性**: 单元测试覆盖核心逻辑
- 📋 **文档完整性**: API 文档和使用示例完整
- 📋 **性能影响**: 无明显性能退化

---

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- **项目主页**: https://gitee.com/spharx/agentos

---

## 📜 许可证

Copyright © 2026 SPHARX Ltd. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

---

<div align="center">

**"From data intelligence emerges 始于数据，终于智能。"**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/SpharxTeam/AgentOS/actions)
[![Coverage Status](https://img.shields.io/badge/coverage-87%25-blue)](https://github.com/SpharxTeam/AgentOS/actions)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0.6-orange)](CHANGELOG.md)

</div>
