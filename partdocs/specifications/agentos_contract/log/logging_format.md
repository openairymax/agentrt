# AgentOS 日志格式规范

**版本**: Doc V1.5  
**状态**: 正式发布  
**归属**: AgentOS 可观测性核心规范  
**许可证**: GPL-3.0  
**理论基础**: 可观测性设计原则、系统工程方法 (总体协调)  
**关联规范**: [通信协议规范](./protocol_contract.md)、[系统调用 API 规范](./syscall_api_contract.md)、[日志打印规范](../coding_standard/Log_guide.md)

---

## 编制说明

### 本文档定位

日志格式规范是 AgentOS 规范体系的核心组成部分，属于**操作层规范**。本规范定义了 AgentOS 各组件的日志输出格式、字段含义、级别定义及存储策略，是实现系统可观测性的基础。

### 与设计哲学的关系

本规范根植于 AgentOS 设计哲学 [1] 的核心理论：

- **可观测性设计**: 日志是可观测性的三大支柱之一 (日志、指标、追踪),本规范确保日志的统一性和可追溯性
- **系统工程方法**: 通过统一的日志格式，实现跨组件、跨层级的行为分析和故障诊断
- **安全内生思想**: 日志内置审计字段，支持安全事件追溯和合规审计
- **层次分解方法**: 日志记录器名称反映模块层次结构，便于问题定位

### 适用范围

本规范适用于以下场景：

1. **内核开发者**: atoms/模块的日志记录
2. **服务开发者**: backs/守护进程的日志记录
3. **应用开发者**: openhub/app/应用的日志记录
4. **运维人员**: 日志收集、聚合和分析
5. **安全审计人员**: 审计日志审查和合规检查

### 术语定义

本规范使用的术语定义见 [统一术语表](../TERMINOLOGY.md),关键术语包括：

| 术语 | 简要定义 | 来源 |
|------|---------|------|
| TraceID | 分布式追踪的唯一标识 | [通信协议规范](./protocol_contract.md) |
| SpanID | 追踪跨度的唯一标识 | 本规范 |
| 结构化日志 | JSON 格式的机器可读日志 | 本规范 |

---

## 第 1 章 引言

### 1.1 背景与意义

AgentOS 作为一个分布式多智能体操作系统，其运行涉及内核、服务层、安全层、应用层等多个组件，跨进程、跨主机协作。为了有效诊断问题、追踪请求链路、分析系统行为，必须建立统一的日志格式规范。

**挑战:**
- **异构组件**: 内核 (C)、服务 (Python/Go/Node.js)、应用 (多语言) 使用不同的技术栈
- **分布式追踪**: 一个请求可能跨越多个服务和主机
- **海量日志**: 高并发场景下每天产生数十亿条日志
- **安全合规**: 需要满足审计和合规要求

**解决方案:**
- 统一的 JSON 日志格式
- 标准化的字段命名和类型
- 分布式的 TraceID/SpanID 机制
- 分层次的日志级别定义

### 1.2 目标与范围

本规范旨在实现以下目标：

1. **结构化**: 日志以 JSON 格式输出，便于机器解析和自动聚合
2. **可追溯**: 每条日志携带 `trace_id` 和 `span_id`,支持分布式追踪
3. **可审计**: 关键操作日志包含 `agent_id`、`session_id` 等字段，满足审计需求
4. **可扩展**: 允许各模块添加自定义字段，但必须遵循核心字段规范
5. **高性能**: 最小化日志记录开销，避免影响系统性能

### 1.3 设计原则

#### 原则 1-1【统一格式】：所有组件使用相同的 JSON 行格式

**解释**: 统一的格式降低日志采集、聚合和分析的复杂度。

**实施指南:**
- 所有日志输出为单行 JSON 对象
- 不使用 Pretty Print，减少体积
- 字段顺序保持一致 (可选，但推荐)

#### 原则 1-2【最小开销】：核心字段最小化，避免日志体积膨胀

