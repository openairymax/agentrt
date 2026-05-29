# Python 统一工具包

`scripts/toolkit/`

## 概述

`toolkit/` 模块提供 AgentOS 运维和开发的 Python 工具集合，共 **17 个文件**（含 15 个源模块），包括系统诊断、性能测试、多层记忆管理、Token 统计与预算、配置模板引擎、检查点管理、契约验证、CLI 增强、事件总线、日志、插件系统、安全模块和遥测系统。

> **版本**：v0.1.0 预览版，API 可能随版本迭代发生变化。
> **状态**：🔶 预览版

## 目录结构

```
toolkit/                           # 共 17 个文件
├── README.md                      # 本文档
├── __init__.py                    # 包入口（统一导出所有工具类）
└── src/                           # 工具模块源码（15 个文件）
    ├── __init__.py                # 内部模块入口
    ├── initializer.py             # 配置初始化器（默认配置/完整性验证/环境特定配置/备份恢复）
    ├── doctor.py                  # 系统健康诊断（8 大类别：系统/Python/构建/项目/配置/网络/安全/性能）
    ├── benchmark.py               # 性能基准测试（IPC 延迟/内存分配/上下文切换/调度吞吐/JSON 解析）
    ├── memory_manager.py          # 多层记忆管理器（L1 原始/L2 特征/L3 结构化/L4 模式）
    ├── token_utils.py             # Token 工具集（多策略计数 + 预算分配/追踪/告警）
    ├── config_engine.py           # 配置模板引擎（Jinja2，dev/staging/production/testing）
    ├── checkpoint_manager.py      # 状态检查点管理器（创建/恢复/轮转/JSON 序列化）
    ├── validate_contracts.py      # 接口契约验证（syscall 头文件/配置格式/API 响应合规）
    ├── cli.py                     # 交互式 CLI 增强（彩色输出/进度条/spinner/选择菜单/表格）
    ├── events.py                  # 事件总线系统（同步/异步/优先级/过滤/历史回放/分布式追踪）
    ├── logger.py                  # 日志模块（终端颜色/格式化输出/进度条/spinner/表格渲染）
    ├── plugin.py                  # 插件系统（动态发现/元数据管理/依赖解析/执行隔离）
    ├── security.py                # 安全模块（输入净化/路径检查/注入防护/权限最小化/审计）
    └── telemetry.py               # 遥测系统（实时指标/基准追踪/趋势分析，兼容 Prometheus）
```

## 使用方式

```python
from scripts.toolkit import (
    ConfigInitializer,
    AgentOSDoctor,
    MemoryManager,
    TokenCounter,
    TokenBudget,
    CheckpointManager,
    AgentOSBenchmark,
    ContractValidator,
    ConfigEngine,
    PluginRegistry,
    EventBus,
    SecurityManager,
    MetricsCollector,
)
```

### 常用操作

```python
doctor = AgentOSDoctor()
doctor.run_all()

mm = MemoryManager()
mm.stats()

counter = TokenCounter()
budget = TokenBudget(total=100000)
count = counter.estimate("Hello, world!")
budget.use(count)

initializer = ConfigInitializer()
initializer.init()

validator = ContractValidator()
validator.validate_syscall_headers()
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
