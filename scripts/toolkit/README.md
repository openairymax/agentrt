# Python 统一工具包

`scripts/toolkit/`

## 概述

`toolkit/` 模块提供 AgentRT 运维和开发的 Python 工具集合，包含 15 个源模块，覆盖系统诊断、性能测试、多层记忆管理、Token 统计与预算、配置模板引擎、检查点管理、契约验证、CLI 增强、事件总线、日志、插件系统、安全模块和遥测系统。该模块是 AgentRT Python 生态的核心基础设施，为上层运维脚本和开发工具提供统一的工具类接口。

工具包的设计原则：

- **统一导出**：`__init__.py` 统一导出所有工具类，用户只需 `from scripts.toolkit import ...` 即可使用
- **模块化设计**：每个模块职责单一、接口清晰，可独立使用也可组合编排
- **正式发行版**：v0.1.0 正式版，API 已稳定
- **Prometheus 兼容**：遥测模块兼容 Prometheus 指标格式，可无缝集成到现有监控体系

> **版本**：v0.1.0
> **状态**：✅ 正式版，API 已稳定。

## 与 agentos/ 模块对应关系

| toolkit/ 模块 | 对应的 agentos/ 模块 | 用途 |
|---------------|---------------------|------|
| `initializer.py` | `commons/`, `daemon/` | 配置初始化器（默认配置/完整性验证/环境特定配置/备份恢复） |
| `doctor.py` | `commons/`, `daemon/`, `manager/` | 系统健康诊断（8 大类别：系统/Python/构建/项目/配置/网络/安全/性能） |
| `benchmark.py` | `atoms/corekern/`, `atoms/coreloopthree/` | 性能基准测试（IPC 延迟/内存分配/上下文切换/调度吞吐/JSON 解析） |
| `memory_manager.py` | `atoms/memory/`, `atoms/memoryrovol/` | 多层记忆管理器（L1 原始/L2 特征/L3 结构化/L4 模式） |
| `token_utils.py` | `daemon/`, `manager/` | Token 工具集（多策略计数 + 预算分配/追踪/告警） |
| `config_engine.py` | `commons/`, `daemon/` | 配置模板引擎（Jinja2，dev/staging/production/testing） |
| `checkpoint_manager.py` | `daemon/` | 状态检查点管理器（创建/恢复/轮转/JSON 序列化） |
| `validate_contracts.py` | `atoms/syscall/`, `commons/` | 接口契约验证（syscall 头文件/配置格式/API 响应合规） |
| `cli.py` | `daemon/`, `manager/` | 交互式 CLI 增强（彩色输出/进度条/spinner/选择菜单/表格） |
| `events.py` | `daemon/`, `cupolas/` | 事件总线系统（同步/异步/优先级/过滤/历史回放/分布式追踪） |
| `logger.py` | 全部模块 | 日志模块（终端颜色/格式化输出/进度条/spinner/表格渲染） |
| `plugin.py` | `daemon/`, `manager/` | 插件系统（动态发现/元数据管理/依赖解析/执行隔离） |
| `security.py` | `commons/`, `daemon/` | 安全模块（输入净化/路径检查/注入防护/权限最小化/审计） |
| `telemetry.py` | `daemon/`, `manager/` | 遥测系统（实时指标/基准追踪/趋势分析，兼容 Prometheus） |

## 目录结构