**解释**: 日志是系统的副产品，不应过度消耗资源。

**实施指南:**
- 只记录必需的核心字段 (8 个)
- 可选字段按需添加
- 避免记录大对象 (如完整响应体)

#### 原则 1-3【安全脱敏】：敏感信息不应出现在日志中

**解释**: 日志可能泄露敏感信息，必须在输出前脱敏。

**实施指南:**
- 密码、Token、API Key 等必须脱敏
- 个人信息 (PII) 应加密或哈希
- 生产环境禁止记录调试级别的敏感数据

---

## 第 2 章 日志格式

### 2.1 基本格式

每条日志为一行 JSON 对象，不包含换行符。

**示例:**
```json
{"timestamp":1701234567.890,"level":"info","logger":"agentos.cognition","trace_id":"abc123","session_id":"sess_456","message":"User input received","file":"router.c","line":128,"input_preview":"开发电商应用"}
```

**格式要求:**
- 单行 JSON，无换行
- 键名使用双引号
- 字符串值使用双引号
- 数字不使用引号
- 不使用注释

### 2.2 必需字段

所有日志必须包含以下 8 个核心字段：

| 字段 | 类型 | 说明 | 示例 | 备注 |
| :--- | :--- | :--- | :--- | :--- |
| `timestamp` | number | Unix 时间戳 (秒，支持小数毫秒) | `1701234567.890` | 精度至少到毫秒 |
| `level` | string | 日志级别 (见 3.1 节) | `"info"` | 小写 |
| `logger` | string | 日志记录器名称 | `"agentos.cognition"` | 反向域名格式 |
| `trace_id` | string | 分布式追踪 ID(若存在) | `"abc123"` | 可选，但强烈推荐 |
| `session_id` | string | 会话 ID(若存在) | `"sess_456"` | 可选 |
| `message` | string | 人类可读的日志消息 | `"User input received"` | 简洁明了 |
| `file` | string | 源文件名 | `"router.c"` | 相对路径 |
| `line` | integer | 源文件行号 | `128` | 整数 |

**字段详细说明:**

#### timestamp (时间戳)

- **类型**: number (浮点数)
- **格式**: Unix Epoch 秒数，小数部分表示毫秒
- **时区**: UTC
- **精度**: 至少到毫秒 (3 位小数)

**生成方式 (C 语言):**
```c
#include <sys/time.h>

struct timeval tv;
gettimeofday(&tv, NULL);
double timestamp = tv.tv_sec + tv.tv_usec / 1000000.0;
// 示例：1701234567.890123
```

**生成方式 (Python):**
```python
import time
timestamp = time.time()
# 示例：1701234567.890
```

#### level (日志级别)

- **类型**: string
- **取值**: debug, info, warn, error, critical (小写)
- **说明**: 详见 3.1 节

#### logger (记录器名称)

- **类型**: string
- **格式**: `agentos.<module>.<submodule>`
- **说明**: 反向域名格式，反映模块层次

**示例:**
```
agentos.cognition              # 认知层
agentos.cognition.planner      # 认知层 - 规划器
agentos.execution              # 执行层
agentos.memory                 # 记忆层
agentos.kernel.syscall         # 内核 - 系统调用
agentos.services.llm_d         # 服务 - LLM 守护进程
agentos.apps.ecommerce         # 应用 - 电商应用
```

#### trace_id (追踪 ID)

- **类型**: string
- **格式**: 16 进制字符串，建议 12-32 字符
- **说明**: 分布式追踪的唯一标识，贯穿整个请求链路

**生成方式:**
```python
import uuid
trace_id = uuid.uuid4().hex[:16]  # 16 字符
# 示例："abc123def456789"
```

#### session_id (会话 ID)

- **类型**: string
- **格式**: `sess_` 前缀 + 随机字符串
- **说明**: 用户会话的唯一标识

**示例:**
```
sess_456abc789def
sess_user_alice_20260322
```

