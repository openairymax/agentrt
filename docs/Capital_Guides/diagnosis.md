Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 故障诊断指南

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪

---

## 📋 分层诊断方法论

AgentOS 采用**分层诊断方法论**，从外到内逐层定位问题：

```
用户报告问题
    ↓
┌─────────────────────────────────────────────┐
│  Layer 1: 用户层 (User Layer)               │
│  检查：请求参数 · 用户权限 · 客户端配置       │
└─────────────────┬───────────────────────────┘
                  ↓
┌─────────────────────────────────────────────┐
│  Layer 2: 网关层 (Gateway Layer)            │
│  检查：路由规则 · 负载均衡 · TLS/认证        │
└─────────────────┬───────────────────────────┘
                  ↓
┌─────────────────────────────────────────────┐
│  Layer 3: 服务层 (Service Layer)             │
│  检查：守护进程状态 · API 响应 · 资源使用     │
└─────────────────┬───────────────────────────┘
                  ↓
┌─────────────────────────────────────────────┐
│  Layer 4: 内核层 (Kernel Layer)              │
│  检查：IPC 通信 · 系统调用 · 内存管理         │
└─────────────────┬───────────────────────────┘
                  ↓
┌─────────────────────────────────────────────┐
│  Layer 5: 数据层 (Data Layer)                │
│  检查：PostgreSQL · Redis · 文件系统          │
└─────────────────────────────────────────────┘
```

---

## 🔍 诊断工具箱

### 一键诊断脚本

