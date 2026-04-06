Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 错误码手册

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪

---

## 📋 概述

本文档定义了 AgentOS 系统中所有标准错误码，包括 HTTP 状态码、JSON-RPC 错误码、业务错误码等。

### 错误响应格式

所有 API 错误遵循统一格式：

```json
{
    "error": {
        "code": 1001,
        "message": "Invalid API key",
        "details": "API key expired at 2026-04-01T00:00:00Z",
        "request_id": "req-abc123",
        "documentation_url": "https://docs.spharx.cn/errors/1001"
    },
    "jsonrpc": "2.0",
    "id": null
}
```

---

## 🌐 HTTP 状态码

### 2xx 成功

| 状态码 | 名称 | 描述 |
|--------|------|------|
| 200 | OK | 请求成功 |
| 201 | Created | 资源创建成功 |
| 202 | Accepted | 请求已接受（异步处理） |
| 204 | No Content | 成功但无返回内容 |

### 3xx 重定向

| 状态码 | 名称 | 描述 |
|--------|------|------|
| 304 | Not Modified | 资源未修改 |

### 4xx 客户端错误

| 状态码 | 名称 | 描述 | 常见原因 |
|--------|------|------|----------|
| 400 | Bad Request | 请求参数错误 | 参数缺失、格式错误、类型不匹配 |
| 401 | Unauthorized | 未认证 | 缺少或无效的认证令牌 |
| 403 | Forbidden | 无权限 | 权限不足、IP 被限制 |
| 404 | Not Found | 资源不存在 | ID 错误、资源已删除 |
| 405 | Method Not Allowed | 方法不允许 | 使用了错误的 HTTP 方法 |
| 406 | Not Acceptable | 无法接受 | Accept 头指定的格式不支持 |
| 408 | Request Timeout | 请求超时 | 客户端发送时间过长 |
| 409 | Conflict | 冲突 | 资源状态冲突（如重复创建） |
| 413 | Payload Too Large | 请求体过大 | 超过大小限制（默认 10MB） |
| 415 | Unsupported Media Type | 不支持的媒体类型 | Content-Type 不正确 |
| 422 | Unprocessable Entity | 无法处理的实体 | 语义错误（参数值无效） |
| 429 | Too Many Requests | 请求过于频繁 | 触发速率限制 |

### 5xx 服务端错误

| 状态码 | 名称 | 描述 | 处理建议 |
|--------|------|------|----------|
| 500 | Internal Server Error | 内部服务器错误 | 联系技术支持 |
| 502 | Bad Gateway | 网关错误 | 后端服务不可用 |
| 503 | Service Unavailable | 服务不可用 | 维护中或过载 |
| 504 | Gateway Timeout | 网关超时 | 后端服务响应超时 |

---

## 📡 JSON-RPC 标准错误码

| 错误码 | 名称 | 描述 |
|--------|------|------|
| -32700 | Parse error | 解析错误（无效 JSON） |
| -32600 | Invalid Request | 无效请求对象 |
| -32601 | Method not found | 方法不存在 |
| -32602 | Invalid params | 无效的方法参数 |
| -32603 | Internal error | 内部 JSON-RPC 错误 |

---

## 🔢 业务错误码 (1000-5999)

### 认证与授权 (1000-1099)

| 错误码 | HTTP 状态码 | 名称 | 描述 | 解决方案 |
|--------|------------|------|------|----------|
| **1001** | 401 | Invalid API Key | API Key 无效或过期 | 检查并更新 API Key |
| **1002** | 401 | Token Expired | JWT Token 已过期 | 刷新 Token 或重新登录 |
| **1003** | 401 | Invalid Token Format | Token 格式错误 | 检查 Token 格式（Bearer xxx） |
| **1004** | 403 | Insufficient Permissions | 权限不足 | 申请所需权限 |
| **1005** | 403 | IP Address Restricted | IP 地址被限制 | 将 IP 加入白名单 |
| **1006** | 403 | Account Suspended | 账户已被暂停 | 联系管理员 |
| **1007** | 401 | MFA Required | 需要多因素认证 | 完成 MFA 验证 |
| **1008** | 403 | Rate Limit Exceeded | 触发速率限制 | 降低请求频率，等待冷却期 |

