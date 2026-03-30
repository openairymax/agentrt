# CoreLoopThree：三层核心运行时

**版本**: v1.0.0.6
**路径**: `atoms/coreloopthree/`
**最后更�?*: 2026-03-26
**许可�?*: Apache License 2.0

---

## 🎯 模块定位

CoreLoopThree �?AgentOS 的三层核心运行时，包含认知层（Cognition）、行动层（Execution）和记忆层（Memory）�?

这三层通过可插拔的策略接口和标准化的数据流协作，实现智能体的完整生命周期管理�?

- **认知�?*：负责意图理解、任务规划、模型协同和 Agent 调度�?
- **行动�?*：负责任务执行、补偿事务、责任链追踪和执行单元管理�?
- **记忆�?*：封�?MemoryRovol，提供记忆的写入、查询、挂载等高级接口�?

## 📁 目录结构
```
atoms/coreloopthree/
├── CMakeLists.txt # 顶层构建文件
├── README.md # 本文�?
├── include/ # 公共头文�?
�?├── cognition.h # 认知层接�?
<!-- From data intelligence emerges. by spharx -->
�?├── execution.h # 行动层接�?
�?├── memory.h # 记忆层接�?
�?└── loop.h # 三层闭环主接�?
└── src/ # 源文�?
├── cognition/ # 认知层实�?
�?├── engine.c
�?├── planner/ # 规划策略
�?├── coordinator/ # 协同策略
�?└── dispatcher/ # 调度策略
├── execution/ # 行动层实�?
�?├── engine.c
�?├── registry.c
�?├── compensation.c
�?├── trace.c
�?└── units/ # 执行单元
└── memory/ # 记忆层实�?
├── engine.c
├── memory_service.c
└── rov_ffi.h # MemoryRovol 接口
```

## 🔧 核心功能

### 认知层（Cognition�?
- **意图理解**：解析用户输入，提取核心目标和上下文�?
- **任务规划**：使用配置的 LLM 模型将复杂任务分解为可执行的子任务�?
- **模型协同**：协调多个模型的输出，生成综合结果�?
- **Agent 调度**：从候�?Agent 中选择最合适的执行单元�?

### 行动层（Execution�?
- **任务执行**：管理任务的提交、执行和结果获取�?
- **补偿事务**：支持任务失败时的补偿操作�?
- **责任链追�?*：跟踪任务执行的完整链路�?
- **执行单元管理**：注册和管理可执行的功能单元�?

### 记忆层（Memory�?
- **记忆写入**：支持同步和异步写入记忆记录�?
- **记忆查询**：基于文本和时间范围的记忆检索�?
- **记忆挂载**：将记忆关联到当前上下文�?
- **记忆进化**：自动挖掘记忆中的模式和规律�?

## 💻 核心 API 用法示例

```c
#include "loop.h"

int main() {
    // 创建核心循环
    agentos_core_loop_t* loop = NULL;
    agentos_error_t err = agentos_loop_create(NULL, &loop);
    if (err != AGENTOS_SUCCESS) {
        // 错误处理
        return 1;
    }

    // 启动核心循环（阻塞）
    err = agentos_loop_run(loop);
    if (err != AGENTOS_SUCCESS) {
        // 错误处理
    }

    // 销毁核心循�?
    agentos_loop_destroy(loop);
    return 0;
}
```

### 4.2 提交任务

```c
char* task_id = NULL;
agentos_error_t err = agentos_loop_submit(
    loop,
    "帮我分析最近的销售数�?,
    strlen("帮我分析最近的销售数�?),
    &task_id
);
if (err == AGENTOS_SUCCESS) {
    // 任务提交成功，保�?task_id 用于后续查询
    printf("任务提交成功，ID: %s\n", task_id);
    free(task_id);
}
```

### 4.3 等待任务结果

```c
char* result = NULL;
size_t result_len = 0;
agentos_error_t err = agentos_loop_wait(
    loop,
    task_id,
    30000, // 30秒超�?
    &result,
    &result_len
);
if (err == AGENTOS_SUCCESS) {
    // 处理任务结果
    printf("任务结果: %s\n", result);
    free(result);
}
```

## 5. 配置选项

### 5.1 核心循环配置