#### message (日志消息)

- **类型**: string
- **要求**:
  - 简洁明了，一句话描述发生了什么
  - 使用过去时态 (已发生)
  - 避免模糊词汇 ("处理中"、"某事")
  - 包含关键上下文信息

**好的示例:**
```
"User input received"
"Task submitted successfully"
"Memory search completed with 5 results"
"Session created for user alice"
```

**差的示例:**
```
"Processing..."           # 太模糊
"Do something"            # 无意义
"Error occurred"          # 缺少上下文
"Task done"               # 不够具体
```

#### file (源文件名)

- **类型**: string
- **格式**: 相对路径，从项目根目录开始
- **说明**: 便于快速定位代码位置

**示例:**
```
atoms/corekern/src/router.c
backs/llm_d/main.py
openhub/app/ecommerce/api.py
```

#### line (行号)

- **类型**: integer
- **说明**: 源代码的行号，从 1 开始计数

### 2.3 可选字段

根据场景需要，可以添加以下可选字段：

| 字段 | 类型 | 说明 | 适用场景 | 示例 |
| :--- | :--- | :--- | :--- | :--- |
| `span_id` | string | 当前跨度 ID | 分布式追踪 | `"span_789"` |
| `parent_span_id` | string | 父跨度 ID | 分布式追踪 | `"span_456"` |
| `error` | string | 错误信息 | 错误日志 | `"Connection timeout"` |
| `error_code` | integer | 错误码 | 错误日志 | `-1` (AGENTOS_EINVAL) |
| `duration_ms` | number | 操作耗时 (毫秒) | 性能日志 | `125.5` |
| `agent_id` | string | Agent ID | Agent 相关日志 | `"com.agentos.pm.v1"` |
| `task_id` | string | 任务 ID | 任务执行日志 | `"task_abc123"` |
| `record_id` | string | 记忆记录 ID | 记忆操作日志 | `"mem_xyz789"` |
| `input_preview` | string | 输入预览 (截断后) | 用户输入日志 | `"开发电商应用"` |
| `output_preview` | string | 输出预览 (截断后) | 响应日志 | `"{\"result\": \"success\"}"` |
| `user_data` | object | 自定义结构化数据 | 扩展信息 | `{"user_id": "alice"}` |
| `request_id` | string | 请求 ID | HTTP 网关日志 | `"req_123456"` |
| `method` | string | 方法名 | RPC 日志 | `"llm.complete"` |
| `params_size` | number | 参数大小 (字节) | 性能分析 | `1024` |
| `response_size` | number | 响应大小 (字节) | 性能分析 | `2048` |

**使用建议:**
- 按需添加，避免滥用
- 保持字段命名一致
- 复杂对象使用嵌套 JSON

### 2.4 字段命名规范

#### 命名规则

- **风格**: snake_case (下划线分隔的小写字母)
- **长度**: 建议不超过 32 字符
- **前缀**: 避免使用保留字 (`timestamp`, `level`, `message` 等)

**推荐做法:**
```python
# ✅ 好的命名
input_preview
output_preview
user_id
task_duration_ms
cognition_plan_id
```

**不推荐做法:**
```python
# ❌ 坏的命名
inputPreview          # camelCase
INPUT_PREVIEW         # 大写
preview               # 语义不明确
myCustomField         # 没有上下文
```

#### 自定义字段前缀

为避免字段冲突，自定义字段建议以模块名作为前缀：

```
# 认知层模块
cognition_plan_id
cognition_model_used

# 执行层模块
execution_tool_name
execution_result_type

# 记忆层模块
memory_search_query
memory_result_count
```

---

## 第 3 章 日志级别

### 3.1 级别定义

AgentOS 使用标准日志级别，按严重性递增：

