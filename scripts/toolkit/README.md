# Python 工具集

`scripts/toolkit/`

## 概述

`toolkit/` 模块提供 AgentOS 运维和开发的 Python 工具集合，包括系统诊断、性能测试、内存管理、Token 统计、配置管理、契约验证、日志、事件、安全、遥测等功能。该模块整合了原 `core/` 目录的模块，提供统一的 Python 工具入口。

## 模块结构

```
toolkit/
├── __init__.py              # 模块入口
├── doctor.py                # 系统健康检查
├── benchmark.py             # 性能基准测试
├── memory_manager.py        # 内存管理工具
├── token_utils.py           # Token 统计工具
├── config_engine.py         # 配置管理引擎
├── initializer.py           # 初始化工具
├── checkpoint_manager.py    # 状态检查点管理
├── validate_contracts.py    # 契约验证
├── cli.py                   # 命令行接口
├── events.py                # 事件系统
├── logger.py                # 日志模块
├── plugin.py                # 插件管理
├── security.py              # 安全模块
└── telemetry.py             # 遥测模块
```

## 工具说明

### doctor.py - 系统健康检查

检查项：
- Python 环境版本
- 依赖包完整性
- 磁盘空间
- 网络连接
- Docker 状态（如可用）

### benchmark.py - 性能测试

测试项：
- CPU 计算性能
- 内存分配速度
- I/O 读写性能
- JSON 序列化性能

### memory_manager.py - 内存管理

功能：
- 实时内存监控
- 内存泄漏检测
- 垃圾回收触发
- 内存快照

### token_utils.py - Token 工具

功能：
- Token 计数
- Token 预算估算
- 文本编码策略

### config_engine.py - 配置管理

功能：
- 配置文件加载和解析
- 配置项验证
- 多环境配置切换

### checkpoint_manager.py - 检查点管理

功能：
- 运行状态检查点保存
- 检查点恢复
- 增量状态追踪

### validate_contracts.py - 契约验证

功能：
- API 契约一致性验证
- 接口规范检查

### cli.py - 命令行接口

功能：
- toolkit 统一命令行入口
- 子命令分发

### events.py - 事件系统

功能：
- 事件发布和订阅
- 异步事件处理

### logger.py - 日志模块

功能：
- 结构化日志输出
- 多级别日志控制

### plugin.py - 插件管理

功能：
- 插件注册和加载
- 插件生命周期管理

### security.py - 安全模块

功能：
- 权限验证
- 安全策略执行

### telemetry.py - 遥测模块

功能：
- 运行指标采集
- 性能数据上报

## 快速开始

```bash
cd scripts/toolkit

# 运行全面诊断
python doctor.py

# 详细模式
python doctor.py --verbose

# JSON 输出
python doctor.py --json

# 性能测试
python benchmark.py

# 指定测试时长（秒）
python benchmark.py --duration 60

# 内存管理
python memory_manager.py --stats
python memory_manager.py --gc
```

---

© 2026 SPHARX Ltd. All Rights Reserved.
