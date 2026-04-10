Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# 安装指南

**版本**: Doc V1.8
**最后更新**: 2026-04-10
**难度**: ⭐⭐ 基础
**预计时间**: 15-30分钟（取决于安装方式）
**作者**: Team  
**维护者**: AgentOS 文档团队

---

## 📋 安装方式总览

| 方式 | 适用场景 | 复杂度 | 可定制性 |
|------|---------|--------|---------|
| **Docker Compose** | 快速体验、开发测试 | ⭐ 低 | 中 |
| **Kubernetes** | 生产部署、集群管理 | ⭐⭐⭐ 高 | 高 |
| **源码编译** | 深度开发、性能优化 | ⭐⭐ 中 | 最高 |
| **包管理器** | 生产环境快速部署 | ⭐⭐ 中 | 低 |

---

## 🔧 方式一：Docker Compose安装（推荐）

### 系统要求

- **Docker Engine**: >= 20.10
- **Docker Compose**: >= 2.0
- **磁盘空间**: >= 10GB
- **内存**: >= 4GB（推荐8GB）

### 详细步骤

#### 1. 安装Docker

**Ubuntu/Debian:**

```bash
# 更新apt索引
sudo apt-get update

# 安装依赖
sudo apt-get install -y \
    ca-certificates \
    curl \
    gnupg \
    lsb-release

# 添加Docker官方GPG密钥
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg

# 添加Docker仓库
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# 安装Docker Engine
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin

# 将当前用户添加到docker组（避免每次sudo）
sudo usermod -aG docker $USER
newgrp docker
```

**macOS:**

