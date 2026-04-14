# AgentOS 故障排查与FAQ手册

**版本**: Doc V1.8 | **最后更新**: 2026-04-13 | **作者**: AgentOS Team

## 概述

本手册涵盖 AgentOS 常见故障的诊断方法、解决方案和预防措施，按系统模块分类组织。

---

## 一、网关故障

### 1.1 网关启动失败

**症状**: `gateway_d` 进程无法启动或立即退出

**诊断步骤**:

```bash
# 1. 检查端口占用
lsof -i :18789 || netstat -tlnp | grep 18789

# 2. 检查配置文件
cat agentos/gateway/config/gateway.yaml

# 3. 前台模式启动查看详细日志
./bin/gateway_d --port 18789 --foreground --log-level debug

# 4. 检查依赖服务
curl -s http://localhost:6379/ping 2>/dev/null || echo "Redis不可达"
psql -h localhost -U agentos -c "SELECT 1" 2>/dev/null || echo "PostgreSQL不可达"
```

**常见原因与解决**:

| 原因 | 错误信息 | 解决方案 |
|------|---------|---------|
| 端口被占用 | `bind: address already in use` | 更换端口或终止占用进程 |
| 配置文件缺失 | `config not found` | 检查配置路径，使用 `--config` 指定 |
| Redis不可达 | `redis connection refused` | 启动Redis: `redis-server` |
| 证书无效 | `SSL certificate error` | 检查证书路径和有效期 |
| 权限不足 | `permission denied` | 检查文件权限和运行用户 |

### 1.2 网关响应超时

**症状**: 请求发出后长时间无响应

**诊断步骤**:

```bash
# 1. 检查网关健康状态
curl -s http://localhost:18789/health | python3 -m json.tool

# 2. 检查熔断器状态
curl -s http://localhost:18789/api/v1/rpc \
  -d '{"jsonrpc":"2.0","method":"circuit_breaker.status","id":1}'

# 3. 检查服务发现状态
curl -s http://localhost:18789/api/v1/rpc \
  -d '{"jsonrpc":"2.0","method":"service.list","id":1}'

# 4. 检查系统资源
top -bn1 | head -20
df -h
free -h
```

**常见原因与解决**:

| 原因 | 诊断特征 | 解决方案 |
|------|---------|---------|
| 熔断器开启 | `circuit_breaker.status` 返回 OPEN | 等待恢复或检查下游服务 |
| 后端服务不可达 | `service.list` 显示 unhealthy | 重启后端服务 |
| 内存不足 | `free -h` 显示可用内存低 | 增加内存或减少并发 |
| 连接池耗尽 | 大量请求排队 | 增大连接池配置 |
| 死锁 | CPU低但无响应 | 重启网关，检查锁使用 |

---

## 二、协议兼容性故障

### 2.1 JSON-RPC 请求返回 Parse Error

**错误码**: -32700

**诊断**:

```bash
# 验证JSON格式
echo '{"jsonrpc":"2.0","method":"agent.list","id":1}' | python3 -m json.tool

# 常见格式问题
# ❌ 缺少jsonrpc字段
# ❌ id不是数字或字符串
# ❌ params不是对象或数组
# ❌ method不是字符串
```

**解决**: 确保请求严格遵循 JSON-RPC 2.0 规范

### 2.2 MCP 工具调用失败

**症状**: `tools/call` 返回错误

**诊断步骤**:

```bash
# 1. 验证工具是否存在
curl -s -X POST http://localhost:18789/api/v1/mcp \
  -d '{"jsonrpc":"2.0","method":"tools/list","id":1}'

# 2. 检查参数格式
# 确保arguments匹配inputSchema定义

# 3. 检查工具服务状态
curl -s http://localhost:18789/api/v1/rpc \
  -d '{"jsonrpc":"2.0","method":"service.list","params":{"protocol":"mcp"},"id":1}'
```

**常见原因**:

| 原因 | 解决方案 |
|------|---------|
| 工具名拼写错误 | 对照 `tools/list` 返回的名称 |
| 参数类型不匹配 | 检查 `inputSchema` 定义 |
| 工具服务离线 | 重启 tool_d 守护进程 |
| 权限不足 | 检查API Key和权限配置 |

### 2.3 A2A 智能体发现无结果

**症状**: `agent/discover` 返回空列表

**诊断步骤**:

```bash
# 1. 检查是否有注册的智能体
curl -s -X POST http://localhost:18789/api/v1/rpc \
  -d '{"jsonrpc":"2.0","method":"agent.list","id":1}'

# 2. 检查服务发现健康
curl -s http://localhost:18789/api/v1/rpc \
  -d '{"jsonrpc":"2.0","method":"service.discovery.status","id":1}'

# 3. 检查Redis连接（服务发现依赖Redis）
redis-cli ping
```

