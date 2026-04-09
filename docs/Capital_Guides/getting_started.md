Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 快速入门指南

**版本**: Doc V1.8
**最后更新**: 2026-04-09
**难度**: ⭐ 初学者友好
**预计时间**: 15 分钟
**作者**: Team
  - Zhixian Zhou | Spharx Ltd. zhouzhixian@spharx.cn
  - Liren Wang | Spharx Ltd. wangliren@spharx.cn
  - Chen Zhang | SJTU CSC Lab. yoyoke@sjtu.edu.cn
  - Yunwen Xu | SJTU CSC Lab. willing419@sjtu.edu.cn
  - Daxiang Zhu | IndieBros. zdxever@sina.com

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v1.0.0 | 2026-03-21 | 初始版本，包含环境要求、快速安装和 Hello World 示例 |
| v1.0.1 | 2026-03-22 | 添加了错误处理示例和常见问题解答 |
| v1.0.2 | 2026-03-23 | 更新了配置示例和性能优化建议 |

---

## 📋 目录

1. [理论指导：体系并行论视角](#0-理论指导体系并行论视角)
2. [环境要求](#1-环境要求)
3. [快速安装](#2-快速安装)
4. [Hello World](#3-hello-world-第一个-agent)
5. [下一步学习](#4-下一步学习)

---

## 0. 理论指导：体系并行论视角

在开始使用 AgentOS 之前，了解其背后的 **体系并行论 (Multibody Cybernetic Intelligent System, MCIS)** 理论基础将帮助你更好地理解系统的设计哲学和架构思想。

### MCIS 核心概念快速入门

AgentOS 是基于 **体系并行论 (MCIS)** 理论构建的智能体操作系统。MCIS 理论的核心思想是通过多个正交且协同的 **体 (Body)** 构建复杂的智能系统：

- **认知体 (Cognition Body)** → 负责智能决策、任务规划、意图理解
- **执行体 (Execution Body)** → 负责任务执行、工具调用、环境交互  
- **记忆体 (Memory Body)** → 负责经验存储、知识积累、上下文管理
- **基础体 (Base Body)** → 提供运行时基础设施、通信机制、资源管理
- **可观测体 (Observability Body)** → 负责系统状态监控、日志记录、性能分析

### 五维正交体系的设计美学

AgentOS 的设计遵循 **五维正交体系**，确保系统在不同维度上的正交分离与协同统一：

1. **系统观维度** → 系统工程层次分解，关注系统整体结构与组件关系
2. **内核观维度** → 微内核架构与形式化验证，关注系统安全性与可靠性
3. **认知观维度** → 双系统认知理论与智能决策，关注智能体认知过程
4. **工程观维度** → 性能优化与极致细节，关注系统实现质量与效率
5. **设计美学维度** → 简约至上与用户体验，关注系统易用性与美感

### 快速入门的理论映射

作为 AgentOS 的初学者，你将接触到的核心组件对应 MCIS 中的不同 **体**：

- **Agent（智能体）** → 完整的 MCIS 系统实例，包含认知体、执行体、记忆体的协同工作
- **Skill（技能）** → 执行体的具体能力实现，完成特定任务
- **Memory（记忆）** → 记忆体的具体实现，存储智能体经验与知识
- **CoreLoopThree（核心三层循环）** → 认知体、执行体、记忆体的运行时协同机制

通过本快速入门指南，你将在实践中体验 MCIS 理论的具体实现，为后续深入学习和开发奠定坚实的理论基础。

---

## 1. 环境要求

### 1.1 硬件要求

| 组件 | 最低配置 | 推荐配置 |
| :--- | :--- | :--- |
| **CPU** | 4 核 2.0 GHz | 8 核 3.0 GHz+ |
<!-- From data intelligence emerges. by spharx -->
| **内存** | 8 GB | 16 GB+ |
| **存储** | 10 GB SSD | 50 GB NVMe SSD |
| **网络** | 10 Mbps | 100 Mbps+ |

### 1.2 软件依赖

**必需依赖**:
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    pkg-manager \
    python3 \
    python3-pip

# macOS
xcode-select --install
brew install cmake openssl python3
```

**编译器版本要求**:
- GCC 11+ 或 Clang 14+
- CMake 3.20+
- Python 3.8+

### 1.3 可选依赖

```bash
# FAISS GPU 加速（可选）
sudo apt-get install -y nvidia-cuda-toolkit

# Docker 容器化（可选）
curl -fsSL https://get.docker.com | bash
```

---

## 2. 快速安装

### 2.1 克隆项目

```bash
git clone https://github.com/spharx/agentos.git
cd agentos
```

### 2.2 构建项目

```bash
# 创建构建目录
mkdir -p build && cd build

# 配置（Release 模式）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译（使用所有 CPU 核心）
make -j$(nproc)

# 安装（可选）
sudo make install
```

**构建选项**:
```bash
# 启用调试模式
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 启用测试
cmake .. -DAGENTOS_BUILD_TESTS=ON

# 禁用 GPU 加速
cmake .. -DAGENTOS_USE_CUDA=OFF
```

### 2.3 初始化配置

```bash
# 返回项目根目录
cd ..

# 复制配置文件
cp agentos/manager/manager.example.yaml agentos/manager/manager.yaml

# 编辑配置（可选）
vim agentos/manager/manager.yaml
```

**关键配置项**:
```yaml
# agentos/manager/manager.yaml
kernel:
  log_level: INFO          # 日志级别
  max_workers: 8           # 最大工作线程数
  
memory:
  faiss_index_type: IVF1024,PQ64  # FAISS 索引类型
  cache_size: 100000       # LRU 缓存大小
  
services:
  llm_d:
    enabled: true
    default_provider: openai
  tool_d:
    enabled: true
    python_path: /usr/bin/python3
```

---

## 3. Hello World：第一个 Agent

### 3.0 理论视角：从 Hello World 理解 MCIS

在创建你的第一个 Agent 之前，让我们从 **体系并行论 (MCIS)** 的视角理解 Hello World 示例所展示的核心概念：

**MCIS 视角的 Hello World 流程**：
1. **初始化 AgentOS 系统** → 启动 **基础体 (Base Body)**，为智能体提供运行时环境
2. **创建会话** → 建立 **认知体 (Cognition Body)** 的上下文环境，管理智能体交互状态
3. **提交认知任务** → **认知体** 处理输入信息，进行意图理解与任务规划
4. **等待任务完成** → **执行体 (Execution Body)** 执行具体操作，**记忆体 (Memory Body)** 记录执行过程
5. **清理资源** → 系统资源的优雅释放，体现 **控制论负反馈** 的闭环管理

**五维正交体系的具体体现**：
- **系统观维度** → 会话管理、任务调度、资源清理的系统工程层次分解
- **内核观维度** → 系统调用接口（sys_session_create, sys_task_submit）的安全性与稳定性
- **认知观维度** → 认知任务的提交与处理，体验智能体的决策过程
- **工程观维度** → 错误处理、资源管理、性能优化的工程实践
- **设计美学维度** → 简洁的API设计、清晰的代码结构、友好的用户体验

通过这个简单的 Hello World 示例，你将实际体验 MCIS 理论在 AgentOS 中的具体实现，理解智能体系统的基本工作原理。

### 3.1 创建项目结构

```bash
mkdir -p my_first_agent
cd my_first_agent

# 创建目录结构
mkdir -p src manager
```

### 3.2 编写 Agent 代码

**C 语言版本**:
```c
// src/my_agent.c
#include "agentos.h"
#include <stdio.h>

int main(int argc, char* argv[]) {
    // 1. 初始化 AgentOS
    agentos_error_t err = agentos_init();
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Failed to initialize AgentOS: %s\n", agentos_strerror(err));
        return 1;
    }
    
    printf("🚀 AgentOS initialized successfully!\n");
    
    // 2. 创建会话
    char* session_id;
    err = sys_session_create("my_session", &session_id);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Failed to create session: %s\n", agentos_strerror(err));
        return 1;
    }
    printf("✅ Session created: %s\n", session_id);
    
    // 3. 提交认知任务
    const char* task_json = 
        "{"
        "\"type\": \"cognition\","
        "\"input\": \"你好，世界！\","
        "\"priority\": 2"
        "}";
    
    char* task_id;
    err = sys_task_submit(task_json, &task_id);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Failed to submit task: %s\n", agentos_strerror(err));
        free(session_id);
        return 1;
    }
    printf("✅ Task submitted: %s\n", task_id);
    
    // 4. 等待任务完成
    char* result;
    err = sys_task_wait(task_id, task_json, 5000, &result);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Task failed: %s\n", agentos_strerror(err));
        free(session_id);
        free(task_id);
        return 1;
    }
    printf("✅ Task completed: %s\n", result);
    
    // 5. 写入记忆
    err = sys_memory_write(session_id, task_json, result);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Failed to write memory: %s\n", agentos_strerror(err));
        free(session_id);
        free(task_id);
        free(result);
        return 1;
    }
    printf("✅ Memory written\n");
    
    // 6. 清理资源
    free(session_id);
    free(task_id);
    free(result);
    agentos_shutdown();
    
    printf("🎉 Hello World from AgentOS!\n");
    return 0;
}
```

**Python 版本** (更简洁):
```python
#!/usr/bin/env python3
# src/my_agent.py

