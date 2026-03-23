# AgentOS Scripts 模块

## 概述

AgentOS Scripts 是 AgentOS 项目的命令行工具集，提供构建、安装、部署、运维等完整工具链。

## 目录结构

```
scripts/
├── core/                      # 核心框架
│   ├── __init__.py           # 核心模块入口
│   ├── plugin.py             # 插件系统
│   ├── events.py             # 事件总线
│   ├── security.py            # 安全模块
│   ├── telemetry.py           # 遥测模块
│   ├── config.py             # 配置引擎
│   └── cli.py                # 交互式CLI
├── lib/                       # 共享库
│   ├── common.sh             # 通用工具
│   ├── error.sh             # 错误码
│   ├── log.sh               # 日志系统
│   └── platform.sh           # 平台检测
├── build/                     # 构建脚本
├── ops/                       # 运维工具
├── dev/                       # 开发工具
├── init/                      # 初始化工具
├── deploy/                    # 部署脚本
├── templates/                 # 配置模板
└── tests/                     # 测试套件
    ├── shell/                # Shell测试
    └── python/              # Python测试
```

## 快速开始

### 系统要求

- **Linux/macOS**: Bash 4.0+, CMake 3.20+
- **Windows**: PowerShell 5.0+ 或 WSL
- **Python**: 3.9+ (用于工具脚本)

### 安装依赖

```bash
# Python 依赖
pip install pytest pytest-cov pytest-asyncio

# Shell 测试框架 (可选)
brew install bats-core    # macOS
apt-get install bats      # Ubuntu/Debian
```

### 运行测试

```bash
# 运行所有测试
pytest scripts/tests/python/

# 运行 Shell 测试
bash scripts/tests/shell/test_common_utils.sh

# CI/CD 验证
bash .github/workflows/scripts-ci.yml
```

## 核心框架

### 插件系统

```python
from scripts.core.plugin import PluginRegistry, Plugin

class MyPlugin(Plugin):
    metadata = PluginMetadata(name="my_plugin", version="1.0.0")

    def initialize(self, config):
        return True

    def execute(self, ctx):
        return PluginResult(plugin_id=self.name, success=True)

# 使用
registry = PluginRegistry()
registry.register(MyPlugin())
registry.execute_plugin("my_plugin")
```

### 事件总线

```python
from scripts.core.events import EventBus, Event, EventType

bus = EventBus()
bus.publish(Event(type=EventType.BUILD_COMPLETED, source="build"))

for event in bus.get_history():
    print(event.type.value)
```

### 安全模块

```python
from scripts.core.security import SecurityManager

manager = SecurityManager()
result = manager.validate_path("/tmp/agentos_test")
if result.valid:
    print("Path is safe")
```

## 开发指南

### 添加新脚本

1. 在对应目录创建脚本
2. 添加版权声明和帮助信息
3. 使用 `lib/` 中的共享函数
4. 添加单元测试

### 代码风格

- Shell: `set -euo pipefail`
- Python: 类型注解 + docstring
- 错误码: 统一错误码体系

## 许可证

Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.