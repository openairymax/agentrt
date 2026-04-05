# 快速开始指南

**版本**: 1.0.0  
**难度**: ⭐ 入门  
**预计时间**: 5分钟  

---

## 🎯 本指南目标

在5分钟内，让您体验AgentOS的核心能力：创建智能体、提交任务、观察执行过程。

---

## 📋 前置条件

| 组件 | 最低要求 | 推荐配置 |
|------|---------|---------|
| 操作系统 | Ubuntu 20.04+ / macOS 12+ / Windows 10+ | Ubuntu 22.04 LTS |
| CPU | 2核心 | 4核心以上 |
| 内存 | 4GB RAM | 8GB RAM以上 |
| 磁盘空间 | 10GB可用空间 | 20GB SSD |
| Docker | 20.10+ | 最新稳定版 |
| Python | 3.9+ | 3.11 |
| Git | 2.30+ | 最新版本 |

---

## 🚀 方式一：Docker快速体验（推荐）

### 步骤1：克隆仓库

```bash
git clone https://gitcode.com/spharx/agentos.git && cd agentos
```

### 步骤2：配置环境变量

```bash
# 复制环境变量模板
cp docker/.env.example docker/.env

# 编辑关键配置（至少修改以下两项）
nano docker/.env
```

必须修改的配置项：

```env
# 数据库密码（不要使用默认值！）
POSTGRES_PASSWORD=your_secure_password_here

# Redis密码
REDIS_PASSWORD=your_redis_password_here

# LLM API密钥
AGENTOS_LLM_API_KEY=sk-your-actual-api-key
```

### 步骤3：启动服务

```bash
# 启动所有服务（首次需要构建镜像，约5-10分钟）
docker compose -f docker/docker-compose.yml up -d

# 查看服务状态
docker compose -f docker/docker-compose.yml ps
```

预期输出：

```
NAME                      STATUS                    PORTS
agentos-kernel-dev        Up (healthy)              0.0.0.0:8080->8080/tcp, 0.0.0.0:9090->9090/tcp
agentos-postgres-dev      Up (healthy)              0.0.0.0:5432->5432/tcp
agentos-redis-dev         Up (healthy)              0.0.0.0:6379->6379/tcp
```

### 步骤4：验证安装

```bash
# 测试内核健康检查
curl http://localhost:8080/health

# 预期响应：
# {"status": "ok", "version": "1.0.0", "uptime": 120}
```

---

## 🛠️ 方式二：源码编译安装

### 步骤1：安装依赖

**Ubuntu/Debian:**

```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential cmake ninja-build gcc g++ \
    python3 python3-pip python3-venv \
    git libssl-dev libyaml-dev libcjson-dev \
    pkg-config curl valgrind
```

**macOS (使用Homebrew):**

```bash
brew install cmake ninja gcc python3 openssl yaml-cpp cjson
```

### 步骤2：克隆并编译

```bash
git clone https://gitcode.com/spharx/agentos.git && cd agentos

# 配置CMake
mkdir build && cd build
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_WITH_SANITIZERS=ON

# 编译（约3-5分钟）
cmake --build . --parallel $(nproc)

# 运行测试
ctest --output-on-failure
```

### 步骤3：启动内核

```bash
# 回到项目根目录
cd ..

# 创建配置目录
mkdir -p config data logs

# 复制默认配置
cp agentos/config/default.yaml config/

# 启动内核
./build/bin/agentos-kernel --config config/default.yaml
```

---

## ✨ 第一个智能体任务

### 使用Python SDK

```python
from agentos import AgentOSClient

# 连接到AgentOS内核
client = AgentOSClient(
    base_url="http://localhost:8080",
    api_key="your_api_key"  # 开发模式可为空
)

# 创建智能体
agent = client.create_agent(
    name="my-first-agent",
    description="我的第一个AgentOS智能体"
)

# 提交任务
task = agent.submit_task(
    description="分析以下财报并生成摘要：[粘贴财报内容]",
    priority="normal",
    timeout_seconds=300
)

# 等待任务完成
result = task.wait_for_completion()

print(f"任务结果: {result.output}")
print(f"执行耗时: {result.duration_ms}ms")
```

### 使用命令行工具

```bash
# 安装CLI工具
pip install agentos-toolkit

# 初始化客户端
agentos init --url http://localhost:8080

# 提交任务
agentos task submit --description "分析这份财报" --file report.pdf

# 查看任务列表
agentos task list

# 查看任务详情
agentos task get <task_id>
```

---

## 📊 观察执行过程

### 实时日志

```bash
# 查看内核日志
docker logs -f agentos-kernel-dev

# 查看特定模块日志
docker logs agentos-kernel-dev 2>&1 | grep cognition
```

### Prometheus监控

打开浏览器访问：

- **Prometheus**: http://localhost:9091
- **Grafana**: http://localhost:3000 (用户名/密码: admin/admin)

查看关键指标：

- `agentos_ipc_requests_total` — IPC请求总数
- `agentos_memory_records_total` — 记忆条目数量
- `agentos_cupolas_permission_checks_total` — 权限检查次数

---

## 🎉 下一步

恭喜！您已经成功运行了第一个AgentOS任务。

接下来可以学习：

- [**安装完整版**](installation.md) — 了解所有安装选项和高级配置
- [**配置指南**](configuration.md) — 自定义记忆系统、LLM服务等
- [**架构概览**](architecture/overview.md) — 理解微内核、三层循环等核心概念
- [**API参考**](api/kernel-api.md) — 掌握完整的API接口

---

## ❓ 常见问题

**Q: Docker启动失败？**

A: 检查Docker版本（需20.10+），确保端口8080/5432/6379未被占用。

**Q: 内存不足？**

A: 关闭其他应用，或调整 `.env` 中的 `MEMORY_LIMIT` 参数。

**Q: LLM API调用失败？**

A: 检查 `.env` 中的 `AGENTOS_LLM_API_KEY` 是否正确，网络是否能访问OpenAI API。

---

> *"千里之行，始于足下。"*

**© 2026 SPHARX Ltd. All Rights Reserved.**