from agentos import AgentOS, AgentOSLogger

def main():
    # 1. 初始化
    os = AgentOS()
    logger = AgentOSLogger("my_agent")
    logger.info("🚀 AgentOS initialized!")
    
    # 2. 创建会话
    session = os.session_create("my_session")
    logger.info(f"✅ Session created: {session.id}")
    
    # 3. 提交任务
    task = os.task_submit(
        task_type="cognition",
        input="你好，世界！",
        priority=2
    )
    logger.info(f"✅ Task submitted: {task.id}")
    
    # 4. 等待完成
    result = task.wait(timeout=5.0)
    logger.info(f"✅ Task completed: {result.output}")
    
    # 5. 写入记忆
    session.memory_write(task.input, result.output)
    logger.info("✅ Memory written")
    
    # 6. 清理
    os.shutdown()
    logger.info("🎉 Hello World from AgentOS!")

if __name__ == "__main__":
    main()
```

### 3.3 编译和运行

**C 语言版本**:
```bash
# 编译
gcc -o my_agent src/my_agent.c \
    -I/path/to/agentos/include \
    -L/path/to/agentos/lib \
    -lagentos -lpthread

# 设置库路径
export LD_LIBRARY_PATH=/path/to/agentos/lib:$LD_LIBRARY_PATH