### 2.4 OpenAI API 兼容性问题

**症状**: Chat Completions 返回非预期格式

**诊断步骤**:

```bash
# 1. 验证请求格式
curl -v -X POST http://localhost:18789/v1/chat/completions \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer test-key" \
  -d '{"model":"agentos-default","messages":[{"role":"user","content":"test"}]}'

# 2. 检查模型列表
curl -s http://localhost:18789/v1/models \
  -H "Authorization: Bearer test-key"

# 3. 检查LLM服务状态
curl -s http://localhost:18789/api/v1/rpc \
  -d '{"jsonrpc":"2.0","method":"service.list","params":{"type":"llm"},"id":1}'
```

**常见原因**:

| 原因 | 解决方案 |
|------|---------|
| model参数错误 | 使用 `/v1/models` 查看可用模型 |
| 缺少Authorization头 | 添加 `Authorization: Bearer <key>` |
| messages格式错误 | 确保role为system/user/assistant |
| LLM服务不可达 | 检查llm_d守护进程状态 |

---

## 三、IPC与服务发现故障

### 3.1 IPC通信失败

**症状**: 服务间消息传递失败

**诊断步骤**:

```bash
# 1. 检查IPC通道状态
./scripts/agentos-debug ipc

# 2. 检查共享内存段
ipcs -m

# 3. 检查信号量
ipcs -s

# 4. 检查消息队列
ipcs -q
```

**常见原因**:

| 原因 | 解决方案 |
|------|---------|
| 共享内存不足 | 增大 `/proc/sys/kernel/shmmax` |
| 通道未创建 | 确保发送方先启动 |
| 权限不匹配 | 检查IPC对象权限 |
| 系统限制 | 调整 `/proc/sys/kernel/msgmni` |

### 3.2 服务发现延迟高

**症状**: 服务注册/发现响应慢

**诊断**:

```bash
# 检查Redis延迟
redis-cli --latency

# 检查服务发现配置
cat agentos/daemon/common/include/service_discovery.h | grep DEFAULT

# 检查心跳间隔
grep heartbeat agentos/gateway/config/*.yaml
```

**优化方案**:

| 方案 | 配置 |
|------|------|
| 缩短心跳间隔 | `health_check_interval_ms: 3000` |
| 使用本地缓存 | 启用 `svc_cache.h` |
| 调整负载均衡策略 | 使用 `LEAST_CONNECTION` |

---

## 四、熔断器故障

### 4.1 熔断器频繁开启

**症状**: 服务频繁被熔断

**诊断步骤**:

```bash
# 1. 查看熔断器状态
curl -s http://localhost:18789/api/v1/rpc \
  -d '{"jsonrpc":"2.0","method":"circuit_breaker.list","id":1}'

# 2. 查看失败原因
# 检查下游服务日志

# 3. 检查网络连通性
ping downstream-service
traceroute downstream-service
```

**调整建议**:

| 参数 | 默认值 | 建议调整 |
|------|--------|---------|
| `failure_threshold` | 5 | 提高到10（容忍更多瞬态故障） |
| `recovery_timeout_ms` | 30000 | 缩短到15000（更快恢复） |
| `half_open_max_calls` | 3 | 提高到5（更多试探请求） |

### 4.2 熔断器无法恢复

**症状**: 熔断器一直处于OPEN状态

**诊断**:

```bash
# 检查下游服务是否恢复
curl -s http://downstream-service:port/health

# 手动重置熔断器（仅紧急情况）
curl -s -X POST http://localhost:18789/api/v1/rpc \
  -d '{"jsonrpc":"2.0","method":"circuit_breaker.reset","params":{"service":"target"},"id":1}'
```

---

## 五、构建与编译故障

### 5.1 CMake配置失败

**常见错误**:

| 错误 | 原因 | 解决方案 |
|------|------|---------|
| `Could not find cJSON` | 缺少cJSON库 | `apt install libcjson-dev` |
| `Could not find libcurl` | 缺少curl库 | `apt install libcurl4-openssl-dev` |
| `Could not find libyaml` | 缺少yaml库 | `apt install libyaml-dev` |
| `CMake version too old` | CMake版本低 | 升级到3.20+ |
| `C compiler too old` | GCC版本低 | 升级到GCC 11+ |

### 5.2 编译错误

