# 部署指南

**版本**: 1.0.0  
**最后更新**: 2026-04-05  
**适用场景**: 生产环境部署  

---

## 📋 部署方式选择

| 方式 | 适用规模 | 运维复杂度 | 推荐度 |
|------|---------|-----------|-------|
| **Docker Compose** | 开发/测试/小规模生产 | ⭐⭐ 低 | ⭐⭐⭐ 推荐 |
| **Kubernetes** | 中大规模生产/高可用 | ⭐⭐⭐ 高 | ⭐⭐⭐⭐ 强烈推荐 |
| **裸机部署** | 特殊需求/性能极致 | ⭐⭐⭐⭐ 极高 | ⭐ 仅特殊场景 |

---

## 🐳 方式一：Docker Compose 生产部署

### 前置条件

- **操作系统**: Ubuntu 22.04 LTS / CentOS Stream 9 / Debian 12
- **硬件要求**:
  - CPU: ≥4核心（推荐8核心）
  - 内存: ≥16GB RAM（推荐32GB）
  - 磁盘: ≥100GB SSD（推荐NVMe）
  - 网络: ≥100Mbps带宽
- **软件依赖**:
  - Docker Engine >= 24.0
  - Docker Compose >= 2.20
  - OpenSSL（用于生成证书）

---

### 步骤1：服务器初始化

```bash
# 更新系统
sudo apt-get update && sudo apt-get upgrade -y

# 安装基础工具
sudo apt-get install -y \
    curl wget git vim htop net-tools \
    openssl ca-certificates \
    ufw fail2ban

# 配置防火墙
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow ssh
sudo ufw allow 80/tcp    # HTTP (OpenLab)
sudo ufw allow 443/tcp   # HTTPS (OpenLab)
sudo ufw allow 8080/tcp  # IPC API (内网访问)
sudo ufw enable

# 设置Fail2Ban保护SSH
sudo systemctl enable fail2ban
sudo systemctl start fail2ban

# 优化内核参数
sudo tee -a /etc/sysctl.d/99-agentos.conf <<EOF
# 网络优化
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.ipv4.tcp_tw_reuse = 1
net.core.netdev_max_backlog = 65535

# 文件描述符限制
fs.file-max = 2097152
fs.inotify.max_user_watches = 524288

# 内存优化
vm.swappiness = 10
vm.dirty_ratio = 10
vm.dirty_background_ratio = 5
EOF

sudo sysctl -p /etc/sysctl.d/99-agentos.conf

# 提高文件描述符限制
sudo tee -a /etc/security/limits.d/agentos.conf <<EOF
* soft nofile 65535
* hard nofile 65535
* soft nproc 65535
* hard nproc 65535
EOF
```

---

### 步骤2：安装Docker

```bash
# 添加Docker GPG密钥
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg

# 添加Docker仓库
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# 安装Docker
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# 启动并设置开机自启
sudo systemctl enable docker
sudo systemctl start docker

# 将当前用户添加到docker组
sudo usermod -aG docker $USER
newgrp docker

# 验证安装
docker --version
docker compose version
```

---

### 步骤3：准备SSL证书（HTTPS）

#### 方式A: Let's Encrypt免费证书（推荐）

```bash
# 安装Certbot
sudo apt-get install -y certbot python3-certbot-nginx

# 申请证书（需要域名已解析到此服务器）
sudo certbot certonly --standalone \
    -d openlab.spharx.cn \
    -d api.spharx.cn \
    --email admin@spharx.cn \
    --agree-tos \
    --no-eff-email

# 证书路径：
# /etc/letsencrypt/live/openlab.spharx.cn/fullchain.pem
# /etc/letsencrypt/live/openlab.spharx.cn/privkey.pem

# 设置自动续期
sudo systemctl enable certbot.timer
sudo systemctl start certbot.timer
```

#### 方式B: 自签名证书（仅限内网/测试）

```bash
# 创建证书目录
sudo mkdir -p /opt/agentos/ssl
cd /opt/agentos/ssl

# 生成自签名证书（有效期10年）
openssl req -x509 -nodes -days 3650 \
    -newkey rsa:4096 \
    -keyout privkey.pem \
    -out fullchain.pem \
    -subj "/CN=agentos.local/O=SPHARX Ltd./C=CN"

# 设置权限
sudo chmod 600 privkey.pem
sudo chmod 644 fullchain.pem
```

---

### 步骤4：部署AgentOS