| 级别 | 数值 | 名称 | 说明 | 示例 |
| :--- | :--- | :--- | :--- | :--- |
| 0 | `DEBUG` | 调试 | 调试信息，仅开发环境 | 变量值、函数进入/退出 |
| 1 | `INFO` | 信息 | 常规信息，记录正常流程 | 服务启动、请求到达、任务完成 |
| 2 | `WARN` | 警告 | 警告信息，潜在问题 | 重试、配置缺失、降级 |
| 3 | `ERROR` | 错误 | 错误信息，操作失败 | API 调用失败、内存不足 |
| 4 | `CRITICAL` | 严重错误 | 严重错误，可能导致系统崩溃 | 致命异常、关键组件不可用 |

### 3.2 使用指南

#### DEBUG (调试级别)

**用途**: 开发和调试阶段使用，记录详细的执行细节。

**适用场景:**
- 函数进入/退出
- 变量值和中间状态
- 条件分支选择
- 循环迭代信息

**示例:**
```json
{"timestamp":1701234567.890,"level":"debug","logger":"agentos.cognition","message":"Entering generate_plan function","file":"planner.c","line":45,"plan_type":"dag"}
```

**⚠️ 注意**: 生产环境应关闭 DEBUG 日志，避免影响性能。

#### INFO (信息级别)

**用途**: 记录系统正常运行的重要事件。

**适用场景:**
- 服务启动/停止
- 请求到达/完成
- 任务提交/完成
- 用户操作记录
- 配置加载

**示例:**
```json
{"timestamp":1701234567.890,"level":"info","logger":"agentos.services.llm_d","message":"LLM request processed successfully","trace_id":"abc123","duration_ms":1250,"tokens_used":45}
```

#### WARN (警告级别)

**用途**: 记录潜在问题或不推荐的行为，但不影响系统继续运行。

**适用场景:**
- 自动重试
- 配置项缺失 (使用默认值)
- 性能降级
- 废弃 API 调用
- 资源接近配额

**示例:**
```json
{"timestamp":1701234567.890,"level":"warn","logger":"agentos.memory","message":"Memory search timeout, returning partial results","trace_id":"abc123","timeout_ms":1000,"results_found":3}
```

#### ERROR (错误级别)

**用途**: 记录错误事件，操作失败但系统仍可继续运行。

**适用场景:**
- API 调用失败
- 数据库连接失败
- 文件读写错误
- 参数校验失败
- 权限检查失败

**示例:**
```json
{"timestamp":1701234567.890,"level":"error","logger":"agentos.kernel.syscall","message":"Task submit failed: invalid parameters","trace_id":"abc123","error_code":-1,"error":"Input length exceeds limit"}
```

#### CRITICAL (严重错误级别)

**用途**: 记录严重错误事件，可能导致系统崩溃或数据丢失。

**适用场景:**
- 内存溢出
- 磁盘已满
- 关键依赖不可用
- 数据损坏
- 安全漏洞

**示例:**
```json
{"timestamp":1701234567.890,"level":"critical","logger":"agentos.kernel.core","message":"Out of memory, cannot allocate task context","error_code":-2,"available_memory_mb":0}
```

### 3.3 级别配置

#### 开发环境

推荐配置：
```yaml
logging:
  level: DEBUG
  format: json
  output: stdout
```

#### 生产环境

推荐配置：
```yaml
logging:
  level: INFO
  format: json
  output: file
  rotation:
    max_size_mb: 100
    max_files: 10
    compress: true
```

---

## 第 4 章 日志存储

### 4.1 存储路径

日志文件统一存放在 `partdata/logs/` 目录下，按模块分类：

```
partdata/logs/
├── kernel/                 # 内核日志 (atoms/ 模块)
│   ├── core.log
│   ├── coreloopthree.log
│   ├── memoryrovol.log
│   └── syscall.log
├── services/               # 服务层日志 (backs/)
│   ├── llm_d.log
│   ├── market_d.log
│   ├── monit_d.log
│   ├── sched_d.log
│   └── tool_d.log
├── apps/                   # 应用层日志 (openhub/app/)
│   ├── ecommerce.log
│   └── videoedit.log
└── traces/                 # 追踪数据 (OpenTelemetry spans)
    └── spans/
```

