# 调度服务 (sched_d)

**版本**: v1.0.0.6  
**最后更新**: 2026-03-25  
**许可证**: Apache License 2.0

---

## 🎯 模块定位

`sched_d` 是 AgentOS 的**智能调度中心**，负责任务的资源分配和 Agent 选择优化。作为系统的"大脑皮层"，本服务基于多维度评分函数和实时负载情况，动态决策最优执行路径。

### 核心价值

- **多策略调度**: 支持轮询、加权、ML 预测等多种调度算法
- **负载均衡**: 基于实时指标的动态资源分配
- **优先级管理**: 支持任务优先级队列和抢占式调度
- **可插拔架构**: 调度策略运行时动态加载替换

---

## 📁 目录结构

```
sched_d/
├── README.md                 # 本文档
├── CMakeLists.txt            # 构建配置
├── include/
│   ├── scheduler_service.h   # 服务接口定义
│   └── strategy_interface.h  # 策略接口抽象
├── src/
│   ├── main.c                # 程序入口
│   ├── service.c             # 服务主逻辑
│   ├── scheduler_core.c      # 调度核心引擎
│   ├── load_balancer.c       # 负载均衡器
│   ├── priority_queue.c      # 优先级队列管理
│   ├── strategies/           # 调度策略实现
│   │   ├── round_robin.c     # 轮询策略
│   │   ├── weighted.c        # 加权策略
│   │   ├── ml_based.c        # ML 预测策略
│   │   └── priority.c        # 优先级策略
│   └── metrics_collector.c   # 指标采集器
├── tests/                    # 单元测试
│   ├── test_scheduler.c
│   ├── test_strategies.c
│   └── test_load_balance.c
└── manager/
    └── sched.yaml            # 服务配置模板
```

---

## 🔧 核心功能

### 1. 调度策略引擎

#### 策略接口定义

```c
// 调度策略接口
typedef struct {
    const char* name;
    int (*init)(void* manager);
    int (*select_agent)(agent_list_t* agents, task_t* task);
    void (*cleanup)(void);
} scheduling_strategy_t;
```

#### 内置策略

| 策略名称 | 算法 | 适用场景 | 性能特点 |
|---------|------|---------|---------|
| **Round Robin** | 轮询 | 均匀负载 | O(1) 时间复杂度 |
| **Weighted** | 加权轮询 | 异构资源 | 支持权重动态调整 |
| **Priority** | 优先级队列 | 紧急任务 | O(log n) 插入 |
| **ML-Based** | 机器学习预测 | 复杂场景 | 需要训练数据 |

### 2. 评分函数

调度决策的核心算法：

```c
// Agent 评分函数
Score(agent) = w₁ · (1/cost) + w₂ · success_rate + w₃ · trust_score
               + w₄ · availability + w₅ · specialization
```

**评分因子详解**:

| 因子 | 说明 | 取值范围 | 权重 |
|------|------|---------|------|
| `cost` | Agent 调用成本 (token 或时间) | > 0 | w₁=0.3 |
| `success_rate` | 历史成功率 | 0-1 | w₂=0.25 |
| `trust_score` | 信任评分 (用户评价) | 0-1 | w₃=0.2 |
| `availability` | 当前可用性 | 0-1 | w₄=0.15 |
| `specialization` | 专业匹配度 | 0-1 | w₅=0.1 |

**示例计算**:
```python
# Python 示例代码
def calculate_score(agent, task):
    cost_score = 1.0 / (agent.avg_cost + 0.1)
    success_score = agent.success_rate
    trust_score = agent.trust_rating
    avail_score = 1.0 if agent.status == 'idle' else 0.5
    spec_score = compute_specialization(agent.skills, task.required_skills)
    
    return (0.3 * cost_score + 
            0.25 * success_score + 
            0.2 * trust_score + 
            0.15 * avail_score + 
            0.1 * spec_score)
```

### 3. 负载均衡

实时监控和动态调整：

```yaml
# 负载均衡配置
load_balancing:
  algorithm: weighted_least_connections
  
  weights:
    cpu_weight: 0.4
    memory_weight: 0.3
    network_weight: 0.2
    disk_weight: 0.1
  
  thresholds:
    high_load_percent: 80
    low_load_percent: 20
    rebalance_interval_sec: 30
  
  health_check:
    enabled: true
    interval_sec: 10
    timeout_sec: 5
    unhealthy_threshold: 3
```

### 4. 优先级队列

支持多级优先级和抢占：