```bash
# 创建项目目录
sudo mkdir -p /opt/agentos
cd /opt/agentos

# 克隆代码仓库
sudo git clone https://gitcode.com/spharx/agentos.git .

# 切换到稳定版本标签
sudo git checkout v1.0.0

# 创建数据目录
sudo mkdir -p data/{kernel,daemon,postgres,redis,openlab,prometheus}

# 设置权限
sudo chown -R $(whoami):$(whoami) /opt/agentos

# 复制并编辑环境变量
cp docker/.env.example docker/.env
nano docker/.env
```

**必须修改的生产环境配置项**：

```env
# ==========================================
# ⚠️  必须修改的安全配置项
# ==========================================

# PostgreSQL密码（强密码要求：16位以上）
POSTGRES_PASSWORD=YourSuperSecureP@ssw0rd2026!

# Redis密码
REDIS_PASSWORD=YourRedisS3cretK3y2026!

# LLM API密钥
AGENTOS_LLM_API_KEY=sk-proj-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

# OpenLab会话密钥（用于Cookie加密，32位随机字符串）
OPENLAB_SECRET_KEY=$(openssl rand -base64 32)

# Grafana管理员密码
GRAFANA_ADMIN_PASSWORD=YourGrafanaAdminP@ss2026!

# ==========================================
# 生产环境优化配置
# ==========================================

# 日志级别（生产环境不建议DEBUG）
LOG_LEVEL=INFO

# 内核资源限制
KERNEL_REPLICAS=2  # 至少2个实例实现高可用
MEMORY_LIMIT=8192
CPU_LIMIT=8

# 数据库性能调优
PG_MAX_CONNECTIONS=200
PG_SHARED_BUFFERS=1GB
PG_EFFECTIVE_CACHE_SIZE=4GB

# Redis内存限制
REDIS_MAX_MEMORY=2gb

# Prometheus数据保留
PROMETHEUS_RETENTION=30d

# 数据卷路径（使用SSD）
DATA_VOLUME_PATH=/data/agentos
```

---

### 步骤5：构建并启动服务

```bash
# 构建所有镜像（首次约10-15分钟）
docker compose -f docker/docker-compose.prod.yml build --parallel

# 启动所有服务
docker compose -f docker/docker-compose.prod.yml up -d

# 查看服务状态
docker compose -f docker/docker-compose.prod.yml ps

# 查看日志（确认无错误）
docker compose -f docker/docker-compose.prod.yml logs -f
```

**预期输出**：

```
NAME                      STATUS                    PORTS
agentos-kernel-prod       Up (health: healthy)       0.0.0.0:8080->8080/tcp, 0.0.0.0:9090->9090/tcp
agentos-daemon-prod       Up                        8001-8006/tcp
agentos-postgres-prod     Up (health: healthy)       0.0.0.0:5432->5432/tcp
agentos-redis-prod        Up (health: healthy)       0.0.0.0:6379->6379/tcp
agentos-openlab-prod      Up                        0.0.0.0:443->443/tcp, 0.0.0.0:8000->8000/tcp
agentos-prometheus-prod   Up                        0.0.0.0:9091->9090/tcp
agentos-grafana-prod      Up                        0.0.0.0:3000->3000/tcp
```

---

### 步骤6：验证部署

```bash
# 1. 健康检查
curl http://localhost:8080/health
# 预期: {"status":"ok","version":"1.0.0",...}

# 2. 测试IPC API
curl http://localhost:8080/api/v1/time/now
# 预期: {"success":true,"data":{...}}

# 3. 测试数据库连接
docker exec agentos-postgres-prod psql -U agentos -d agentos -c "SELECT version();"

# 4. 测试Redis连接
docker exec agentos-redis-prod redis-cli -a $REDIS_PASSWORD ping

# 5. 访问OpenLab
# 浏览器打开: https://your-domain.com
# 或本地: https://localhost（需信任自签名证书）

# 6. 访问Grafana监控
# 浏览器打开: http://your-server-ip:3000
# 用户名: admin / 密码: 您设置的密码
```

---

## ☸️ 方式二：Kubernetes 部署

详细Kubernetes部署文档请参考：[Kubernetes部署指南](../operations/kubernetes-deployment.md)

以下是快速部署命令摘要：