### 4.2 日志轮转

#### 轮转策略

采用基于大小和时间的混合轮转策略：

- **大小限制**: 单个文件最大 100MB
- **时间限制**: 每天轮转一次
- **保留策略**: 保留最近 10 个文件
- **压缩**: 旧文件使用 gzip 压缩

#### 文件命名

轮转后的文件命名格式：
```
{module}.log.{timestamp}.{gz}
```

**示例:**
```
llm_d.log.20260322.gz
syscall.log.20260321.gz
```

### 4.3 日志收集

#### 收集方式

推荐使用 Filebeat 或 Fluentd 进行日志收集：

**Filebeat 配置示例:**
```yaml
filebeat.inputs:
- type: log
  enabled: true
  paths:
    - partdata/logs/**/*.log
  json.keys_under_root: true
  processors:
    - add_host_metadata: ~
    - add_cloud_metadata: ~

output.elasticsearch:
  hosts: ["localhost:9200"]
  index: "agentos-%{+yyyy.MM.dd}"
```

#### 索引策略

Elasticsearch 索引命名：
```
agentos-{date}
```

**生命周期管理:**
- 热节点 (Hot): 最近 3 天，SSD 存储
- 温节点 (Warm): 3-30 天，HDD 存储
- 冷节点 (Cold): 30 天以上，归档或删除

---

## 第 5 章 分布式追踪

### 5.1 TraceID 贯穿

每个外部请求在入口处生成唯一 TraceID，通过以下方式传递：

#### HTTP 请求

**请求头:**
```http
X-Trace-ID: abc123def456
```

**处理逻辑:**
```python
@app.route('/rpc')
def rpc_handler():
    trace_id = request.headers.get('X-Trace-ID', generate_trace_id())
    set_global_trace_id(trace_id)
    # 后续所有日志自动携带 trace_id
```

#### JSON-RPC 参数

**请求体:**
```json
{
  "jsonrpc": "2.0",
  "method": "llm.complete",
  "params": {
    "_trace_id": "abc123def456"
  },
  "id": 1
}
```

**处理逻辑:**
```python
def handle_request(req):
    trace_id = req['params'].get('_trace_id', generate_trace_id())
    with trace_context(trace_id):
        # 执行请求
        pass
```

### 5.2 SpanID 机制

#### Span 概念

Span 代表追踪中的一个操作单元，包含：

- **span_id**: 当前操作的唯一标识
- **parent_span_id**: 父操作的 span_id
- **trace_id**: 所属的追踪 ID

#### Span 层级关系

```
Trace: abc123def456
└─ Span: span_001 (HTTP Gateway)
   └─ Span: span_002 (LLM Service)
      └─ Span: span_003 (Model Inference)
```

**日志示例:**
```json
// HTTP 网关日志
{"timestamp":1701234567.890,"level":"info","logger":"agentos.gateway","message":"Request received","trace_id":"abc123","span_id":"span_001"}

// LLM 服务日志
{"timestamp":1701234568.123,"level":"info","logger":"agentos.llm_d","message":"Processing LLM request","trace_id":"abc123","span_id":"span_002","parent_span_id":"span_001"}

// 模型推理日志
{"timestamp":1701234569.456,"level":"info","logger":"agentos.inference","message":"Model inference completed","trace_id":"abc123","span_id":"span_003","parent_span_id":"span_002","duration_ms":1234}
```

### 5.3 追踪上下文传播

#### 进程内传播

使用线程本地存储 (TLS) 或异步上下文：

**Python 示例 ( threading):**
```python
import threading

_trace_context = threading.local()

def set_trace_context(trace_id, span_id):
    _trace_context.trace_id = trace_id
    _trace_context.span_id = span_id

def get_trace_context():
    return getattr(_trace_context, 'trace_id', None), getattr(_trace_context, 'span_id', None)
```

