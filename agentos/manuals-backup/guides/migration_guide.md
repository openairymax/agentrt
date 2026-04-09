Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 迁移指南

**版本**: Doc V1.7
**最后更新**: 2026-03-31
**适用对象**: 从旧版本迁移的开发者和运维工程师
**作者**: LirenWang

---

## 1. 概述

本文档提供了从 AgentOS 旧版本迁移到新版本的系统性指南。迁移的本质是一个**控制论反馈过程**：旧系统输出的偏差信号驱动新系统的参数校准，直至达到稳态。

AgentOS 采用语义化版本号（MAJOR.MINOR.PATCH），其中：

- **MAJOR**：表示不兼容的 API 更改——对应系统工程的"接口重新定义"
- **MINOR**：表示向后兼容的功能添加——对应"子系统增量升级"
- **PATCH**：表示向后兼容的 bug 修复——对应"负反馈校准"

### 1.1 迁移哲学

AgentOS 的迁移设计遵循三条原则：

1. **分层迁移**：从底层微内核到上层服务，逐层验证，确保每层迁移后的系统稳定性
2. **双路径并行**：System 1（快速迁移脚本）处理标准化变更；System 2（人工审查）处理架构级变更
3. **回滚保障**：每一步迁移操作都有对应的回滚方案，保证系统可恢复

### 1.2 版本兼容性保证

- 在相同 MAJOR 版本内，所有 MINOR 和 PATCH 版本都是向后兼容的
- 当 MAJOR 版本变更时，会提供完整的迁移工具和自动化脚本
- 破坏性更改会在 `CHANGELOG.md` 和本指南中同步标注

---

## 2. 从 v0.x 迁移到 v1.0

v1.0 是 AgentOS 的首个生产就绪版本，引入了完整的微内核架构（corekern）、三层运行时（coreloopthree）、四层记忆系统（memoryrovol）和安全穹顶（cupolas）。

### 2.1 架构级变更

#### 2.1.1 目录结构重组

v0.x 采用扁平结构，v1.0 严格按照系统工程分层原则重新组织：

```c
/* v0.x 目录结构（扁平） */
core/
├── include/
├── src/
└── CMakeLists.txt

/* v1.0 目录结构（分层） */
agentos/atoms/
├── corekern/       /* 微内核：仅4个原子机制 */
│   ├── include/    /* 7个头文件 */
│   └── src/        /* 13个源文件 */
├── coreloopthree/  /* 三层认知运行时 */
├── memoryrovol/    /* 四层记忆系统 */
├── syscall/        /* 系统调用接口 */
├── agentos/cupolas/          /* 安全穹顶 */
└── utils/          /* 通用工具 */
```

#### 2.1.2 API 版本管理

所有公共头文件现在包含 API 版本宏定义：

```c
#include "cognition.h"

/**
 * @brief 检查 API 版本兼容性
 * @note 迁移时必须添加此检查，确保编译时版本一致性
 */
#if COGNITION_API_VERSION_MAJOR != 1
    #error "Incompatible cognition API version"
#endif
```

#### 2.1.3 符号导出管理

引入了 `AGENTOS_API` 宏用于精确控制符号可见性：

```c
/* 所有公共函数声明 */
AGENTOS_API agentos_error_t
agentos_cognition_create(const agentos_config_t* manager,
                         agentos_cognition_engine_t** out_engine);

/* CMake 自动处理 AGENTOS_BUILDING_SHARED 宏 */
```

#### 2.1.4 统一错误处理

错误码使用 16 位十六进制格式，按模块分类：

```c
/* 旧版本：裸整数错误码 */
if (result < 0) {
    printf("Error: %d\n", result);
    return result;
}

/* v1.0：结构化错误码 */
agentos_error_t err = agentos_cognition_create(NULL, &engine);
if (err != AGENTOS_SUCCESS) {
    AGENTOS_LOG_ERROR("cognition_init",
        "Failed to create engine: %s (code=0x%04X)",
        agentos_strerror(err), err);
    return err;
}
```

### 2.2 迁移步骤

#### 步骤 1：更新包含路径

```cmake
# v0.x
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../core/include")

# v1.0 - 按层引入
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/../agentos/atoms/corekern/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../agentos/atoms/coreloopthree/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../agentos/atoms/memoryrovol/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../agentos/atoms/syscall/include"
)
```

#### 步骤 2：更新资源管理

