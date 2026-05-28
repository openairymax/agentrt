# Python 工具集

`scripts/toolkit/`

## 概述

`toolkit/` 模块提供 AgentOS 运维和开发的 Python 工具集合，包括系统诊断、性能测试、内存管理、Token 统计、配置管理、日志、事件、安全、遥测等功能。

## 目录结构

```
toolkit/
├── README.md
├── __init__.py              # 包入口（公开 API）
└── src/
    ├── __init__.py          # 内部模块入口
    ├── initializer.py       # 环境初始化
    ├── doctor.py            # 系统健康检查
    ├── benchmark.py         # 性能基准测试
    ├── memory_manager.py    # 内存管理工具
    ├── token_utils.py       # Token 统计工具
    ├── config_engine.py     # 配置管理引擎
    ├── checkpoint_manager.py # 状态检查点管理
    ├── validate_contracts.py # 契约验证
    ├── cli.py               # 命令行接口
    ├── events.py            # 事件系统
    ├── logger.py            # 日志模块
    ├── plugin.py            # 插件管理
    ├── security.py          # 安全模块
    └── telemetry.py         # 遥测模块
```

## 使用方式

```python
from scripts.toolkit import (
    ConfigInitializer,
    AgentOSDoctor,
    MemoryManager,
    TokenCounter,
    PluginRegistry,
    EventBus,
    SecurityManager,
    MetricsCollector,
)
```