**Python 示例 (asyncio):**
```python
import contextvars

_trace_context = contextvars.ContextVar('trace_context', default=(None, None))

async def set_trace_context(trace_id, span_id):
    _trace_context.set((trace_id, span_id))

async def get_trace_context():
    return _trace_context.get()
```

#### 跨进程传播

通过 RPC 或消息队列传递追踪上下文：

**gRPC Metadata:**
```python
metadata = [('x-trace-id', trace_id), ('x-span-id', span_id)]
response = stub.Call(request, metadata=metadata)
```

**消息队列 Headers:**
```python
producer.send(
    topic='tasks',
    value=message,
    headers={'trace_id': trace_id.encode(), 'span_id': span_id.encode()}
)
```

---

## 第 6 章 安全与审计

### 6.1 敏感信息脱敏

#### 必须脱敏的字段

以下字段在记录到日志前必须脱敏：

| 字段类型 | 示例 | 脱敏方式 |
|---------|------|---------|
| 密码 | `password` | 完全不记录 |
| Token | `api_key`, `access_token` | 只显示前 4 后 4 字符 |
| 个人身份信息 | `email`, `phone` | 哈希或掩码 |
| 支付信息 | `credit_card` | 完全不记录 |

#### 脱敏实现

**Token 脱敏示例:**
```python
def mask_token(token):
    if len(token) <= 8:
        return "****"
    return f"{token[:4]}****{token[-4:]}"

# 使用
api_key = "sk_abc123def456ghi789"
logger.info(f"API key used: {mask_token(api_key)}")
# 输出："API key used: sk_a****i789"
```

**邮箱脱敏示例:**
```python
import hashlib

def hash_email(email):
    return hashlib.sha256(email.encode()).hexdigest()[:16]

# 使用
user_email = "alice@example.com"
logger.info(f"User email: {hash_email(user_email)}")
# 输出："User email: a1b2c3d4e5f6g7h8"
```

### 6.2 审计日志

#### 审计事件类型

以下事件必须记录审计日志：

1. **认证事件**: 登录成功/失败、登出、Token 刷新
2. **授权事件**: 权限变更、角色分配
3. **数据访问**: 敏感数据读取、修改、删除
4. **配置变更**: 系统配置修改、用户配置修改
5. **安全事件**: 入侵检测、异常访问、违规操作

#### 审计日志格式

审计日志应包含额外字段：

```json
{
  "timestamp": 1701234567.890,
  "level": "audit",
  "logger": "agentos.audit",
  "trace_id": "abc123",
  "session_id": "sess_456",
  "message": "User login successful",
  "event_type": "AUTH_LOGIN",
  "actor_id": "user_alice",
  "target_resource": "system",
  "action": "login",
  "result": "success",
  "ip_address": "192.168.1.100",
  "user_agent": "Mozilla/5.0..."
}
```

---

## 第 7 章 最佳实践

### 7.1 实践指南

#### 实践 7-1【结构化消息】：日志消息应结构化，便于查询分析

**解释**: 将固定信息提取为字段，而不是全部放在 message 中。

**✅ 推荐做法:**
```json
{
  "level": "info",
  "message": "Task completed",
  "task_id": "task_123",
  "duration_ms": 1234,
  "result": "success"
}
```

**❌ 不推荐做法:**
```json
{
  "level": "info",
  "message": "Task task_123 completed in 1234ms with result success"
}
```

#### 实践 7-2【控制日志量】：避免高频低价值日志

**解释**: 过多的日志会淹没重要信息，增加存储成本。

**✅ 推荐做法:**
```python
# 使用采样
if random.random() < 0.01:  # 1% 采样率
    logger.debug("Processing batch item %d", item_id)
```

**❌ 不推荐做法:**
```python
# 每条都记录
for item in large_batch:
    logger.debug("Processing item %d", item.id)  # 可能产生数万条日志
```

