# customer-support-agent — 完整 Pipeline + Hook 系统

> 难度：高级 | 预计时间：45分钟 | 主题：Hook 系统与生产级 Pipeline

## 项目说明

本项目演示 AgentRT 的 Hook（钩子）系统，构建一个生产级的客服 Agent Pipeline。

`customer-support-agent` 配备了三个关键 Hook：
- **security_reminder**：在工具调用前检查安全性，防止敏感信息泄露
- **audit_hook**：记录所有操作到审计日志，满足合规要求
- **cost_tracker**：追踪 LLM 调用成本，防止超支

### 你将学到

- HookPlugin 的完整开发流程
- Agent 生命周期中的 Hook 注入点
- 如何构建生产级 Agent Pipeline
- 安全审计、成本控制等生产环境最佳实践

## 目录结构

```
customer-support-agent/
├── README.md                     # 本文件
├── config.yaml                   # 运行时配置（含 Hook 和安全策略）
├── agents/
│   └── support.agent.yaml        # Agent 定义，注册 Hook
└── hooks/
    └── support_hooks.py          # 自定义 Hook 实现
```

## 运行方式

```bash
# 1. 启动 Agent
agentrt run --config config.yaml

# 2. 模拟客户咨询
curl -X POST http://localhost:8080/api/v1/chat \
  -H "Content-Type: application/json" \
  -d '{"message": "我的订单 #12345 还没收到，请帮我查一下"}'

# 3. 查看审计日志
cat ./logs/audit.log

# 4. 查看成本统计
curl -X GET http://localhost:8080/api/v1/metrics
```

## 关键概念

### 1. Hook 系统 — Agent 生命周期拦截

Hook 是 AgentRT 中拦截和修改 Agent 行为的核心机制。Hook 在 Agent 生命周期的关键节点被触发：

```
Agent 生命周期：
  on_agent_start → on_llm_request → on_llm_response
      → on_tool_call → on_tool_result → on_agent_end

每个节点都可以被 Hook 拦截：
  ┌─ on_agent_start: 初始化上下文、权限检查
  ├─ on_llm_request:  请求改写、成本预估
  ├─ on_llm_response: 响应过滤、敏感信息脱敏
  ├─ on_tool_call:    安全审查、审批流程
  ├─ on_tool_result:  结果验证、审计记录
  └─ on_agent_end:    资源清理、成本统计
```

### 2. HookPlugin 开发

```python
from agentos.plugin_types import HookPlugin

class SecurityReminderHook(HookPlugin):
    """安全提醒 Hook：在工具调用前检查安全性。"""

    def on_tool_call(self, ctx, tool_name="", tool_input=None):
        # 检查是否包含敏感信息
        if self._contains_sensitive_data(tool_input):
            return HookResult(allowed=False, reason="包含敏感信息")
        return HookResult(allowed=True)

    def on_llm_response(self, ctx, response=None, usage=None):
        # 对响应进行敏感信息脱敏
        response = self._redact_sensitive(response)
        return HookResult(allowed=True, modified_data=response)
```

| Hook 方法 | 触发时机 | 典型用途 |
|-----------|---------|---------|
| `on_agent_start` | Agent 启动 | 初始化、权限检查 |
| `on_agent_end` | Agent 结束 | 资源清理、统计 |
| `on_tool_call` | 工具调用前 | 安全审查、审批 |
| `on_tool_result` | 工具返回后 | 结果验证、审计 |
| `on_llm_request` | LLM 请求前 | 请求改写、成本预估 |
| `on_llm_response` | LLM 响应后 | 过滤、脱敏、成本记录 |
| `on_memory_read` | 读取记忆 | 访问控制 |
| `on_memory_write` | 写入记忆 | 数据合规检查 |

### 3. 生产级 Pipeline

本示例的客服 Agent Pipeline：

```
用户请求
  → [security_reminder] 检查请求安全性
  → [Agent 推理] 分析用户意图
  → [on_tool_call] 审查工具调用安全性
  → [工具执行] 查询订单/账户信息
  → [on_tool_result] 记录操作到审计日志
  → [Agent 生成回答]
  → [on_llm_response] 脱敏处理 + 成本记录
  → 返回用户
```

### 4. 三个核心 Hook

| Hook | 优先级 | 职责 |
|------|--------|------|
| `security_reminder` | 100（最高） | 安全检查，阻止敏感操作 |
| `audit_hook` | 50 | 审计记录，满足合规要求 |
| `cost_tracker` | 10 | 成本追踪，防止超支 |

优先级数值越高越先执行。`security_reminder` 优先级最高，确保安全检查在所有其他 Hook 之前执行。

## 扩展建议

- **限流 Hook**：添加 `rate_limit_hook`，限制每分钟请求数
- **A/B 测试 Hook**：在 `on_llm_request` 中切换不同模型进行 A/B 测试
- **缓存 Hook**：在 `on_llm_request` 前检查缓存，命中则跳过 LLM 调用
- **告警 Hook**：当成本超阈值或安全事件频发时发送告警
- **多语言 Hook**：在 `on_llm_response` 后自动翻译响应语言
- **SLA 监控**：在 `on_agent_start` 和 `on_agent_end` 之间计算响应时间，超 SLA 告警
