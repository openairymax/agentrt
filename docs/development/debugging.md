Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 调试指南

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪

---

## 📋 概述

本指南提供 AgentOS 系统的调试技巧、工具使用和常见问题诊断方法。

---

## 🔧 调试工具箱

### 1. 日志系统

AgentOS 采用**统一日志规范**，所有组件输出结构化日志：

#### 日志格式

```json
{
    "timestamp": "2026-04-06T10:00:00.123Z",
    "level": "INFO",
    "logger": "agentos.kernel.ipc",
    "message": "IPC call completed",
    "context": {
        "call_id": "call-abc123",
        "method": "memory.store",
        "duration_ms": 0.85,
        "caller_ip": "127.0.0.1"
    },
    "trace_id": "trace-xyz789",
    "span_id": "span-def012"
}
```

#### 日志级别

| 级别 | 用途 | 示例 |
|------|------|------|
| **DEBUG** | 详细调试信息 | 函数入口/出口、变量值 |
| **INFO** | 一般信息 | 请求处理、状态变更 |
| **WARNING** | 警告信息 | 性能降级、重试操作 |
| **ERROR** | 错误信息 | 处理失败、异常捕获 |
| **CRITICAL** | 严重错误 | 系统崩溃、数据损坏 |

#### 查看实时日志

```bash
# 内核服务日志
journalctl -u agentos-kernel -f --since "5 min ago"

# Docker 容器日志
docker logs -f agentos-kernel-1 --tail 100

# 过滤特定级别
docker logs agentos-kernel-1 2>&1 | grep ERROR

# 使用 jq 解析 JSON 日志
docker logs agentos-kernel-1 2>&1 | jq 'select(.level == "ERROR")'
```

### 2. 性能分析工具

#### GDB 调试 (C/C++ 内核)

```bash
# 启动 GDB 调试内核
gdb ./bin/agentos-kernel

(gdb) set pagination off
(gdb) run --config /path/to/config.yaml

# 设置断点
(gdb) break agentos_ipc_call
(gdb) break agentos_memory_store

# 执行到断点
(gdb) continue

# 查看调用栈
(gdb) bt
(gbt) bt full  # 包含局部变量

# 查看变量
(gdb) print *request
(gdb) print response->status_code

# 单步执行
(gdb) next      # 单步（不进入函数）
(gdb) step      # 单步（进入函数）
(gdb) finish    # 跳出当前函数

# 监视点（数据断点）
(gdb) watch response->error_code
(gdb) continue  # 当 error_code 变化时停止
```

#### Valgrind 内存检测

```bash
# 检测内存泄漏
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./bin/agentos-kernel --config config.yaml

# 检测内存访问错误
valgrind --tool=memcheck \
         ./bin/agentos-kernel

# 性能剖析（Callgrind）
valgrind --tool=callgrind ./bin/agentos-kernel
callgrind_annotate callgrind.out.*
```

#### Python 调试 (守护进程)

```python
# 使用 pdb 断点调试
import pdb; pdb.set_trace()

# 使用 ipdb（增强版 pdb）
import ipdb; ipdb.set_trace()

# 远程调试（使用 remote-pdb）
from remote_pdb import RemotePdb
RemotePdb('0.0.0.0', 4444).set_trace()

# 连接到远程调试会话
# 在另一个终端: telnet localhost 4444
```

```bash
# 使用 cProfile 进行性能分析
python -m cProfile -s cumtime your_script.py

# 使用 py-spy 进行无侵入性能分析
py-spy top --pid <PID>
py-spy record -o profile.svg --pid <PID>
```

### 3. 网络调试工具

```bash
# 抓取 IPC 通信包
tcpdump -i lo port 8080 -w ipc_capture.pcap

# 分析抓包文件
tcpdump -r ipc_capture.pcap -A

# 使用 Wireshark GUI 分析
wireshark ipc_capture.pcap

# 测试 API 端点
curl -v http://localhost:8080/health

# 压力测试
ab -n 10000 -c 100 http://localhost:8080/api/v1/ping

# WebSocket 调试
wscat -c ws://localhost:8080/ws
```

### 4. 数据库调试

```bash
# PostgreSQL
psql -U agentos -d agentos

# 查看活跃连接
SELECT * FROM pg_stat_activity WHERE datname = 'agentos';

# 查看慢查询
SELECT query, calls, total_time, mean_time
FROM pg_stat_statements
ORDER BY total_time DESC LIMIT 20;

# 分析查询计划
EXPLAIN ANALYZE SELECT * FROM memory_records WHERE id = 'xxx';

# Redis
redis-cli

# 查看键空间
INFO keyspace

# 监控命令
MONITOR  # 实时查看所有命令

# 慢日志
CONFIG GET slowlog-log-slower-than
SLOWLOG GET 10
```