```
toolkit/
├── README.md                      # 本文档
├── __init__.py                    # 包入口（统一导出所有工具类）
└── src/                           # 工具模块源码（15 个文件）
    ├── __init__.py                #   内部模块入口
    ├── initializer.py             #   配置初始化器（默认配置/完整性验证/环境特定配置/备份恢复）
    ├── doctor.py                  #   系统健康诊断（8 大类别：系统/Python/构建/项目/配置/网络/安全/性能）
    ├── benchmark.py               #   性能基准测试（IPC 延迟/内存分配/上下文切换/调度吞吐/JSON 解析）
    ├── memory_manager.py          #   多层记忆管理器（L1 原始/L2 特征/L3 结构化/L4 模式）
    ├── token_utils.py             #   Token 工具集（多策略计数 + 预算分配/追踪/告警）
    ├── config_engine.py           #   配置模板引擎（Jinja2，dev/staging/production/testing）
    ├── checkpoint_manager.py      #   状态检查点管理器（创建/恢复/轮转/JSON 序列化）
    ├── validate_contracts.py      #   接口契约验证（syscall 头文件/配置格式/API 响应合规）
    ├── cli.py                     #   交互式 CLI 增强（彩色输出/进度条/spinner/选择菜单/表格）
    ├── events.py                  #   事件总线系统（同步/异步/优先级/过滤/历史回放/分布式追踪）
    ├── logger.py                  #   日志模块（终端颜色/格式化输出/进度条/spinner/表格渲染）
    ├── plugin.py                  #   插件系统（动态发现/元数据管理/依赖解析/执行隔离）
    ├── security.py                #   安全模块（输入净化/路径检查/注入防护/权限最小化/审计）
    └── telemetry.py               #   遥测系统（实时指标/基准追踪/趋势分析，兼容 Prometheus）
```

## 核心组件说明

### 诊断类

#### initializer.py — 配置初始化器

系统配置的初始化和验证工具：

- **默认配置生成**：根据环境自动生成合理的默认配置
- **完整性验证**：检查配置文件是否包含所有必需字段，验证字段类型和取值范围
- **环境特定配置**：支持 dev/staging/production/testing 四种环境的差异化配置
- **备份恢复**：配置变更前自动备份，支持一键回滚到上一版本

#### doctor.py — 系统健康诊断

全面的系统健康检查工具，覆盖 8 大类别：

1. **系统检查**：操作系统版本、内核参数、系统资源
2. **Python 检查**：Python 版本、已安装包、虚拟环境
3. **构建检查**：CMake 版本、编译器版本、构建目录
4. **项目检查**：项目结构完整性、文件权限、Git 状态
5. **配置检查**：配置文件语法、字段完整性、环境变量
6. **网络检查**：端口占用、DNS 解析、外部服务连通性
7. **安全检查**：文件权限、密钥管理、安全配置
8. **性能检查**：CPU 负载、内存使用、磁盘 I/O

### 性能类

#### benchmark.py — 性能基准测试

AgentRT 核心组件的性能基准测试工具：

- **IPC 延迟测试**：测量进程间通信的延迟分布
- **内存分配测试**：测量不同大小内存分配的延迟和吞吐量
- **上下文切换测试**：测量线程/进程上下文切换的开销
- **调度吞吐测试**：测量任务调度器的吞吐量
- **JSON 解析测试**：测量 JSON 序列化/反序列化的性能

### 记忆类

#### memory_manager.py — 多层记忆管理器

AgentRT 智能体的多层记忆管理系统：

- **L1 原始层**：原始输入数据的直接存储，高保真但占用空间大
- **L2 特征层**：提取关键特征的结构化存储，平衡保真度和空间效率
- **L3 结构化层**：高度结构化的知识表示，支持复杂查询
- **L4 模式层**：抽象的模式和规则，支持推理和泛化
- 支持层间迁移、淘汰策略和容量管理

#### checkpoint_manager.py — 状态检查点管理器

智能体运行状态的检查点管理：

- **检查点创建**：捕获当前运行状态并序列化为 JSON
- **检查点恢复**：从 JSON 反序列化恢复到指定状态
- **检查点轮转**：按策略自动清理过期的检查点
- **JSON 序列化**：支持复杂对象图的序列化和反序列化

### Token 类

#### token_utils.py — Token 工具集

LLM Token 的统计和预算管理工具：

- **TokenCounter**：多策略 Token 计数器，支持精确计数和估算两种模式
- **TokenBudget**：Token 预算管理器，支持预算分配、使用追踪和超限告警
- **多策略计数**：支持 BPE 分词器精确计数和字符级估算
- **预算分配**：按优先级和权重分配 Token 预算给不同模块