```bash
# 添加Helm仓库
helm repo add spharx https://charts.spharx.cn/agentos
helm repo update

# 创建命名空间
kubectl create namespace agentos

# 创建Secret（敏感信息）
kubectl create secret generic agentos-secrets \
    --namespace=agentos \
    --from-literal=postgres-password='your_password' \
    --from-literal=redis-password='your_password' \
    --from-literal=llm-api-key='sk-your-key' \
    --from-literal=openlab-secret-key=$(openssl rand -base64 32)

# 安装AgentOS
helm install agentos spharx/agentos \
    --namespace agentos \
    --values production-values.yaml \
    --set global.image.tag=v1.0.0 \
    --set kernel.replicas=3 \
    --set postgresql.primary.persistence.size=100Gi \
    --set monitoring.enabled=true

# 检查部署状态
kubectl get pods -n agentos -w
```

---

## 🔒 安全加固清单

部署完成后，请逐项检查以下安全配置：

### 网络安全

- [ ] 防火墙仅开放必要端口（80, 443, 22）
- [ ] 内部服务端口（8080, 5432, 6379）不对外暴露
- [ ] 使用VPN或跳板机访问内部服务
- [ ] 启用Fail2Ban防止暴力破解

### 服务安全

- [ ] 所有容器以非root用户运行
- [ ] 容器使用只读文件系统
- [ ] 禁止特权模式（privileged: false）
- [ ] 丢弃所有不必要的Linux capabilities
- [ ] 吟用资源限制（CPU/内存）

### 数据安全

- [ ] 数据库使用强密码（≥16位）
- [ ] Redis启用AUTH认证
- [ ] API密钥不硬编码在镜像中（使用Secrets/K8s Secrets）
- [ ] 启用TLS加密（HTTPS）
- [ ] 定期备份数据

### 审计与监控

- [ ] 启用Cupolas审计日志
- [ ] 配置Prometheus + Grafana监控
- [ ] 设置异常告警（邮件/Webhook）
- [ ] 定期审查访问日志

### 安全配置检查脚本

```bash
#!/bin/bash
# security-check.sh - AgentOS生产环境安全检查

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass_count=0
fail_count=0
warn_count=0

check() {
    if eval "$1" > /dev/null 2>&1; then
        echo -e "${GREEN}✅ PASS${NC}: $2"
        ((pass_count++))
    else
        echo -e "${RED}❌ FAIL${NC}: $2"
        ((fail_count++))
    fi
}

warn() {
    echo -e "${YELLOW}⚠️  WARN${NC}: $1"
    ((warn_count++))
}

echo "=========================================="
echo "  AgentOS Production Security Check v1.0"
echo "=========================================="
echo ""

echo "--- Network Security ---"
check "ss -tlnp | grep -q ':22'" "SSH端口开放"
check "! ss -tlnp | grep -q ':5432'" "PostgreSQL端口未对外开放"
check "! ss -tlnp | grep -q ':6379'" "Redis端口未对外开放"
check "ufw status | grep -q 'Status: active'" "防火墙已启用"

echo ""
echo "--- Container Security ---"
check "docker inspect agentos-kernel-prod --format '{{.Config.User}}' | grep -v root" "内核容器非root用户"
check "docker inspect agentos-kernel-prod --format '{{.HostConfig.ReadonlyRootfs}}' | grep -q true" "内核容器只读文件系统"
check "docker inspect agentos-kernel-prod --format '{{.HostConfig.Privileged}}' | grep -q false" "内核容器未启用特权模式"

echo ""
echo "--- Data Security ---"
grep -q 'POSTGRES_PASSWORD=.*\{16,\}' docker/.env && check "true" "PostgreSQL密码强度足够" || warn "PostgreSQL密码可能不够强"
grep -q 'REDIS_PASSWORD=.*\{12,\}' docker/.env && check "true" "Redis密码强度足够" || warn "Redis密码可能不够强"
grep -qv 'sk-' docker/.env && check "true" "API密钥已配置" || warn "API密钥可能是默认值"

echo ""
echo "--- Monitoring & Auditing ---"
check "docker ps | grep -q prometheus" "Prometheus监控已部署"
check "docker ps | grep -q grafana" "Grafana可视化已部署"
grep -q 'AGENTOS_AUDIT_ENABLED=true' docker/.env && check "true" "审计日志已启用" || warn "审计日志可能未启用"

echo ""
echo "=========================================="
echo -e "Results: ${GREEN}$pass_count passed${NC}, ${RED}$fail_count failed${NC}, ${YELLOW}$warn_count warnings${NC}"
echo "=========================================="

if [ $fail_count -gt 0 ]; then
    exit 1
fi
```

使用方法：

```bash
chmod +x security-check.sh
./security-check.sh
```

---

## 📊 性能基准测试

部署完成后，建议进行性能基线测试：

