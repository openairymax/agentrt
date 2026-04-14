# AgentOS 最佳实践指南

**版本**: Doc V1.8 | **最后更新**: 2026-04-13 | **作者**: AgentOS Team

## 概述

本指南汇总 AgentOS 开发与运维中的最佳实践，涵盖架构设计、协议集成、性能优化、安全防护和运维管理五个维度。

---

## 一、架构设计最佳实践

### 1.1 微内核原则

AgentOS 遵循微内核架构，核心原则：

| 原则 | 实践 | 反模式 |
|------|------|--------|
| 最小内核 | 内核仅包含IPC、调度、内存管理 | 在内核中实现业务逻辑 |
| 插件式扩展 | 通过守护进程(daemon)扩展功能 | 修改内核代码添加功能 |
| 消息通信 | 服务间通过IPC Service Bus通信 | 服务间直接函数调用 |
| 独立部署 | 每个守护进程可独立启停 | 所有服务耦合在同一进程 |

### 1.2 五大框架使用规范

```c
// ✅ 正确：通过统一框架接口管理生命周期
agentos_fw_manager_t* mgr = agentos_fw_manager_create();
agentos_fw_init(mgr, AGENTOS_FW_AGENT);
agentos_fw_start(mgr, AGENTOS_FW_AGENT);

// ❌ 错误：直接操作框架内部
coreloopthree_loop_t* loop = clt_loop_create();  // 绕过抽象层
```

**框架选择决策树**:

```
你的需求是什么？
├── 智能体认知/执行/记忆循环 → Agent Framework (CoreLoopThree)
├── 多层记忆存储与检索 → Memory Framework (MemoryRovol)
├── 任务调度与同步 → Task Framework (CoreKern)
├── 安全防护与审计 → Safety Framework (Cupolas)
└── 工具注册与执行 → Tool Framework (tool_d)
```

### 1.3 分层架构通信

```
应用层 → 系统调用接口 (syscalls.h)
           ↓
守护进程层 → IPC Service Bus (ipc_service_bus.h)
           ↓
公共层 → 统一工具库 (commons/utils/)
           ↓
原子层 → 内核核心 (atoms/corekern/)
```

---

## 二、协议集成最佳实践

### 2.1 协议选择指南

| 场景 | 推荐协议 | 理由 |
|------|---------|------|
| 内部微服务通信 | JSON-RPC 2.0 | 轻量、标准化、双向支持 |
| AI工具集成 | MCP | 专为LLM工具调用设计 |
| 多智能体协作 | A2A | 支持智能体发现与任务委派 |
| 兼容现有AI客户端 | OpenAI API | 最大化生态兼容性 |
| 外部未知客户端 | 自动检测 | 网关自动路由 |

### 2.2 协议转换注意事项

```c
// ✅ 正确：使用协议路由器处理转换
protocol_router_route(router, incoming_request, &outgoing_response);

// ❌ 错误：手动拼接协议消息
char* msg = malloc(1024);
sprintf(msg, "{\"jsonrpc\":\"2.0\",\"method\":\"%s\"}", mcp_method);
```

**关键约束**:
- MCP → JSON-RPC: 工具调用映射为 `tool.execute` 方法
- A2A → JSON-RPC: 智能体发现映射为 `agent.discover` 方法
- OpenAI → JSON-RPC: Chat Completions 映射为 `llm.chat` 方法
- 转换过程中保留原始请求ID用于追踪

### 2.3 熔断器配置

```c
// 生产环境推荐配置
circuit_breaker_config_t config = {
    .failure_threshold = 5,        // 5次失败触发熔断
    .recovery_timeout_ms = 30000,  // 30秒恢复等待
    .half_open_max_calls = 3,      // 半开状态最多3次试探
    .failover_strategy = AGENTOS_CB_FAILOVER_RETRY  // 失败后重试
};
```

---

## 三、性能优化最佳实践

### 3.1 性能基准参考值

| 操作 | P50 目标 | P95 目标 | P99 目标 |
|------|---------|---------|---------|
| JSON-RPC 服务列表 | < 5ms | < 20ms | < 50ms |
| MCP 工具调用 | < 10ms | < 50ms | < 100ms |
| A2A 智能体发现 | < 15ms | < 80ms | < 150ms |
| 协议转换 | < 2ms | < 10ms | < 30ms |
| 服务发现查询 | < 3ms | < 15ms | < 40ms |

### 3.2 内存管理

```c
// ✅ 正确：使用内存池减少分配开销
agentos_mem_pool_t* pool = agentos_mem_pool_create(4096, 64);
void* buf = agentos_mem_pool_alloc(pool, 256);
agentos_mem_pool_free(pool, buf);
agentos_mem_pool_destroy(pool);

// ❌ 错误：频繁malloc/free
for (int i = 0; i < 10000; i++) {
    char* buf = malloc(256);
    // ... 使用 ...
    free(buf);  // 频繁分配释放导致碎片
}
```

### 3.3 IPC优化

- 使用共享内存通道传输大块数据 (>4KB)
- 使用管道通道传输控制消息 (<4KB)
- 批量发送消息减少系统调用次数
- 启用消息压缩（高延迟网络）

### 3.4 指标导出

```c
// ✅ 正确：使用统一指标系统
agentos_metrics_counter_inc("gateway_requests_total", 1, 
    "protocol", "jsonrpc", "method", "agent.list");
agentos_metrics_histogram_observe("request_duration_seconds", 
    duration_ms / 1000.0,
    "protocol", "jsonrpc");

// ❌ 错误：自定义日志统计
log_info("request count: %d, duration: %dms", count, duration);
```

---

## 四、安全防护最佳实践

### 4.1 安全穹顶(Cupolas)四重防护