#### 实践 7-3【使用日志上下文管理器】：自动注入公共字段

**解释**: 使用上下文管理器自动设置 trace_id、session_id 等字段。

**实现示例:**
```python
from contextlib import contextmanager

@contextmanager
def log_context(**kwargs):
    old_context = get_log_context()
    new_context = {**old_context, **kwargs}
    set_log_context(new_context)
    try:
        yield
    finally:
        set_log_context(old_context)

# 使用
with log_context(trace_id="abc123", session_id="sess_456"):
    process_request()  # 内部所有日志自动携带上下文
```

### 7.2 性能优化

#### 异步日志

使用异步写入避免阻塞主线程：

```python
import logging
import concurrent.futures

executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)

class AsyncHandler(logging.Handler):
    def emit(self, record):
        executor.submit(self._write, record)
    
    def _write(self, record):
        # 实际写入文件或网络
        pass
```

#### 批量写入

累积多条日志后批量写入：

```python
class BatchHandler:
    def __init__(self, batch_size=100, flush_interval=5):
        self.buffer = []
        self.batch_size = batch_size
        self.flush_interval = flush_interval
    
    def write(self, log_entry):
        self.buffer.append(log_entry)
        if len(self.buffer) >= self.batch_size:
            self.flush()
    
    def flush(self):
        if self.buffer:
            # 批量写入
            write_to_file(self.buffer)
            self.buffer.clear()
```

---

## 附录 A: 快速索引

### A.1 按场景分类

**HTTP 请求日志**: 5.1 节  
**分布式追踪**: 5.2-5.3 节  
**审计日志**: 6.2 节  
**错误日志**: 3.2 节 (ERROR 级别)  
**性能日志**: 2.3 节 (duration_ms 字段)  

### A.2 常用字段速查

| 用途 | 字段名 | 类型 | 示例 |
|------|-------|------|------|
| 时间 | `timestamp` | number | `1701234567.890` |
| 级别 | `level` | string | `"info"` |
| 追踪 | `trace_id` | string | `"abc123def456"` |
| 会话 | `session_id` | string | `"sess_456"` |
| 性能 | `duration_ms` | number | `125.5` |
| 错误 | `error_code` | integer | `-1` |
| Agent | `agent_id` | string | `"com.agentos.pm.v1"` |
| 任务 | `task_id` | string | `"task_abc123"` |

---

## 附录 B: 与其他规范的引用关系

| 引用规范 | 关系说明 |
|---------|---------|
| [通信协议规范](./protocol_contract.md) | 本规范要求所有通信过程记录日志，支持 TraceID 贯穿 |
| [系统调用 API 规范](./syscall_api_contract.md) | 系统调用的错误处理和审计日志应遵循本规范 |
| [日志打印规范](../coding_standard/Log_guide.md) | 本规范定义日志格式，Log_guide.md 定义打印方法和最佳实践 |
| [统一术语表](../TERMINOLOGY.md) | 本规范使用的术语定义和解释 |

---

## 参考文献

[1] AgentOS 设计哲学。../../philosophy/Design_Principles.md  
[2] 架构设计原则。../架构设计原则.md  
[3] 统一术语表。../TERMINOLOGY.md  
[4] OpenTelemetry Specification. https://opentelemetry.io/docs/specs/otel/  
[5] Structured Logging Best Practices. https://github.com/open-telemetry/community/blob/main/cross-specification/stable/telemetry-specification.md  
[6] ELK Stack Documentation. https://www.elastic.co/guide/index.html  

---

## 版本历史

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|---------|
| v2.0.0 | 2026-03-22 | AgentOS 架构委员会 | 基于设计哲学进行全面重构，优化结构和表达 |
| v1.0.0 | - | 初始版本 | 初始发布 |

---

**最后更新**: 2026-03-22  
**维护者**: AgentOS 架构委员会