---

## 🐛 常见问题诊断流程

### 问题分类与定位

```
用户报告问题
    ↓
┌─────────────────┬─────────────────┬─────────────────┐
│   网络层问题     │   应用层问题     │   数据层问题     │
├─────────────────┼─────────────────┼─────────────────┤
│ • 连接超时       │ • 500 错误       │ • 数据库连接失败  │
│ • DNS 解析失败   │ • 响应缓慢       │ • 查询超时       │
│ • TLS 握手失败   │ • 内存泄漏       │ • 死锁           │
│ • 防火墙阻止     │ • CPU 100%       │ • 数据不一致     │
└────────┬────────┴────────┬────────┴────────┬────────┘
         ↓                ↓                ↓
    tcpdump/wireshap   GDB/profiler     psql/redis-cli
```

### 诊断检查清单

#### 快速健康检查

```bash
#!/bin/bash
# agentos-diagnose.sh - 快速诊断脚本

echo "=== AgentOS 系统诊断 ==="
echo ""

echo "[1] 服务状态检查"
systemctl status agentos-kernel || docker ps | grep agentos

echo ""
echo "[2] 端口监听检查"
netstat -tlnp | grep -E '(8080|8001|8002|5432|6379)' || ss -tlnp

echo ""
echo "[3] 资源使用情况"
free -h
df -h /app/data
top -bn1 | head -15

echo ""
echo "[4] 日志错误统计"
journalctl -u agentos-kernel --since "1 hour ago" | grep -c ERROR
docker logs agentos-kernel-1 2>&1 | tail -50 | grep -i error

echo ""
echo "[5] 数据库连接检查"
pg_isready -h localhost -p 5432 -U agentos
redis-cli ping

echo ""
echo "[6] IPC 延迟测试"
time curl -s http://localhost:8080/health > /dev/null

echo ""
echo "[7] 内存索引状态"
curl -s http://localhost:8080/api/v1/memory/stats | jq .

echo ""
echo "=== 诊断完成 ==="
```

### 分层诊断方法

#### 第一层：网络层

```bash
# 1. 连通性测试
ping kernel-hostname
telnet kernel-hostname 8080

# 2. DNS 解析
nslookup kernel-hostname
dig kernel-hostname

# 3. 防火墙检查
iptables -L -n | grep 8080
ufw status

# 4. TLS 证书检查
openssl s_client -connect localhost:443 -servername agentos
```

#### 第二层：应用层

```bash
# 1. 进程状态
ps aux | grep agentos
top -p $(pgrep -f agentos)

# 2. 线程状态
pstree -p $(pgrep -f agentos)
ls /proc/<PID>/task/

# 3. 文件描述符
ls -la /proc/<PID>/fd/
lsof -p <PID>

# 4. 内存映射
cat /proc/<PID>/maps
pmap <PID>

# 5. 环境变量
cat /proc/<PID>/environ | tr '\0' '\n'
```

#### 第三层：内核层 (Syscall)

```bash
# 使用 strace 跟踪系统调用
strace -f -e trace=network,write -p <PID>

# 统计系统调用频率
strace -c -p <PID>

# 使用 ltrace 跟踪库函数调用
ltrace -p <PID>

# 使用 perf 进行性能分析
perf top -p <PID>
perf record -g -p <PID> -- sleep 30
perf report
```

---

## 📊 性能瓶颈分析

### CPU 瓶颈

**症状**: CPU 使用率持续 > 80%，响应延迟增加

```bash
# 识别 CPU 密集型进程
top -o %CPU

# 使用 perf 分析热点函数
perf record -g -a -- sleep 30
perf report --stdio | head -100

# 火焰图生成
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

**优化方向**:
- 算法优化（降低时间复杂度）
- 缓存热点数据
- 异步 I/O 替代同步阻塞
- 并行化计算密集任务

### 内存瓶颈

**症状**: OOM Killer 触发，频繁 GC/swap

```bash
# 内存使用分析
free -h
vmstat 1 10
cat /proc/meminfo

# 内存泄漏检测
valgrind --leak-check=full ./program
massif-visualizer massif.out.*

# 对象分配跟踪（Python）
import tracemalloc
tracemalloc.start()
# ... code ...
snapshot = tracemalloc.take_snapshot()
for stat in snapshot.statistics(10):
    print(stat)
```

**优化方向**:
- 修复内存泄漏
- 优化数据结构
- 增加内存池复用
- 流式处理替代全量加载

### I/O 瓶颈

**症状**: 高 iowait，磁盘利用率接近 100%

```bash
# I/O 状态监控
iostat -xz 1
iotop

