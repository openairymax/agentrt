Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# 常见问题 FAQ

**版本**: Doc V1.8  
**最后更新**: 2026-04-09  
**维护者**: @spharx-team/support  

---

本文档收集了用户和开发者最常遇到的问题及其解决方案。

---

## 🚀 安装与启动

### Q1: Docker容器启动失败，提示端口被占用？

**症状**:
```
Error starting userland proxy: listen tcp4 0.0.0.0:8080: bind: address already in use
```

**原因**: 8080端口已被其他进程占用。

**解决方案**:

```bash
# 1. 查找占用端口的进程
lsof -i :8080
# 或
ss -tlnp | grep 8080

# 2. 查看进程详情
ps -p <PID> -o pid,cmd

# 3. 选择以下任一方式：
# 方式A: 停止占用进程（如果不重要）
kill <PID>

# 方式B: 修改AgentOS端口
# 编辑 docker/.env
KERNEL_IPC_PORT=8081  # 改为其他端口

# 方式C: 停止其他服务后重启AgentOS
docker compose -f docker/docker-compose.yml down
docker compose -f docker/docker-compose.yml up -d
```

**预防措施**:

在 `.env` 中自定义端口以避免冲突：
```env
KERNEL_IPC_PORT=18080
POSTGRES_PORT=15432
REDIS_PORT=16379
```

---

### Q2: Docker构建失败，提示内存不足？

**症状**:
```
fatal error: killed signal cc1plus (out of memory)
```

**原因**: 系统可用内存不足以进行并行编译。

**解决方案**:

```bash
# 方式1: 减少Docker构建使用的内存
# 编辑 /etc/docker/daemon.json
{
  "memory": 4096000000,  // 限制Docker使用4GB
  "cpus": 4
}

# 重启Docker
sudo systemctl restart docker

# 方式2: 限制构建并行度
# 编辑 docker/Dockerfile.kernel
RUN cmake --build . --parallel 2  # 从$(nproc)改为固定值

# 方式3: 增加swap空间
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

---

### Q3: 数据库连接失败？

**症状**:
```
could not connect to server: Connection refused
```

**排查步骤**:

```bash
# 1. 检查PostgreSQL容器状态
docker ps | grep postgres

# 2. 查看PostgreSQL日志
docker logs agentos-postgres-dev 2>&1 | tail -50

# 3. 手动测试连接
docker exec -it agentos-postgres-dev psql -U agentos -d agentos -c "SELECT version();"

# 4. 检查网络连通性
docker exec agentos-kernel-dev ping postgres -c 3

# 5. 检查环境变量中的连接字符串
grep DATABASE_URL docker/.env
```

**常见原因及解决**:

| 原因 | 解决方案 |
|------|---------|
| PostgreSQL未完全启动 | 等待健康检查通过（约10-30秒） |
| 密码错误 | 检查 `.env` 中的 `POSTGRES_PASSWORD` |
| 网络隔离 | 确保两个容器在同一Docker网络中 |
| 数据损坏 | 删除卷重新初始化：`docker volume rm agentos_postgres-data` |

---

## 🔑 配置问题

### Q4: 如何配置多个LLM提供商？

**场景**: 同时使用OpenAI和DeepSeek，根据任务复杂度自动选择。

**解决方案**:

编辑 `config/llm.yaml`:

```yaml
providers:
  primary:
    name: openai
    api_key: ${AGENTOS_LLM_API_KEY}
    base_url: https://api.openai.com/v1
    model: gpt-4-turbo
    max_tokens: 4096
    temperature: 0.7

  secondary:
    name: deepseek
    api_key: ${DEEPSEEK_API_KEY}
    base_url: https://api.deepseek.com/v1
    model: deepseek-chat
    max_tokens: 4096
    temperature: 0.7

routing_strategy:
  type: complexity_based  # 或 cost_based, round_robin
  threshold: 0.7  # 复杂度阈值，超过使用primary

cost_optimization:
  enabled: true
  budget_per_day: 50.0  # 每日预算(美元)
  prefer_secondary_for_simple_tasks: true
