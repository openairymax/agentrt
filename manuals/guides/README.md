Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 使用指南

**版本**: Doc V1.5
**最后更新**: 2026-03-23
**状态**: 生产就绪

---

## 1. 概述

AgentOS 使用指南提供从入门到精通的完整学习路径。所有代码示例基于 **C11 标准**，构建系统为 **CMake**，遵循 AgentOS 微内核架构的设计原则。

### 1.1 学习路径

```
┌───────────────────────────────────────────────┐
│           AgentOS 学习路径                     │
├───────────────────────────────────────────────┤
│  1. 快速开始                                   │
│     getting_started.md                        │
│     环境准备 → 编译构建 → 第一个 Agent         │
├───────────────────────────────────────────────┤
│  2. 开发指南                                   │
│     create_agent.md · create_skill.md         │
│     Agent 生命周期 · Skill 契约 · 双系统路径   │
├───────────────────────────────────────────────┤
│  3. 部署运维                                   │
│     deployment.md                             │
│     单机/集群/Docker/K8s → 监控告警           │
├───────────────────────────────────────────────┤
│  4. 性能调优                                   │
│     kernel_tuning.md                          │
│     反馈闭环调优法 → 各子系统参数 → 场景配置   │
├───────────────────────────────────────────────┤
│  5. 故障排查                                   │
│     troubleshooting.md                        │
│     分层诊断 → 工具链 → 故障速查               │
├───────────────────────────────────────────────┤
│  6. 版本迁移                                   │
│     migration_guide.md                        │
│     v0.x→v1.0 → v1.x→v1.y → 跨 MAJOR 迁移   │
└───────────────────────────────────────────────┘
```

### 1.2 文档索引

| 文档 | 主题 | 难度 | 状态 |
|------|------|------|------|
| [快速开始](folder/getting_started.md) | 环境搭建、编译、首次运行 | 入门 | 生产就绪 |
| [创建 Agent](folder/create_agent.md) | Agent 生命周期、双系统路径、契约定义 | 进阶 | 生产就绪 |
| [创建 Skill](folder/create_skill.md) | Skill 契约、实现模式、管道组合 | 进阶 | 生产就绪 |
| [部署指南](folder/deployment.md) | 多环境部署、Docker、K8s、监控 | 进阶 | 生产就绪 |
| [内核调优](folder/kernel_tuning.md) | 反馈闭环调优法、子系统参数、场景配置 | 高级 | 生产就绪 |
| [故障排查](folder/troubleshooting.md) | 分层诊断、工具链、故障速查表 | 进阶 | 生产就绪 |
| [迁移指南](folder/migration_guide.md) | 版本升级、API 变更、自动化迁移 | 进阶 | 生产就绪 |

---

## 2. 快速开始

### 2.1 系统要求

| 组件 | 最低要求 | 推荐配置 |
|------|----------|----------|
| 操作系统 | Linux 5.4+ / macOS 12+ / Windows 10+ (WSL2) | Ubuntu 22.04 LTS |
| 编译器 | GCC 11+ / Clang 14+ / MSVC 2022 | GCC 13.2 |
| CMake | 3.20+ | 3.28+ |
| 内存 | 4 GB | 8 GB+ |
| 存储 | 10 GB | 20 GB+ |
| 依赖 | libevent, OpenSSL, FAISS | 同左（最新稳定版） |

### 2.2 依赖安装

```bash
# Ubuntu/Debian
sudo apt update && sudo apt install -y \
    build-essential cmake git \
    libevent-dev libssl-dev libfaiss-dev

# macOS (Homebrew)
brew install cmake libevent openssl faiss

# Windows (vcpkg)
vcpkg install libevent openssl
```

### 2.3 编译构建

```bash
git clone https://gitee.com/spharx/agentos.git
cd agentos

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)

# 运行测试
ctest --output-on-failure
```

### 2.4 第一个 Agent

```c
/* hello_agent.c - 第一个 Agent 示例 */
#include <stdio.h>
#include "agentos/agent.h"

int main(void)
{
    agentos_config_t manager = {
        .name        = "hello_agent",
        .description = "My first AgentOS agent",
        .max_tasks   = 16,
    };

    agentos_agent_t* agent = NULL;
    agentos_error_t err = agentos_agent_create(&manager, &agent);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Agent creation failed: 0x%04X\n", err);
        return 1;
    }

    printf("Agent '%s' created successfully.\n", manager.name);
    printf("State: %s\n", agentos_state_name(agentos_agent_state(agent)));

    agentos_agent_destroy(agent);
    return 0;
}
```

### 2.5 核心概念速查

