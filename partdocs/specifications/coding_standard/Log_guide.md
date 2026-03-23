# AgentOS 日志打印规范

## 版本信息

| 版本 | 日期 | 作者 | 变更说明 |
|------|------|------|----------|
| v1.2 | 2026-03-21 | AgentOS 架构委员会 | 基于控制论和可观测性理论重构 |
| v1.0 | - | 初始版本 | 原始日志规范 |

**文档状态**：正式发布  
**适用范围**：AgentOS 所有模块的日志打印活动  
**理论基础**：工程控制论（反馈理论、可观测性）、信息论  
**关联规范**：[架构设计原则](../架构设计原则.md)（原则 O-5）

---

## 第 1 章 日志系统架构

### 1.1 日志的控制论意义

#### 1.1.1 日志作为反馈机制

从控制论角度看，日志系统是 AgentOS 的**感觉神经系统**，承担以下关键功能：

1. **状态观测**：采集系统运行状态信息
2. **偏差检测**：识别实际行为与预期目标的偏离
3. **反馈信号**：为控制系统提供调节依据
4. **历史追溯**：记录系统演化轨迹用于分析优化

```
传感器（代码埋点） → 信号传输（日志输出） → 控制器（运维人员/自动化系统）
                                                         ↓
执行器（系统调节） ← 决策制定（问题分析） ← 信息处理（日志分析）
```

#### 1.1.2 日志的可观测性理论

**可观测性三要素**：

1. **Logs（日志）**：离散事件记录，回答"发生了什么"
2. **Metrics（指标）**：聚合统计数据，回答"系统状态如何"
3. **Traces（追踪）**：请求链路记录，回答"请求经过哪些组件"

本规范主要针对 Logs，但需与 Metrics 和 Traces 协调设计。

### 1.2 日志系统设计原则

#### 原则 1-1【信噪比最大化】：在提供足够信息和减少噪音间找到平衡

**解释**：日志的价值 = 有用信息量 / 日志总量。过多的日志会淹没关键信息，过少的日志无法有效观测。

**实施指南**：
- 关键流程节点必须记录（高价值信息）
- 高频正常操作减少记录（降低噪音）
- 异常情况详细记录（故障诊断需要）

#### 原则 1-2【分层观测】：按控制层次组织日志级别

```
战略层（FATAL/ERROR）   → 系统级异常，需要立即干预
战术层（WARN）          → 功能级异常，需要关注和处理
操作层（INFO）          → 业务流程记录，用于审计和追溯
调试层（DEBUG/TRACE）   → 详细技术信息，用于问题定位
```

#### 原则 1-3【自适应调节】：日志系统应支持动态级别调整和流量控制

**解释**：固定不变的日志策略无法适应变化的环境，需要具备负反馈调节能力。

**实施指南**：
- 支持运行时调整日志级别
- 实现日志流量限制防止资源耗尽
- 异常情况自动提升相关模块日志级别

---

## 第 2 章 日志分级与分类

### 2.1 日志级别定义

基于控制论的"偏差程度"概念，定义以下日志级别：

#### FATAL（致命级）

**定义**：系统发生灾难性故障，即将或已经崩溃，需要立即干预。

**特征**：
- 系统核心功能不可用
- 数据丢失或损坏风险
- 安全防线被突破
- 无法自动恢复

**示例**：
```cpp
HiLog::FATAL(LABEL, "Database connection pool exhausted, all retries failed");
HiLog::FATAL(LABEL, "Kernel panic: Unable to mount root filesystem");
```

**响应要求**：7×24 小时立即响应，自动触发告警

#### ERROR（错误级）

**定义**：功能发生错误，影响正常使用，可以恢复但代价较高。

**特征**：
- 单个功能模块失效
- 用户体验受损
- 需要人工介入或系统重置
- 可能扩散为 FATAL

**示例**：
```cpp
HiLog::ERROR(LABEL, "Failed to load user profile after 3 retries");
HiLog::ERROR(LABEL, "Network timeout: API request failed with status 500");
```

**响应要求**：工作日 4 小时内响应，纳入问题跟踪系统

#### WARN（警告级）

**定义**：发生非预期情况，对用户影响不大，可以自动恢复或简单处理。

**特征**：
- 系统降级运行但未失效
- 性能下降但功能可用
- 可能发展为 ERROR
- 通常可自动恢复