```

---

### Q5: 记忆系统占用过多磁盘空间怎么办？

**症状**: 磁盘空间快速增长，主要被PostgreSQL占用。

**解决方案**:

```bash
# 1. 查看记忆数据占用
docker exec agentos-postgres-dev psql -U agentos -d agentos -c "
  SELECT schemaname, relname, pg_total_relation_size(relid) as size
  FROM pg_stat_user_tables
  ORDER BY size DESC
  LIMIT 10;
"

# 2. 配置自动清理策略
# 编辑 config/memory.yaml
memoryrovol:
  l1:
    ttl_seconds: 3600  # L1记忆1小时过期
    max_entries: 10000
  l2:
    ttl_seconds: 86400  # L2记忆1天过期
    max_entries: 100000
  auto_compaction:
    enabled: true
    schedule: "0 3 * * *"  # 每天凌晨3点执行
    retention_days: 30  # 保留30天数据

# 3. 手动清理过期数据
docker exec agentos-kernel-dev python3 -m agentos.tools.memory_cleanup --dry-run
# 确认无误后执行：
docker exec agentos-kernel-dev python3 -m agentos.tools.memory_cleanup
```

---

### Q6: 如何调整安全级别？

**场景**: 开发环境需要更宽松的安全策略以提高效率。

**解决方案**:

编辑 `docker/.env`:

```env
# 开发环境（宽松模式）
AGENTOS_PERMISSION_MODE=relaxed
AGENTOS_SANITIZER_MODE=relaxed
AGENTOS_AUDIT_ENABLED=false

# 生产环境（严格模式，推荐）
AGENTOS_PERMISSION_MODE=strict
AGENTOS_SANITIZER_MODE=strict
AGENTOS_AUDIT_ENABLED=true
```

**各模式对比**:

| 特性 | strict | relaxed | disabled |
|------|--------|---------|----------|
| 权限检查 | 全部检查 | 仅关键操作 | 不检查 |
| 输入净化 | 全部净化 | 仅危险字符 | 不净化 |
| 审计日志 | 全部记录 | 仅异常事件 | 不记录 |
| 性能影响 | ~5% | ~1% | 0% |
| **安全性** | ✅ 生产级 | ⚠️ 开发用 | ❌ 危险 |

⚠️ **警告**: 生产环境严禁使用 `disabled` 模式！

---

## 🐛 运行时问题

### Q7: 任务卡在"running"状态不结束？

**症状**: 任务长时间显示running状态，无进度更新。

**排查步骤**:

```bash
# 1. 查看任务详情
curl http://localhost:8080/api/v1/tasks/<task_id> | jq .

# 2. 查看内核实时日志
docker logs -f agentos-kernel-dev 2>&1 | grep -E "(task_id=<task_id>|ERROR|WARN)"

# 3. 检查LLM服务状态
curl http://localhost:8001/health

# 4. 检查系统资源使用
docker stats agentos-kernel-dev --no-stream

# 5. 取消卡住的任务
curl -X POST http://localhost:8080/api/v1/tasks/<task_id>/cancel
```

**常见原因**:

| 原因 | 现象 | 解决方案 |
|------|------|---------|
| LLM API超时 | 任务停在cognition/planning阶段 | 增加 `AGENTOS_LLM_TIMEOUT` |
| 工具执行阻塞 | 任务停在action阶段 | 检查工具服务是否正常 |
| 死锁 | CPU使用率100%，无日志输出 | 重启内核服务 |
| 内存不足 | OOM Killer终止进程 | 增加内存限制 |

---

### Q8: Prometheus指标数据缺失？

**症状**: Grafana仪表盘显示"No Data"。

**排查步骤**:

```bash
# 1. 确认Prometheus能访问内核metrics端点
docker exec agentos-prometheus-dev wget -qO- http://kernel:9090/metrics | head -20

# 2. 检查Prometheus配置
cat docker/monitoring/prometheus.yml | grep -A5 "agentos-kernel"

# 3. 查看Prometheus目标状态
# 打开 http://localhost:9091/targets
# 确认kernel目标状态为UP