| 概念 | 定义 | 对应模块 |
|------|------|----------|
| **Agent** | 自主执行任务的智能实体，拥有完整生命周期 | coreloopthree/ |
| **Skill** | Agent 的能力单元，遵循 Skill 契约 | gateway/ |
| **Task** | 工作单元，由调度器分配给执行单元 | corekern/ (Task) |
| **Memory** | 四层渐进记忆系统，支持语义检索 | memoryrovol/ |
| **Session** | 交互会话上下文，关联 Agent 和记忆 | syscall/ |
| **TraceID** | 全链路追踪标识，贯穿所有日志 | utils/ (trace) |

### 2.6 架构层次概览

```
┌─────────────────────────────────────────────┐
│              daemon/ 用户态服务                │
│     llm_d · market_d · monit_d · tool_d     │
├─────────────────────────────────────────────┤
│             gateway/ 网关层                  │
│          HTTP · WebSocket · Stdio           │
├─────────────────────────────────────────────┤
│          syscall/ 系统调用接口                │
├─────────────────────────────────────────────┤
│            cupolas/ 安全穹顶                   │
├──────────────┬──────────────────────────────┤
│ corekern/    │  coreloopthree/              │
│ IPC·Mem·Task │  认知→规划→调度→执行          │
│ Time         │                              │
├──────────────┴──────────────────────────────┤
│         memoryrovol/ 四层记忆系统            │
├─────────────────────────────────────────────┤
│             utils/ 公共工具库                │
└─────────────────────────────────────────────┘
```

---

## 3. 命令行工具速查

```bash
# 系统状态
agentos-cli status                  # 全局状态概览
agentos-cli health-check            # 健康检查
agentos-cli version                 # 版本信息

# 微内核层
agentos-cli kernel stats            # 内核统计
agentos-cli kernel ipc-stats        # IPC Binder 统计
agentos-cli kernel thread-stats     # 线程分布

# 三层运行时
agentos-cli cognition stats         # 认知引擎统计
agentos-cli execution stats         # 执行引擎统计
agentos-cli memory metrics          # 记忆系统指标
agentos-cli memory layer-stats --layer=l2  # 特定层统计

# 任务管理
agentos-cli task list               # 列出所有任务
agentos-cli task queue-stats        # 队列统计
agentos-cli task submit "描述"      # 提交任务

# Agent 管理
agentos-cli agent list              # 列出所有 Agent
agentos-cli agent history <id>      # Agent 状态历史
agentos-cli agent pause <id>        # 暂停 Agent
agentos-cli agent stop <id>         # 停止 Agent
agentos-cli agent destroy <id>      # 销毁 Agent

# 安全穹顶
agentos-cli security policy show <id>  # 查看 Agent 安全策略
agentos-cli security audit --last=100  # 审计日志

# 诊断
agentos-cli diagnostics export --output=report.json
```

---

## 4. 配置文件结构

AgentOS 使用 YAML 配置文件，遵循层次化设计：

```yaml
# agentos.yaml - 主配置文件
kernel:
  ipc:
    max_connections: 1024
    connect_timeout_ms: 5000
  task:
    max_workers: 8
    worker_queue_depth: 4096

coreloopthree:
  cognition:
    model_path: "/opt/agentos/models/"
    max_concurrent_inferences: 4
  execution:
    max_concurrent_tasks: 32
    task_timeout_sec: 300
  scheduling:
    strategy: weighted_round_robin

memory:
  l1:
    max_storage_mb: 4096
    auto_gc_interval_sec: 300
  l2:
    faiss:
      index_type: IVF256,PQ32
      nprobe: 16
    cache:
      size: 100000
  forgetting:
    enabled: true
    strategy: ebbinghaus
    decay_rate: 0.1

cupolas:
  workbench:
    enabled: true
    sandbox_type: namespace
  permission:
    default_policy: deny_all
  sanitizer:
    max_input_size_mb: 10

telemetry:
  logging:
    level: INFO
    format: json
    async: true
  metrics:
    enabled: true
    export_interval_sec: 15
  tracing:
    enabled: true
    sampler_rate: 0.1
```

---

## 5. 开发指南

### 5.1 项目结构

```
agentos/
├── atoms/
│   ├── corekern/        # 微内核：7 头文件，13 源文件
│   │   ├── include/     # ipc.h, mem.h, task.h, time.h, ...
│   │   └── src/
│   ├── coreloopthree/   # 三层认知运行时
│   │   ├── include/     # cognition.h, planning.h, execution.h
│   │   └── src/
│   ├── memoryrovol/     # 四层记忆系统
│   │   ├── include/     # memory.h, l1_volume.h, l2_feature.h, ...
│   │   └── src/
│   ├── syscall/         # 系统调用接口
│   ├── cupolas/           # 安全穹顶
│   └── utils/           # 公共工具库
├── daemon/               # 用户态服务（_d 后缀）
│   ├── llm_d/           # LLM 推理服务
│   ├── market_d/        # 技能市场服务
│   ├── monit_d/         # 监控服务
│   └── tool_d/          # 工具调用服务
├── gateway/             # 网关层
├── manager/              # 配置文件
├── lodges/            # 运行时数据
│   ├── logs/            # 日志文件
│   └── memory/          # 记忆持久化
├── tests/               # 测试套件
└── CMakeLists.txt       # 顶层构建配置
```