### 配置类

#### config_engine.py — 配置模板引擎

基于 Jinja2 的配置模板引擎：

- **模板渲染**：使用 Jinja2 模板语法生成配置文件
- **多环境支持**：内置 dev/staging/production/testing 四种环境配置
- **变量覆盖**：支持环境变量和命令行参数覆盖模板变量
- **配置验证**：渲染后自动验证配置文件的语法和语义

### 基础设施类

#### cli.py — 交互式 CLI 增强

终端交互体验增强工具：

- **彩色输出**：支持 256 色终端输出，自动检测终端能力
- **进度条**：支持确定性和不确定性进度条
- **Spinner**：长时间操作的旋转指示器
- **选择菜单**：交互式单选/多选菜单
- **表格渲染**：终端表格格式化输出

#### logger.py — 日志模块

统一的日志输出模块：

- **终端颜色**：根据日志级别自动着色
- **格式化输出**：支持自定义日志格式
- **进度条**：集成进度条显示
- **Spinner**：集成旋转指示器
- **表格渲染**：集成表格格式化输出

#### events.py — 事件总线系统

发布-订阅模式的事件总线：

- **同步/异步**：支持同步和异步两种事件分发模式
- **优先级**：事件处理器支持优先级排序
- **过滤**：支持基于事件类型和内容的过滤器
- **历史回放**：支持事件历史记录和回放
- **分布式追踪**：支持跨服务的事件追踪

### 扩展类

#### plugin.py — 插件系统

动态插件管理框架：

- **动态发现**：自动扫描和发现插件模块
- **元数据管理**：插件的版本、依赖、描述等元数据管理
- **依赖解析**：自动解析和满足插件间的依赖关系
- **执行隔离**：插件在沙箱环境中执行，防止异常传播

#### security.py — 安全模块

安全防护和审计工具：

- **输入净化**：过滤和转义用户输入中的危险字符
- **路径检查**：防止路径遍历攻击
- **注入防护**：检测和阻止 SQL 注入、命令注入等攻击
- **权限最小化**：基于角色的访问控制，遵循最小权限原则
- **审计日志**：记录所有安全相关操作的审计日志

### 可观测性类

#### telemetry.py — 遥测系统

系统运行指标的采集和暴露：

- **实时指标**：CPU、内存、延迟等实时运行指标
- **基准追踪**：性能基准测试结果的追踪和对比
- **趋势分析**：基于历史数据的趋势分析和异常检测
- **Prometheus 兼容**：支持 Prometheus 指标格式，可无缝集成到现有监控体系

### 验证类

#### validate_contracts.py — 接口契约验证

接口合规性验证工具：

- **syscall 头文件验证**：验证 syscall 接口头文件的一致性和完整性
- **配置格式验证**：验证配置文件是否符合约定的格式规范
- **API 响应合规**：验证 API 响应是否符合接口契约定义

## 使用方式

### 导入工具类

```python
from scripts.toolkit import (
    ConfigInitializer,
    AgentOSDoctor,
    MemoryManager,
    TokenCounter,
    TokenBudget,
    CheckpointManager,
    AgentOSBenchmark,
    ContractValidator,
    ConfigEngine,
    PluginRegistry,
    EventBus,
    SecurityManager,
    MetricsCollector,
)
```

### 系统诊断

```python
doctor = AgentOSDoctor()
doctor.run_all()

# 只运行特定类别
doctor.run_category("python")
doctor.run_category("security")

# 生成诊断报告
report = doctor.generate_report()
```

### 配置初始化

```python
initializer = ConfigInitializer()
initializer.init()

# 指定环境
initializer.init(env="production")

# 验证配置
initializer.validate()

# 备份和恢复
initializer.backup()
initializer.restore()
```

### 记忆管理

```python
mm = MemoryManager()

# 存储记忆
mm.store("L1", raw_data)
mm.store("L2", features)
mm.store("L3", structured_data)
mm.store("L4", patterns)

# 检索记忆
result = mm.retrieve("L3", query)

# 查看统计
mm.stats()
```