```yaml
priority:
  levels:
    - name: critical
      value: 100
      preemptive: true
    - name: high
      value: 75
      preemptive: false
    - name: normal
      value: 50
      preemptive: false
    - name: low
      value: 25
      preemptive: false
  
  scheduling:
    algorithm: strict_priority  # strict_priority/fair_sharing
    max_queue_size: 10000
    timeout_sec: 3600
```

---

## 🌐 API 接口

### RESTful API

#### 提交调度请求

```http
POST /api/v1/schedule
Content-Type: application/json

{
  "task_id": "task-12345",
  "task_type": "data_analysis",
  "required_skills": ["python", "pandas"],
  "priority": "high",
  "constraints": {
    "max_cost": 0.01,
    "deadline": "2026-03-25T12:00:00Z"
  }
}

Response 200:
{
  "schedule_id": "sched-001",
  "assigned_agent": {
    "id": "agent-data-001",
    "name": "数据分析师",
    "score": 0.92
  },
  "estimated_start": "2026-03-25T10:35:00Z",
  "estimated_completion": "2026-03-25T11:30:00Z"
}
```

#### 查询调度状态

```http
GET /api/v1/schedules/{schedule_id}

Response 200:
{
  "schedule_id": "sched-001",
  "status": "running",  # pending/running/completed/failed
  "progress_percent": 45,
  "current_agent": "agent-data-001",
  "metrics": {
    "cpu_usage": 65.2,
    "memory_usage": 2.1GB,
    "elapsed_time_sec": 1234
  }
}
```

#### 获取 Agent 列表

```http
GET /api/v1/agents/available
Query Parameters:
  - skill: 过滤技能
  - min_score: 最低评分
  - status: 状态过滤

Response 200:
{
  "total": 15,
  "available": 8,
  "agents": [
    {
      "id": "agent-data-001",
      "skills": ["python", "data_analysis"],
      "score": 0.92,
      "status": "idle",
      "current_load": 0.3
    },
    ...
  ]
}
```

#### 更新调度策略

```http
PUT /api/v1/strategy
Content-Type: application/json

{
  "strategy_name": "weighted_round_robin",
  "manager": {
    "weights": {
      "agent-001": 3,
      "agent-002": 2,
      "agent-003": 1
    }
  }
}

Response 200:
{
  "status": "updated",
  "effective_at": "2026-03-25T10:30:00Z"
}
```

### gRPC 接口

```protobuf
service SchedulerService {
  // 调度决策
  rpc ScheduleTask(ScheduleRequest) returns (ScheduleResponse);
  
  // 取消调度
  rpc CancelSchedule(CancelRequest) returns (CancelResponse);
  
  // 查询状态
  rpc GetScheduleStatus(StatusRequest) returns (StatusResponse);
  
  // 列出可用 Agent
  rpc ListAvailableAgents(AgentListRequest) returns (AgentListResponse);
  
  // 更新策略
  rpc UpdateStrategy(StrategyUpdateRequest) returns (StrategyResponse);
}
```

---

## ⚙️ 配置选项

### 服务配置 (manager/sched.yaml)

```yaml
server:
  port: 8083
  grpc_port: 9083
  max_connections: 1000

scheduler:
  default_strategy: weighted_round_robin  # 默认策略
  
  strategies:
    round_robin:
      enabled: true
    weighted:
      enabled: true
      default_weights:
        - agent_id: agent-001
          weight: 3
        - agent_id: agent-002
          weight: 2
    ml_based:
      enabled: false
      model_path: /var/agentos/models/scheduler_model.pkl
      retrain_interval_hours: 24

load_balancer:
  enabled: true
  algorithm: weighted_least_connections
  health_check_interval_sec: 10
  
priority:
  enabled: true
  levels:
    critical: 100
    high: 75
    normal: 50
    low: 25

scoring:
  weights:
    cost: 0.3
    success_rate: 0.25
    trust_score: 0.2
    availability: 0.15
    specialization: 0.1

logging:
  level: info
  format: json
  output: /var/agentos/logs/sched_d.log
```

---

## 🚀 使用指南

### 启动服务

```bash
# 方式 1: 直接启动
./build/daemon/sched_d/agentos-sched-d

# 方式 2: 指定配置文件
./build/daemon/sched_d/agentos-sched-d --manager ../manager/service/sched_d/sched.yaml

# 方式 3: systemd 管理
sudo systemctl enable agentos-sched-d
sudo systemctl start agentos-sched-d
```

### 提交调度任务

```bash
curl -X POST http://localhost:8083/api/v1/schedule \
  -H "Content-Type: application/json" \
  -d '{
    "task_id": "task-001",
    "task_type": "backend_development",
    "required_skills": ["python", "fastapi", "postgresql"],
    "priority": "normal",
    "max_budget": 0.05
  }'
```