**示例**：
```cpp
HiLog::WARN(LABEL, "High memory usage detected: 85% of total capacity");
HiLog::WARN(LABEL, "Slow query detected: Execution time exceeded threshold (5s)");
```

**响应要求**：定期审查，趋势分析，预防性优化

#### INFO（信息级）

**定义**：记录业务关键流程节点，用于还原主要运行过程。

**特征**：
- 正常业务流程的关键里程碑
- 重要的状态变更
- 用户操作的审计记录
- 默认版本中开启

**示例**：
```cpp
HiLog::INFO(LABEL, "User login successful: userId=12345, ip=192.168.1.100");
HiLog::INFO(LABEL, "Order created: orderId=ORD20260321001, amount=299.00");
```

**响应要求**：用于审计、统计、问题排查参考

#### DEBUG（调试级）

**定义**：比 INFO 更详细的流程记录，用于分析问题。

**特征**：
- 函数入口出口参数
- 中间计算结果
- 条件分支选择
- 仅在调试版本或调试开关打开时输出

**示例**：
```cpp
HiLog::DEBUG(LABEL, "Entering CalculatePrice: basePrice=199, discount=0.8, taxRate=0.13");
HiLog::DEBUG(LABEL, "Cache hit for key=user_profile_12345");
```

**使用要求**：正式发布版本默认关闭，按需开启

### 2.2 日志级别选择矩阵

| 场景类型 | 频率 | 影响范围 | 推荐级别 | 示例 |
|---------|------|---------|---------|------|
| 系统崩溃 | 极低 | 全局 | FATAL | 内核 panic |
| 功能失效 | 低 | 局部 | ERROR | 数据库连接失败 |
| 性能下降 | 中 | 局部 | WARN | 内存使用率高 |
| 业务流程 | 高 | 单请求 | INFO | 订单创建成功 |
| 详细跟踪 | 很高 | 单操作 | DEBUG | 函数参数值 |

---

## 第 3 章 日志内容规范

### 3.1 信息论视角下的日志内容

从信息论角度看，日志是**消除不确定性**的信息载体。好的日志应该：

1. **高信息量**：能够显著减少系统状态的不确定性
2. **低冗余度**：避免重复和无意义的信息
3. **可解码性**：接收方能够准确理解含义

### 3.2 日志内容基本要求

#### 规则 3-1【语言规范】：日志使用英文描述，拼写无误，符合语法规范

**【反例】**
```cpp
HiLog::ERROR(LABEL, "Error happened");  // 哪里出错？什么原因？
HiLog::ERROR(LABEL, "1234");            // 完全不知道什么含义
```

**【正例】**
```cpp
HiLog::ERROR(LABEL, "Failed to connect to Redis server: host=redis-master, port=6379, error=Connection refused");
```

#### 规则 3-2【禁止隐私信息】：日志中禁止打印隐私敏感信息

**隐私信息范围**：
- 硬件序列号
- 个人账号和密码
- 生物识别信息
- 身份证号等身份证明
- 医疗记录等敏感数据

**【反例】**
```cpp
HiLog::INFO(LABEL, "User password verification: userId=admin, password=Secret123");
```

**【正例】**
```cpp
HiLog::INFO(LABEL, "User authentication result: userId=admin, success=false, reason=invalid_password");
```

#### 规则 3-3【禁止无关信息】：日志中禁止打印与业务无关的信息

**禁止内容**：
- issue 单号、需求单号
- 公司部门名称、开发人员姓名
- 工号、名字缩写
- 当天的心情、天气等

#### 规则 3-4【避免重复】：日志中禁止打印重复信息

**【反例】**
```cpp
// 文件 A.cpp
HiLog::ERROR(LABEL, "Database connection failed");

// 文件 B.cpp（不同位置，相同内容）
HiLog::ERROR(LABEL, "Database connection failed");
```

**【正例】**
```cpp
// 添加上下文区分
HiLog::ERROR(LABEL, "Primary database connection failed: host=db-primary");
HiLog::ERROR(LABEL, "Secondary database connection failed: host=db-backup");
```

#### 规则 3-5【禁止业务调用】：禁止在日志打印语句中调用业务接口函数

**解释**：日志不应影响业务逻辑，避免产生副作用。

**【反例】**
```cpp
HiLog::INFO(LABEL, "Processing order: %s", order.ToString().c_str());  // ToString() 可能有副作用
```

