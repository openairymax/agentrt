# 认知工具模块

**版本**: v1.0.0  
**最后更新**: 2026-04-06  
**许可证**: Apache License 2.0

---

## 🎯 概述

认知工具模块是AgentOS智能体操作系统的核心认知组件，提供多智能体协作中的计划生成、任务调度、结果协调等关键功能。本模块旨在减少认知相关代码的重复，提供统一的认知抽象接口。

## 🏗️ 核心功能

### 1. Agent信息管理

提供Agent性能统计和权重计算能力：

```c
// 初始化Agent信息
agent_info_t agent;
agent_info_init(&agent, "agent-001");

// 更新性能统计
agent_info_update_stats(&agent, true, 150);  // 成功，延迟150ms

// 计算权重（用于调度决策）
double weight = agent_info_calculate_weight(&agent);

// 清理资源
agent_info_cleanup(&agent);
```

**核心数据结构**:
- `agent_info_t` - Agent基本信息与性能指标
- `task_info_t` - 任务描述与优先级
- `plan_result_t` - 计划生成结果
- `dispatch_result_t` - 调度选择结果
- `coordination_result_t` - 多Agent协调结果

### 2. 任务调度与协调

```c
// 选择最佳Agent执行任务
dispatch_result_t dispatch;
cognition_select_best_agent(agents, agent_count, &task, &dispatch);

// 生成执行计划
plan_result_t plan;
cognition_generate_plan(&task, &plan);

// 协调多个Agent的结果
coordination_result_t coord;
cognition_coordinate_results(results, result_count, &coord);
```

### 3. 任务优先级计算

```c
// 基于截止时间和任务类型计算优先级
uint64_t priority = cognition_calculate_task_priority(&task);

// 评估计划质量（0-100分）
int quality = cognition_evaluate_plan_quality(plan_content, &task);
```

## 🔧 主要API接口

| 函数名 | 功能描述 | 复杂度 |
|--------|---------|--------|
| `agent_info_init()` | 初始化Agent信息 | O(1) |
| `agent_info_update_stats()` | 更新性能统计 | O(1) |
| `agent_info_calculate_weight()` | 计算Agent权重 | O(1) |
| `cognition_select_best_agent()` | 选择最佳Agent | O(n) |
| `cognition_generate_plan()` | 生成执行计划 | O(1) |
| `cognition_coordinate_results()` | 协调多Agent结果 | O(n) |
| `cognition_calculate_task_priority()` | 计算任务优先级 | O(1) |
| `cognition_evaluate_plan_quality()` | 评估计划质量 | O(n) |

## 📊 使用示例

### 场景1：多Agent任务分配

```c
#include "cognition_common.h"

int main() {
    // 定义多个Agent
    agent_info_t agents[3];
    agent_info_init(&agents[0], "data-agent");
    agent_info_init(&agents[1], "compute-agent");
    agent_info_init(&agents[2], "storage-agent");
    
    // 模拟历史性能数据
    for (int i = 0; i < 100; i++) {
        agent_info_update_stats(&agents[0], true, 120 + rand() % 50);
        agent_info_update_stats(&agents[1], true, 80 + rand() % 30);
        agent_info_update_stats(&agents[2], true, 200 + rand() % 80);
    }
    
    // 定义任务
    task_info_t task;
    task_info_init(&task, "task-001", "compute", "matrix_multiplication");
    
    // 选择最佳Agent
    dispatch_result_t dispatch;
    if (cognition_select_best_agent(agents, 3, &task, &dispatch) == 0) {
        printf("Selected agent: %s (confidence: %.2f)\n",
               dispatch.selected_agent, dispatch.confidence);
    }
    
    // 清理资源
    task_info_cleanup(&task);
    for (int i = 0; i < 3; i++) {
        agent_info_cleanup(&agents[i]);
    }
    dispatch_result_cleanup(&dispatch);
    
    return 0;
}
```

### 场景2：计划生成与质量评估

```c
// 生成执行计划
plan_result_t plan;
cognition_generate_plan(&task, &plan);

if (plan.success) {
    printf("Generated plan (%zu bytes):\n%s\n", plan.plan_size, plan.plan);
    
    // 评估计划质量
    int quality = cognition_evaluate_plan_quality(plan.plan, &task);
    printf("Plan quality score: %d/100\n", quality);
    
    if (quality >= 80) {
        printf("✅ Plan meets quality threshold\n");
    } else {
        printf("⚠️ Plan needs improvement\n");
    }
}

plan_result_cleanup(&plan);
```

## ⚠️ 注意事项

### 内存管理
- 所有带 `_init()` 后缀的结构体必须配对调用 `_cleanup()` 
- 字符串字段由模块内部管理，调用者不应手动释放
- `plan_result_t.plan` 和错误信息字符串在 cleanup 时自动释放

### 线程安全
- ✅ `agent_info_update_stats()` 是线程安全的（原子操作）
- ⚠️ `cognition_select_best_agent()` 非线程安全，需外部加锁
- ⚠️ 其他函数在单线程环境下使用

### 性能建议
- 对于高频调用的场景，建议缓存 `agent_info_t` 权重值
- 批量更新统计信息比逐次更新效率更高
- 任务优先级计算考虑了时间因素，避免在热路径中频繁调用

## 🔗 依赖关系

### 上游依赖
- **无** - 本模块是基础工具层，不依赖其他commons子模块

### 下游使用者
- **execution模块** - 使用调度结果指导命令执行
- **strategy模块** - 结合加权评分算法优化选择逻辑
- **CoreKern内核** - 多Agent协作的核心调度器

### 外部依赖
- `<stdint.h>` - 整数类型定义
- `<stdbool.h>` - 布尔类型
- `<stddef.h>` - size_t 等类型

## 📈 质量指标

| 指标 | 当前值 | 目标值 |
|------|--------|--------|
| 圈复杂度（平均） | 2.1 | <3.0 |
| 代码重复率 | 0% | <5% |
| Doxygen覆盖 | 100% | >95% |
| 单元测试覆盖 | 待补充 | >90% |

## 🔄 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|---------|
| v1.0.0 | 2026-04-06 | 初始版本，包含完整的Agent管理和任务调度功能 |

---

## 📞 技术支持

如有问题或建议，请提交Issue至项目仓库。

---

**© 2026 SPHARX Ltd. All Rights Reserved.**  
**"From data intelligence emerges."**
