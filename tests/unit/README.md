# 单元测试

`tests/unit/`

## 概述

`unit/` 目录是 AgentOS 的单元测试中心，共 **147 个文件**（含 3 个结构文件），涵盖从底层内核到上层框架的全面验证。C 语言使用 CMocka 框架，Python 使用 pytest 框架。

> **版本**：v0.1.0

## 与 agentos/ 模块对应关系

| tests/unit/ 目录 | 对应的 agentos/ 模块 | 测试内容 |
|------------------|---------------------|----------|
| `atoms/corekern/` | `agentos/atoms/corekern/` | 微内核核心（IPC/Binder、内存管理、任务调度、定时器） |
| `atoms/coreloopthree/` | `agentos/atoms/coreloopthree/` | 三环核心运行时（认知环/执行环/学习环） |
| `atoms/memory/` | `agentos/atoms/memory/` | 内置记忆子系统（L1+L2 层） |
| `atoms/syscall/` | `agentos/atoms/syscall/` | 系统调用接口（任务/内存/会话/遥测/Agent） |
| `commons/` | `agentos/commons/` | 统一基础库（平台抽象/日志/配置/内存/同步） |
| `cupolas/` | `agentos/cupolas/` | 安全穹顶层（安全防护/输入清洗/权限管理/审计） |
| `daemon/gateway_d/` | `agentos/daemon/gateway_d/` | API 网关守护进程 |
| `daemon/llm_d/` | `agentos/daemon/llm_d/` | LLM 服务守护进程（多 Provider） |
| `daemon/tool_d/` | `agentos/daemon/tool_d/` | 工具执行守护进程 |
| `daemon/sched_d/` | `agentos/daemon/sched_d/` | 任务调度守护进程 |
| `daemon/market_d/` | `agentos/daemon/market_d/` | 应用市场守护进程 |
| `daemon/monit_d/` | `agentos/daemon/monit_d/` | 监控告警守护进程 |
| `daemon/common/` | `agentos/daemon/common/` | 公共服务库（19 个组件） |
| `heapstore/` | `agentos/heapstore/` | 运行时数据存储（SQLite + 内存后端混合存储） |
| `manager/` | `agentos/manager/` | 统一配置管理中心（JSON Schema + 热重载） |
| `openlab/` | `agentos/openlab/` | 开放生态系统（Apps/Contrib/Markets） |
| `sdk/` | `agentos/toolkit/` | 多语言 SDK（Python/Rust/Go/TypeScript） |
| `toolkit/` | `agentos/toolkit/` | 工具包测试 |
| `gateway/` | `agentos/gateway/` | 协议网关层（HTTP/WS/Stdio → JSON-RPC） |

## 目录结构

```
unit/                              # 共 147 个文件
├── README.md                      # 本文档
├── CMakeLists.txt                 # 单元测试构建入口
├── __init__.py                    # Python 包初始化
├── atoms/                         # Atoms 层单元测试（25 个文件）
│   ├── corekern/                  #   内核核心测试（7 个文件）
│   ├── coreloopthree/             #   双思考系统测试（11 个文件）
│   ├── memory/                    #   内置记忆测试（1 个文件）
│   └── syscall/                   #   系统调用层测试（6 个文件）
├── commons/                       # Commons 层单元测试（19 个文件）
│   ├── unit/                      #   公共工具单元测试（17 个文件）
│   └── integration/               #   公共模块集成测试（2 个文件）
├── cupolas/                       # Cupolas 安全模块测试（10 个文件）
│   ├── benchmark/                 #   安全基准测试
│   ├── fuzz/                      #   模糊测试
│   ├── integration/               #   安全集成测试
│   ├── stress/                    #   压力测试
│   └── unit/                      #   安全单元测试
├── daemon/                        # Daemon 守护进程测试（39 个文件）
│   ├── common/                    #   公共守护进程测试（9 个文件）
│   ├── gateway_d/                 #   网关守护进程测试（5 个文件）
│   ├── llm_d/                     #   LLM 守护进程测试（7 个文件）
│   ├── market_d/                  #   市场守护进程测试（5 个文件）
│   ├── monit_d/                   #   监控守护进程测试（4 个文件）
│   ├── sched_d/                   #   调度守护进程测试
│   └── tool_d/                    #   工具守护进程测试（7 个文件）
├── heapstore/                     # 堆存储测试（13 个文件）
├── manager/                       # 管理器与配置测试（8 个文件）
├── openlab/                       # OpenLab 测试（10 个文件）
├── sdk/                           # SDK 测试（4 个文件）
│   ├── python/                    #   Python SDK 测试
│   └── rust/                      #   Rust SDK 测试
└── toolkit/                       # 工具包测试（10 个文件）
```

## 运行方式

```bash
# 全部单元测试
pytest tests/unit/ -v -m unit

# 按模块运行
pytest tests/unit/atoms/ -v
pytest tests/unit/daemon/ -v
pytest tests/unit/commons/ -v
pytest tests/unit/cupolas/ -v

# C 语言单元测试
cd build && ctest -N | grep test_
```

## 测试覆盖指标

| agentos/ 模块 | 测试文件数 | 测试类型 |
|--------------|-----------|----------|
| `atoms/` | 25 | CMocka + pytest |
| `commons/` | 19 | pytest |
| `cupolas/` | 10 | CMocka + pytest |
| `daemon/` | 39 | CMocka + pytest |
| `heapstore/` | 13 | CMocka + pytest |
| `manager/` | 8 | pytest |
| `openlab/` | 10 | pytest |
| `toolkit/` | 14 | pytest |

---

© 2026 SPHARX Ltd. All Rights Reserved.