### 查询调度结果

```bash
# 查询特定调度
curl http://localhost:8083/api/v1/schedules/sched-001

# 查看正在运行的任务
curl "http://localhost:8083/api/v1/schedules?status=running"
```

### 切换调度策略

```bash
# 切换到加权策略
curl -X PUT http://localhost:8083/api/v1/strategy \
  -H "Content-Type: application/json" \
  -d '{
    "strategy_name": "weighted",
    "manager": {
      "weights": {"agent-001": 5, "agent-002": 3}
    }
  }'
```

### 查看可用 Agent

```bash
# 列出所有可用 Agent
curl http://localhost:8083/api/v1/agents/available

# 按技能过滤
curl "http://localhost:8083/api/v1/agents/available?skill=python&min_score=0.8"
```

---

## 📊 性能指标

基于标准测试环境 (Intel i7-12700K, 32GB RAM):

| 指标 | 数值 | 测试条件 |
|------|------|---------|
| 调度延迟 | < 1ms | 单次调度决策 |
| 吞吐量 | 10000+ 任务/秒 | 轮询策略 |
| 并发连接 | 1000+ | 每服务实例 |
| 评分计算 | < 100μs | 单个 Agent 评分 |
| 负载均衡周期 | 30s | 默认重平衡间隔 |
| 健康检查延迟 | < 5ms | 单次检查 |

---

## 🧪 测试

### 运行单元测试

```bash
cd daemon/build
ctest -R sched_d --output-on-failure

# 运行特定测试
ctest -R test_scheduler --verbose
ctest -R test_strategies --verbose
```

### 策略对比测试

```python
import requests
import statistics

def benchmark_strategy(strategy_name, num_tasks=1000):
    """测试不同策略的性能"""
    latencies = []
    
    for i in range(num_tasks):
        start = time.time()
        response = requests.post(
            'http://localhost:8083/api/v1/schedule',
            json={
                'task_id': f'task-{i}',
                'task_type': 'test',
                'priority': 'normal'
            }
        )
        end = time.time()
        latencies.append((end - start) * 1000)
    
    return {
        'avg': statistics.mean(latencies),
        'p95': sorted(latencies)[int(len(latencies) * 0.95)],
        'p99': sorted(latencies)[int(len(latencies) * 0.99)]
    }

# 测试轮询策略
rr_result = benchmark_strategy('round_robin')
print(f"Round Robin - Avg: {rr_result['avg']:.2f}ms")

# 测试加权策略
weighted_result = benchmark_strategy('weighted')
print(f"Weighted - Avg: {weighted_result['avg']:.2f}ms")
```

---

## 🔧 故障排查

### 问题 1: 调度失败率高

**症状**: 大量任务调度失败

**解决方案**:
```bash
# 检查可用 Agent 数量
curl http://localhost:8083/api/v1/agents/available

# 查看调度日志
tail -f /var/agentos/logs/sched_d.log | grep "ERROR"

# 检查 Agent 健康状态
curl http://localhost:8083/api/v1/health
```

### 问题 2: 负载不均衡

**症状**: 部分 Agent 过载，部分空闲

**解决方案**:
```bash
# 查看各 Agent 负载分布
curl http://localhost:8083/api/v1/metrics/agent_load

# 调整权重配置
cat > /tmp/weights.json << EOF
{
  "strategy": "weighted",
  "weights": {"overloaded-agent": 1, "idle-agent": 5}
}
EOF
curl -X PUT http://localhost:8083/api/v1/strategy \
  -H "Content-Type: application/json" \
  -d @/tmp/weights.json
```

### 问题 3: 优先级不生效

**症状**: 高优先级任务未优先执行

**解决方案**:
```bash
# 检查优先级配置
curl http://localhost:8083/api/v1/manager/priority

# 查看队列状态
curl http://localhost:8083/api/v1/queue/status

# 验证调度策略
curl http://localhost:8083/api/v1/strategy/current
```

---

## 🔗 相关文档

- [服务层总览](../README.md) - daemon 架构说明
- [CoreLoopThree 架构](../../paper/architecture/folder/coreloopthree.md) - 调度器理论根基
- [Agent 调度算法](../../paper/architecture/folder/coreloopthree.md#调度官评分函数) - 评分函数详解
- [部署指南](../../paper/guides/folder/deployment.md) - 生产环境部署

---

<div align="center">

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges 始于数据，终于智能。"*

</div>

