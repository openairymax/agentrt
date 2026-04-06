Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 已知问题与解决方案

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 活跃维护

---

## 📋 问题分类索引

| 类别 | 影响程度 | 数量 | 最后更新 |
|------|----------|------|----------|
| 🔴 严重 (Critical) | 生产不可用 | 0 | - |
| 🟠 高 (High) | 功能受限 | 2 | 2026-04-05 |
| 🟡 中 (Medium) | 性能下降 | 5 | 2026-04-06 |
| 🔵 低 (Low) | 小问题/优化 | 8 | 2026-04-06 |
| ✅ 已解决 | 已修复 | 12 | - |

---

## 🟠 高优先级问题

### ISSUE-001: PostgreSQL 连接池耗尽导致服务拒绝

**影响范围**: 所有依赖数据库的服务（kernel, daemon, openlab）

**症状**:
```
FATAL: remaining connection slots are reserved for non-replication superuser connections
ERROR: could not accept new connection: pool exhausted
```

**根本原因**:
- 默认 `max_connections = 100` 在高并发场景下不足
- 连接泄漏：部分代码未正确关闭连接
- 长事务占用连接不释放

**临时解决方案**:

```sql
-- 1. 增加最大连接数（立即生效）
ALTER SYSTEM SET max_connections = 500;
SELECT pg_reload_conf();

-- 2. 终止空闲时间过长的连接
SELECT pg_terminate_backend(pid)
FROM pg_stat_activity
WHERE state = 'idle'
  AND query_start < NOW() - INTERVAL '30 minutes'
  AND pid != pg_backend_pid();

-- 3. 监控连接使用情况
SELECT state, count(*) FROM pg_stat_activity GROUP BY state;
```

**永久解决方案**:

1. **应用层**: 使用连接池（如 SQLAlchemy Pool, PgBouncer）
   ```python
   # Python 示例
   engine = create_engine(
       "postgresql://agentos:password@localhost/agentos",
       pool_size=20,
       max_overflow=10,
       pool_timeout=30,
       pool_recycle=3600,
       pool_pre_ping=True  # 自动检测断开连接
   )
   ```

2. **中间件层**: 部署 PgBouncer 连接池代理
   ```ini
   # pgbouncer.ini
   [databases]
   agentos = host=localhost port=5432 dbname=agentos

   [pgbouncer]
   pool_mode = transaction
   default_pool_size = 25
   max_client_conn = 1000
   reserve_pool_size = 5
   reserve_pool_timeout = 3
   ```

3. **代码审查**: 确保所有数据库操作使用 `with` 上下文管理器

**预计修复版本**: v1.1.0
**相关 PR**: #1234, #1235

---

### ISSUE-002: FAISS 索引在向量数 >100万时查询延迟飙升

**影响范围**: L2 记忆检索性能

**症状**:
- L2 查询 P99 延迟从 <10ms 增加到 >100ms
- CPU 使用率显著上升
- 内存占用增加

**根本原因**:
- IVF 索引的 `nprobe` 参数未随数据量动态调整
- 未使用 HNSW 或 PQ 量化优化

**临时解决方案**:

```python
# 动态调整 nprobe 参数
def adaptive_nprobe(index_size):
    """根据索引大小自适应调整探测数"""
    if index_size < 100000:
        return int(index_size ** 0.25)
    elif index_size < 1000000:
        return 256
    else:
        return min(512, int(index_size ** 0.15))

index.nprobe = adaptive_nprobe(index.ntotal)
```

**永久解决方案**:

1. 升级到 **HNSW + IVFPQ 混合索引**
2. 实现自动索引重建和参数调优机制
3. 引入近似搜索精度与速度权衡配置

**预计修复版本**: v1.0.7
**相关 PR**: #1245

---

## 🟡 中等优先级问题

### ISSUE-003: Docker 容器内存泄漏

**影响**: 长时间运行的容器 OOM 重启

**症状**:
```
containerd[12345]: time="..." level=warning msg="OOMKilled container"
```

**原因分析**:
- Python 进程的循环引用未被 GC 回收
- C 扩展模块内存分配问题
- 日志文件无限增长

**解决方案**:

```bash
# 1. 启用容器内存限制和监控
docker update --memory=4g --memory-swap=4g agentos-kernel-1

# 2. 配置日志轮转（在 docker-compose.yml 中）
logging:
  driver: json-file
  options:
    max-size: "50m"
    max-file: "5"

# 3. 应用层添加内存监控和告警
import gc
import resource
import psutil

def memory_monitor(threshold_mb=3000):
    process = psutil.Process()
    mem_info = process.memory_info()
    if mem_info.rss / 1024 / 1024 > threshold_mb:
        gc.collect()
        if process.memory_info().rss / 1024 / 1024 > threshold_mb:
            send_alert(f"Memory usage high: {mem_info.rss/1024/1024:.1f}MB")

# 定期调用
import threading
threading.Timer(60.0, memory_monitor).start()
```

**状态**: 已提供 workaround，v1.0.6 将包含完整修复

---

### ISSUE-004: WebSocket 连接在高并发下不稳定

**影响**: OpenLab 实时交互功能

**症状**:
- 连接频繁断开重连
- 消息丢失或乱序
- 服务端 CPU 100%

**解决方案**:

```yaml
# nginx websocket 优化
location /ws {
    proxy_pass http://backend;
    proxy_http_version 1.1;

    # WebSocket 特定头
    proxy_set_header Upgrade $http_upgrade;
    proxy_set_header Connection "upgrade";

    # 超时设置
    proxy_read_timeout 3600s;
    proxy_send_timeout 3600s;

    # 缓冲区调优
    proxy_buffering off;
}

# Gunicorn worker 调整（gevent 异步模式）
workers: 4
worker_class: gevent
worker_connections: 1000
timeout: 120
keepalive: 5
graceful_timeout: 30
```

**状态**: 已修复于 v1.0.5

---

### ISSUE-005: LLM 流式输出在某些客户端中断

**影响**: 长文本生成场景

**症状**:
- 流式响应中途停止
- 客户端报 `Connection reset by peer`
- 错误码: 502 Bad Gateway

**原因**: Nginx 默认 `proxy_buffering on` 导致缓冲区溢出

**解决方案**:

```nginx
location /api/v1/llm/chat/stream {
    proxy_pass http://llm_d:8001;
    proxy_buffering off;          # 关键：禁用缓冲
    proxy_cache off;
    proxy_read_timeout 300s;      # 增加超时
    chunked_transfer_encoding on;
}
```

**状态**: 已修复于 v1.0.5

---

### ISSUE-006: Redis AOF 重写导致性能抖动

**影响**: 记忆系统写入延迟偶尔尖峰

**症状**:
- 每 15 分钟出现一次写入延迟峰值（>100ms）
- Redis CPU 使用率短暂飙升至 90%+

**原因**: AOF 重写（BGREWRITEAOF）消耗大量 I/O 和 CPU

**解决方案**:

```bash
# redis.conf 优化
auto-aof-rewrite-percentage 200     # 从默认 100 提升到 200（减少重写频率）
auto-aof-rewrite-min-size 512mb      # 最小重写大小提升
aof-rewrite-incremental-fsync yes    # 增量同步减少阻塞
aof-use-rdb-preamble yes             # RDB+AOF 混合持久化

# 或者切换到纯 RDB 持久化（如果可接受少量数据丢失）
save 900 1
save 300 10
save 60 10000
appendonly no
```

**状态**: 已提供配置建议，v1.0.6 将优化默认值

---

### ISSUE-007: 时区处理不一致导致调度任务执行时间错误

**影响**: Cron 定时任务、定时备份

**症状**:
- 任务实际执行时间比预期早/晚若干小时
- 日志显示的时间戳与本地时间不符

**解决方案**:

```python
# 统一使用 UTC 时间存储，显示时转换时区
from datetime import datetime, timezone
import pytz

def get_current_time_utc():
    return datetime.now(timezone.utc)

def to_local_time(utc_dt, tz_name='Asia/Shanghai'):
    tz = pytz.timezone(tz_name)
    return utc_dt.astimezone(tz)

# Docker 环境变量强制设置
TZ=Asia/Shanghai  # 所有容器统一时区
```

**状态**: 已修复于 v1.0.6

---

## 🔵 低优先级问题 & 优化建议

### ISSUE-008 ~ ISSUE-015: （简要列表）

| ID | 问题描述 | 影响 | 建议 | 状态 |
|----|----------|------|------|------|
| 008 | 文档中部分链接失效 | 用户体验 | 运行链接检查脚本 | 待修复 |
| 009 | Grafana 仪表盘加载慢 | 运维效率 | 减少 panel 数量或启用缓存 | 已知限制 |
| 010 | Docker 镜像构建时间过长 | 开发效率 | 使用 BuildKit 缓存 | 已有 workaround |
| 011 | 某些日志级别过于详细 | 磁盘空间 | 生产环境调整为 WARNING | 配置项已提供 |
| 012 | API 响应缺少 CORS 头（某些端点） | 前端集成 | 更新网关配置 | v1.0.6 |
| 013 | 错误消息不够友好（内部错误暴露） | 安全性 | 统一错误处理中间件 | v1.0.7 |
| 014 | Prometheus 标标命名不规范 | 可观测性 | 遵循 Prometheus 命名规范 | v1.0.6 |
| 015 | 部分配置文件示例值与生产环境不符 | 部署风险 | 添加环境标识注释 | 已更新文档 |

---

## ✅ 已解决问题归档

### ARCHIVED-001 ~ ARCHIVED-012

详见 [CHANGELOG](../references/changelog.md) 的 Fixed Bugs 章节。

---

## 🔗 报告新问题

如果您遇到本文档未列出的问题：

1. **检查是否为已知问题**: 搜索本页面关键词
2. **收集诊断信息**:
   ```bash
   # 运行诊断工具
   ./scripts/agentos_diagnose.sh > diagnosis_output.txt

   # 收集关键日志
   journalctl -u agentos-kernel --since "1 hour ago" --no-pager > kernel_logs.txt
   ```
3. **提交 Issue**:
   - GitHub Issues: https://github.com/SpharxTeam/AgentOS/issues/new
   - 邮箱: support@spharx.cn
   - 包含信息：
     - AgentOS 版本 (`agentos --version`)
     - 操作系统 (`uname -a`)
     - 复现步骤
     - 期望行为 vs 实际行为
     - 诊断输出附件

---

## 📊 问题趋势统计

```mermaid
graph LR
    subgraph Q1 2026
        A[Open: 45] --> B[Fixed: 32]
        A --> C[In Progress: 8]
        A --> D[Wontfix: 5]
    end
```

**问题解决率**: 71%
**平均修复周期**: 3.2 天
**P0/P1 平均响应时间**: < 4 小时

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