v1.0 引入了严格的资源所有权模型，每个资源都有明确的生命周期：

```c
/**
 * @brief 资源创建与释放的标准范式
 * @note 遵循 RAII 原则：创建即绑定，作用域即释放
 */
agentos_cognition_engine_t* engine = NULL;
agentos_error_t err = agentos_cognition_create(NULL, &engine);
if (err != AGENTOS_SUCCESS) {
    return err;
}

/* 使用引擎... */

/* 明确释放资源 - 析构顺序与构造顺序相反 */
agentos_cognition_destroy(engine);
```

#### 步骤 3：迁移到新的系统调用接口

v1.0 将所有内核交互统一为 `agentos_syscall_invoke()` 入口：

```c
/* v0.x：直接调用内核函数 */
agentos_task_submit("process data", &task_id);

/* v1.0：通过系统调用接口 */
agentos_syscall_req_t req = {
    .category = AGENTOS_SYS_TASK,
    .action   = AGENTOS_SYS_TASK_SUBMIT,
    .payload  = &(agentos_task_submit_args_t){
        .description = "process data",
    }
};
agentos_syscall_rsp_t rsp;
agentos_error_t err = agentos_syscall_invoke(&req, &rsp);
```

#### 步骤 4：更新记忆系统接口

v1.0 的 MemoryRovol 采用四层渐进抽象架构：

```c
/**
 * @brief 写入记忆记录到 L1 原始卷
 * @param memory 记忆引擎实例
 * @param record 记忆记录（包含内容、元数据、重要度）
 * @param out_id 输出分配的记录 ID
 * @return AGENTOS_SUCCESS 成功，其他值表示失败
 */
agentos_memory_record_t record = {
    .content     = "Hello, AgentOS!",
    .content_len = 14,
    .metadata    = "{\"type\": \"greeting\"}",
    .importance  = 0.8,
    .source_agent = "agent_001",
    .trace_id    = "trace_123",
};
char* record_id = NULL;
err = agentos_memory_write(memory, &record, &record_id);

/* 查询记忆 - 支持语义检索 */
agentos_memory_query_t query = {
    .text      = "greeting",
    .limit     = 10,
    .threshold = 0.5,
    .layer     = MEMORY_LAYER_ALL,
};
agentos_memory_result_t* result = NULL;
err = agentos_memory_query(memory, &query, &result);

/* 释放结果链 */
agentos_memory_result_free(result);
free(record_id);
```

#### 步骤 5：更新 CMake 配置

```cmake
/* 符号可见性控制 */
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if(BUILD_SHARED_LIBS)
    add_definitions(-DAGENTOS_BUILDING_SHARED)
endif()

/* 链接新架构的库 */
target_link_libraries(my_agent PRIVATE
    agentos_corekern
    agentos_coreloopthree
    agentos_memoryrovol
    agentos_syscall
)
```

### 2.3 迁移验证清单

完成迁移后，使用以下清单逐项验证：

| 验证项 | 检查方法 | 预期结果 |
|--------|----------|----------|
| 编译通过 | `cmake --build build/` | 零错误零警告 |
| 单元测试 | `ctest -R unit --output-on-failure` | 全部通过 |
| 资源泄漏 | `valgrind --leak-check=full` | zero leaks |
| IPC 通信 | `ctest -R integration --label-regex ipc` | 延迟 < 10μs |
| 记忆读写 | `ctest -R integration --label-regex memory` | 10k+ entries/sec |
| 系统调用 | `ctest -R integration --label-regex syscall` | 全部通过 |
| 日志输出 | 检查 `agentos/heapstore/logs/` | 格式正确、trace_id 连通 |

---

## 3. 从 v1.x 迁移到 v1.y

所有 v1.x 版本都是向后兼容的，可直接升级。以下是各版本间的增量变更：

### 3.1 v1.0 → v1.1

| 变更类型 | 内容 | 影响 |
|----------|------|------|
| 新增 | `agentos_memory_evolve()` 触发记忆进化 | 可选采用 |
| 改进 | FAISS 向量检索性能优化 30% | 自动生效 |
| 修复 | IPC Binder 连接池泄漏 | 自动生效 |

### 3.2 v1.1 → v1.2

| 变更类型 | 内容 | 影响 |
|----------|------|------|
| 新增 | `agentos_compensation_get_human_queue()` | 可选采用 |
| 改进 | 补偿事务可靠性增强 | 自动生效 |
| 改进 | LRU 缓存命中率提升 15% | 自动生效 |