### Agent 相关 (1100-1199)

| 错误码 | HTTP 状态码 | 名称 | 描述 | 解决方案 |
|--------|------------|------|------|----------|
| **1101** | 404 | Agent Not Found | Agent 不存在 | 检查 Agent ID |
| **1102** | 400 | Invalid Agent Name | Agent 名称无效 | 名称需符合规范（2-64字符） |
| **1103** | 409 | Agent Already Exists | Agent 已存在 | 使用不同的名称 |
| **1104** | 422 | Invalid Agent Config | Agent 配置无效 | 检查配置参数 |
| **1105** | 409 | Agent Already Running | Agent 已在运行 | 先停止再启动 |
| **1106** | 409 | Agent Not Running | Agent 未运行 | 先启动 Agent |
| **1107** | 500 | Agent Startup Failed | Agent 启动失败 | 查看日志获取详细信息 |
| **1108** | 500 | Agent Crashed | Agent 崩溃 | 查看崩溃日志和堆栈信息 |
| **1109** | 403 | Agent Quota Exceeded | Agent 数量达到上限 | 升级套餐或删除不需要的 Agent |
| **1110** | 500 | Agent Timeout | Agent 执行超时 | 增加超时时间或优化任务 |

### 任务相关 (1200-1299)

| 错误码 | HTTP 状态码 | 名称 | 描述 | 解决方案 |
|--------|------------|------|------|----------|
| **1201** | 404 | Task Not Found | 任务不存在 | 检查任务 ID |
| **1202** | 400 | Invalid Task Name | 任务名称无效 | 检查名称格式 |
| **1203** | 422 | Invalid Task Payload | 任务负载无效 | 检查 payload 结构 |
| **1204** | 409 | Task Already Completed | 任务已完成 | 不能对已完成的任务操作 |
| **1205** | 409 | Task Cancelled | 任务已取消 | 创建新任务 |
| **1206** | 409 | Task Execution Failed | 任务执行失败 | 查看错误详情并重试 |
| **1207** | 408 | Task Timeout | 任务执行超时 | 增加超时时间 |
| **1208** | 403 | Task Queue Full | 任务队列已满 | 稍后重试或增加队列容量 |
| **1209** | 422 | Invalid Schedule Expression | 调度表达式无效 | 检查 Cron 表达式语法 |
| **1210** | 500 | Scheduler Error | 调度器内部错误 | 联系技术支持 |

### 记忆系统相关 (1300-1399)

| 错误码 | HTTP 状态码 | 名称 | 描述 | 解决方案 |
|--------|------------|------|------|----------|
| **1301** | 404 | Memory Record Not Found | 记录不存在 | 检查记录 ID |
| **1302** | 400 | Invalid Query | 查询无效 | 检查查询参数 |
| **1303** | 413 | Data Too Large | 数据过大 | 分批存储或压缩数据 |
| **1304** | 500 | Memory Index Error | 内存索引错误 | 重建索引或联系支持 |
| **1305** | 503 | Memory Service Unavailable | 记忆服务不可用 | 稍后重试 |
| **1306** | 422 | Embedding Failed | 向量化生成失败 | 检查输入文本内容 |
| **1307** | 500 | L2 Index Corrupted | L2 索引损坏 | 重建 FAISS 索引 |
| **1308** | 500 | L3 Graph Error | 图数据库错误 | 检查图数据库连接 |
| **1309** | 500 | L4 Mining Error | 模式挖掘错误 | 查看详细日志 |
| **1310** | 403 | Memory Quota Exceeded | 存储配额用尽 | 清理旧数据或升级配额 |

### LLM 服务相关 (1400-1499)