### 5.2 编码规范要点

- 语言标准：**C11**（严格模式 `-std=c11 -pedantic`）
- 命名约定：`agentos_` 模块前缀 + `snake_case`
- 符号导出：公共 API 使用 `AGENTOS_API` 宏
- 错误处理：统一使用 `agentos_error_t`（16 位十六进制格式）
- 资源管理：创建-使用-释放范式，析构顺序与构造顺序相反
- 线程安全：所有公共 API 必须线程安全
- 日志规范：包含模块名、trace_id、结构化 JSON

### 5.3 错误处理范式

```c
#include "agentos/error.h"

agentos_error_t process_data(agentos_cognition_engine_t* engine,
                              const char* input)
{
    if (!engine || !input) {
        return AGENTOS_ERROR_INVALID_ARGUMENT;
    }

    agentos_error_t err = AGENTOS_SUCCESS;

    err = agentos_cognition_infer(engine, input, NULL);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("process_data",
            "Inference failed: %s (code=0x%04X, trace=%s)",
            agentos_strerror(err), err,
            agentos_trace_current());
        return err;
    }

    return AGENTOS_SUCCESS;
}
```

---

## 6. 部署要点

### 6.1 部署模式选择

| 模式 | 适用场景 | 复杂度 | 可用性 |
|------|----------|--------|--------|
| 开发模式 | 本地开发调试 | 低 | 单节点 |
| 单机生产 | 中小规模部署 | 中 | 单节点 |
| 集群模式 | 高可用、水平扩展 | 高 | 多节点 |
| 嵌入模式 | IoT/边缘计算 | 低 | 单设备 |

### 6.2 Docker 快速部署

```bash
# 构建镜像
docker build -t spharx/agentos:latest .

# 运行容器
docker run -d --name agentos \
    -p 8080:8080 \
    -v /path/to/manager:/etc/agentos \
    -v /path/to/data:/var/lib/agentos \
    spharx/agentos:latest
```

### 6.3 systemd 服务

```ini
[Unit]
Description=AgentOS Core Service
After=network.target

[Service]
Type=notify
ExecStart=/usr/local/bin/agentos-core --manager /etc/agentos/agentos.yaml
Restart=on-failure
RestartSec=5
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

---

## 7. 监控与可观测性

### 7.1 三时间尺度监控

| 时间尺度 | 监控目标 | 典型工具 |
|----------|----------|----------|
| 实时（τ<100ms） | IPC 延迟、任务切换 | perf, eBPF |
| 轮次内（任务周期） | 任务吞吐、记忆读写 | agentos-cli metrics |
| 跨轮次（会话级） | 资源趋势、错误率 | Prometheus + Grafana |

### 7.2 关键告警阈值

| 指标 | Warning | Critical |
|------|---------|----------|
| IPC 延迟 p99 | > 50μs | > 100μs |
| 任务队列积压 | > 500 | > 1000 |
| 记忆写入延迟 p99 | > 50ms | > 100ms |
| 进程 RSS | > 4GB | > 8GB |
| CPU 使用率 | > 80% (5min) | > 90% (5min) |

---

## 8. 常见问题速查

| 问题 | 快速诊断 | 详见 |
|------|----------|------|
| 编译失败 | 检查依赖、CMake 版本 | [故障排查](folder/troubleshooting.md#2-构建问题) |
| 库加载失败 | `ldconfig -p \| grep agentos` | [故障排查](folder/troubleshooting.md#31-共享库加载失败) |
| IPC 超时 | `agentos-cli kernel ipc-stats` | [故障排查](folder/troubleshooting.md#32-ipc-通信超时) |
| 检索延迟高 | `agentos-cli memory index-status` | [故障排查](folder/troubleshooting.md#41-向量检索延迟升高) |
| 内存增长 | `valgrind --leak-check=full` | [故障排查](folder/troubleshooting.md#42-内存占用持续增长) |
| 版本迁移 | `agentos-migrate --analyze` | [迁移指南](folder/migration_guide.md) |

---

## 相关文档

- [架构设计原则](../architecture/folder/architectural_design_principles.md) - 系统设计的理论基础
- [设计哲学](../philosophy/README.md) - 认知理论、记忆理论、设计原则
- [API 参考](../api/README.md) - SDK / Syscall / Core 三层 API
- [规范标准](../specifications/README.md) - 契约规范、编码标准、术语表

---

© 2026 SPHARX Ltd. All Rights Reserved.
*"From data intelligence emerges."*
