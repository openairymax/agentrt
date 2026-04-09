Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 部署指南

**版本**: Doc V1.8
**最后更新**: 2026-04-09
**难度**: ⭐⭐⭐ 高级
**预计时间**: 45 分钟
**作者**: Team
  - Zhixian Zhou | Spharx Ltd. zhouzhixian@spharx.cn
  - Liren Wang | Spharx Ltd. wangliren@spharx.cn
  - Chen Zhang | SJTU CSC Lab. yoyoke@sjtu.edu.cn
  - Yunwen Xu | SJTU CSC Lab. willing419@sjtu.edu.cn
  - Daxiang Zhu | IndieBros. zdxever@sina.com

---

## 1. 概述

AgentOS 支持多种部署模式，从单机开发环境到分布式生产集群。本文档覆盖完整的部署流程：环境准备、编译构建、配置调优、服务管理、监控告警和升级回滚。

### 1.0 理论指导：MCIS视角的部署策略

部署是将 **体系并行论 (MCIS)** 理论框架从设计阶段转化为运行阶段的关键过程。从 MCIS 视角理解 AgentOS 的部署策略，有助于构建更加稳健、可扩展、可维护的生产系统。

#### 部署在 MCIS 系统中的理论意义

在 MCIS 理论中，部署过程体现了系统从概念到运行的完整转化：

1. **系统观的工程实现** → 部署是将系统工程层次分解理论转化为实际运行架构的过程
2. **控制论的反馈调节** → 部署过程中的监控与调整形成系统运行的负反馈回路
3. **动态平衡的实践** → 通过负载均衡、故障转移等机制维持系统运行的动态平衡
4. **渐进式抽象的部署体现** → 从开发环境到生产环境的渐进式部署验证

#### 部署模式与 MCIS 多体协同的映射

不同的部署模式对应 MCIS 中不同 **体 (Body)** 的协同方式：

- **开发模式** → 认知体、执行体、记忆体在单节点上的紧密集成，便于调试与开发
- **单机生产模式** → 完整的 MCIS 系统在单节点上的独立运行，适用于小规模场景
- **集群模式** → MCIS 系统在多个节点上的分布式运行，实现能力扩展与高可用性
- **混合模式** → 认知体、执行体、记忆体在不同类型节点上的优化部署，实现云边协同
- **嵌入式模式** → MCIS 系统的轻量化部署，适用于资源受限的边缘环境

#### 五维正交体系在部署中的体现

部署策略需要综合考虑 **五维正交体系** 的所有维度：

1. **系统观维度** → 系统架构的部署规划、组件间的网络拓扑、数据流的设计
2. **内核观维度** → 安全部署策略、权限控制、资源隔离机制
3. **认知观维度** → 智能体决策能力的部署优化、模型服务的分布式部署
4. **工程观维度** → 部署自动化、性能优化、容错机制、监控告警
5. **设计美学维度** → 部署流程的简洁性、文档的清晰性、故障恢复的优雅性

#### 部署的理论原则

基于 MCIS 理论的部署遵循以下原则：

1. **渐进式部署原则** → 从开发到测试到生产的渐进式验证与部署
2. **正交分离原则** → 不同组件的部署相互独立，支持独立升级与扩展
3. **反馈调节原则** → 部署过程中的监控与自动调整，形成自适应的部署系统
4. **安全内生原则** → 部署过程的安全验证、加密传输、权限控制
5. **可观测性原则** → 部署后的系统可观测性保障，支持运行状态监控与故障诊断

通过 MCIS 理论视角理解部署策略，你将能够设计出更加系统、稳健、可扩展的部署方案，为 AgentOS 在生产环境中的稳定运行提供理论指导与实践框架。

### 1.1 支持的部署模式