**【正例】**
```cpp
std::string orderStr = order.ToString();
HiLog::INFO(LABEL, "Processing order: %s", orderStr.c_str());
```

### 3.3 日志格式模式

#### 模式 3-1【事件记录】who do what

**格式**：`Actor Action Object [Result]`

**示例**：
```cpp
HiLog::INFO(LABEL, "UserService created new user: userId=12345, email=user@example.com");
HiLog::INFO(LABEL, "PaymentGateway processed payment: orderId=ORD001, amount=299.00, status=success");
```

#### 模式 3-2【状态变化】state_name:s1→s2, reason:msg

**格式**：`StateName: OldState -> NewState, Reason: Message`

**示例**：
```cpp
HiLog::INFO(LABEL, "Connection state: CONNECTED -> DISCONNECTED, reason: Server timeout after 30s");
HiLog::WARN(LABEL, "Order status: PENDING -> CANCELLED, reason: Payment timeout exceeded 15 minutes");
```

#### 模式 3-3【参数值】name1=value1, name2=value2

**格式**：键值对列表，逗号分隔

**示例**：
```cpp
HiLog::DEBUG(LABEL, "Function parameters: inputSize=1024, compressionRatio=0.7, outputSize=716");
```

#### 模式 3-4【成功结果】xxx successful

**示例**：
```cpp
HiLog::INFO(LABEL, "Database backup completed successfully: duration=300s, size=1.2GB");
```

#### 模式 3-5【失败结果】xxx failed, please xxx

**示例**：
```cpp
HiLog::ERROR(LABEL, "Connect to server failed, please check network configuration: host=api.example.com, port=443");
```

---

## 第 4 章 日志打印策略

### 4.1 打印时机控制

#### 规则 4-1【高频代码禁打日志】：高频代码的正常流程中禁止打印日志

**高频场景**：
- 被高频调用的接口函数（>1000 次/秒）
- 大数据量处理的循环中
- 高频的软硬件中断处理中
- 协议数据流处理中
- 多媒体音视频流处理中
- 显示屏幕刷新处理中

**例外**：错误分支可以打印 ERROR 级别日志

#### 规则 4-2【频率限制】：可能重复发生的日志需要进行频率限制

**实施方法**：

**方法 1：时间间隔限制**
```cpp
// 每秒最多打印一次
static std::chrono::steady_clock::time_point last_log_time;
auto now = std::chrono::steady_clock::now();
if (now - last_log_time > std::chrono::seconds(1)) {
    HiLog::WARN(LABEL, "High memory usage detected: 85%");
    last_log_time = now;
}
```

**方法 2：计数限制**
```cpp
// 每 100 次只打印一次
static std::atomic<int> counter{0};
if (++counter % 100 == 0) {
    HiLog::DEBUG(LABEL, "Processing packet #%d", counter.load());
}
```

**方法 3：采样打印**
```cpp
// 随机采样 1%
if (rand() % 100 == 0) {
    HiLog::DEBUG(LABEL, "Request processing details: ...");
}
```

#### 规则 4-3【关键点必打】：在基本不可能发生的点必须要打印日志

**解释**：根据墨菲定律，只要有可能发生就一定会发生。这些点一旦发生问题就是疑难杂症。

**典型场景**：
- switch 语句的 default 分支（理论上不会走到）
- 断言检查失败（认为永远不成立的条件）
- 防御性编程检查（认为不会出现的异常情况）

**示例**：
```cpp
switch (status) {
    case STATUS_A: HandleA(); break;
    case STATUS_B: HandleB(); break;
    case STATUS_C: HandleC(); break;
    default:
        // 理论上不会到这里，但必须记录
        HiLog::FATAL(LABEL, "Unexpected status value: %d in state machine", static_cast<int>(status));
        abort();
}
```

### 4.2 性能影响最小化

#### 规则 4-4【延迟生成】：日志字符串应在日志打印时再生成

**解释**：推迟字符串构造可以减少不必要的开销，当日志级别关闭时避免生成。

**【反例】**
```cpp
std::string detailed_info = GenerateDetailedInfo();  // 总是生成
HiLog::DEBUG(LABEL, "%s", detailed_info.c_str());     // 但 DEBUG 可能关闭
```

**【正例】**
```cpp
// 使用 lambda 延迟生成
HiLog::DEBUG(LABEL, "%s", [](){ return GenerateDetailedInfo(); }());
```

