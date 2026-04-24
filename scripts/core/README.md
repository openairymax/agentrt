# 核心功能脚本

`scripts/core/`

## 概述

`core/` 目录包含 AgentOS 系统的核心运维脚本，负责系统的初始化引导、进程生命周期管理、日志轮转清理等基础运维操作，是保障系统稳定运行的基础设施。

## 脚本列表

| 脚本 | 说明 |
|------|------|
| `bootstrap.sh` | 系统初始化引导，检查运行环境并启动核心服务 |
| `service_manager.sh` | 守护进程管理器，支持 start/stop/restart/status 操作 |
| `log_rotator.sh` | 日志轮转与清理，按大小或时间自动归档旧日志 |
| `health_check.sh` | 健康检查脚本，验证各组件运行状态 |

## 使用示例

```bash
# 系统初始化
./core/bootstrap.sh

# 管理守护进程
./core/service_manager.sh start  --name gateway_d
./core/service_manager.sh stop   --name llm_d
./core/service_manager.sh status --name all

# 日志轮转
./core/log_rotator.sh --max-size 100M --keep 7

# 健康检查
./core/health_check.sh --verbose
```

## 功能特性

- **bootstrap.sh**: 运行环境检测、依赖检查、配置文件加载、核心服务按序启动
- **service_manager.sh**: 基于 PID 文件的进程管理、优雅停止（SIGTERM）、强制终止（SIGKILL）
- **log_rotator.sh**: 支持按日志大小（--max-size）和保留天数（--keep）策略清理
- **health_check.sh**: 端口检测、进程存活检测、API 响应检测，支持 JSON 格式输出

---

© 2026 SPHARX Ltd. All Rights Reserved.