```bash
# 安装压测工具
pip install locust

# 创建测试脚本
cat > locustfile.py <<'EOF'
from locust import HttpUser, task, between

class AgentOSUser(HttpUser):
    wait_time = between(1, 3)

    @task(3)
    def health_check(self):
        self.client.get("/health")

    @task(2)
    def get_time(self):
        self.client.get("/api/v1/time/now")

    @task(1)
    def create_task(self):
        self.client.post("/api/v1/tasks", json={
            "description": "Performance test task",
            "priority": "normal",
            "timeout_seconds": 30
        })
EOF

# 运行压测（100个并发用户，持续5分钟）
locust -f locustfile.py --host=http://localhost:8080 \
    --users 100 --spawn-rate 10 --run-time 5m \
    --headless --csv perf_results

# 查看结果
cat perf_results_stats.csv
```

**生产环境推荐指标**:

| 指标 | 目标值 | 说明 |
|------|--------|------|
| P50延迟 | <50ms | 中位数响应时间 |
| P99延迟 | <500ms | 99%请求延迟 |
| 错误率 | <0.1% | HTTP错误比例 |
| 吞吐量 | >500 QPS | 每秒请求数 |
| CPU使用率 | <70% | 平均CPU占用 |
| 内存使用率 | <80% | 平均内存占用 |

---

## 🔄 运维操作手册

### 日常运维

```bash
# 查看服务状态
docker compose -f docker/docker-compose.prod.yml ps

# 查看实时日志
docker compose -f docker/docker-compose.prod.yml logs -f --tail=100

# 查看资源使用
docker stats --no-stream

# 清理无用资源（谨慎使用！）
docker system prune -a --volumes
```

### 备份恢复

```bash
# === 备份 ===

# 创建备份目录
BACKUP_DIR="/backup/agentos/$(date +%Y%m%d_%H%M%S)"
mkdir -p $BACKUP_DIR

# 备份数据库
docker exec agentos-postgres-prod pg_dump -U agentos -d agentos | gzip > $BACKUP_DIR/postgres.sql.gz

# 备份Redis（如使用RDB持久化）
docker cp agentos-redis-prod:/data/dump.rdb $BACKUP_DIR/redis-dump.rdb 2>/dev/null || echo "No RDB dump"

# 备份应用数据
tar czf $BACKUP_DIR/app-data.tar.gz ./data/

# 备份配置文件
cp docker/.env $BACKUP_DIR/env.backup

# 备份证书
sudo cp -r /etc/letsencrypt/live/* $BACKUP_DIR/ssl-certs/

# 列出备份内容
du -sh $BACKUP_DIR/*

echo "✅ Backup completed: $BACKUP_DIR"


# === 恢复 ===

# 停止服务
docker compose -f docker/docker-compose.prod.yml down

# 恢复数据库
gunzip -c $BACKUP_DIR/postgres.sql.gz | docker exec -i agentos-postgres-prod psql -U agentos -d agentos

# 恢复Redis
docker cp $BACKUP_DIR/redis-dump.rdb agentos-redis-prod:/data/dump.rdb

# 恢复应用数据
tar xzf $BACKUP_DIR/app-data.tar.gz

# 启动服务
docker compose -f docker/docker-compose.prod.yml up -d

echo "✅ Restore completed"
```

### 日志分析

```bash
# 查看错误日志
docker compose -f docker/docker-compose.prod.yml logs 2>&1 | grep -i error | tail -100

# 统计错误类型
docker compose -f docker/docker-compose.prod.yml logs 2>&1 | grep -oP '"level":"\K[^"]+' | sort | uniq -c | sort -rn

# 查看慢请求（>1秒）
docker compose -f docker/docker-compose.prod.yml logs kernel 2>&1 | grep -oP 'duration_ms":\K\d+' | awk '$1>1000' | wc -l

# 导出日志到文件进行分析
docker compose -f docker/docker-compose.prod.yml logs > agentos-$(date +%Y%m%d).log 2>&1
```

---

## 📚 相关文档

- [**Docker完整文档**](../docker/README.md) — Docker详细配置说明
- [**Kubernetes部署**](../operations/kubernetes-deployment.md) — K8s集群编排
- [**监控运维**](../operations/monitoring.md) — Prometheus+Grafana配置
- [**安全加固**](../operations/security-hardening.md) — 企业级安全配置
- [**故障排查**](../troubleshooting/common-issues.md) — 问题诊断指南

---

> *"部署不是终点，而是服务的起点。"*

**© 2026 SPHARX Ltd. All Rights Reserved.**