```bash
#!/bin/bash
# agentos_diagnose.sh - 全自动诊断脚本

set -e

COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_NC='\033[0m'

echo "=========================================="
echo "  AgentOS System Diagnostic Tool v1.0"
echo "  $(date)"
echo "=========================================="

pass_count=0
warn_count=0
fail_count=0

check_pass() {
    echo -e "${COLOR_GREEN}[PASS]${COLOR_NC} $1"
    pass_count=$((pass_count + 1))
}

check_warn() {
    echo -e "${COLOR_YELLOW}[WARN]${COLOR_NC} $1"
    warn_count=$((warn_count + 1))
}

check_fail() {
    echo -e "${COLOR_RED}[FAIL]${COLOR_NC} $1"
    fail_count=$((fail_count + 1))
}

echo ""
echo "--- [Layer 5] Data Layer Checks ---"

# PostgreSQL 检查
if pg_isready -h localhost -p 5432 -U agentos 2>/dev/null | grep -q "accepting connections"; then
    check_pass "PostgreSQL is running and accepting connections"

    # 连接数检查
    CONN_COUNT=$(psql -U agentos -d agentos -t -c "SELECT count(*) FROM pg_stat_activity;" 2>/dev/null | tr -d ' ')
    if [ "$CONN_COUNT" -lt 100 ]; then
        check_pass "PostgreSQL connection count: $CONN_COUNT (healthy)"
    else
        check_warn "PostgreSQL connection count: $CONN_COUNT (high, consider pooling)"
    fi

    # 慢查询检查
    SLOW_QUERIES=$(psql -U agentos -d agentos -t -c "
        SELECT count(*) FROM pg_stat_statements WHERE mean_exec_time > 1000;
    " 2>/dev/null | tr -d ' ')
    if [ "$SLOW_QUERIES" -eq 0 ]; then
        check_pass "No slow queries detected (>1s)"
    else
        check_warn "Found $SLOW_QUERIES slow queries"
    fi
else
    check_fail "PostgreSQL is not responding"
fi

# Redis 检查
if redis-cli ping 2>/dev/null | grep -q PONG; then
    check_pass "Redis is responding"

    # 内存使用检查
    REDIS_MEMORY=$(redis-cli info memory 2>/dev/null | grep used_memory_human | cut -d: -f2 | tr -d '\r')
    check_pass "Redis memory usage: $REDIS_MEMORY"

    # 键空间检查
    REDIS_KEYS=$(redis-cli dbsize 2>/dev/null)
    check_pass "Redis total keys: $REDIS_KEYS"
else
    check_fail "Redis is not responding"
fi

echo ""
echo "--- [Layer 4] Kernel Layer Checks ---"

# IPC 端口检查
if nc -z localhost 8080 2>/dev/null; then
    check_pass "Kernel IPC port (8080) is listening"

    # 健康检查
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/health)
    if [ "$HTTP_CODE" = "200" ]; then
        check_pass "Kernel health endpoint returns 200 OK"

        HEALTH_RESPONSE=$(curl -s http://localhost:8080/health)
        STATUS=$(echo $HEALTH_RESPONSE | jq -r '.status' 2>/dev/null)
        if [ "$STATUS" = "healthy" ] || [ "$STATUS" = "ok" ]; then
            check_pass "Kernel status: $STATUS"
        else
            check_warn "Kernel status: $STATUS (may have issues)"
        fi
    else
        check_fail "Kernel health endpoint returned HTTP $HTTP_CODE"
    fi
else
    check_fail "Kernel IPC port (8080) is not listening"
fi

# Prometheus 监控端口
if nc -z localhost 9090 2>/dev/null; then
    check_pass "Prometheus metrics port (9090) is listening"
else
    check_warn "Prometheus metrics port (9090) is not listening (monitoring may be disabled)"
fi

echo ""
echo "--- [Layer 3] Service Layer Checks ---"

# 检查所有守护进程服务
for SERVICE in llm_d market_d monit_d tool_d sched_d gateway_d; do
    PORT=$(get_port_for_service $SERVICE)

    if nc -z localhost $PORT 2>/dev/null; then
        check_pass "$SERVICE service is running on port $PORT"
    else
        check_fail "$SERVICE service is NOT running on port $PORT"
    fi
done

echo ""
echo "--- [Layer 2] Gateway Layer Checks ---"

if nc -z localhost 8006 2>/dev/null; then
    check_pass "API Gateway (8006) is listening"

    # 测试路由转发
    GATEWAY_RESP=$(curl -s http://localhost:8006/api/v1/llm/health 2>/dev/null)
    if [ -n "$GATEWAY_RESP" ]; then
        check_pass "Gateway routing to llm_d works"
    else
        check_warn "Gateway routing test inconclusive"
    fi
else
    check_fail "API Gateway (8006) is not listening"
fi

echo ""
echo "--- [Layer 1] Resource Usage Checks ---"

# CPU 使用率
CPU_USAGE=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
CPU_INT=${CPU_USAGE%.*}
if [ "$CPU_INT" -lt 70 ]; then
    check_pass "CPU usage: ${CPU_USAGE}% (normal)"
elif [ "$CPU_INT" -lt 90 ]; then
    check_warn "CPU usage: ${CPU_USAGE}% (high)"
else
    check_fail "CPU usage: ${CPU_USAGE}% (critical)"
fi

# 内存使用率
MEM_INFO=$(free -g | awk '/^Mem:/ {print $3 "/" $2 "GB used (" int($3/$2*100) "%)"}')
MEM_PERCENT=$(free -g | awk '/^Mem:/ {printf "%.0f", $3/$2*100}')
if [ "$MEM_PERCENT" -lt 70 ]; then
    check_pass "Memory: $MEM_INFO (normal)"
elif [ "$MEM_PERCENT" -lt 90 ]; then
    check_warn "Memory: $MEM_INFO (high)"
else
    check_fail "Memory: $MEM_INFO (critical)"
fi

# 磁盘使用率
DISK_USAGE=$(df -h /app/data | tail -1 | awk '{print $5}' | tr -d '%')
DISK_INT=${DISK_USAGE%.*}
if [ "$DISK_INT" -lt 75 ]; then
    check_pass "Disk (/app/data): ${DISK_USAGE}% used (normal)"
elif [ "$DISK_INT" -lt 90 ]; then
    check_warn "Disk (/app/data): ${DISK_USAGE}% used (high, consider cleanup)"
else
    check_fail "Disk (/app/data): ${DISK_USAGE}% used (critical)"
fi

# Docker 容器状态
CONTAINER_COUNT=$(docker ps --format '{{.Names}}' 2>/dev/null | wc -l)
check_pass "Running containers: $CONTAINER_COUNT"

EXITED_CONTAINERS=$(docker ps -a --filter "status=exited" --format '{{.Names}}' 2>/dev/null)
if [ -z "$EXITED_CONTAINERS" ]; then
    check_pass "No stopped containers"
else
    check_warn "Stopped containers found: $EXITED_CONTAINERS"
fi

echo ""
echo "--- Recent Error Logs (Last 10 lines) ---"
docker logs agentos-kernel-1 2>&1 | grep -i error | tail -10 || echo "(No recent errors)"

echo ""
echo "=========================================="
echo "  Diagnostic Summary"
echo "=========================================="
echo -e "${COLOR_GREEN}Passed:${COLOR_NC}   $pass_count"
echo -e "${COLOR_YELLOW}Warnings:${COLOR_NC} $warn_count"
echo -e "${COLOR_RED}Failed:${COLOR_NC}   $fail_count"
echo ""

TOTAL=$((pass_count + warn_count + fail_count))
SCORE=$((pass_count * 100 / TOTAL))

if [ $fail_count -eq 0 ] && [ $warn_count -eq 0 ]; then
    echo -e "${COLOR_GREEN}✅ System Health Score: ${SCORE}% (All checks passed)${COLOR_NC}"
    exit 0
elif [ $fail_count -eq 0 ]; then
    echo -e "${COLOR_YELLOW}⚠️  System Health Score: ${SCORE}% (Warnings detected)${COLOR_NC}"
    exit 1
else
    echo -e "${COLOR_RED}❌ System Health Score: ${SCORE}% (Failures detected!)${COLOR_NC}"
    exit 2
fi
```