| 模式 | 适用场景 | 节点数 | 高可用 |
|------|---------|--------|--------|
| **开发模式** | 本地开发调试 | 1 | 否 |
| **单机生产** | 小规模生产环境 | 1 | 否 |
| **集群模式** | 中大规模生产环境 | 3+ | 是 |
| **混合模式** | 云+边缘协同 | 2+ | 是 |
| **嵌入式模式** | IoT / 边缘设备 | 1 | 否 |

---

## 2. 环境准备

### 2.1 系统要求

| 项目 | 最低要求 | 推荐配置 |
|------|---------|---------|
| **操作系统** | Linux 5.10+ / Windows 10+ / macOS 12+ | Ubuntu 22.04 LTS |
| **CPU** | x86_64 / ARM64 | 4 核+ |
| **内存** | 2 GB | 8 GB+ |
| **磁盘** | 10 GB | 50 GB SSD |
| **网络** | 本地回环 | 千兆以太网 |

### 2.2 依赖安装

**Linux (Ubuntu/Debian)**:

```bash
# 基础编译工具
sudo apt update
sudo apt install -y build-essential cmake git pkg-manager

# 可选依赖
sudo apt install -y libssl-dev libcurl4-openssl-dev libyaml-dev

# AgentOS CLI
wget -qO- https://releases.agentos.io/cli/install.sh | bash
```

**Windows**:

```powershell
# 使用 vcpkg 安装依赖
vcpkg install cmake pkg-manager

# AgentOS CLI
Invoke-WebRequest -Uri "https://releases.agentos.io/cli/install.ps1" -UseBasicParsing | Invoke-Expression
```

**macOS**:

```bash
brew install cmake openssl curl yaml-cpp
```

---

## 3. 编译构建

### 3.1 从源码构建

```bash
# 克隆仓库
git clone https://github.com/SpharxTeam/AgentOS.git
cd AgentOS

# 创建构建目录
mkdir build && cd build

# 配置 — 开发模式
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 配置 — 生产模式
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DAGENTOS_ENABLE_TESTS=OFF \
    -DAGENTOS_ENABLE_EXAMPLES=OFF

# 构建（并行）
cmake --build . --parallel $(nproc)

# 运行测试（可选）
ctest --output-on-failure

# 安装
sudo cmake --install .
```

### 3.2 交叉编译

```bash
# ARM64 交叉编译
cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# RISC-V 交叉编译
cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64-linux-gnu.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

### 3.3 CMake 配置选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `CMAKE_BUILD_TYPE` | `Release` | 构建类型：Debug/Release/RelWithDebInfo |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | 安装路径 |
| `AGENTOS_ENABLE_TESTS` | `ON` | 启用测试 |
| `AGENTOS_ENABLE_EXAMPLES` | `ON` | 启用示例 |
| `AGENTOS_ENABLE_SANITIZERS` | `OFF` | 启用 AddressSanitizer |
| `AGENTOS_ENABLE_LTO` | `OFF` | 启用链接时优化 |
| `AGENTOS_LOG_LEVEL` | `INFO` | 默认日志级别 |

---

## 4. 配置

### 4.1 目录结构

安装后的目录布局：

```
/usr/local/
├── lib/agentos/
│   ├── libagentos_core.so        # 内核库
│   ├── libagentos_syscall.so     # 系统调用库
│   ├── libagentos_memory.so      # 记忆系统库
│   └── agentos/daemon/                    # 用户态服务
│       ├── agent_manager_d       # Agent 管理器
│       ├── llm_d                 # LLM 网关
│       ├── market_d              # 技能市场
│       ├── monit_d               # 监控服务
│       └── tool_d                # 工具服务
├── share/agentos/
│   ├── agentos/manager/                   # 默认配置
│   │   ├── agentos.yaml          # 主配置
│   │   ├── security/             # 安全配置
│   │   │   ├── policy.yaml       # 权限策略
│   │   │   └── sanitizer.yaml    # 净化规则
│   │   └── memory/               # 记忆配置
│   │       └── rovol.yaml        # 记忆卷配置
│   └── contracts/                # 契约文件
├── bin/
│   ├── agentos-cli               # 命令行工具
│   └── agentosd                  # 内核守护进程
└── var/
    ├── run/agentos/              # 运行时文件
    │   ├── agentos.sock          # IPC 套接字
    │   └── agentos.pid           # PID 文件
    └── lib/agentos/              # 持久化数据
        └── memory/               # 记忆存储