| 错误码 | HTTP 状态码 | 名称 | 描述 | 解决方案 |
|--------|------------|------|------|----------|
| **1401** | 404 | Model Not Found | 模型不存在 | 检查模型名称 |
| **1402** | 400 | Invalid Model Parameters | 模型参数无效 | 检查参数范围 |
| **1403** | 413 | Prompt Too Long | Prompt 过长 | 缩短文本或分段处理 |
| **1404** | 500 | LLM Provider Error | LLM 提供商错误 | 检查提供商状态 |
| **1405** | 402 | Insufficient Credits | 余额不足 | 充值或切换模型 |
| **1406** | 500 | Embedding Error | 向量化错误 | 检查文本编码 |
| **1407** | 429 | Rate Limited by Provider | 提供商限流 | 降低请求频率 |
| **1408** | 500 | Streaming Error | 流式传输错误 | 重试或使用非流式模式 |
| **1409** | 400 | Invalid Response Format | 响应格式无效 | 检查模型输出格式设置 |
| **1410** | 500 | Context Length Exceeded | 上下文长度超出限制 | 减少历史消息数量 |

### 工具执行相关 (1500-1599)

| 错误码 | HTTP 状态码 | 名称 | 描述 | 解决方案 |
|--------|------------|------|------|----------|
| **1501** | 404 | Tool Not Found | 工具不存在 | 检查工具名称 |
| **1502** | 400 | Invalid Tool Parameters | 工具参数无效 | 检查参数 schema |
| **1503** | 403 | Tool Execution Denied | 工具执行被拒绝 | 检查权限配置 |
| **1504** | 500 | Tool Execution Failed | 工具执行失败 | 查看工具日志 |
| **1505** | 408 | Tool Timeout | 工具执行超时 | 增加超时时间 |
| **1506** | 500 | Sandbox Error | 沙箱错误 | 检查沙箱配置 |
| **1507** | 403 | Resource Limit Exceeded | 资源限制超出 | 减少资源使用 |
| **1508** | 500 | Network Access Denied | 网络访问被拒绝 | 启用网络权限 |
| **1509** | 400 | Invalid Output Format | 输出格式无效 | 检查工具输出解析 |
| **1510** | 500 | Dependency Missing | 依赖缺失 | 安装所需依赖 |

### 安全相关 (1600-1699)

| 错误码 | HTTP 状态码 | 名称 | 描述 | 解决方案 |
|--------|------------|------|------|----------|
| **1601** | 400 | Input Sanitization Failed | 输入净化失败 | 清理特殊字符 |
| **1602** | 403 | SQL Injection Detected | 检测到 SQL 注入 | 移除危险输入 |
| **1603** | 403 | XSS Attack Detected | 检测到 XSS 攻击 | 转义 HTML 字符 |
| **1604** | 403 | Command Injection Detected | 检测到命令注入 | 移除 shell 元字符 |
| **1605** | 403 | Path Traversal Detected | 检测到路径遍历 | 使用规范化路径 |
| **1606** | 403 | SSRF Attempt Detected | 检测到 SSRF 尝试 | 验证 URL 白名单 |
| **1607** | 500 | Audit Log Write Failure | 审计日志写入失败 | 检查磁盘空间和权限 |
| **1608** | 500 | Permission Check Error | 权限检查错误 | 检查权限规则文件 |
| **1609** | 403 | Certificate Invalid | 证书无效 | 更新 TLS 证书 |
| **1610** | 429 | Brute Force Detected | 暴力破解检测 | 使用验证码或锁定账户 |

### 系统相关 (1700-1799)

| 错误码 | HTTP 状态码 | 名称 | 描述 | 解决方案 |
|--------|------------|------|------|----------|
| **1701** | 500 | Internal Server Error | 内部服务器错误 | 查看服务器日志 |
| **1702** | 503 | Service Unavailable | 服务暂时不可用 | 稍后重试 |
| **1703** | 503 | Maintenance Mode | 维护模式 | 等待维护完成 |
| **1704** | 500 | Database Connection Error | 数据库连接错误 | 检查数据库连接字符串 |
| **1705** | 500 | Cache Error | 缓存错误 | 检查 Redis/Memcached 连接 |
| **1706** | 500 | IPC Communication Error | IPC 通信错误 | 检查内核服务状态 |
| **1707** | 500 | Configuration Error | 配置错误 | 检查配置文件语法 |
| **1708** | 413 | Request Entity Too Large | 请求体过大 | 减小请求体大小 |
| **1709** | 500 | Serialization Error | 序列化错误 | 检查数据格式兼容性 |
| **1710** | 500 | Deserialization Error | 反序列化错误 | 检查数据完整性 |