# 磁盘 I/O 追踪
sudo biosnoop -t 10  # bcc-tools
blktrace -d /dev/sda -o -

# 文件系统延迟
fileslower 1.0  # bcc-tools
```

**优化方向**:
- 增加 SSD 缓存层
- 批量写入替代单条写入
- 异步 I/O
- 读写分离
- 分区表优化

### 锁竞争

**症状**: 多线程性能不升反降

```bash
# 锁竞争检测
perf lock contention -a -p <PID>

# pthread 锁分析
pthread_mutex_stats  # 自定义工具或 GDB

# Go 协程调度分析
go tool trace trace.out
```

**优化方向**:
- 减小锁粒度
- 无锁数据结构
- 读写锁替代互斥锁
- 减少临界区范围

---

## 🎯 特定场景调试

### 场景 1: IPC 通信故障

```bash
# Step 1: 验证端口监听
ss -tlnp | grep 8080

# Step 2: 测试基本连通性
curl -v http://localhost:8080/health

# Step 3: 查看 IPC 日志
journalctl -u agentos-kernel | grep ipc | tail -50

# Step 4: 抓包分析
tcpdump -i lo port 8080 -A -vvv

# Step 5: 检查共享内存
ipcs -m
ipcrm -M <shmid>  # 清理残留共享内存
```

### 场景 2: 记忆系统性能下降

```bash
# L1: 检查 SQLite 性能
sqlite3 /app/data/memory/l1/l1_index.db "ANALYZE;"
sqlite3 /app/data/memory/l1/l1_index.db "PRAGMA integrity_check;"

# L2: 检查 FAISS 索引状态
python -c "
import faiss
index = fa.read_index('/app/data/memory/l2/faiss.index')
print(f'Index size: {index.ntotal}')
print(f'Is trained: {index.is_trained}')
"

# L3: 图数据库查询计划
psql -U agentos -d agentos_graph -c "
EXPLAIN ANALYZE SELECT * FROM relations WHERE source = 'entity-001';
"

# L4: 挖掘任务状态
curl -s http://localhost:8080/api/v1/memory/l4/status | jq .
```

### 场景 3: Agent 行为异常

```bash
# Step 1: 查看 Agent 状态
curl http://localhost:8080/api/v1/agents/{agent_id}/status | jq .

# Step 2: 查看对话历史
curl http://localhost:8080/api/v1/agents/{agent_id}/sessions/latest | jq .

# Step 3: 查看 Agent 日志
docker logs agent-agent-{id}-1 2>&1 | tail -100

# Step 4: 检查记忆内容
curl "http://localhost:8080/api/v1/memory/search?query=recent&agent_id={agent_id}" | jq .

# Step 5: LLM 调用追踪
journalctl -u llm_d | grep {agent_id} | tail -50
```

---

## 🛠️ 调试最佳实践

### 1. 可重现的最小示例

在报告 Bug 前，创建可重现的最小代码示例：

```python
# minimal_repro.py
"""最小可重现示例"""
from agentos import AgentOSClient

def main():
    client = AgentOSClient(base_url="http://localhost:8080")
    agent = client.create_agent(AgentConfig(name="debug-test"))

    # 这一步触发 bug
    response = agent.chat("Specific input that triggers the bug")
    print(response)  # Unexpected behavior here

if __name__ == "__main__":
    main()
```

### 2. 二分法定位问题

通过注释掉一半代码来快速定位问题区域：

```python
def complex_function():
    part_a()  # 注释掉这半部分
    part_b()  # 保留这半部分
    # 如果 bug 消失，说明问题在 part_a
```

### 3. 日志增强技巧

临时添加详细日志帮助定位问题：

```python
import logging
logger = logging.getLogger(__name__)

def suspicious_function(x, y):
    logger.debug(f"Entering with x={x}, y={y}")
    result = compute(x, y)
    logger.debug(f"Result: {result}, type: {type(result)}")
    return result
```

### 4. 断言式防御编程

添加运行时断言捕获异常状态：

```python
def process_data(data):
    assert isinstance(data, list), f"Expected list, got {type(data)}"
    assert len(data) > 0, "Data should not be empty"
    assert all(isinstance(item, dict) for item in data), "All items must be dicts"
    # ... processing logic
```

---

## 📚 相关文档

- **[测试指南](testing.md)** — 测试策略与工具
- **[故障排查](../troubleshooting/common-issues.md)** — 常见问题解决方案
- **[统一日志系统](../../agentos/manuals/architecture/logging_system.md)** — 日志规范
- **[内核调优指南](../../agentos/manuals/guides/kernel_tuning.md)** — 性能调优

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