```

### 4.2 主配置文件

```yaml
# /etc/agentos/agentos.yaml

agentos:
  version: "1.0.0.6"
  instance_id: "prod-node-01"
  environment: production

corekern:
  ipc:
    max_channels: 4096
    max_message_size: 1048576    # 1MB
    timeout_ms: 5000
  memory:
    pool_size_mb: 256
    max_allocations: 100000
  task:
    max_concurrent: 256
    scheduler: "priority_fifo"
  time:
    precision_ns: 1000           # 1 微秒精度

coreloopthree:
  cognitive:
    model_primary: "default"
    model_auxiliary: "fast"
    max_retries: 3
    timeout_ms: 30000
  planning:
    strategy: "incremental"
    max_stages: 64
    replan_interval_ms: 5000
  execution:
    strategy: "parallel"
    max_workers: 8
    compensation_enabled: true

memoryrovol:
  layers:
    L1:
      enabled: true
      backend: "mmap"
      max_size_mb: 512
      ttl_seconds: 3600
    L2:
      enabled: true
      backend: "lmdb"
      max_size_mb: 2048
      index_type: "hnsw"
    L3:
      enabled: true
      backend: "sqlite"
      max_size_mb: 10240
    L4:
      enabled: true
      backend: "custom"
      persistence: true

cupolas:
  workbench:
    enabled: true
    isolation_level: "process"
  permission:
    engine: "rbac"
    policy_file: "/etc/agentos/security/policy.yaml"
  sanitizer:
    level: "strict"
    rules_file: "/etc/agentos/security/sanitizer.yaml"
  audit:
    enabled: true
    backend: "file"
    path: "/var/log/agentos/audit.log"
    rotation: "daily"

telemetry:
  logging:
    level: "INFO"
    format: "json"
    output: "/var/log/agentos/agentos.log"
    rotation:
      max_size_mb: 100
      max_files: 10
  metrics:
    enabled: true
    endpoint: "http://prometheus:9090"
    interval_seconds: 15
  tracing:
    enabled: true
    sampler: "probabilistic"
    rate: 0.1
    endpoint: "http://jaeger:14268/api/traces"

network:
  gateway:
    type: "http_ws"
    bind_address: "0.0.0.0"
    http_port: 8080
    ws_port: 8081
    max_connections: 10000
  tls:
    enabled: true
    cert_file: "/etc/agentos/certs/server.pem"
    key_file: "/etc/agentos/certs/server.key"
```

### 4.3 安全配置

```yaml
# /etc/agentos/security/policy.yaml

version: "1.2.0"

default_policy: deny

roles:
  admin:
    permissions: ["*"]
  developer:
    permissions: [
      "task.submit", "task.query", "task.cancel",
      "memory.read", "memory.write",
      "agent.list", "agent.status",
      "skill.list", "skill.execute"
    ]
  viewer:
    permissions: [
      "task.query", "agent.list", "agent.status",
      "skill.list"
    ]

agents:
  default:
    sandbox: workbench
    permissions: ["memory.own", "task.own"]
    resource_limits:
      max_memory: "256MB"
      max_cpu: "50%"
      max_tasks: 64
      timeout: 120
```

---

## 5. 服务管理

### 5.1 Systemd 配置

```ini
# /etc/systemd/system/agentos.service
[Unit]
Description=AgentOS Kernel Daemon
After=network.target
Documentation=https://docs.agentos.io

[Service]
Type=notify
User=agentos
Group=agentos
ExecStart=/usr/local/bin/agentosd --manager /etc/agentos/agentos.yaml
ExecReload=/bin/kill -HUP $MAINPID
Restart=on-failure
RestartSec=5
LimitNOFILE=65536
TimeoutStopSec=30