---

## 🎯 常见问题诊断树

### 问题 1: 无法连接到 AgentOS API

```
无法连接 API
    │
    ├── 检查网络连通性
    │   ├── ping kernel-hostname → 失败 → DNS 配置错误或主机不可达
    │   └── ping 成功 → 继续
    │
    ├── 检查端口监听
    │   ├── netstat -tlnp | grep 8080 → 无输出 → 内核未启动
    │   └── 有输出 → 继续
    │
    ├── 检查防火墙
    │   ├── iptables -L -n | grep 8080 → 被拒绝 → 防火墙阻止
    │   └── 允许 → 继续
    │
    ├── 检查 TLS（如果使用 HTTPS）
    │   ├── openssl s_client -connect host:443 → 证书错误 → TLS 配置问题
    │   └── 成功 → 继续
    │
    └── 检查应用日志
        ├── 日志显示启动失败 → 查看具体错误信息
        └── 日志正常 → 可能是临时性故障，重试
```

### 问题 2: Agent 响应缓慢

```
Agent 响应慢 (>5s)
    │
    ├── 定位瓶颈层
    │   ├── curl -w '%{time_connect}' → >1s → 网络/网关延迟
    │   ├── curl -w '%{time_starttransfer}' → >3s → 后端处理慢
    │   └── 都正常 → 客户端渲染慢
    │
    ├── 如果是后端处理慢
    │   ├── 检查 LLM 服务
    │   │   ├── LLM TTFT >2s → 模型加载/推理慢
    │   │   └── LLM 正常 → 继续检查
    │   │
    │   ├── 检查记忆系统
    │   │   ├── L2 搜索 >50ms → FAISS 索引需优化
    │   │   └── L2 正常 → 继续检查
    │   │
    │   └── 检查系统资源
    │       ├── CPU >90% → 计算密集型任务过多
    │       ├── 内存 >85% → GC/swap 开销大
    │       └── I/O wait 高 → 磁盘瓶颈
    │
    └── 解决方案
        ├── 增加 LLM 并发实例
        ├── 优化 FAISS nprobe 参数
        ├── 扩容资源（水平/垂直）
        └── 启用缓存层
```

