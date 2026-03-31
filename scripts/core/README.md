﻿﻿﻿# AgentOS Scripts Core 模块

## 模块概述

`scripts/core/` 包含 AgentOS Scripts 的核心框架组件，提供插件系统、事件总线、安全模块、遥测等核心功能。

## 组件

### plugin.py - 插件系统

灵活的可扩展插件架构，支持动态加载和执行。

**主要类:**
- `PluginRegistry` - 插件注册表
- `Plugin` - 插件基类
- `PluginContext` - 执行上下文
- `PluginResult` - 执行结果

### events.py - 事件总线

统一的事件处理框架，支持同步/异步事件处理和事件历史。

**主要类:**
- `EventBus` - 事件总线
- `Event` - 事件数据模型
- `EventHandler` - 事件处理器

### security.py - 安全模块

输入验证、路径安全、命令注入防护等安全保障。

**主要类:**
- `SecurityManager` - 安全管理器
- `InputValidator` - 输入验证器
- `ValidationResult` - 验证结果

### telemetry.py - 遥测模块

性能指标收集和监控系统，支持 Prometheus 格式导出。

**主要类:**
- `MetricsCollector` - 指标收集器
- `Timer` - 计时器上下文管理器

### manager.py - 配置引擎

基于 Jinja2 的配置模板渲染，支持多环境配置。

**主要类:**
- `ConfigEngine` - 配置引擎
- `ConfigTemplate` - 配置模板

### cli.py - 交互式 CLI

增强的命令行界面，包括彩色输出、进度条、选择菜单等。

**主要类:**
- `AgentOSCLI` - CLI 主类
- `ProgressBar` - 进度条
- `Spinner` - 旋转指示器
- `Table` - 表格格式化器

## 使用示例

```python
from scripts.core import (
    PluginRegistry,
    EventBus,
    SecurityManager,
    TelemetryCollector,
    AgentOSCLI
)

# 初始化组件
registry = PluginRegistry()
bus = EventBus()
security = SecurityManager()
telemetry = TelemetryCollector()
cli = AgentOSCLI(verbose=True)

# 使用
cli.info("Starting AgentOS...")
telemetry.counter("requests")
```

## 测试

```bash
pytest scripts/tests/python/test_core.py -v
```