# 安全加固
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/agentos /var/log/agentos
PrivateTmp=true

[Install]
WantedBy=multi-user.target
```

```bash
# 启动服务
sudo systemctl daemon-reload
sudo systemctl enable agentos
sudo systemctl start agentos

# 查看状态
sudo systemctl status agentos

# 查看日志
sudo journalctl -u agentos -f
```

### 5.2 Windows 服务

```powershell
# 注册为 Windows 服务
agentosd --install --manager C:\AgentOS\manager\agentos.yaml

# 启动服务
Start-Service AgentOS

# 查看状态
Get-Service AgentOS
```

### 5.3 Docker 部署

```dockerfile
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y build-essential cmake git
COPY . /src/AgentOS
WORKDIR /src/AgentOS
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --parallel

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libssl3 libcurl4
COPY --from=builder /src/AgentOS/build /usr/local
COPY --from=builder /src/AgentOS/manager /etc/agentos
EXPOSE 8080 8081
CMD ["agentosd", "--manager", "/etc/agentos/agentos.yaml"]
```

```yaml
# docker-compose.yaml
version: "3.8"
services:
  agentos:
    build: .
    ports:
      - "8080:8080"
      - "8081:8081"
    volumes:
      - ./manager:/etc/agentos
      - agentos-data:/var/lib/agentos
      - agentos-logs:/var/log/agentos
    environment:
      - AGENTOS_ENV=production
    restart: unless-stopped
    deploy:
      resources:
        limits:
          memory: 2G
          cpus: "2.0"

  prometheus:
    image: prom/prometheus:latest
    ports:
      - "9090:9090"
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    volumes:
      - ./monitoring/grafana:/etc/grafana

volumes:
  agentos-data:
  agentos-logs:
```

---

## 6. 集群部署

### 6.1 Kubernetes 部署

```yaml
# kubernetes/agentos-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: agentos
  labels:
    app: agentos
spec:
  replicas: 3
  selector:
    matchLabels:
      app: agentos
  template:
    metadata:
      labels:
        app: agentos
    spec:
      containers:
        - name: agentos
          image: spharx/agentos:1.0.0.6
          ports:
            - containerPort: 8080
            - containerPort: 8081
          resources:
            requests:
              memory: "512Mi"
              cpu: "500m"
            limits:
              memory: "2Gi"
              cpu: "2000m"
          volumeMounts:
            - name: manager
              mountPath: /etc/agentos
            - name: data
              mountPath: /var/lib/agentos
          livenessProbe:
            httpGet:
              path: /health
              port: 8080
            initialDelaySeconds: 10
            periodSeconds: 30
          readinessProbe:
            httpGet:
              path: /ready
              port: 8080
            initialDelaySeconds: 5
            periodSeconds: 10
      volumes:
        - name: manager
          configMap:
            name: agentos-manager
        - name: data
          persistentVolumeClaim:
            claimName: agentos-data
```

### 6.2 高可用配置

集群模式下需要以下外部依赖：

| 组件 | 用途 | 推荐方案 |
|------|------|---------|
| **服务发现** | 节点注册与发现 | etcd / Consul |
| **消息队列** | 跨节点任务分发 | NATS / RabbitMQ |
| **共享存储** | 记忆数据持久化 | NFS / Ceph / S3 |
| **负载均衡** | 请求分发 | Nginx / HAProxy |
| **监控** | 指标收集和告警 | Prometheus + Grafana |
| **日志** | 集中式日志 | ELK Stack / Loki |

---

## 7. 监控与告警

### 7.1 健康检查

```bash
# HTTP 健康检查
curl -s http://localhost:8080/health | jq .