#### 规则 4-5【避免长日志】：日志打印长度不要过长，尽可能使日志记录显示在一行以内

**建议**：
- 单行长度控制在 100 字符左右
- 尽量不要超过 160 字符
- 超长内容考虑分条或截断

---

## 第 5 章 HiLog 接口使用规范

### 5.1 Domain ID 分配的系统工程方法

#### 规则 5-1【独立 Domain】：每个业务须有独立的 Domain ID

**解释**：Domain ID 是日志系统的"命名空间"，用于隔离不同业务的日志流，支持独立的度量和管理。

**Domain ID 结构**：
```
32 位整数，16 进制表示：0xD0xxxyy
├─ D0: domain 域标识（固定）
├─ xxx: 高 12 位，DFX 分配值（业务领域）
└─ yy: 低 8 位，业务内部分配（模块/层次）
```

**申请流程**：
1. 向 DFX 领域提交申请
2. 说明业务范围和组织归属
3. DFX 分配高 12 位标识
4. 业务内部自行分配低 8 位

#### 规则 5-2【层次化分配】：每个业务内部基于 Domain ID 分配机制按层次、模块粒度划分

**示例：蓝牙（BT）业务 Domain ID 分配**

| Domain 名称 | Domain ID | 说明 |
|-----------|---------|------|
| BT | 0xD000100 | 蓝牙总领域 |
| BT-App1 | 0xD000101 | 应用层 1 |
| BT-App2 | 0xD000102 | 应用层 2 |
| BT-Service1 | 0xD000103 | 服务层 1 |
| BT-Service2 | 0xD000104 | 服务层 2 |
| BT-HAL | 0xD000105 | 硬件抽象层 |
| BT-Driver1 | 0xD000106 | 驱动层 1 |
| BT-Driver2 | 0xD000107 | 驱动层 2 |

**层次映射**：
```
0xD0001yy
       └─ yy: 00=总领域，01-7F=层次/模块，80-FF=保留
```

### 5.2 流量管控

#### 规则 5-3【阈值管理】：日志服务会对每个业务的日志量进行流量管控，修改默认阈值需要经过评审

**默认阈值**：10240 字节/秒/Domain ID

**超阈值处理**：
1. 系统自动限流，丢弃超出部分的日志
2. 记录限流事件（FATAL 级别）
3. 通知运维人员

**阈值调整流程**：
1. 提交书面申请，说明调整理由和预期值
2. DFX 领域组织评审
3. 评估对其他业务的影响
4. 批准后实施调整

### 5.3 隐私参数标识

#### 规则 5-4【隐私标识】：正确填写日志格式化隐私参数标识{public},{private}

**解释**：HiLog API 会自动过滤标记为{private}的参数内容，保护敏感信息。

**【示例】**
```cpp
// 源码
HiLog::INFO(LABEL, "Device Name:%{public}s, IP:%{private}s, Token:%{private}s.", 
            deviceName, ipAddress, authToken);

// 日志输出
Device Name:MyPad001, IP:<private>, Token:<private>.
```

**使用要求**：
- 默认所有参数标记为{private}
- 明确确认不含隐私的参数标记为{public}
- 定期审查{public}参数的安全性

---

## 第 6 章 日志质量管理

### 6.1 日志质量评估指标

#### 指标 1：日志覆盖率

**定义**：关键流程节点的日志记录比例

**计算公式**：
```
日志覆盖率 = (有关键日志的流程节点数 / 总流程节点数) × 100%
```

**目标值**：≥95%

#### 指标 2：日志准确率

**定义**：日志内容与实际情况的一致性

**评估方法**：抽样审查，对比日志与实际行为

**目标值**：≥98%

#### 指标 3：日志信噪比

**定义**：有价值日志与总日志量的比例

**计算公式**：
```
信噪比 = (ERROR + WARN + 关键 INFO)数量 / 总日志数量 × 100%
```

**目标值**：INFO 级别以上 ≥30%，DEBUG 级别 ≥5%

### 6.2 日志质量改进 PDCA 循环

#### Plan（计划）

- 分析当前日志质量问题（如噪音过多、关键信息缺失）
- 设定改进目标和优先级
- 制定改进行动计划

#### Do（执行）

- 实施日志优化（删除无用日志、补充关键日志）
- 调整日志级别和频率
- 更新日志规范文档