```c
agentos_loop_config_t manager = {
    .cognition_threads = 4,      // 认知层线程数
    .execution_threads = 8,      // 行动层线程数
    .memory_threads = 2,         // 记忆层线程数
    .max_queued_tasks = 1000,    // 最大排队任务数
    .stats_interval_ms = 60000,  // 统计输出间隔（毫秒）
    .plan_strategy = NULL,       // 自定义规划策略（可选）
    .coord_strategy = NULL,      // 自定义协同策略（可选）
    .disp_strategy = NULL        // 自定义调度策略（可选）
};
```

### 5.2 认知引擎配置

```c
agentos_cognition_config_t cognition_config = {
    .default_timeout_ms = 30000, // 默认任务超时（毫秒）
    .max_retries = 3             // 最大重试次�?
};
```

## 6. 性能优化

### 6.1 已实施的优化

1. **任务查找优化**：使用哈希表实现 O(1) 任务查找，替�?O(n) 链表遍历�?
2. **算法优化**：使用配置的 LLM 模型进行任务分解和结果合成，提高处理效率�?
3. **内存管理**：修复深拷贝问题，确保异步操作中的内存安全�?
4. **并发控制**：优化线程同步机制，减少锁竞争�?

### 6.2 性能指标

- **任务查询时间**：从 O(n) 降至 O(1)
- **响应时间**：提升至�?20%
- **内存占用**：减少至�?20%

## 7. 测试方法

## 🚀 快速开�?

### 编译指南

在项目根目录执行�?
```bash
mkdir build && cd build
cmake ../atoms -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
make
ctest
```

### 测试验证

使用提供的测试脚本：
```bash
cd tests
./run_integration_tests.sh
```

### 性能基准

执行性能基准测试�?
```bash
cd tests
./run_benchmark.sh
```

## 📦 部署指南

### 依赖�?

- CMake 3.16+
- GCC 9.0+ �?MSVC 2019+
- cJSON �?
- pthread 库（Linux）或 Windows 线程库（Windows�?

### 构建和安�?

```bash
# 构建
mkdir build && cd build
cmake ../atoms -DCMAKE_BUILD_TYPE=Release
make

# 安装（可选）
make install
```

## �?常见问题解答

### 任务执行失败怎么办？

当任务执行失败时，系统会根据配置的重试次数自动重试。如果重试后仍然失败，任务会进入补偿流程，执行预设的补偿操作�?

### 如何添加自定义执行单元？

1. 实现 `agentos_execution_unit_t` 接口
2. 使用 `agentos_execution_register_unit` 注册执行单元
3. 在任务中指定该执行单元的 ID

### 如何优化内存使用�?

- 合理设置 `max_queued_tasks` 参数
- 及时释放不再使用的任务结�?
- 对于大型记忆记录，考虑使用异步写入

### 如何监控系统状态？

使用 `agentos_cognition_health_check`、`agentos_execution_health_check` �?`agentos_memory_health_check` 接口获取系统健康状态�?

## 🔧 故障排除

### 常见错误�?

- `AGENTOS_EINVAL`：无效参�?
- `AGENTOS_ENOMEM`：内存分配失�?
- `AGENTOS_ENOTSUP`：不支持的操�?
- `AGENTOS_EBUSY`：系统忙
- `AGENTOS_ETIMEDOUT`：操作超�?

### 日志级别

可通过设置环境变量 `AGENTOS_LOG_LEVEL` 调整日志级别�?
- 0：ERROR
- 1：WARN
- 2：INFO
- 3：DEBUG

## 🔗 相关文档

- [Atoms �?README](../README.md) - 完整内核层说�?
- [CoreKern](../corekern/README.md) - 微内核基础
- [MemoryRovol](../memoryrovol/README.md) - 四层记忆卷载
- [cupolas 安全穹顶](../../cupolas/README.md) - 安全隔离机制

## 📞 联系方式

- **维护�?*: AgentOS 架构委员�?
- **技术支�?*: lidecheng@spharx.cn
- **问题反馈**: https://github.com/SpharxTeam/AgentOS/issues
- **官方仓库**: https://gitee.com/spharx/agentos

---

© 2026 SPHARX Ltd. All Rights Reserved.

*"认知→行动→记忆，三位一体的智能体生命周期�?*