### 3.3 升级建议

- 始终升级到同一 MAJOR 版本内的最新 PATCH 版本
- 新增 API 为可选采用，不影响现有代码
- 建议在每个 MINOR 版本发布后进行回归测试

---

## 4. 跨 MAJOR 版本迁移策略

当需要从 v1.x 迁移到 v2.x 时，遵循以下系统工程方法论：

### 4.1 迁移流水线

```
评估 → 隔离 → 迁移 → 验证 → 切换 → 监控
  ↑                                      │
  └──────────── 反馈闭环 ─────────────────┘
```

### 4.2 评估阶段

- 审查 CHANGELOG 中的所有破坏性变更
- 运行 `agentos-migrate --analyze` 生成影响分析报告
- 评估迁移复杂度和所需资源

### 4.3 隔离阶段

```bash
# 创建迁移分支
git checkout -b migrate/v2.x

# 使用 Docker 隔离测试环境
docker run -v $(pwd):/workspace agentos/migrate:latest \
    /workspace/scripts/migrate.sh --from=v1.x --to=v2.x
```

### 4.4 验证阶段

```bash
# 运行完整测试套件
ctest --output-on-failure

# 性能基准对比
python scripts/benchmark.py --compare baseline.json migrated.json

# 内存泄漏检查
valgrind --leak-check=full --error-exitcode=1 \
    ./build/bin/agentos_test_integration
```

### 4.5 回滚方案

```bash
# 如果迁移后验证失败，一键回滚
agentos-migrate --rollback --to=v1.x

# 或使用 Git
git checkout main && git branch -D migrate/v2.x
```

---

## 5. 常见迁移问题

### 5.1 编译问题

**问题**: `AGENTOS_API` 未定义

**诊断**: 确认头文件包含路径和 CMake 宏定义

```bash
# 检查 CMake 配置输出中的 AGENTOS_BUILDING_SHARED
cmake --build build/ -- VERBOSE=1 | grep AGENTOS_BUILDING_SHARED
```

**解决**: 确保 `agentos.h` 在所有自定义头文件之前被包含

### 5.2 链接问题

**问题**: 找不到符号 `agentos_some_function`

**诊断**: 检查符号是否使用 `AGENTOS_API` 导出

```bash
# Linux: 检查动态库导出符号
nm -D libagentos_corekern.so | grep agentos_some_function

# macOS: 检查动态库导出符号
nm -gU libagentos_corekern.dylib | grep agentos_some_function
```

### 5.3 运行时问题

**问题**: 资源泄漏导致内存持续增长

**诊断**: 使用 AddressSanitizer 定位泄漏源

```bash
cmake .. -DENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build .
ctest --output-on-failure
```

### 5.4 性能问题

**问题**: 记忆查询延迟升高

**诊断**: 检查 FAISS 索引状态和 LRU 缓存命中率

```bash
# 查看记忆层性能指标
agentos-cli metrics memory --format=json | jq '.layer_l2.latency_p99'
```

---

## 6. 自动化迁移工具

AgentOS 提供自动化迁移辅助工具：

```bash
# 分析迁移影响
agentos-migrate --analyze --from=v0.9 --to=v1.0

# 自动应用标准迁移规则
agentos-migrate --apply --from=v0.9 --to=v1.0

# 生成迁移报告
agentos-migrate --report --output=migration_report.md
```

---

## 7. 版本历史

| 版本 | 日期 | 主要变更 |
|------|------|----------|
| v1.0.0 | 2026-03-21 | 首个生产就绪版本：微内核 + 三层运行时 + 四层记忆 |
| v1.0.1 | 2026-04-01 | 修复内存泄漏，改进错误处理 |
| v1.1.0 | 2026-05-01 | 新增记忆进化功能，FAISS 检索优化 30% |
| v1.2.0 | 2026-06-01 | 新增人工介入补偿队列，LRU 缓存优化 |

---

## 相关文档

- [架构设计原则](../architecture/ARCHITECTURAL_PRINCIPLES.md) - 理解版本演进的设计动机
- [系统调用 API](../api/README.md) - v1.0 新的系统调用接口
- [编码规范](../specifications/coding_standard/C_coding_style_guide.md) - v1.0 编码标准
- [故障排查](troubleshooting.md) - 迁移过程中的问题诊断

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*