# 4. 检查Prometheus日志
docker logs agentos-prometheus-dev 2>&1 | tail -50

# 5. 手动测试指标采集
curl http://localhost:9090/metrics | grep agentos_
```

**如果指标确实不存在**:

可能内核未启用metrics导出。检查配置：

```yaml
# config/kernel.yaml
monitoring:
  prometheus:
    enabled: true
    port: 9090
    path: /metrics
```

---

### Q9: OpenLab无法连接到内核API？

**症状**: OpenLab页面显示"无法连接到后端服务"。

**排查步骤**:

```bash
# 1. 检查OpenLab容器是否能解析kernel主机名
docker exec agentos-openlab-dev ping kernel -c 2

# 2. 检查OpenLab的环境变量
docker exec agentos-openlab-dev env | grep AGENTOS_IPC_URL

# 3. 手动从OpenLab容器测试连接
docker exec agentos-openlab-dev curl http://kernel:8080/health

# 4. 检查内核容器的端口映射
docker port agentos-kernel-dev

# 5. 如果使用host网络模式，确保防火墙允许
sudo ufw allow 8080/tcp
```

**常见配置错误**:

```yaml
# ❌ 错误：使用了localhost（在容器内指向自身）
AGENTOS_IPC_URL=http://localhost:8080

# ✅ 正确：使用服务名（Docker DNS解析）
AGENTOS_IPC_URL=http://kernel:8080
```

---

## 🔒 安全问题

### Q10: 如何更换数据库密码？

**步骤**:

```bash
# 1. 停止所有服务
docker compose -f docker/docker-compose.yml down

# 2. 修改.env文件
nano docker/.env
# 修改 POSTGRES_PASSWORD=新密码

# 3. （可选）备份数据
docker run --rm -v agentos_postgres-data:/data alpine tar czf /backup/pg-backup.tar.gz -C /data .

# 4. 启动服务（新密码生效）
docker compose -f docker/docker-compose.yml up -d

# 5. 验证连接
docker exec agentos-postgres-dev psql -U agentos -d agentos -c "SELECT 1;"
```

---

### Q11: 发现安全漏洞如何处理？

**步骤**:

1. **不要公开发布漏洞详情**
2. **通过安全渠道报告**:
   - Email: wangliren@spharx.cn
   - PGP公钥: [SECURITY.md](../../.gitcode/SECURITY.md)
3. **等待确认**:
   - Critical: 24小时内响应
   - High: 3个工作日内响应
4. **获得CVE编号**（如适用）
5. **等待补丁发布后再公开**

**临时缓解措施**:

```bash
# 如果发现严重漏洞且补丁未发布：

# 1. 限制网络访问
iptables -A INPUT -p tcp --dport 8080 -s <你的IP> -j ACCEPT
iptables -A INPUT -p tcp --dport 8080 -j DROP

# 2. 启用更严格的权限模式
AGENTOS_PERMISSION_MODE=strict
AGENTOS_SANITIZER_MODE=strict

# 3. 启用详细审计
AGENTOS_AUDIT_ENABLED=true
```

---

## 📊 性能问题

### Q12: 系统响应缓慢，如何诊断？

**诊断流程**:

```bash
# 1. 检查系统资源
docker stats --no-stream

# 2. 查看慢查询日志
docker logs agentos-postgres-dev 2>&1 | grep "duration:"

# 3. 查看LLM API延迟
curl http://localhost:9090/metrics | grep llm_request_duration

# 4. 分析Prometheus慢请求
# 打开 Grafana → Dashboard → AgentOS Performance
# 查看 P95/P99 延迟趋势图

# 5. 生成性能报告
docker exec agentos-kernel-dev python3 -m agentos.tools.profile --output profile-report.html
```

**常见瓶颈及优化**:

| 瓶颈 | 症状 | 优化方案 |
|------|------|---------|
| LLM API延迟高 | cognition阶段慢 | 使用缓存、切换更快模型 |
| 数据库慢查询 | memory检索慢 | 添加索引、优化查询、读写分离 |
| 内存压力高 | GC频繁、OOM | 增加内存、优化数据结构 |
| CPU密集计算 | action阶段慢 | 并行化、使用更高效算法 |
| 网络I/O瓶颈 | IPC延迟高 | 批量发送、压缩消息 |

---

### Q13: 如何进行容量规划？

**计算公式**:

```python
# QPS估算
expected_qps = concurrent_users * requests_per_user