# 响应示例
{
  "status": "healthy",
  "version": "1.0.0.6",
  "uptime_seconds": 86400,
  "components": {
    "corekern": "healthy",
    "coreloopthree": "healthy",
    "memoryrovol": "healthy",
    "cupolas": "healthy",
    "backends": {
      "agent_manager_d": "running",
      "llm_d": "running",
      "market_d": "running",
      "monit_d": "running",
      "tool_d": "running"
    }
  }
}
```

### 7.2 关键指标

| 指标 | 说明 | 告警阈值 |
|------|------|---------|
| `agentos_tasks_total` | 任务总数 | - |
| `agentos_tasks_active` | 活跃任务数 | > 200 |
| `agentos_tasks_failed_rate` | 任务失败率 | > 5% |
| `agentos_ipc_messages_per_sec` | IPC 消息吞吐 | < 100 (异常低) |
| `agentos_memory_usage_bytes` | 内存使用量 | > 80% 容量 |
| `agentos_ipc_latency_ms_p99` | IPC P99 延迟 | > 50ms |
| `agentos_agent_count` | 注册 Agent 数 | < 1 (异常) |

### 7.3 日志管理

```bash
# 查看实时日志
agentos-cli log tail --level INFO --follow

# 按组件过滤
agentos-cli log tail --component coreloopthree --follow

# 按 TraceID 追踪
agentos-cli log trace --trace-id abc123
```

---

## 8. 升级与回滚

### 8.1 滚动升级

```bash
# 1. 检查当前版本
agentos-cli version

# 2. 下载新版本
agentos-cli update --version 1.0.0.7 --dry-run

# 3. 执行升级
sudo agentos-cli update --version 1.0.0.7

# 4. 验证升级
agentos-cli version
agentos-cli health check
```

### 8.2 回滚

```bash
# 回滚到上一版本
sudo agentos-cli rollback

# 回滚到指定版本
sudo agentos-cli rollback --version 1.0.0.5
```

### 8.3 数据迁移

```bash
# 升级前备份记忆数据
agentos-cli memory backup --output /tmp/memory_backup.tar.gz

# 升级后验证数据完整性
agentos-cli memory verify --backup /tmp/memory_backup.tar.gz
```

---

## 9. 故障排查

参见 [故障排查指南](troubleshooting.md) 获取详细的故障诊断流程。

### 9.1 常见部署问题

| 问题 | 原因 | 解决方案 |
|------|------|---------|
| 端口被占用 | 其他服务占用 8080 | 修改 `network.gateway.http_port` |
| 权限不足 | 非 root 用户无法绑定端口 | 使用 `setcap` 或修改端口 > 1024 |
| 内存不足 | 记忆系统配置过大 | 调整 `memoryrovol.layers.*.max_size_mb` |
| IPC 超时 | 消息队列溢出 | 增加 `corekern.ipc.max_channels` |
| Agent 启动失败 | 契约验证未通过 | 检查 `agent_contract.json` |

---

## 10. 安全加固

### 10.1 生产环境安全清单

- [ ] 启用 TLS 加密通信
- [ ] 配置防火墙规则（仅开放必要端口）
- [ ] 使用非 root 用户运行
- [ ] 启用审计日志
- [ ] 配置输入净化规则
- [ ] 定期更新依赖（`agentos-cli security audit`）
- [ ] 限制资源使用（cgroup / namespace）
- [ ] 启用 SELinux / AppArmor

### 10.2 安全扫描

```bash
# 检查依赖漏洞
agentos-cli security scan --sbom sbom.json

# 检查配置安全
agentos-cli security audit --manager /etc/agentos/agentos.yaml

# 检查许可证合规
agentos-cli security license --report
```

---

## 11. 相关文档

- [快速开始](getting_started.md)
- [内核调优](kernel_tuning.md)
- [故障排查](troubleshooting.md)
- [架构设计原则](../architecture/ARCHITECTURAL_PRINCIPLES.md)
- [迁移指南](migration_guide.md)

---

**最后更新**: 2026-03-23  
**维护者**: AgentOS 运维团队

---

© 2026 SPHARX Ltd. All Rights Reserved.  
*"From data intelligence emerges."*