#### Check（检查）

- 收集日志使用反馈
- 分析问题定位效率是否提升
- 测量日志质量指标

#### Act（处理）

- 成功经验标准化（纳入规范）
- 失败教训案例化（加入培训）
- 遗留问题进入下一轮 PDCA

### 6.3 日志自适应调节

#### 机制 1：动态级别调整

**场景**：系统检测到异常时自动提升相关模块日志级别

**实现**：
```cpp
// 伪代码示例
if (system.AnomalyDetected()) {
    LogLevelManager::SetLevel("PaymentModule", LogLevel::DEBUG);
    // 自动提升支付模块日志级别以便详细跟踪
}
```

#### 机制 2：智能采样

**场景**：正常情况下采样打印，异常时全量打印

**实现**：
```cpp
// 伪代码示例
if (request.IsSuccess()) {
    // 成功请求 1% 采样
    if (rand() % 100 == 0) {
        HiLog::INFO(LABEL, "Request succeeded: %s", request.GetId().c_str());
    }
} else {
    // 失败请求全量记录
    HiLog::ERROR(LABEL, "Request failed: %s, error=%s", 
                 request.GetId().c_str(), request.GetError().c_str());
}
```

---

## 附录 A：常见场景日志打印要求

### A.1 数据库操作

**增删改查操作**：
- 记录操作类型、操作对象、操作结果
- 不记录具体内容（防止泄露隐私）
- 性能敏感操作记录耗时

**示例**：
```cpp
HiLog::INFO(LABEL, "Database INSERT: table=users, rows=1, duration=5ms, result=success");
HiLog::WARN(LABEL, "Database DELETE: table=sessions, rows=0, warning=no matching records");
```

### A.2 文件操作

**常规操作**：
- 记录操作类型、文件名（系统文件）、操作结果
- 不记录文件内容
- 批量操作汇总记录

**示例**：
```cpp
HiLog::INFO(LABEL, "File created: path=/var/log/app.log, size=1024KB");
HiLog::INFO(LABEL, "Batch delete completed: directory=/tmp, deletedCount=150, totalSize=50MB");
```

### A.3 线程操作

**线程生命周期**：
- 记录创建、启动、暂停、终止
- 记录线程号、线程名

**示例**：
```cpp
HiLog::INFO(LABEL, "Thread started: name=Worker-1, id=0x12345678");
HiLog::ERROR(LABEL, "Thread deadlock detected: name=DataProcessor, blockedFor=30s");
```

### A.4 网络通信

**连接管理**：
- 记录连接建立、维持、断开及原因

**示例**：
```cpp
HiLog::INFO(LABEL, "TCP connection established: local=192.168.1.100:5000, remote=10.0.0.1:8080");
HiLog::WARN(LABEL, "TCP connection closed: reason=peer_timeout, duration=300s");
```

---

## 附录 B：与其他规范的引用关系

| 引用文档 | 关系说明 |
|---------|---------||
| [架构设计原则](../架构设计原则.md) | 本规范是原则 O-5（高效开发原则）在可观测性方面的实施细则 |
| [C&C++安全编程指南](./C&Cpp-secure-coding-guide.md) | 日志中的错误处理应遵循安全编程指南的异常处理规范 |
| [代码门禁要求](../代码门禁要求_v2.md) | 日志规范的符合性通过 CodeCheck 进行检查 |
| [统一术语表](../TERMINOLOGY.md) | 本规范使用的术语定义和解释 |

---

## 附录 C：快速索引

### 按规则编号

- **1-1** 信噪比最大化
- **1-2** 分层观测
- **1-3** 自适应调节
- **3-1** 语言规范
- **3-2** 禁止隐私信息
- **3-3** 禁止无关信息
- **3-4** 避免重复
- **3-5** 禁止业务调用
- **4-1** 高频代码禁打日志
- **4-2** 频率限制
- **4-3** 关键点必打
- **4-4** 延迟生成
- **4-5** 避免长日志
- **5-1** 独立 Domain
- **5-2** 层次化分配
- **5-3** 阈值管理
- **5-4** 隐私标识

### 按主题分类

**日志级别**：2.1 节  
**内容规范**：3.1-3.5  
**打印策略**：4.1-4.5  
**HiLog 接口**：5.1-5.4  
**质量管理**：6.1-6.3