```
请求 → [输入净化] → [权限裁决] → [虚拟工位] → [审计追踪]
         Sanitizer    Permission   Workbench     Audit
```

**必须启用的安全策略**:
1. **输入净化**: 所有外部输入必须经过sanitizer处理
2. **权限裁决**: 每个系统调用必须经过权限检查
3. **虚拟工位**: 不受信任的代码必须在沙箱中执行
4. **审计追踪**: 关键操作必须记录审计日志

### 4.2 密钥管理

```c
// ✅ 正确：使用密钥库
cupolas_vault_t* vault = cupolas_vault_open("vault.db");
const char* api_key = cupolas_vault_get(vault, "openai_api_key");

// ❌ 错误：硬编码密钥
const char* api_key = "sk-xxxxxxxxxxxxxxxx";  // 绝对禁止！
```

### 4.3 熔断器安全策略

- 外部API调用必须配置熔断器
- 熔断触发后使用降级策略而非直接失败
- 半开状态限制试探请求数量
- 记录熔断事件到审计日志

---

## 五、运维管理最佳实践

### 5.1 服务发现配置

```c
// 生产环境推荐：加权最少连接策略
service_discovery_config_t config = {
    .strategy = AGENTOS_SD_LEAST_CONNECTION_WEIGHTED,
    .health_check_interval_ms = 5000,
    .health_check_timeout_ms = 3000,
    .unhealthy_threshold = 3,
    .healthy_threshold = 2
};
```

### 5.2 配置管理

```c
// ✅ 正确：使用配置管理器热加载
config_manager_t* cm = config_manager_create("config.yaml");
config_manager_watch(cm, "/gateway/protocols", on_config_change, NULL);

// ❌ 错误：直接读取配置文件
FILE* f = fopen("config.yaml", "r");  // 无热加载、无版本控制
```

### 5.3 监控告警

**必须配置的告警规则**:

| 指标 | 条件 | 级别 |
|------|------|------|
| 网关请求延迟P95 | > 500ms | Warning |
| 网关请求延迟P99 | > 1000ms | Critical |
| 熔断器开启 | 任何服务 | Critical |
| 服务健康检查失败 | 连续3次 | Critical |
| 审计日志写入失败 | 任何 | Critical |
| 内存使用率 | > 85% | Warning |
| 磁盘使用率 | > 90% | Critical |

### 5.4 Docker部署

```yaml
# 生产环境推荐配置
services:
  gateway:
    image: spharx/agentos:latest
    deploy:
      replicas: 2
      resources:
        limits:
          cpus: '2.0'
          memory: 2G
        reservations:
          cpus: '0.5'
          memory: 512M
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:18789/health"]
      interval: 10s
      timeout: 5s
      retries: 3
    restart_policy:
      condition: on-failure
      delay: 5s
      max_attempts: 5
```

---

## 六、代码质量最佳实践

### 6.1 Doxygen注释规范

```c
/**
 * @brief 创建协议路由器实例
 *
 * 创建并初始化一个新的协议路由器，支持JSON-RPC/MCP/A2A/OpenAI
 * 四种协议的自动检测与路由。
 *
 * @param config 路由器配置，NULL则使用默认配置
 * @return 协议路由器实例，失败返回NULL
 *
 * @since 2.0.0
 * @see protocol_router_destroy()
 *
 * @note 线程安全：创建后可在多线程中使用
 * @warning 调用者负责调用protocol_router_destroy()释放资源
 */
protocol_router_t* protocol_router_create(const protocol_router_config_t* config);
```

### 6.2 错误处理

```c
// ✅ 正确：使用统一错误码 + 日志
int rc = ipc_service_bus_send(bus, &msg);
if (rc != AGENTOS_OK) {
    AGENTOS_LOG_ERROR("IPC send failed: %s (rc=%d)", 
        agentos_error_string(rc), rc);
    return rc;
}

// ❌ 错误：忽略返回值
ipc_service_bus_send(bus, &msg);  // 可能静默失败
```

### 6.3 测试规范

- 单元测试覆盖率 ≥ 85%
- 新功能必须包含集成测试
- 协议兼容性测试覆盖所有4种协议
- 性能基准测试记录P50/P95/P99延迟
- 安全测试覆盖输入净化和权限检查

---

## 七、CI/CD最佳实践

### 7.1 质量门禁标准

| 门禁 | 标准模式 | 严格模式 | 发布模式 |
|------|---------|---------|---------|
| 测试覆盖率 | ≥70% | ≥85% | ≥90% |
| 文档覆盖率 | ≥60% | ≥75% | ≥80% |
| 安全漏洞(High) | 0 | 0 | 0 |
| 代码复杂度 | ≤15 | ≤10 | ≤10 |
| 协议兼容性 | 全部通过 | 全部通过 | 全部通过 |

### 7.2 发布流程

```bash
# 1. 运行一键发布脚本（自动执行质量门禁）
./scripts/release.sh 2.0.0 stable

# 2. 或手动分步执行
./scripts/release.sh 2.0.0-rc.1 rc    # 候选版
./scripts/release.sh 2.0.0 stable      # 正式版

# 紧急发布（跳过非关键门禁）
SKIP_TESTS=1 ./scripts/release.sh 2.0.1 stable
```

---

## 参考文档

| 文档 | 路径 |
|------|------|
| 架构设计 | [Capital_Architecture/](../Capital_Architecture/) |
| API参考 | [Capital_API/](../Capital_API/) |
| 编码规范 | [Capital_Specifications/](../Capital_Specifications/) |
| 协议集成 | [protocol_integration_guide.md](protocol_integration_guide.md) |
| 故障排查 | [troubleshooting_faq.md](troubleshooting_faq.md) |