---

## 🔧 错误处理最佳实践

### Python 示例

```python
from agentos import (
    AgentOSError,
    AuthenticationError,
    NotFoundError,
    RateLimitError,
    ValidationError
)

def safe_chat(agent, message):
    try:
        response = agent.chat(message)
        return response.content
    except AuthenticationError as e:
        print(f"❌ 认证失败: {e}")
        print("请检查 API Key 是否正确")
        return None
    except RateLimitError as e:
        print(f"⏳ 请求过于频繁，请在 {e.retry_after} 秒后重试")
        time.sleep(e.retry_after)
        return safe_chat(agent, message)  # 自动重试
    except ValidationError as e:
        print(f"⚠️ 参数验证失败: {e.details}")
        return None
    except NotFoundError as e:
        print(f"🔍 资源未找到: {e.resource_type}={e.resource_id}")
        return None
    except AgentOSError as e:
        print(f"❌ AgentOS 错误 [{e.code}]: {e.message}")
        if e.documentation_url:
            print(f"📖 文档: {e.documentation_url}")
        return None
```

### Go 示例

```go
package main

import (
    "fmt"
    "time"

    agentos "github.com/spharx/agentos-go-sdk"
)

func safeChat(agent *agentos.Agent, message string) (string, error) {
    resp, err := agent.Chat(context.Background(), &agentos.ChatRequest{
        Message: message,
    })

    var agentErr *agentos.Error
    if errors.As(err, &agentErr) {
        switch agentErr.Code {
        case 1001, 1002:
            return "", fmt.Errorf("认证失败: %s", agentErr.Message)
        case 1008:
            retryAfter := time.Duration(agentErr.RetryAfter) * time.Second
            fmt.Printf("限流中，等待 %v...\n", retryAfter)
            time.Sleep(retryAfter)
            return safeChat(agent, message)
        case 1101:
            return "", fmt.Errorf("Agent 不存在")
        default:
            return "", fmt.Errorf("错误 [%d]: %s", agentErr.Code, agentErr.Message)
        }
    }

    if err != nil {
        return "", fmt.Errorf("未知错误: %w", err)
    }

    return resp.Content, nil
}
```

---

## 📊 错误监控与告警

### Prometheus 指标

```yaml
# 错误计数指标
- name: agentos_errors_total
  type: Counter
  help: "按错误码分类的错误总数"
  labels: [error_code, http_status, service]

# 错误率指标
- name: agentos_error_rate
  type: Gauge
  help: "错误率（百分比）"
  labels: [service, endpoint]
```

### Grafana 告警规则

```yaml
groups:
  - name: agentos_error_alerts
    rules:
      - alert: HighErrorRate
        expr: |
          sum(rate(agentos_errors_total[5m])) /
          sum(rate(http_requests_total[5m])) > 0.05
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "AgentOS 错误率超过 5%"
          description: "错误率: {{ $value | humanizePercentage }}"

      - alert: CriticalAuthFailures
        expr: |
          sum(rate(agentos_errors_total{error_code=~"100[1-8]"}[1m])) > 10
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "认证失败激增"
          description: "每分钟超过 10 次认证失败"
```

---

## 📞 获取帮助

### 自助诊断

```bash
# 查看 AgentOS 日志中的错误
journalctl -u agentos-kernel --since "1 hour ago" | grep ERROR

# 检查错误码详细信息
curl https://docs.spharx.cn/api/errors/{error_code}

# 运行诊断工具
agentos-diagnose --check-all
```

### 联系支持

- **一般问题**: docs@spharx.cn
- **技术支持**: support@spharx.cn
- **安全问题**: wangliren@spharx.cn
- **GitHub Issues**: https://github.com/SpharxTeam/AgentOS/issues

---

## 📚 相关文档

- **[内核 API](kernel-api.md)** — API 接口参考
- **[守护进程 API](daemon-api.md)** — Daemon 服务接口
- **[Python SDK](python-sdk.md)** — Python SDK 错误处理
- **[Go SDK](go-sdk.md)** — Go SDK 错误处理
- **[故障排查指南](../troubleshooting/common-issues.md)** — 常见问题解决

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