### 问题 3: 记忆检索返回空结果

```
记忆搜索无结果
    │
    ├── 检查数据是否已存储
    │   ├── L1 记录数 = 0 → 存储流程有问题
    │   └── L1 有数据 → 继续
    │
    ├── 检查向量化是否成功
    │   ├── L2 索引向量数 = 0 → Embedding 服务异常
    │   └── 向量数正常 → 继续
    │
    ├── 检查查询质量
    │   ├── 查询词太短/太泛 → 优化查询策略
    │   └── 查询合理 → 继续
    │
    ├── 检查相似度阈值
    │   ├── threshold 太高 → 降低阈值（如 0.7→0.5）
    │   └── 阈值正常 → 继续
    │
    └── 检查索引损坏
        ├── 重建 FAISS 索引
        └── 验证索引完整性
```

---

## 📊 性能基线对比

### 创建性能快照

```python
#!/usr/bin/env python3
# performance_baseline.py - 性能基线记录与对比

import json
import time
import statistics
from datetime import datetime
from dataclasses import dataclass
from typing import List, Dict

@dataclass
class MetricPoint:
    name: str
    value: float
    unit: str
    timestamp: datetime

class PerformanceBaseline:
    def __init__(self, baseline_file: str = "/var/log/agentos/performance_baseline.json"):
        self.baseline_file = baseline_file
        self.baseline = self._load_baseline()

    def _load_baseline(self) -> Dict:
        try:
            with open(self.baseline_file, 'r') as f:
                return json.load(f)
        except FileNotFoundError:
            return {}

    def save_baseline(self):
        with open(self.baseline_file, 'w') as f:
            json.dump(self.baseline, f, indent=2, default=str)

    def record_metric(self, name: str, value: float, unit: str):
        """记录指标"""
        metric = MetricPoint(
            name=name,
            value=value,
            unit=unit,
            timestamp=datetime.now()
        )

        if name not in self.baseline:
            self.baseline[name] = {"history": []}

        self.baseline[name]["history"].append({
            "value": value,
            "timestamp": metric.timestamp.isoformat()
        })

        # 只保留最近 100 个数据点
        if len(self.baseline[name]["history"]) > 100:
            self.baseline[name]["history"] = self.baseline[name]["history"][-100:]

    def analyze_trend(self, name: str) -> Dict:
        """分析趋势"""
        if name not in self.baseline or len(self.baseline[name]["history"]) < 2:
            return {"status": "insufficient_data"}

        history = [h["value"] for h in self.baseline[name]["history"]]
        recent = history[-10:] if len(history) >= 10 else history
        older = history[-20:-10] if len(history) >= 20 else history[:len(recent)]

        recent_avg = statistics.mean(recent)
        older_avg = statistics.mean(older) if older else recent_avg
        change_pct = ((recent_avg - older_avg) / older_avg * 100) if older_avg != 0 else 0

        # 判断趋势
        if change_pct > 20:
            trend = "degrading"  # 性能下降
            status = "warning"
        elif change_pct < -20:
            trend = "improving"   # 性能改善
            status = "good"
        else:
            trend = "stable"
            status = "normal"

        return {
            "status": status,
            "trend": trend,
            "recent_average": round(recent_avg, 2),
            "older_average": round(older_avg, 2),
            "change_percent": round(change_pct, 2),
            "current_value": history[-1],
            "min": min(history),
            "max": max(history),
            "std_dev": round(statistics.stdev(history), 2) if len(history) > 1 else 0
        }

    def generate_report(self) -> str:
        """生成诊断报告"""
        report_lines = [
            "=" * 60,
            "  AgentOS Performance Baseline Report",
            f"  Generated: {datetime.now().isoformat()}",
            "=" * 60,
            ""
        ]

        for metric_name in sorted(self.baseline.keys()):
            analysis = self.analyze_trend(metric_name)

            status_icon = {"good": "✅", "normal": "📊", "warning": "⚠️", "insufficient_data": "❓"}
            icon = status_icon.get(analysis["status"], "❓")

            report_lines.extend([
                f"{icon} {metric_name}",
                f"   Current: {analysis['current_value']} | Avg: {analysis['recent_average']} | Trend: {analysis['trend']} ({analysis['change_percent']:+.1f}%)",
                f"   Range: [{analysis['min']:.2f}, {analysis['max']:.2f}] | StdDev: {analysis['std_dev']:.2f}",
                ""
            ])

        report_lines.extend([
            "=" * 60,
            "",
            "Recommendations:",
            ""
        ])

        # 自动生成建议
        for metric_name in self.baseline:
            analysis = self.analyze_trend(metric_name)
            if analysis["trend"] == "degrading":
                report_lines.append(f"- ⚠️  {metric_name} is degrading. Consider investigating cause.")
            elif analysis["std_dev"] > analysis["recent_average"] * 0.5:
                report_lines.append(f"- 📊 {metric_name} has high variability. Check for resource contention.")

        report_text = "\n".join(report_lines)
        print(report_text)
        return report_text


if __name__ == "__main__":
    baseline = PerformanceBaseline()

    # 收集当前指标（示例）
    import subprocess

    def get_ipc_latency():
        start = time.time()
        subprocess.run(["curl", "-s", "-o", "/dev/null", "http://localhost:8080/health"], capture_output=True)
        return (time.time() - start) * 1000  # ms

    def get_memory_usage():
        result = subprocess.run(["free"], capture_output=True, text=True)
        for line in result.stdout.split('\n'):
            if 'Mem:' in line:
                parts = line.split()
                used = int(parts[2])
                total = int(parts[1])
                return (used / total) * 100
        return 0

    baseline.record_metric("ipc_latency_ms", get_ipc_latency(), "ms")
    baseline.record_metric("memory_usage_percent", get_memory_usage(), "%")

    baseline.save_baseline()
    baseline.generate_report()
```

