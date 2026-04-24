# 运维工具集

`scripts/toolkit/`

## 概述

`toolkit/` 模块提供 AgentOS 日常运维操作的便捷工具集合，包括系统诊断、性能测试、内存管理、Token 统计、配置管理等功能，帮助运维人员快速掌握系统运行状态。

## 模块结构

```
toolkit/
├── __init__.py              # 模块入口
├── doctor.py                # 系统健康检查
├── benchmark.py             # 性能基准测试
├── memory_manager.py        # 内存管理工具
├── checkpoint_manager.py    # 状态检查点管理
├── token_utils.py           # Token 统计工具
├── config_engine.py         # 配置管理引擎
├── initializer.py           # 初始化工具
└── validate_contracts.py    # 契约验证
```

## 快速开始

### 系统健康检查

```bash
cd scripts/toolkit

# 运行全面诊断
python doctor.py

# 详细模式
python doctor.py --verbose

# JSON 输出
python doctor.py --json
```

### 性能基准测试

```bash
# 运行性能测试
python benchmark.py

# 指定测试时长（秒）
python benchmark.py --duration 60
```

### 内存管理

```bash
# 查看内存使用情况
python memory_manager.py --stats

# 触发垃圾回收
python memory_manager.py --gc
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

---

© 2026 SPHARX Ltd. All Rights Reserved.