下载并安装 [Docker Desktop for Mac](https://www.docker.com/products/docker-desktop/)。

**Windows:**

下载并安装 [Docker Desktop for Windows](https://www.docker.com/products/docker-desktop/)（需要WSL2）。

#### 2. 克隆AgentOS仓库

```bash
git clone https://gitcode.com/spharx/agentos.git && cd agentos
```

#### 3. 配置环境变量

```bash
# 复制模板文件
cp docker/.env.example docker/.env

# 编辑配置
nano docker/.env
```

**必填项说明：**

```env
# ========================================
# 必须修改的安全配置项
# ========================================

# PostgreSQL密码（强度要求：12位以上，包含大小写字母、数字、特殊字符）
POSTGRES_PASSWORD=YourSecureP@ssw0rd2026!

# Redis密码
REDIS_PASSWORD=YourRedisS3cretKey!

# LLM API密钥（根据您使用的提供商填写）
AGENTOS_LLM_API_KEY=sk-proj-xxxxxxxxxxxxxxxxxxxxxxxx

# OpenLab会话加密密钥（32位随机字符串）
OPENLAB_SECRET_KEY=$(openssl rand -base64 32)
```

#### 4. 构建并启动服务

```bash
# 构建所有镜像（首次约5-10分钟）
docker compose -f docker/docker-compose.yml build

# 启动所有服务
docker compose -f docker/docker-compose.yml up -d

# 查看启动日志（确认无错误）
docker compose -f docker/docker-compose.yml logs -f
```

#### 5. 验证安装

```bash
# 检查所有容器状态
docker compose -f docker/docker-compose.yml ps

# 预期输出（所有容器应为Up状态）：
# NAME                      STATUS                    PORTS
# agentos-kernel-dev        Up (health: healthy)       ...
# agentos-postgres-dev      Up (health: healthy)       ...
# agentos-redis-dev         Up (health: healthy)       ...

# 测试健康检查端点
curl http://localhost:8080/health

# 预期响应：
# {"status":"ok","version":"1.0.0","uptime":42,"components":{"ipc":"ok","memory":"ok","security":"ok"}}
```

#### 6. （可选）启用监控栈

```bash
# 启用Prometheus + Grafana
docker compose -f docker/docker-compose.yml --profile monitoring up -d prometheus grafana

# 访问Grafana：http://localhost:3000
# 默认账号：admin / admin
```

---

## ☸️ 方式二：Kubernetes部署

### 前置条件

- Kubernetes集群（v1.25+）
- kubectl已配置并可访问集群
- Helm 3.x（推荐）
- PersistentVolume支持（用于数据持久化）

### 使用Helm Chart部署

```bash
# 添加AgentOS Helm仓库
helm repo add spharx https://charts.spharx.cn/agentos
helm repo update

# 创建命名空间
kubectl create namespace agentos

# 安装AgentOS（自定义values）
helm install agentos spharx/agentos \
    --namespace agentos \
    --values my-values.yaml \
    --set global.image.tag=v1.0.0 \
    --set postgresql.auth.password=your_password \
    --set redis.password=your_password \
    --set kernel.env.AGENTOS_LLM_API_KEY=your_llm_key
```

**my-values.yaml 示例：**

```yaml
global:
  imagePullSecrets:
    - name: registry-credentials

kernel:
  replicas: 3
  resources:
    requests:
      cpu: "2"
      memory: "2Gi"
    limits:
      cpu: "4"
      memory: "4Gi"

postgresql:
  primary:
    persistence:
      size: 50Gi

redis:
  master:
    persistence:
      size: 10Gi

monitoring:
  enabled: true
  grafana:
    adminPassword: your_grafana_password
```

### 验证部署

```bash
# 查看所有Pod状态
kubectl get pods -n agentos

# 查看服务状态
kubectl get svc -n agentos

# 端口转发访问内核API
kubectl port-forward svc/agentos-kernel 8080:8080 -n agentos

# 测试
curl http://localhost:8080/health
```

详细Kubernetes部署文档请参考：[Kubernetes部署指南](../operations/kubernetes-deployment.md)

---

## 📦 方式三：源码编译安装

### 系统依赖

**Ubuntu 22.04 LTS（推荐开发环境）：**

```bash
sudo apt-get update && sudo apt-get install -y \
    # C/C++ 编译工具链
    build-essential cmake ninja-build gcc g++ clang \
    # Python环境
    python3 python3-dev python3-pip python3-venv \
    # 系统库
    libssl-dev libyaml-dev libcjson-dev libpq-dev \
    # 版本控制
    git \
    # 调试和分析工具
    valgrind gdb \
    # 静态分析
    clang-tools cppcheck \
    # 其他工具
    pkg-config curl wget
```

### 编译步骤

#### 1. 获取源码

```bash
git clone --recursive https://gitcode.com/spharx/agentos.git && cd agentos

# 如果忘记--recursive，手动初始化子模块
git submodule update --init --recursive
```

#### 2. 配置CMake

```bash
mkdir -p build && cd build

# Release版本（生产环境）
cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=ON \
    -DBUILD_WITH_SANITIZERS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr/local

# Debug版本（开发调试）
# cmake .. \
#     -G Ninja \
#     -DCMAKE_BUILD_TYPE=Debug \
#     -DBUILD_TESTS=ON \
#     -DBUILD_WITH_SANITIZERS=ON \
#     -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
#     -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
```

**CMake选项说明：**

| 选项 | 默认值 | 说明 |
|------|-------|------|
| `CMAKE_BUILD_TYPE` | Release | Debug/Release/RelWithDebInfo |
| `BUILD_TESTS` | OFF | 是否编译测试代码 |
| `BUILD_WITH_SANITIZERS` | OFF | 启用ASan/TSan/UBSan |
| `CMAKE_INSTALL_PREFIX` | /usr/local | 安装路径前缀 |

#### 3. 编译

```bash
# 并行编译（利用所有CPU核心）
cmake --build . --parallel $(nproc)

# 仅编译内核
cmake --build . --target agentos-kernel --parallel $(nproc)

# 查看编译进度
cmake --build . --verbose  # 显示详细编译命令
```

**预期编译时间：**

| 环境 | 时间 | 说明 |
|------|------|------|
| 4核8GB | 5-8分钟 | 完整编译 |
| 8核16GB | 2-4分钟 | 完整编译 |
| 16核32GB | 1-2分钟 | 完整编译 |

#### 4. 运行测试

```bash
# 运行所有单元测试
ctest --output-on-failure

# 运行特定测试套件
ctest -R memoryrovol --output-on-failure

# 输出详细测试报告
ctest --output-on-failure --output-junit test-results.xml

# 查看测试覆盖率（如果启用了coverage）
make coverage  # 或 ninja coverage
```

#### 5. 安装

```bash
# 安装到系统路径
sudo cmake --install .

# 或者仅安装到本地目录
DESTDIR=./package cmake --install .

# 验证安装
agentos-kernel --version
# 预期输出：AgentOS Kernel v1.0.0 (Build: 20260405)
```

#### 6. 后续配置

```bash
# 回到项目根目录
cd ..

# 创建必要目录
mkdir -p config data logs

# 复制默认配置
cp agentos/config/examples/*.yaml config/

# 编辑配置文件
nano config/kernel.yaml
```

---

## 🔄 升级指南

### Docker升级

```bash
# 拉取最新镜像
docker compose -f docker/docker-compose.yml pull

# 重新创建容器（保留数据）
docker compose -f docker/docker-compose.yml up -d

# 执行数据库迁移（如果有schema变更）
docker compose -f docker/docker-compose.yml exec postgres psql -U agentos -d agentos -c "SELECT version();"
```

### 源码升级

```bash
# 拉取最新代码
git pull origin main

# 更新子模块
git submodule update --init --recursive

# 重新编译
cd build
cmake ..
cmake --build . --parallel $(nproc)

# 运行测试确保兼容性
ctest --output-on-failure

# 重启服务
sudo systemctl restart agentos-kernel
```

---

## ❓ 故障排查

### 编译错误

**问题**: `cmake找不到OpenSSL`

**解决**:
```bash
# 安装开发包
sudo apt-get install libssl-dev

# 手动指定路径
cmake .. -DOPENSSL_ROOT_DIR=/usr
```

**问题**: `内存不足导致编译失败`

**解决**:
```bash
# 减少并行数
cmake --build . --parallel 2

# 或增加swap空间
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

### 运行时错误

**问题**: `端口被占用`

**解决**:
```bash
# 查找占用进程
lsof -i :8080

# 杀掉进程或修改端口
kill <PID>
# 或修改 config/kernel.yaml 中的端口配置
```

**问题**: `数据库连接失败`

**解决**:
```bash
# 检查PostgreSQL是否运行
docker ps | grep postgres

# 查看PostgreSQL日志
docker logs agentos-postgres-dev

# 检查连接字符串是否正确
grep DATABASE_URL docker/.env
```

---

## 📚 相关文档

- [**配置指南**](configuration.md) — 完整的配置选项说明
- [**Docker部署**](../docker/README.md) — Docker详细文档
- [**Kubernetes部署**](../operations/kubernetes-deployment.md) — K8s生产部署
- [**故障排查**](../troubleshooting/common-issues.md) — 更多问题解决方案

---

> *"工欲善其事，必先利其器。"*

**© 2026 SPHARX Ltd. All Rights Reserved.**
