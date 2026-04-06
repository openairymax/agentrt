Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 安全穹顶 (Cupolas)

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪
**相关原则**: E-1 (安全内生), E-2 (可观测性)
**核心文档**: [架构设计原则 - Cupolas 章节](../../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md#cupolas)

---

## 🛡️ 设计理念

Cupolas（安全穹顶）是 AgentOS 的**四层纵深防御安全体系**，实现安全内生而非附加：

> **安全不是补丁，而是系统的免疫系统**

### 四层防护架构

```
┌─────────────────────────────────────────────────────────────┐
│                    L4: 审计追踪 (Audit)                       │
│   全链路追踪 · 不可篡改日志 · 合规审计报告                    │
│   目标: 事后追溯、合规证明、威胁狩猎                          │
├─────────────────────────────────────────────────────────────┤
│                    L3: 输入净化 (Sanitizer)                   │
│   正则过滤 · 类型检查 · 注入攻击防护                         │
│   目标: 防止 SQL/XSS/命令注入等攻击                           │
├─────────────────────────────────────────────────────────────┤
│                    L2: 权限裁决 (Permission)                  │
│   YAML 规则引擎 · RBAC · 细粒度访问控制                      │
│   目标: 最小权限原则、职责分离                                │
├─────────────────────────────────────────────────────────────┤
│                    L1: 虚拟工位 (Workbench)                   │
│   进程隔离 · 容器隔离 · WASM 沙箱                            │
│   目标: 进程级隔离、资源限制、故障域隔离                       │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔒 第一层：虚拟工位 (Workbench)

### 隔离机制

#### 1. 进程级隔离

```python
class ProcessIsolation:
    """进程级沙箱"""

    def create_sandbox(self, agent_id: str, config: SandboxConfig) -> Sandbox:
        """
        创建隔离的进程沙箱
        """
        # 使用 Linux namespace 和 cgroups
        pid = os.fork()

        if pid == 0:
            # 子进程：应用资源限制

            # 1. Mount namespace（文件系统隔离）
            os.unshare(os.CLONE_NEWNS)

            # 2. Network namespace（网络隔离）
            os.unshare(os.CLONE_NEWNET)

            # 3. PID namespace（进程隔离）
            os.unshare(os.CLONE_NEWPID)

            # 4. 应用 cgroups 资源限制
            self._apply_cgroup_limits(pid, config.resource_limits)

            # 5. 设置 seccomp-bpf 过滤系统调用
            self._apply_seccomp_filter(pid, config.allowed_syscalls)

            # 执行 Agent 代码
            os.execv(config.entrypoint, config.args)
        else:
            # 父进程：监控子进程
            return Sandbox(
                agent_id=agent_id,
                pid=pid,
                config=config,
                status=SandboxStatus.CREATED
            )
```

#### 2. 容器级隔离

```yaml
# Docker 容器隔离配置示例
services:
  agent-sandbox:
    image: spharx/agentos:runtime
    security_opt:
      - no-new-privileges:true  # 禁止提权
      - seccomp:seccomp-profile.json  # 系统调用白名单
    cap_drop:
      - ALL  # 移除所有特权
    cap_add:
      - NET_BIND_SERVICE  # 仅允许绑定端口
    read_only: true  # 只读文件系统
    tmpfs:
      - /tmp:size=512M,mode=1777
      - /run:size=64M,mode=1755
    resources:
      limits:
        cpus: '2'
        memory: 512M
        pids: 100  # 最大进程数
    user: "1000:1000"  # 非 root 用户
```

#### 3. WASM 沙箱（轻量级隔离）

```rust
// WASM 沙箱配置
use wasmtime::{Config, Engine, Store, Module, Instance};

fn create_wasm_sandbox(agent_code: &[u8], resource_limits: ResourceLimits) -> Result<Instance> {
    // 配置 WASM 运行时
    let mut config = Config::new();
    config.wasm_simd(true);
    config.wasm_bulk_memory(true);

    // 创建引擎
    let engine = Engine::new(&config)?;

    // 编译模块
    let module = Module::from_binary(&engine, agent_code)?;

    // 配置资源限制
    let mut store = Store::new(&engine, ());
    store.limiter(|limiter| {
        limiter.memory_size(resource_limits.max_memory_bytes, 0);  // 内存限制
        limiter.table_elements(1000, 0);  // 表元素数量限制
        limiter.instances(1, 0);  // 实例数限制
        limiter.tables(1, 0);  // 表数量限制
        limiter.memories(1, 0);  // 内存数量限制
    });

    // 实例化模块（自动应用所有限制）
    let instance = Instance::new(&mut store, &module, &[])?;

    Ok(instance)
}
```

### 资源限制策略

| 资源类型 | 默认限制 | 最大值 | 监控指标 |
|----------|---------|--------|----------|
| **内存** | 512MB | 4GB | `container_memory_usage_bytes` |
| **CPU** | 2 核 | 8 核 | `container_cpu_usage_seconds_total` |
| **进程数** | 100 | 1000 | `container_pids_current` |
| **文件描述符** | 1024 | 65535 | `container_file_descriptors` |
| **网络带宽** | 100Mbps | 1Gbps | `container_network_transmit_bytes_total` |
| **磁盘 I/O** | 10MB/s | 100MB/s | `container_block_io_time_seconds_total` |

---

## ✅ 第二层：权限裁决 (Permission)

### RBAC 权限模型

```yaml
# permission-rules.yaml
version: "1.0"

# 角色定义
roles:
  admin:
    description: "系统管理员"
    permissions:
      - "*.*"  # 所有权限

  developer:
    description: "开发者"
    permissions:
      - "agent.create"
      - "agent.read"
      - "agent.update"
      - "task.create"
      - "task.read"
      - "memory.read"
      - "memory.write"
      - "config.read"

  operator:
    description: "运维工程师"
    permissions:
      - "agent.read"
      - "task.read"
      - "memory.read"
      - "monitoring.read"
      - "logs.read"

  user:
    description: "普通用户"
    permissions:
      - "agent.read"  # 只能读取自己的 Agent
      - "task.create"  # 可以创建任务
      - "task.read"   # 只能读取自己的任务
      - "memory.read"  # 只能读取自己的记忆

# 权限规则
permissions:
  # Agent 管理
  "agent.create":
    description: "创建新 Agent"
    resource: "agent"
    action: "create"
    effect: "allow"

  "agent.read":
    description: "读取 Agent 信息"
    resource: "agent"
    action: "read"
    condition: "owner_only"  # 只能访问自己的资源

  "agent.delete":
    description: "删除 Agent"
    resource: "agent"
    action: "delete"
    effect: "deny"  # 默认禁止，需要显式授权

  # 任务管理
  "task.execute":
    description: "执行任务"
    resource: "task"
    action: "execute"
    condition: "rate_limit:10/min"  # 速率限制

  # 记忆管理
  "memory.write":
    description: "写入记忆"
    resource: "memory"
    action: "write"
    condition: "quota:1GB"  # 配额限制

  # 系统操作
  "system.config":
    description: "修改系统配置"
    resource: "system"
    action: "config"
    effect: "deny"  # 仅管理员
```

### 权限决策引擎

```python
class PermissionEngine:
    """YAML 规则引擎 + RBAC"""

    def __init__(self, rules_path: str):
        self.rules = self._load_rules(rules_path)
        self.role_assignments = {}
        self.policy_cache = TTLCache(maxsize=10000, ttl=300)  # 5分钟缓存

    def check_permission(self,
                        user_id: str,
                        resource: str,
                        action: str,
                        context: Dict[str, Any] = None) -> Decision:
        """
        检查用户权限
        """
        cache_key = f"{user_id}:{resource}:{action}"

        # 缓存命中
        if cache_key in self.policy_cache:
            return self.policy_cache[cache_key]

        # Step 1: 获取用户角色
        roles = self._get_user_roles(user_id)

        # Step 2: 收集所有权限
        permissions = set()
        for role in roles:
            permissions.update(self.rules['roles'][role].get('permissions', []))

        # Step 3: 匹配具体权限规则
        permission_key = f"{resource}.{action}"
        explicit_deny = False
        explicit_allow = False

        for perm_pattern in permissions:
            if perm_pattern == "*.*":  # 通配符
                explicit_allow = True
                break

            if self._match_pattern(perm_pattern, permission_key):
                rule = self.rules['permissions'].get(perm_pattern, {})
                if rule.get('effect') == 'deny':
                    explicit_deny = True
                elif rule.get('effect') == 'allow':
                    # 检查条件
                    if self._evaluate_conditions(rule.get('condition'), context):
                        explicit_allow = True

        # Step 4: 做出决策
        if explicit_deny:
            decision = Decision(effect=Effect.DENY, reason="Explicit deny rule")
        elif explicit_allow:
            decision = Decision(effect=Effect.ALLOW, reason="Explicit allow rule")
        else:
            decision = Decision(effect=Effect.DENY, reason="No matching allow rule")

        # 写入缓存
        self.policy_cache[cache_key] = decision

        return decision

    def _evaluate_conditions(self, condition_str: str, context: Dict) -> bool:
        """评估条件表达式"""
        if not condition_str:
            return True

        # 解析条件字符串
        # 示例: "owner_only", "rate_limit:10/min", "quota:1GB"
        if condition_str == "owner_only":
            return context.get('user_id') == context.get('resource_owner')

        elif condition_str.startswith("rate_limit:"):
            limit = int(condition_str.split(":")[1].split("/")[0])
            # 检查速率限制（需要 Redis 或内存计数器）
            return self._check_rate_limit(context['user_id'], limit)

        elif condition_str.startswith("quota:"):
            quota = condition_str.split(":")[1]
            # 检查配额使用情况
            return self._check_quota(context['user_id'], quota)

        return True
```

---

## 🧼 第三层：输入净化 (Sanitizer)

### 净化过滤器

```python
class InputSanitizer:
    """多层输入净化"""

    def __init__(self, mode: str = "strict"):
        self.mode = mode
        self.filters = {
            'sql_injection': SQLInjectionFilter(),
            'xss': XSSFilter(),
            'command_injection': CommandInjectionFilter(),
            'path_traversal': PathTraversalFilter(),
            'ssrf': SSRFFilter()
        }

    def sanitize(self, input_data: Any, field_type: str = "text") -> SanitizedResult:
        """
        多层净化输入数据
        """
        result = SanitizedResult(
            original=input_data,
            sanitized=input_data,
            is_safe=True,
            warnings=[],
            blocked=False
        )

        # 类型检查和强制转换
        result.sanitized = self._type_check(result.sanitized, field_type)

        # 应用所有过滤器
        for filter_name, filter_instance in self.filters.items():
            filter_result = filter_instance.filter(result.sanitized)

            if not filter_result.is_safe:
                if self.mode == "strict":
                    # 严格模式：阻止请求
                    result.blocked = True
                    result.warnings.append(
                        f"[{filter_name}] Blocked: {filter_result.reason}"
                    )
                    break
                else:
                    # 宽松模式：记录警告并净化
                    result.sanitized = filter_result.sanitized
                    result.warnings.append(
                        f"[{filter_name}] Sanitized: {filter_result.reason}"
                    )

        return result


class SQLInjectionFilter:
    """SQL 注入检测与净化"""

    def __init__(self):
        # 危险模式正则表达式
        self.dangerous_patterns = [
            r"(?i)(\b(SELECT|INSERT|UPDATE|DELETE|DROP|UNION|ALTER|CREATE|TRUNCATE)\b)",
            r"(--|#|\/\*)",
            r"(\bor\b\s+\d+\s*=\s*\d+)",  # OR 1=1
            r"(;\s*(SELECT|INSERT|UPDATE|DELETE))",
            r"(\bEXEC\b|\bEXECUTE\b)",
            r"(CHAR\(|CONCAT\(|CONCAT_WS\()",
            r"(information_schema|sys\.tables|mysql\.)",
            r"(\bWAITFOR\b\s+DELAY\b)",
            r"(BENCHMARK\(|SLEEP\()",
        ]

    def filter(self, input_str: str) -> FilterResult:
        """检测 SQL 注入"""
        for pattern in self.dangerous_patterns:
            if re.search(pattern, input_str):
                return FilterResult(
                    is_safe=False,
                    reason=f"Potential SQL injection detected: {pattern}",
                    sanitized=re.sub(pattern, "[REMOVED]", input_str, flags=re.IGNORECASE)
                )

        return FilterResult(is_safe=True)


class XSSFilter:
    """XSS 攻击检测与净化"""

    def __init__(self):
        self.dangerous_patterns = [
            r"<script[^>]*>.*?</script>",
            r"javascript:",
            r"on(error|load|click|mouseover|focus|blur)\s*=",
            r"<iframe[^>]*>",
            r"<object[^>]*>",
            r"<embed[^>]*>",
            r"expression\s*\(",
            r"url\s*\(",
            r"document\.cookie",
            r"document\.location",
            r"eval\s*\(",
        ]

    def filter(self, input_str: str) -> FilterResult:
        """检测 XSS 攻击"""
        decoded = self._decode_entities(input_str)

        for pattern in self.dangerous_patterns:
            if re.search(pattern, decoded, re.IGNORECASE | re.DOTALL):
                return FilterResult(
                    is_safe=False,
                    reason=f"Potential XSS attack detected: {pattern}",
                    sanitized=self._escape_html(input_str)
                )

        return FilterResult(is_safe=True)

    def _escape_html(self, text: str) -> str:
        """HTML 实体转义"""
        return (text
                .replace("&", "&amp;")
                .replace("<", "&lt;")
                .replace(">", "&gt;")
                .replace('"', "&quot;")
                .replace("'", "&#x27;"))
```

### 净化性能优化

```python
class CachedSanitizer:
    """带缓存的输入净化器（高性能场景）"""

    def __init__(self, sanitizer: InputSanitizer, cache_size: int = 100000):
        self.sanitizer = sanitizer
        # 使用 LRU 缓存，相同输入无需重复净化
        self.cache = LRUCache(cache_size)

    def sanitize(self, input_data: str, field_type: str = "text") -> SanitizedResult:
        # 生成缓存键（基于输入哈希）
        cache_key = hashlib.sha256(f"{input_data}:{field_type}".encode()).hexdigest()

        # 缓存命中
        if cache_key in self.cache:
            return self.cache[cache_key]

        # 执行净化
        result = self.sanitizer.sanitize(input_data, field_type)

        # 只有安全的输入才缓存（避免缓存攻击载荷）
        if result.is_safe and not result.blocked:
            self.cache[cache_key] = result

        return result
```

---

## 📝 第四层：审计追踪 (Audit)

### 审计日志格式

```python
@dataclass
class AuditEvent:
    """审计事件标准格式"""
    # 必填字段
    event_id: str                              # UUID v4
    timestamp: datetime                         # 事件时间戳（UTC，微秒精度）
    event_type: AuditEventType                  # 事件类型枚举
    actor: AuditActor                           # 操作者信息
    resource: AuditResource                     # 操作资源
    action: str                                 # 操作动作
    outcome: AuditOutcome                       # 操作结果

    # 可选字段
    request_id: Optional[str] = None           # 请求追踪 ID
    session_id: Optional[str] = None           # 会话 ID
    source_ip: Optional[str] = None            # 来源 IP
    user_agent: Optional[str] = None           # 用户代理
    request_data: Optional[Dict] = None         # 请求数据（脱敏后）
    response_data: Optional[Dict] = None        # 响应数据（脱敏后）
    error_message: Optional[str] = None         # 错误信息
    duration_ms: Optional[int] = None          # 操作耗时
    metadata: Optional[Dict] = None             # 自定义元数据

    # 安全字段
    checksum: Optional[str] = None             # SHA256 校验和（防篡改）
    signature: Optional[str] = None            # 数字签名（可选）

class AuditEventType(Enum):
    """审计事件类型"""
    AUTHENTICATION = "authentication"
    AUTHORIZATION = "authorization"
    DATA_ACCESS = "data_access"
    DATA_MODIFICATION = "data_modification"
    CONFIG_CHANGE = "config_change"
    SYSTEM_OPERATION = "system_operation"
    SECURITY_EVENT = "security_event"
    COMPLIANCE_EVENT = "compliance_event"

class AuditOutcome(Enum):
    """操作结果"""
    SUCCESS = "success"
    FAILURE = "failure"
    DENIED = "denied"
    ERROR = "error"
```

### 审计日志写入

```python
class AuditLogger:
    """不可篡改审计日志系统"""

    def __init__(self, config: AuditConfig):
        self.config = config
        self.storage = AuditStorage(config.storage_path)
        self.signer = DigitalSigner(config.private_key_path)  # 可选数字签名
        self.batch_buffer = []
        self.flush_interval_sec = config.flush_interval_sec

        # 启动后台刷新线程
        self._start_flush_thread()

    def log(self, event: AuditEvent) -> None:
        """记录审计事件"""
        # 生成校验和（防篡改）
        event.checksum = self._compute_checksum(event)

        # 可选：数字签名
        if self.config.enable_signing:
            event.signature = self.signer.sign(event.to_json())

        # 写入缓冲区（批量写入提高性能）
        self.batch_buffer.append(event)

        if len(self.batch_buffer) >= self.config.batch_size:
            self._flush()

    def _compute_checksum(self, event: AuditEvent) -> str:
        """计算 SHA256 校验和"""
        event_dict = dataclasses.asdict(event)
        event_dict.pop('checksum', None)  # 排除自身
        event_dict.pop('signature', None)  # 排除签名

        json_str = json.dumps(event_dict, sort_keys=True, ensure_ascii=False)
        return hashlib.sha256(json_str.encode()).hexdigest()

    def _flush(self) -> None:
        """批量刷写到存储"""
        if not self.batch_buffer:
            return

        # 使用链式结构确保完整性（每个记录包含前一个记录的 hash）
        prev_hash = self.storage.get_last_hash()

        for event in self.batch_buffer:
            event.metadata = event.metadata or {}
            event.metadata['prev_hash'] = prev_hash
            event.checksum = self._compute_checksum(event)

            self.storage.append(event)
            prev_hash = event.checksum

        self.batch_buffer.clear()
```

### 审计查询与分析

```python
class AuditAnalyzer:
    """审计数据分析工具"""

    def query(self,
              start_time: datetime,
              end_time: datetime,
              filters: AuditFilters = None,
              limit: int = 1000) -> List[AuditEvent]:
        """查询审计日志"""
        events = self.storage.query_range(start_time, end_time)

        if filters:
            events = self._apply_filters(events, filters)

        return events[:limit]

    def detect_anomalies(self, time_window_min: int = 60) -> List[AnomalyReport]:
        """
        异常检测：
        - 短时间内大量失败认证
        - 异常的数据访问模式
        - 非工作时间的敏感操作
        - 权限提升尝试
        """
        anomalies = []

        # 检测暴力破解
        failed_auths = self._count_events_by_type(
            AuditEventType.AUTHENTICATION,
            outcome=AuditOutcome.FAILURE,
            window=time_window_min
        )
        if failed_auths > self.threshold_brute_force:
            anomalies.append(AnomalyReport(
                type="brute_force_attempt",
                severity="high",
                count=failed_auths,
                time_window=time_window_min
            ))

        # 检测异常数据导出
        large_exports = self._detect_large_data_export(time_window_min)
        if large_exports:
            anomalies.extend(large_exports)

        # 检测非工作时间敏感操作
        after_hours_ops = self._detect_after_hours_operations(time_window_min)
        if after_hours_ops:
            anomalies.extend(after_hours_ops)

        return anomalies

    def generate_compliance_report(self,
                                  start_date: date,
                                  end_date: date,
                                  standards: List[str]) -> ComplianceReport:
        """
        生成合规审计报告
        支持: GDPR, SOC2, ISO27001, HIPAA
        """
        report = ComplianceReport(
            period_start=start_date,
            period_end=end_date,
            standards=standards
        )

        for standard in standards:
            checker = ComplianceCheckerFactory.create(standard)
            result = checker.check(self.storage, start_date, end_date)
            report.add_standard_result(standard, result)

        return report
```

---

## 📊 监控指标

### Prometheus 指标定义

```yaml
# cupolas metrics
- name: agentos_cupolas_workbench_active_sandboxes
  type: Gauge
  help: "当前活跃的沙箱数量"

- name: agentos_cupolas_workbench_sandbox_creation_duration_seconds
  type: Histogram
  help: "沙箱创建延迟分布"

- name: agentos_cupolas_permission_checks_total
  type: Counter
  help: "权限检查总次数"
  labels: [decision]  # allow, deny

- name: agentos_cupolas_permission_check_duration_seconds
  type: Histogram
  help: "权限检查延迟分布"

- name: agentos_cupolas_sanitization_requests_total
  type: Counter
  help: "净化请求总次数"
  labels: [result]  # safe, sanitized, blocked

- name: agentos_cupolas_sanitization_blocked_total
  type: Counter
  help: "被阻止的请求数量"
  labels: [filter_type]  # sql_injection, xss, etc.

- name: agentos_cupolas_audit_events_total
  type: Counter
  help: "审计事件总数"
  labels: [event_type, outcome]

- name: agentos_cupolas_audit_log_write_duration_seconds
  type: Histogram
  help: "审计日志写入延迟"

- name: agentos_cupolas_audit_chain_integrity_violations_total
  type: Counter
  help: "审计链完整性违规数（篡改检测）"
```

---

## ⚙️ 配置示例

```yaml
# cupolas.yaml
cupolas:

  workbench:
    enabled: true
    isolation_type: container  # process | container | wasm
    default_limits:
      memory: "512MB"
      cpu_cores: 2
      max_processes: 100
      network_bandwidth: "100Mbps"
    auto_cleanup:
      enabled: true
      idle_timeout_min: 30  # 30分钟无活动则清理

  permission:
    enabled: true
    mode: strict  # strict | relaxed | disabled
    rules_file: /app/config/permission-rules.yaml
    cache_ttl_sec: 300  # 5分钟缓存
    rate_limiting:
      enabled: true
      default_limit: 100  # 每分钟请求数
      burst: 20  # 允许突发

  sanitizer:
    enabled: true
    mode: strict  # strict | relaxed | disabled
    filters:
      sql_injection:
        enabled: true
        action: block  # block | sanitize | warn
      xss:
        enabled: true
        action: sanitize
      command_injection:
        enabled: true
        action: block
      path_traversal:
        enabled: true
        action: sanitize
      ssrf:
        enabled: true
        action: block
    cache_size: 100000  # 缓存条目数
    max_input_size_mb: 10  # 单次输入最大大小

  audit:
    enabled: true
    log_path: /app/logs/audit.log
    retention_days: 90  # 保留90天
    batch_size: 100  # 批量写入大小
    flush_interval_sec: 5  # 刷新间隔
    enable_signing: false  # 数字签名（可选）
    include_request_data: true  # 是否记录请求数据（脱敏）
    include_response_data: false  # 是否记录响应数据
    sensitive_fields:  # 敏感字段列表（自动脱敏）
      - password
      - api_key
      - token
      - credit_card
    compliance_standards:
      - GDPR
      - SOC2
```

---

## 🧪 测试指南

### 安全测试用例

```bash
# 运行 Cupolas 安全测试套件
pytest tests/cupolas/test_security.py -v

# SQL 注入测试
pytest tests/cupolas/test_sql_injection.py -v

# XSS 测试
pytest tests/cupolas/test_xss.py -v

# 权限绕过测试
pytest tests/cupools/test_permission_bypass.py -v

# 审计日志完整性测试
pytest tests/cupolas/test_audit_integrity.py -v
```

### 渗透测试

```bash
# 使用 OWASP ZAP 进行自动化扫描
zap-cli quick-scan --self-contained http://localhost:8080

# 手动渗透测试清单
# 1. 认证绕过
# 2. 权限提升
# 3. 注入攻击（SQL/XSS/命令/LDAP）
# 4. SSRF
# 5. 文件包含/上传
# 6. 反序列化
# 7. XXE
# 8. CSRF
# 9. 竞态条件
# 10. 业务逻辑漏洞
```

---

## 📚 相关文档

- **[安全设计指南](../../agentos/manuals/specifications/coding_standard/Security_design_guide.md)** — 安全编码最佳实践
- **[架构设计原则](../../agentos/manuals/ARCHITECTURAL_PRINCIPLES.md)** — E-1/E-2 原则详解
- **[统一日志系统](../../agentos/manuals/architecture/logging_system.md)** — 日志规范
- **[运维手册 - 安全加固](operations/security-hardening.md)** — 生产环境安全配置

---

## ⚠️ 注意事项

1. **性能影响**: 严格模式下权限检查会增加 ~5ms 延迟
2. **存储成本**: 审计日志可能占用大量磁盘空间（预估 ~100MB/天）
3. **假阳性**: 输入净化可能误拦截合法输入，需建立白名单机制
4. **合规要求**: 不同行业标准对日志内容和保留期有不同要求
5. **密钥管理**: 数字签名私钥必须安全存储（建议 HSM 或 KMS）

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
