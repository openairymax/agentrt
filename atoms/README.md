# AgentOS 内核层 (Atoms)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`atoms/` 是 AgentOS 的**微内核实现层**，提供操作系统最核心的运行机制。作为整个智能体操作系统的基石，本目录包含：

- **微内核基础** (`corekern/`): IPC、内存、任务、时间四大原子机制
- **三层运行时** (`coreloopthree/`): 认知→行动→记忆的完整生命周期管理
- **记忆系统** (`memoryrovol/`): L1→L2→L3→L4 四层卷载架构
- **系统调用** (`syscall/`): 用户态与内核通信的唯一通道
- **公共工具** (`utils/`): 日志、错误处理、可观测性等基础设施

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

| 依赖 | 最低版本 | 推荐版本 |
|------|---------|----------|
| **CMake** | 3.20 | 3.25+ |
| **GCC/Clang** | GCC 11 / Clang 14 | GCC 12 / Clang 15 |
| **FAISS** | 1.7.0 | 1.8.0+ |
| **SQLite3** | 3.35 | 3.40+ |
| **cJSON** | 1.7.15 | 1.8.0+ |

### 构建步骤

```bash
# 1. 克隆项目
git clone https://gitee.com/spharx/agentos.git
cd agentos

# 2. 创建构建目录
mkdir build && cd build

# 3. 配置 CMake
cmake ../atoms \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DENABLE_TRACING=OFF

# 4. 编译
cmake --build . --parallel $(nproc)

# 5. 运行测试
ctest --output-on-failure
```

### 常用 CMake 选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `CMAKE_BUILD_TYPE` | Release | Debug/Release/RelWithDebInfo |
| `BUILD_TESTS` | OFF | 构建单元测试 |
| `ENABLE_TRACING` | OFF | 启用 OpenTelemetry 追踪 |
| `ENABLE_ASAN` | OFF | 启用 AddressSanitizer |
| `ENABLE_LOGGING` | ON | 启用统一日志系统 |

详细说明：[BUILD.md](BUILD.md)

---

## 📦 核心模块详解

### 1. CoreKern (微内核基础)

**代码量**: ~9,000 LOC  
**职责**: 提供最基本的 OS 机制

| 组件 | 功能 | 性能指标 |
|------|------|----------|
| **IPC Binder** | 进程间通信 | < 1μs 延迟 |
| **内存管理** | RAII 智能指针、内存池 | 零泄漏保证 |
| **任务调度** | 加权轮询算法 | < 1ms 切换 |
| **时间服务** | 高精度计时器 | 纳秒级精度 |

**典型用法**:
```c
#include <agentos.h>

// 初始化内核
agentos_init(NULL);

// 创建任务
task_handle_t task = agentos_task_create(my_function, arg);

// 等待完成
agentos_task_wait(task, 5000);  // 5 秒超时

// 清理
agentos_cleanup();
```

详细文档：[corekern/README.md](corekern/README.md)

---

### 2. CoreLoopThree (三层运行时)

**代码量**: ~15,000 LOC  
**职责**: 智能体完整生命周期管理

#### 架构图

```
┌─────────────────┐
│   认知层        │  ← System 2 慢思考
│  Intent·Plan·Dispatch  │     (深度规划)
└────────┬────────┘
         ↓
┌─────────────────┐
│   行动层        │  ← System 1 快思考
│  Execute·Compensate·Trace│   (模式执行)
└────────┬────────┘
         ↓
┌─────────────────┐
│   记忆层        │  ← MemoryRovol FFI
│  Write·Search·Mount  │     (上下文挂载)
└─────────────────┘
```

**核心 API**:
```c
#include "loop.h"

// 创建核心循环
agentos_core_loop_t* loop;
agentos_loop_create(NULL, &loop);

// 提交任务
char* task_id;
agentos_loop_submit(loop, "分析销售数据", strlen(...), &task_id);

// 等待结果
char* result;
agentos_loop_wait(loop, task_id, 30000, &result, NULL);

// 销毁
agentos_loop_destroy(loop);
```

详细文档：[coreloopthree/README.md](coreloopthree/README.md)  
架构说明：[partdocs/architecture/folder/coreloopthree.md](../partdocs/architecture/folder/coreloopthree.md)

---