---

## 🔧 特定组件诊断命令速查表

### 内核诊断

```bash
# 查看内核启动日志
journalctl -u agentos-kernel --since "1 hour ago" | head -100

# 查看 IPC 统计
curl -s http://localhost:8080/api/v1/kernel/stats/ipc | jq .

# 查看内存使用情况
curl -s http://localhost:8080/api/v1/kernel/stats/memory | jq .

# 查看活跃任务列表
curl -s http://localhost:8080/api/v1/tasks?status=running | jq '.tasks[] | {id, name, priority, created_at}'
```

### LLM 服务诊断

```bash
# 测试 LLM 推理延迟
time curl -X POST http://localhost:8001/api/v1/llm/chat \
  -H "Content-Type: application/json" \
  -d '{"messages":[{"role":"user","content":"Hi"}],"stream":false}'

# 查看 LLM 队列深度
curl -s http://localhost:8001/api/v1/llm/stats | jq .queue_depth

# 查看 Token 使用统计
curl -s http://localhost:8001/api/v1/llm/stats/tokens | jq .
```

### 记忆系统诊断

```bash
# L1 统计
curl -s http://localhost:8080/api/v1/memory/l1/stats | jq .

# L2 索引状态
curl -s http://localhost:8080/api/v1/memory/l2/stats | jq '{vector_count, index_type, dimension, last_build_time}'

# L3 图统计
curl -s http://localhost:8080/api/v1/memory/l3/stats | jq '{total_relations, entity_count, avg_degree}'

# L4 模式统计
curl -s http://localhost:8080/api/v1/memory/l4/stats | jq '{pattern_count, categories, last_mining_time}'
```

---

## 🚨 应急响应流程

### P0/P1 严重故障应急步骤