| 错误 | 原因 | 解决方案 |
|------|------|---------|
| `implicit declaration` | 缺少头文件包含 | 添加 `#include` |
| `redefinition of struct` | 重复定义 | 使用 include guards |
| `undefined reference` | 链接顺序错误 | 调整CMakeLists.txt链接顺序 |
| `pthread_create` 未定义 | 缺少pthread链接 | 添加 `-lpthread` |

---

## 六、Docker故障

### 6.1 容器启动失败

```bash
# 查看容器日志
docker compose -f docker-compose.dev.yml logs gateway

# 查看容器状态
docker compose -f docker-compose.dev.yml ps

# 进入容器调试
docker compose -f docker-compose.dev.yml exec gateway /bin/sh
```

### 6.2 服务间通信失败

```bash
# 检查Docker网络
docker network ls
docker network inspect agentos_default

# 检查DNS解析
docker compose exec gateway nslookup redis
docker compose exec gateway nslookup postgres
```

---

## 七、监控与可观测性故障

### 7.1 Prometheus指标不可用

```bash
# 检查指标端点
curl -s http://localhost:18789/metrics | head -20

# 检查Prometheus配置
cat agentos/gateway/docker/monitoring/prometheus.yml

# 检查Prometheus目标状态
# 访问 http://localhost:9090/targets
```

### 7.2 Grafana仪表盘无数据

```bash
# 检查数据源配置
# 访问 http://localhost:3000/datasources

# 检查Prometheus是否正常采集
curl -s 'http://localhost:9090/api/v1/query?query=up'

# 重新导入仪表盘
# 使用 agentos/gateway/docker/monitoring/grafana_agentos_dashboard.json
```

---

## 八、FAQ

### Q1: AgentOS 支持哪些操作系统？
**A**: Ubuntu 22.04+, macOS 13+, Windows 11 (WSL2)。生产环境推荐Ubuntu 22.04 LTS。

### Q2: 最低硬件要求是什么？
**A**: 开发环境 2核CPU/4GB内存/20GB磁盘；生产环境 4核CPU/8GB内存/50GB磁盘。

### Q3: 如何查看AgentOS版本？
**A**: `./bin/gateway_d --version` 或 `curl http://localhost:18789/health`

### Q4: 如何启用多协议支持？
**A**: 编译时添加 `-DENABLE_MCP=ON -DENABLE_A2A=ON -DENABLE_OPENAI=ON`

### Q5: 熔断器的默认超时时间是多少？
**A**: 30秒 (`recovery_timeout_ms = 30000`)，可通过配置文件调整。

### Q6: 如何配置API Key认证？
**A**: 在配置文件中设置 `gateway.auth.enabled: true` 和 `gateway.auth.keys` 列表。

### Q7: 协议转换有性能开销吗？
**A**: 协议转换延迟 < 2ms (P95)，对整体请求延迟影响可忽略。

### Q8: 如何扩展自定义协议？
**A**: 实现 `protocol_handler_t` 接口并注册到协议路由器。参考 `gateway_protocol_handler.h`。

### Q9: 服务发现支持哪些负载均衡策略？
**A**: 5种：Round-Robin、Weighted、Least-Connection、Random、Least-Load。

### Q10: 如何运行诊断工具？
**A**: `./scripts/agentos-debug` 支持网关/协议/服务/IPC/熔断/告警/配置7类检查。

### Q11: 如何贡献代码？
**A**: 参见 [CONTRIBUTING.md](../../CONTRIBUTING.md)，遵循Conventional Commits规范。

### Q12: 如何报告安全漏洞？
**A**: 发送邮件至 wangliren@spharx.cn，不要公开提交Issue。

---

## 九、诊断工具速查

| 工具 | 路径 | 用途 |
|------|------|------|
| agentos CLI | `scripts/agentos` | 统一命令行管理 |
| agentos-debug | `scripts/agentos-debug` | 7类诊断检查 |
| 协议测试 | `tests/integration/test_protocol_compatibility.py` | 10类协议测试 |
| 性能基准 | `tests/benchmarks/benchmark_performance.py` | 8项基准测试 |
| 一键发布 | `scripts/release.sh` | 7道质量门禁 |
| 文档一致性 | `scripts/tools/docs/verify_consistency.py` | 文档质量检查 |

---

## 参考文档

| 文档 | 路径 |
|------|------|
| 快速入门 | [quickstart_protocol_gateway.md](quickstart_protocol_gateway.md) |
| 最佳实践 | [best_practices.md](best_practices.md) |
| 协议集成 | [protocol_integration_guide.md](protocol_integration_guide.md) |
| 部署指南 | [deployment.md](deployment.md) |
| 安全加固 | [security-hardening.md](security-hardening.md) |
