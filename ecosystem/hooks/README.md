# AgentRT Official Hooks

> 官方 Hook 集合，基于 `HookPlugin` 基类实现，拦截 Agent/LLM 生命周期事件。

## Hook 列表

| Hook | 事件 | 类型 | 说明 |
|------|------|:---:|------|
| [audit_hook.py](audit_hook.py) | on_tool_call | audit | 记录所有工具调用到审计日志 |
| [cost_tracker.py](cost_tracker.py) | on_llm_response | metrics | 追踪 Token 消耗和 API 费用 |
| [security_reminder.py](security_reminder.py) | on_agent_start | security | 注入安全策略到 System Prompt |
| [prompt_injector.py](prompt_injector.py) | on_llm_request | prompt | 动态注入自定义 Prompt 片段 |

## 生命周期事件

```
Agent 生命周期:
├── on_agent_init        ← Agent 初始化
├── on_agent_start       ← Agent 开始执行
├── on_tool_call         ← 工具调用前（可拦截）
├── on_tool_result       ← 工具调用后
├── on_agent_stop        ← Agent 停止
└── on_agent_destroy     ← Agent 销毁

LLM 生命周期:
├── on_llm_request       ← LLM 请求发送前
├── on_llm_response      ← LLM 响应接收后
└── on_llm_error         ← LLM 调用失败
```

## 使用方式

```python
from ecosystem.hooks.security_reminder import SecurityReminderHook

hook = SecurityReminderHook()
# Hook 在 Agent 启动时自动注入安全提醒到 System Prompt
```

## 开发新 Hook

1. 继承 `HookPlugin` 基类
2. 声明监听的事件列表
3. 实现对应的事件处理方法