Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 快速入门指南

**版本**: v1.0.0.5  
**最后更新**: 2026-03-21  
**难度**: ⭐ 初学者友好  
**预计时间**: 15 分钟

## 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v1.0.0 | 2026-03-21 | 初始版本，包含环境要求、快速安装和 Hello World 示例 |
| v1.0.1 | 2026-03-22 | 添加了错误处理示例和常见问题解答 |
| v1.0.2 | 2026-03-23 | 更新了配置示例和性能优化建议 |

---

## 📋 目录

1. [环境要求](#1-环境要求)
2. [快速安装](#2-快速安装)
3. [Hello World](#3-hello-world-第一个-agent)
4. [下一步学习](#4-下一步学习)

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
    pkg-config \
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
cp config/config.example.yaml config/config.yaml

# 编辑配置（可选）
vim config/config.yaml
```

**关键配置项**:
```yaml
# config/config.yaml
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

### 3.1 创建项目结构

```bash
mkdir -p my_first_agent
cd my_first_agent

# 创建目录结构
mkdir -p src config
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
pip install -e tools/python/agentos

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
pip install -e tools/python/agentos
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