```bash
#!/bin/bash
# incident_response.sh - 应急响应脚本

INCIDENT_ID=$(date +%Y%m%d_%H%M%S)
LOG_DIR="/var/log/agentos/incidents/$INCIDENT_ID"
mkdir -p "$LOG_DIR"

echo "=== Incident Response Started ==="
echo "Incident ID: $INCIDENT_ID"

# Step 1: 收集现场证据（5分钟内完成）
echo "[Step 1/6] Collecting evidence..."

# 系统快照
top -bn1 > "$LOG_DIR/top.txt"
free -h > "$LOG_DIR/memory.txt"
df -h > "$LOG_DIR/disk.txt"
netstat -tlnp > "$LOG_DIR/network.ps"
ps auxf > "$LOG_DIR/processes.txt"

# Docker 状态
docker ps -a > "$LOG_DIR/docker_containers.txt"
docker stats --no-stream > "$LOG_DIR/docker_stats.txt"

# 最近日志（各容器）
for container in $(docker ps --format '{{.Names}}'); do
    docker logs --tail 500 "$container" > "$LOG_DIR/${container}.log" 2>&1
done

# 应用日志
journalctl -u agentos-kernel --since "30 min ago" > "$LOG_DIR/journal_kernel.log"
journalctl -u agentos-daemon --since "30 min ago" > "$LOG_DIR/journal_daemon.log"

# Step 2: 尝试快速恢复
echo "[Step 2/6] Attempting quick recovery..."

# 重启失败的服务
FAILED_SERVICES=$(docker ps -a --filter "status=exited" --format '{{.Names}}')
for svc in $FAILED_SERVICES; do
    echo "Restarting: $svc"
    docker restart "$svc"
done

# 等待健康检查
sleep 30

# Step 3: 验证恢复状态
echo "[Step 3/6] Verifying recovery..."

HEALTH_STATUS=$(curl -sf http://localhost:8080/health | jq -r '.status' 2>/dev/null)
if [ "$HEALTH_STATUS" = "healthy" ] || [ "$HEALTH_STATUS" = "ok" ]; then
    echo "✅ System recovered successfully"
else
    echo "❌ Quick recovery failed, escalating..."

    # Step 4: 升级处理
    echo "[Step 4/6] Escalating incident..."

    # 发送告警通知
    send_alert "P0 Incident $INCIDENT_ID: Quick recovery failed. Manual intervention required."

    # 导出完整诊断包
    tar czf "/tmp/incident_${INCIDENT_ID}.tar.gz" -C /var/log/agentos/incidents "$INCIDENT_ID"

    # 生成事件报告
    cat > "$LOG_DIR/incident_report.md" << EOF
# Incident Report: $INCIDENT_ID

## Summary
Quick recovery attempt failed.

## Timeline
- $(date): Incident detected
- $(date): Evidence collection completed
- $(date): Recovery attempted
- $(date): Escalated

## Evidence Files
$(ls -la "$LOG_DIR")

## Next Steps
1. Review logs for root cause
2. Engage on-call engineer
3. Consider failover to standby
EOF
fi

# Step 5: 通知相关人员
echo "[Step 5/6] Sending notifications..."
send_alert "Incident $INCIDENT_ID response completed. Status: $HEALTH_STATUS"

# Step 6: 创建后续行动项
echo "[Step 6/6] Creating follow-up actions..."
cat >> /var/log/agentos/post_incident_actions.txt << EOF

[$(date)] Incident $INCIDENT_ID
- [ ] Conduct root cause analysis
- [ ] Update runbook if needed
- [ ] Schedule post-mortem meeting
- [ ] Implement preventive measures

EOF

echo "=== Incident Response Completed ==="
echo "Evidence saved to: $LOG_DIR"
```

---

## 📚 相关文档

- **[调试指南](../development/debugging.md)** — 详细调试技巧
- **[常见问题FAQ](common-issues.md)** — Top 20 高频问题
- **[已知问题](known-issues.md)** — 已知 Bug 及解决方案
- **[性能调优](performance-tuning.md)** — 性能瓶颈解决

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