# 运行
./my_agent
```

**Python 版本**:
```bash
# 安装 Python SDK
pip install -e agentos/toolkit/python/agentos

# 运行
python3 src/my_agent.py
```

### 3.4 预期输出

```log
2026-03-21 10:30:45.123 [INFO] [my_agent] [trace=abc123] 🚀 AgentOS initialized!
2026-03-21 10:30:45.234 [INFO] [my_agent] [trace=abc123] ✅ Session created: sess_xyz789
2026-03-21 10:30:45.345 [INFO] [my_agent] [trace=abc123] ✅ Task submitted: task_def456
2026-03-21 10:30:46.456 [INFO] [my_agent] [trace=abc123] ✅ Task completed: 你好！有什么可以帮助你的吗？
2026-03-21 10:30:46.567 [INFO] [my_agent] [trace=abc123] ✅ Memory written
2026-03-21 10:30:46.678 [INFO] [my_agent] [trace=abc123] 🎉 Hello World from AgentOS!
```

---

## 4. 下一步学习

恭喜你完成了第一个 Agent！接下来可以学习：

### 4.1 深入学习路径

1. **[创建 Agent 指南](create_agent.md)**:
   - 设计复杂的 Agent 架构
   - 实现自定义技能
   - 集成外部 API

2. **[创建技能指南](create_skill.md)**:
   - 编写 Python 技能
   - 用 Go/Rust实现高性能技能
   - 技能打包和分发

3. **[内核调优指南](kernel_tuning.md)**:
   - 优化 FAISS 索引参数
   - 调整线程池大小
   - 性能监控和分析

4. **[部署指南](deployment.md)**:
   - Docker 容器化部署
   - Kubernetes 集群管理
   - 生产环境配置

### 4.2 示例项目

查看官方示例仓库获取更多灵感：

```bash
# 克隆示例仓库
git clone https://github.com/spharx/agentos-examples.git

# 浏览示例
cd agentos-examples
ls -la

# 运行示例
cd examples/chatbot
python3 main.py
```

### 4.3 获取帮助

- **官方文档**: https://docs.agentos.io
- **GitHub Issues**: https://github.com/spharx/agentos/issues
- **Discord 社区**: https://discord.gg/agentos
- **Stack Overflow**: 标签 `agentos`

---

## 📝 常见问题 FAQ

### Q1: 构建失败，提示找不到头文件

**A**: 确保已正确设置 include 路径：
```bash
export CPATH=/path/to/agentos/include:$CPATH
```

### Q2: 运行时提示找不到共享库

**A**: 设置库路径：
```bash
export LD_LIBRARY_PATH=/path/to/agentos/lib:$LD_LIBRARY_PATH
```

### Q3: Python 导入失败

**A**: 确保已安装 SDK：
```bash
pip install -e agentos/toolkit/python/agentos
```

### Q4: 如何启用 GPU 加速？

**A**: 需要 NVIDIA GPU 和 CUDA Toolkit：
```bash
cmake .. -DAGENTOS_USE_CUDA=ON
```

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*From data intelligence emerges*

</div>
