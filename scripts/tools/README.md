# 运维工具

`scripts/tools/`

## 概述

`tools/` 目录提供 AgentOS 系统日常运维管理所需的实用工具脚本，包括日志分析、性能诊断、配置管理、数据迁移等操作，帮助运维人员快速定位和解决问题。

## 脚本列表

| 脚本 | 说明 |
|------|------|
| `log_analyzer.sh` | 日志分析工具，支持关键词搜索、时间范围过滤、异常模式识别 |
| `perf_diag.sh` | 性能诊断工具，采集 CPU/内存/磁盘/网络等系统指标 |
| `config_backup.sh` | 配置文件备份与恢复 |
| `data_migrate.sh` | 数据迁移工具，支持 LMDB/SQLite/Redis 间的数据同步 |

## 使用示例

```bash
# 日志分析：按时间范围搜索
./tools/log_analyzer.sh --since "2026-04-01" --until "2026-04-23" --level ERROR

# 性能诊断：输出 JSON 格式报告
./tools/perf_diag.sh --duration 60 --json --output report.json

# 备份配置
./tools/config_backup.sh --output /backup/agentos-config-$(date +%Y%m%d).tar.gz

# 数据迁移
./tools/data_migrate.sh --source lmdb:///var/lib/agentos/data --target sqlite:///var/lib/agentos/export.db
```

## 诊断指标

| 指标 | 来源 | 说明 |
|------|------|------|
| CPU 使用率 | `/proc/stat` | 用户态/内核态/空闲时间占比 |
| 内存使用 | `/proc/meminfo` | 总量/已用/缓存/交换分区 |
| 磁盘 I/O | `iostat` | 读写速率、IOPS、等待时间 |
| 网络流量 | `/proc/net/dev` | 收发字节数、错误包数 |
| 进程状态 | `ps` | 各守护进程的 CPU/内存占用、运行时长 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