### 3. MemoryRovol (四层记忆系统)

**代码量**: ~20,000 LOC  
**职责**: 从原始数据到高级模式的完整记忆管理

#### 四层架构

| 层级 | 名称 | 功能 | 技术实现 |
|------|------|------|----------|
| **L1** | Raw Layer | 原始事件存储 | 文件系统 + SQLite 索引 |
| **L2** | Feature Layer | 向量嵌入检索 | FAISS + Embedding 模型 |
| **L3** | Structure Layer | 关系绑定编码 | 绑定算子 + 图神经网络 |
| **L4** | Pattern Layer | 模式挖掘抽象 | 持久同调 + HDBSCAN 聚类 |

**性能指标**:
- L1 写入吞吐：10,000+ 条/秒
- L2 检索延迟：< 10ms (k=10)
- L3 抽象速度：100 条/秒
- L4 挖掘速度：10 万条/分钟

详细文档：[memoryrovol/README.md](memoryrovol/README.md)

---

### 4. Syscall (系统调用)

**代码量**: ~3,000 LOC  
**职责**: 用户态与内核通信的唯一通道

#### 核心接口分类

| 类别 | 接口数量 | 主要函数 |
|------|---------|----------|
| **任务管理** | 4 个 | `submit/query/wait/cancel` |
| **记忆管理** | 4 个 | `write/search/get/delete` |
| **会话管理** | 4 个 | `create/get/close/list` |
| **可观测性** | 2 个 | `metrics/traces` |
| **Agent 管理** | 3 个 | `register/invoke/terminate` |

**线程安全**: 所有 syscall 均为线程安全

详细文档：[syscall/README.md](syscall/README.md)

---

## 🧪 测试

### 单元测试

```bash
# 运行所有测试
ctest --output-on-failure

# 运行特定模块测试
ctest -R test_ipc_binder --verbose
ctest -R test_memory_pool --verbose
```

### 覆盖率报告

```bash
# 生成覆盖率
cmake .. -DENABLE_COVERAGE=ON
make
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

**覆盖率要求**: ≥ 85%

---

## 📊 性能基准

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| IPC Binder 延迟 | < 1μs | 本地调用 |
| 任务调度延迟 | < 1ms | 加权轮询 |
| 记忆检索延迟 | < 10ms | FAISS IVF,PQ k=10 |
| 系统调用开销 | < 5% | 相比直接调用 |

---

## 🔗 相关文档

### 架构设计
- [架构设计原则 v1.6](../partdocs/architecture/folder/architectural_design_principles.md) - 四维正交原则体系
- [微内核设计](../partdocs/architecture/folder/microkernel.md) - CoreKern 详细架构
- [三层运行时](../partdocs/architecture/folder/coreloopthree.md) - CoreLoopThree 理论根基
- [四层记忆系统](../partdocs/architecture/folder/memoryrovol.md) - MemoryRovol 神经科学基础
- [系统调用规范](../partdocs/architecture/folder/syscall.md) - Syscall 接口契约

### 开发指南
- [快速入门](../partdocs/guides/folder/getting_started.md) - 环境搭建与首次编译
- [创建 Agent](../partdocs/guides/folder/create_agent.md) - Agent 开发教程
- [内核调优](../partdocs/guides/folder/kernel_tuning.md) - 性能优化参数

### 技术规范
- [C 编码规范](../partdocs/specifications/coding_standard/C_coding_style_guide.md)
- [安全编程指南](../partdocs/specifications/coding_standard/C&Cpp-secure-coding-guide.md)

---

## 🤝 贡献

欢迎贡献代码！请遵循以下步骤：

1. Fork 项目
2. 创建特性分支：`git checkout -b feature/amazing-feature`
3. 提交更改：`git commit -am 'Add amazing feature'`
4. 推送到分支：`git push origin feature/amazing-feature`
5. 创建 Pull Request

### 编码要求

- ✅ 遵循 C11 标准
- ✅ 使用 `clang-format` 格式化代码
- ✅ 所有公共 API 必须有 Doxygen 注释
- ✅ 单元测试覆盖率 > 85%

---

## 📞 联系方式

- **维护者**: AgentOS 架构委员会
- **技术支持**: lidecheng@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges 始于数据，终于智能。"*

</div>