# 内存需求（每个任务实例）
memory_per_instance = base_memory + (avg_task_memory * max_concurrent_tasks)

# 数据库存储（每年）
storage_per_year = (
    daily_tasks * avg_task_size_bytes * 365
    + memory_l2_l3_growth_rate * 365
) * replication_factor

# 示例：1000并发用户，每用户每天10个任务
expected_qps = 1000 * (10 / 86400) ≈ 0.12 QPS（平均）
peak_qps = expected_qps * 10 ≈ 1.2 QPS（峰值，10倍突发）

memory_needed = 512MB + (50MB * 100) = 5.5GB（支持100并发任务）
storage_yearly = (10000 * 100KB * 365 + 1GB * 365) * 3 ≈ 4TB（3副本）
```

**生产环境推荐配置**:

| 规模 | CPU | 内存 | 存储 | 实例数 |
|------|-----|------|------|-------|
| 小型 (<100用户) | 4核 | 8GB | 100GB SSD | 1 |
| 中型 (100-1000用户) | 8核 | 32GB | 1TB SSD | 3 |
| 大型 (>1000用户) | 16核+ | 64GB+ | 10TB+ SSD | 5+ |

---

## 🔄 升级与迁移

### Q14: 如何从v0.x升级到v1.0？

**升级前准备**:

```bash
# 1. 完整备份
./scripts/backup.sh --full --output ./backups/pre-upgrade-$(date +%Y%m%d)

# 2. 记录当前版本
curl http://localhost:8080/health | jq '.version'

# 3. 查看变更日志
cat ../../CHANGELOG.md | grep -A50 "## [1.0.0]"
```

**升级步骤**:

```bash
# 1. 拉取新代码
git pull origin main
git submodule update --init --recursive

# 2. 更新镜像
docker compose -f docker/docker-compose.yml pull
docker compose -f docker/docker-compose.yml build

# 3. 执行数据库迁移
docker compose -f docker/docker-compose.yml exec postgres psql -U agentos -d agentos -f /migrations/v1.0.0.sql

# 4. 滚动更新（零停机）
docker compose -f docker/docker-compose.yml up -d --no-deps --build kernel

# 5. 验证升级
curl http://localhost:8080/health
./scripts/smoke-test.sh
```

**回滚方案**（如果升级失败）:

```bash
# 1. 停止新版本
docker compose -f docker/docker-compose.yml down

# 2. 恢复备份
./scripts/restore.sh --input ./backups/pre-upgrade-YYYYMMDD

# 3. 启动旧版本
git checkout v0.x.y
docker compose -f docker/docker-compose.yml up -d
```

---

## 📞 获取帮助

如果以上FAQ未能解决您的问题：

### 社区支持

- **GitCode Issues**: https://gitcode.com/spharx/agentos/issues
- **讨论区**: https://gitcode.com/spharx/agentos/discussions

### 商业支持

- **技术支持邮箱**: support@spharx.cn
- **紧急热线**: （请联系您的客户经理）

### 报告Bug时的必填信息

为了快速定位和解决问题，请提供：

1. **AgentOS版本**: `agentos-kernel --version`
2. **操作系统**: `uname -a`
3. **Docker版本**: `docker --version`
4. **复现步骤**: 最小化复现代码
5. **期望行为 vs 实际行为**
6. **相关日志**:
   ```bash
   docker logs agentos-kernel-dev > kernel.log 2>&1
   docker logs agentos-postgres-dev > postgres.log 2>&1
   ```
7. **环境变量**（脱敏后）:
   ```bash
   env | grep AGENTOS > env.txt  # 删除敏感信息后附上
   ```

---

> *"好的FAQ是用户自助解决问题的第一道防线。"*

**© 2026 SPHARX Ltd. All Rights Reserved.**