### Token 管理

```python
counter = TokenCounter()
budget = TokenBudget(total=100000)

# 计数
count = counter.estimate("Hello, world!")
exact_count = counter.count("Hello, world!")

# 预算管理
budget.allocate("module_a", 30000)
budget.allocate("module_b", 20000)
budget.use("module_a", count)

# 检查预算
budget.remaining()
budget.usage_report()
```

### 检查点管理

```python
cp = CheckpointManager()

# 创建检查点
cp_id = cp.create(state)

# 恢复检查点
state = cp.restore(cp_id)

# 列出检查点
cp.list()

# 清理过期检查点
cp.rotate(max_count=10)
```

### 配置模板引擎

```python
engine = ConfigEngine()

# 渲染配置
config = engine.render("config.yaml.j2", env="production")

# 渲染并验证
config = engine.render_and_validate("config.yaml.j2", env="staging")
```

### 事件总线

```python
bus = EventBus()

# 订阅事件
bus.subscribe("task.completed", handler)

# 发布事件
bus.publish("task.completed", {"task_id": "123"})

# 异步发布
await bus.publish_async("task.completed", {"task_id": "123"})
```

### 插件系统

```python
registry = PluginRegistry()

# 发现插件
registry.discover("plugins/")

# 加载插件
plugin = registry.load("my_plugin")

# 执行插件
result = registry.execute("my_plugin", *args, **kwargs)
```

### 安全模块

```python
sec = SecurityManager()

# 输入净化
clean = sec.sanitize_input(user_input)

# 路径检查
sec.validate_path(file_path)

# 权限检查
sec.check_permission(user, resource, action)
```

### 遥测系统

```python
telemetry = MetricsCollector()

# 记录指标
telemetry.record("latency", 42.5)
telemetry.record("throughput", 1000)

# 获取指标
metrics = telemetry.get_metrics()

# Prometheus 格式导出
prometheus_text = telemetry.export_prometheus()
```

### 契约验证

```python
validator = ContractValidator()

# 验证 syscall 头文件
validator.validate_syscall_headers()

# 验证配置格式
validator.validate_config("config.yaml")

# 验证 API 响应
validator.validate_api_response(endpoint, response)
```

## 依赖说明

| 模块 | 核心依赖 | 说明 |
|------|---------|------|
| `initializer.py` | Python 3.8+ | 配置初始化器为纯 Python 实现 |
| `doctor.py` | Python 3.8+, psutil | 系统诊断依赖 psutil 获取系统信息 |
| `benchmark.py` | Python 3.8+, numpy | 性能基准测试依赖 numpy 进行统计计算 |
| `memory_manager.py` | Python 3.8+ | 记忆管理器为纯 Python 实现 |
| `token_utils.py` | Python 3.8+, tiktoken (可选) | Token 计数器可选依赖 tiktoken 进行精确计数 |
| `config_engine.py` | Python 3.8+, Jinja2 | 配置模板引擎依赖 Jinja2 |
| `checkpoint_manager.py` | Python 3.8+ | 检查点管理器为纯 Python 实现 |
| `validate_contracts.py` | Python 3.8+, PyYAML | 契约验证依赖 PyYAML 解析 YAML 配置 |
| `cli.py` | Python 3.8+, rich (可选) | CLI 增强可选依赖 rich 获得更好的终端体验 |
| `events.py` | Python 3.8+ | 事件总线为纯 Python 实现，异步模式需要 asyncio |
| `logger.py` | Python 3.8+ | 日志模块为纯 Python 实现 |
| `plugin.py` | Python 3.8+ | 插件系统为纯 Python 实现 |
| `security.py` | Python 3.8+ | 安全模块为纯 Python 实现 |
| `telemetry.py` | Python 3.8+, prometheus_client | 遥测系统依赖 prometheus_client 导出 Prometheus 格式指标 |

---

© 2026 SPHARX Ltd. All Rights Reserved.
