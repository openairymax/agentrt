Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 安全加固指南

**版本**: 1.0.0
**最后更新**: 2026-04-06
**状态**: 生产就绪
**合规标准**: CIS Benchmark, NIST, SOC2

---

## 📋 安全加固概览

AgentOS 遵循**安全内生原则 (E-1)**，实现纵深防御：

```
┌─────────────────────────────────────────────┐
│           网络层安全 (Network)                │
│   TLS 1.3 · IP 白名单 · DDoS 防护           │
├─────────────────────────────────────────────┤
│           应用层安全 (Application)            │
│   认证授权 · 输入净化 · CSRF 防护            │
├─────────────────────────────────────────────┤
│           容器层安全 (Container)              │
│   最小权限 · 只读文件系统 · 能力限制         │
├─────────────────────────────────────────────┤
│           主机层安全 (Host)                   │
│   内核加固 · 文件权限 · 审计日志            │
└─────────────────────────────────────────────┘
```

---

## 🌐 网络层加固

### TLS/SSL 配置

```nginx
# nginx/tls.conf - 强化的 TLS 配置

server {
    listen 443 ssl http2;
    server_name agentos.spharx.cn;

    # 仅使用 TLS 1.3（禁用旧版本）
    ssl_protocols TLSv1.3;
    ssl_prefer_server_ciphers off;

    # 强密码套件（TLS 1.3）
    ssl_ciphers 'TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256';

    # OCSP Stapling
    ssl_stapling on;
    ssl_stapling_verify on;
    ssl_trusted_certificate /etc/ssl/certs/chain.pem;

    # 会话缓存
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 1d;
    ssl_session_tickets off;  # 禁用会话票（前向保密）

    # HSTS（严格传输安全）
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains; preload" always;

    # 其他安全头
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;
    add_header Content-Security-Policy "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline';" always;

    # 证书配置
    ssl_certificate /etc/ssl/certs/fullchain.pem;
    ssl_certificate_key /etc/ssl/private/privkey.pem;

    # 证书自动续期（Let's Encrypt 或 ACME 协议）
}
```

### 防火墙规则

```bash
#!/bin/bash
# firewall_rules.sh - iptables/UFW 防火墙配置

# 重置规则
iptables -F
iptables -t nat -F

# 默认策略：拒绝所有入站，允许出站
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# 允许回环接口
iptables -A INPUT -i lo -j ACCEPT
iptables -A OUTPUT -o lo -j ACCEPT

# 允许已建立的连接
iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT

# 允许 SSH（限制来源 IP）
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 22 -m state --state NEW -j ACCEPT

# AgentOS 服务端口（仅允许内网访问）
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 8080 -j ACCEPT   # IPC API
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 8001 -j ACCEPT   # LLM Service
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 8002 -j ACCEPT   # Market Service
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 8003 -j Accept   # Monitor Service
iptables -A INPUT -p tcp -s 172.16.0.0/12 --dport 5432 -j ACCEPT # PostgreSQL
iptables -A INPUT -p tcp -s 172.16.0.0/12 --dport 6379 -j ACCEPT # Redis

# Prometheus 监控端口（仅监控服务器可访问）
iptables -A INPUT -p tcp -s <MONITOR_IP> --dport 9090 -j ACCEPT
iptables -A INPUT -p tcp -s <MONITOR_IP> --dport 9100 -j ACCEPT

# 健康检查端点（允许负载均衡器访问）
iptables -A INPUT -p tcp -s <LB_IP> --dport 8080 -m comment --comment "health-check" -j ACCEPT

# 速率限制（防止暴力破解）
iptables -A INPUT -p tcp --dport 22 -m connlimit --connlimit-above 5 -j DROP
iptables -A INPUT -p tcp --dport 8080 -m connlimit --connlimit-above 100 -j DROP

# 日志记录被拒绝的包
iptables -A INPUT -m limit --limit 5/min -j LOG --log-prefix "[DROPPED] "

# 保存规则
iptables-save > /etc/iptables/rules.v4
```

### DDoS 防护

```yaml
# ddos_protection.yaml
ddos_protection:
  enabled: true

  rate_limiting:
    global:
      requests_per_second: 10000
      burst_size: 20000
    per_ip:
      requests_per_second: 100
      burst_size: 200

    per_endpoint:
      /api/v1/chat:
        requests_per_second: 10
        burst_size: 20
      /api/v1/memory/store:
        requests_per_second: 50
        burst_size: 100

  connection_limiting:
    max_connections_per_ip: 50
    max_connections_total: 10000

  ip_reputation:
    enabled: true
    block_list_update_interval: 300  # 5分钟更新黑名单
    allow_list:
      - "10.0.0.0/8"
      - "172.16.0.0/12"

  challenge_response:
    enabled: true
    threshold: 100  # 超过此阈值触发验证码
    provider: "cloudflare"  # 或 reCAPTCHA/hCaptcha
```

---

## 🔐 应用层安全

### 认证与授权强化

```python
# security_config.py - 安全配置示例

from datetime import timedelta
from cryptography.fernet import Fernet

class SecurityConfig:
    """安全配置类"""

    # JWT 配置
    JWT_SECRET_KEY = os.environ.get("JWT_SECRET_KEY")  # 必须 >= 256 bits
    JWT_ALGORITHM = "HS256"
    JWT_ACCESS_TOKEN_EXPIRE = timedelta(minutes=15)
    JWT_REFRESH_TOKEN_EXPIRE = timedelta(days=7)

    # 密码策略
    PASSWORD_POLICY = {
        "min_length": 12,
        "require_uppercase": True,
        "require_lowercase": True,
        "require_digits": True,
        "require_special_chars": True,
        "max_length": 128,
        "forbidden_patterns": ["password", "123456", "qwerty"],
        "history_count": 24,  # 不能使用最近 24 次的密码
        "lockout_threshold": 5,  # 连续失败 5 次锁定
        "lockout_duration": 1800  # 锁定 30 分钟
    }

    # API Key 管理
    API_KEY_LENGTH = 64  # 64 字符随机密钥
    API_KEY_ROTATION_DAYS = 90  # 每 90 天轮换
    API_KEY_MAX_KEYS_PER_USER = 5

    # 会话管理
    SESSION_TIMEOUT_MINUTES = 30
    MAX_SESSIONS_PER_USER = 10
    CONCURRENT_LOGIN_PREVENTION = True  # 单点登录

    # 加密配置
    ENCRYPTION_KEY = Fernet.generate_key()  # AES-256-GCM
    SENSITIVE_FIELDS_ENCRYPTED = [
        "api_key",
        "database_password",
        "llm_api_key"
    ]

    # CORS 配置
    CORS_ALLOWED_ORIGINS = [
        "https://app.spharx.cn",
        "https://admin.spharx.cn"
    ]
    CORS_ALLOW_CREDENTIALS = True
    CORS_MAX_AGE = 3600

    # 安全头
    SECURITY_HEADERS = {
        "Strict-Transport-Security": "max-age=31536000; includeSubDomains",
        "X-Content-Type-Options": "nosniff",
        "X-Frame-Options": "DENY",
        "Content-Security-Policy": "default-src 'self'",
        "Referrer-Policy": "strict-origin-when-cross-origin",
        "Permissions-Policy": "camera=(), microphone=(), geolocation=()"
    }
```

### 输入净化最佳实践

```python
# input_sanitizer.py - 强化版输入净化

import re
import html
from typing import Any, Dict, Optional
from dataclasses import dataclass

@dataclass
class SanitizationResult:
    is_safe: bool
    sanitized_data: Any
    warnings: list[str]
    blocked: bool

class ProductionSanitizer:
    """生产环境输入净化器"""

    def __init__(self):
        self.sql_patterns = [
            r"(?i)(\b(SELECT|INSERT|UPDATE|DELETE|DROP|UNION|ALTER)\b)",
            r"(--|#|\/\*)",
            r"(\bor\b\s+\d+\s*=\s*\d+)",
            r"(;\s*(SELECT|INSERT|UPDATE|DELETE))",
            r"(CHAR\(|CONCAT\(|CONCAT_WS\()",
            r"(information_schema|sys\.tables|mysql\.)",
        ]

        self.xss_patterns = [
            r"<script[^>]*>.*?</script>",
            r"javascript:",
            r"on(error|load|click|mouseover|focus|blur)\s*=",
            r"<iframe[^>]*>",
            r"<object[^>]*>",
            r"<embed[^>]*>",
            r"<svg[^>]* onload=",
            r"data:text/html",
        ]

        self.command_injection_patterns = [
            r"[;&|`$]",
            r"\$\(",
            r"`[^`]+`",
            r"\.\./",
            r"/etc/",
            r"/proc/",
        ]

    def sanitize_request(
        self,
        data: Dict[str, Any],
        schema: Dict[str, type]
    ) -> SanitizationResult:
        """
        根据请求 schema 净化输入数据
        """
        result = SanitizationResult(
            is_safe=True,
            sanitized_data={},
            warnings=[],
            blocked=False
        )

        for field_name, expected_type in schema.items():
            if field_name not in data:
                continue

            value = data[field_name]

            # 类型强制转换和验证
            try:
                sanitized_value = self._type_validate(value, expected_type)
                result.sanitized_data[field_name] = sanitized_value
            except ValueError as e:
                result.warnings.append(f"Field '{field_name}': {str(e)}")
                result.is_safe = False
                result.blocked = True
                return result

            # 特定类型的安全检查
            if expected_type == str:
                check_result = self._sanitize_string(field_name, str(sanitized_value))
                if not check_result.is_safe:
                    result.warnings.extend(check_result.warnings)
                    result.is_safe = False
                    if check_result.blocked:
                        result.blocked = True
                        return result

                    result.sanitized_data[field_name] = check_result.sanitized_value

            # 长度限制
            max_lengths = {
                "name": 256,
                "description": 4096,
                "query": 2048,
                "code": 1048576,  # 1MB for code execution
            }

            if field_name in max_lengths:
                if len(str(sanitized_value)) > max_lengths[field_name]:
                    result.blocked = True
                    result.warnings.append(f"Field '{field_name}' exceeds max length")
                    return result

        return result

    def _sanitize_string(self, field_name: str, value: str) -> SanitizationResult:
        """字符串字段的多层净化"""
        result = SanitizationResult(is_safe=True, sanitized_value=value, warnings=[], blocked=False)

        # SQL 注入检测
        for pattern in self.sql_patterns:
            if re.search(pattern, value):
                result.is_safe = False
                result.warnings.append(f"Potential SQL injection in '{field_name}'")
                result.sanitized_value = re.sub(pattern, "[REMOVED]", value, flags=re.IGNORECASE)
                result.blocked = True
                return result

        # XSS 检测
        decoded = html.unescape(value)
        for pattern in self.xss_patterns:
            if re.search(pattern, decoded, re.IGNORECASE | re.DOTALL):
                result.is_safe = False
                result.warnings.append(f"Potential XSS in '{field_name}'")
                result.sanitized_value = html.escape(value)
                break

        # 命令注入检测
        for pattern in self.command_injection_patterns:
            if re.search(pattern, value):
                result.is_safe = False
                result.warnings.append(f"Potential command injection in '{field_name}'")
                result.blocked = True
                return result

        return result
```

---

## 🐳 容器层安全

### Dockerfile 安全最佳实践

```dockerfile
# Dockerfile.secure - 安全加固版 Dockerfile

# 使用最小基础镜像
FROM python:3.11-slim AS production

# 创建非 root 用户（必须在安装任何东西之前）
RUN groupadd -r appgroup && useradd -r -g appgroup -d /app -s /sbin/nologin appuser

# 安装最小运行时依赖
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    curl \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean \
    && rm -rf /tmp/* /var/tmp/*

# 复制应用代码
COPY --chown=appuser:appgroup . /app/

WORKDIR /app

# 设置安全的目录权限
RUN chmod 755 /app && \
    chmod 700 /app/config && \
    chmod 600 /app/config/*.key 2>/dev/null || true

# 切换到非 root 用户
USER appuser

# 只暴露必要端口
EXPOSE 8000

# 健康检查
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8000/health || exit 1

# 安全环境变量
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=off \
    PIP_DISABLE_PIP_VERSION_CHECK=on

# 使用 exec 形式（PID 1 正确接收信号）
CMD ["gunicorn", "--bind", "0.0.0.0:8000", "--workers", "4", "--threads", "2", \
     "--access-logfile", "-", "--error-logfile", "-", \
     "--timeout", "120", "--graceful-timeout", "30", \
     "main:app"]
```

### Docker Compose 安全配置

```yaml
# docker-compose.security.yml
version: '3.8'

services:
  kernel:
    build:
      context: ..
      dockerfile: docker/Dockerfile.kernel
      target: runtime

    # 安全选项
    security_opt:
      - no-new-privileges:true          # 禁止提权
      - seccomp:seccomp-profile.json    # seccomp 系统调用过滤

    # 移除所有能力，仅添加必要的
    cap_drop:
      - ALL
    cap_add:
      - NET_BIND_SERVICE               # 仅允许绑定端口

    # 只读文件系统（配合 tmpfs）
    read_only: true
    tmpfs:
      - /tmp:size=512M,mode=1777,uid=1000,gid=1000,noexec,nosuid,nodev
      - /run:size=64M,mode=1755,uid=1000,gid=1000,noexec,nosuid,nodev

    # 资源限制
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 4G
          pids: 500                     # 最大进程数
        reservations:
          cpus: '2'
          memory: 2G

    # 用户命名空间隔离（如果内核支持）
    userns_mode: "host"

    # PID 命名空间共享（允许查看容器进程）
    pid: "host"

    # 禁止添加额外特权
    privileged: false

    # 设备白名单（默认不允许任何设备）
    devices: []

    # 网络模式
    networks:
      - agentos-internal              # 内部网络，不暴露到外部

    # 标签用于审计
    labels:
      security.scan: "true"
      compliance.cis: "level-1"
      owner: "security-team"
```

### 容器镜像扫描

```bash
#!/bin/bash
# container_scan.sh - 容器镜像安全扫描脚本

IMAGE_NAME=$1

if [ -z "$IMAGE_NAME" ]; then
    echo "Usage: $0 <image_name>"
    exit 1
fi

echo "=== Scanning image: $IMAGE_NAME ==="

# 1. Trivy 扫描（开源漏洞扫描器）
echo ""
echo "[1/4] Running Trivy vulnerability scan..."
trivy image --severity HIGH,CRITICAL --exit-code 1 "$IMAGE_NAME"

# 2. Docker Scout 扫描（官方工具）
echo ""
echo "[2/4] Running Docker Scout CVE scan..."
docker scout cves "$IMAGE_NAME"

# 3. 检查镜像配置安全性
echo ""
echo "[3/4] Checking image configuration..."
docker inspect --format='{{json .Config.User}}' "$IMAGE_NAME" | grep -q root && echo "⚠️  WARNING: Container runs as root!" || echo "✅ OK: Non-root user"
docker inspect --format='{{json .Config.ReadOnly}}' "$IMAGE_NAME" | grep -q true && echo "✅ OK: Read-only filesystem" || echo "⚠️  WARNING: Writable filesystem"

# 4. 检查敏感信息泄露
echo ""
echo "[4/4] Checking for secrets exposure..."
docker history --no-trunc "$IMAGE_NAME" | grep -iE "(password|secret|key|token)" && echo "⚠️  WARNING: Potential secrets found in layers!" || echo "✅ OK: No obvious secrets"

echo ""
echo "=== Scan completed ==="
```

---

## 🖥️ 主机层安全

### 操作系统加固

```bash
#!/bin/bash
# host_hardening.sh - Linux 主机加固脚本

set -e

echo "=== Starting Host Hardening ==="

# 1. 更新系统并安装安全工具
apt-get update && apt-get upgrade -y
apt-get install -y fail2ban auditd aide rkhunter clamav

# 2. 配置 AIDE（文件完整性检测）
aide --init
mv /var/lib/aide/aide.db.new /var/lib/aide/aide.db
(crontab -l 2>/dev/null; echo "0 3 * * * /usr/bin/aide --check | mail -s 'AIDE Report' admin@spharx.cn") | crontab -

# 3. 配置 Fail2Ban（防暴力破解）
cat > /etc/fail2ban/jail.local << 'EOF'
[DEFAULT]
bantime = 3600
findtime = 600
maxretry = 5
banaction = iptables-multiport

[ssh]
enabled = true
port = ssh
logpath = /var/log/auth.log
maxretry = 3

[agentos-api]
enabled = true
port = 8080,8001,8002,8003
logpath = /var/log/nginx/access.log
filter = agentos-auth
maxretry = 10
bantime = 7200
EOF
systemctl enable fail2ban && systemctl start fail2ban

# 4. 配置审计子系统（auditd）
cat > /etc/audit/rules.d/agentos.rules << 'EOF'
## 监控关键文件变更
-w /etc/passwd -p wa -k identity
-w /etc/group -p wa -k identity
-w /etc/shadow -p wa -k identity
-w /etc/sudoers -p wa -k privilege
-w /etc/ssh/sshd_config -p wa -k ssh_config

## 监控关键目录
-a always,exit -F dir=/app/data -F perm=wa -k agentos_data
-a always,exit -F dir=/app/config -F perm=wa -k agentos_config

## 监控系统调用
-a always,exit -F arch=b64 -S mount -k mounts
-a always,exit -F arch=b64 -S unlink -rmdir -S rename -S link -S symlink -k delete

## 监控网络相关操作
-a always,exit -F arch=b64 -S bind -S connect -k network
EOF
systemctl restart auditd

# 5. 内核参数加固
cat > /etc/sysctl.d/99-security-hardening.conf << 'EOF'
# 禁用 IPv6（如果不使用）
net.ipv6.conf.all.disable_ipv6 = 1

# 启用 SYN Cookie 防御 SYN Flood
net.ipv4.tcp_syncookies = 1

# 禁用 ICMP 重定向
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0

# 启用源路由保护
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1

# 禁用源路由数据包
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0

# 禁用 ICMP 重定向发送
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0

# 内核地址空间布局随机化 (ASLR)
kernel.randomize_va_space = 2

# 禁用核心转储（防止信息泄露）
fs.suid_dumpable = 0

# 限制 dmesg 访问（仅 root 可读）
kernel.dmesg_restrict = 1

# 限制 ptrace 范围（仅允许调试子进程）
kernel.yama.ptrace_scope = 1

# 限制符号链接跟随
fs.protected_symlinks_create = 1
fs.protected_regular = 2

# 禁用模块加载（可选，根据需求）
# kernel.modules_disabled = 1
EOF
sysctl -p /etc/sysctl.d/99-security-hardening.conf

# 6. 文件权限加固
chmod 700 /root
chmod 600 /etc/shadow
chmod 600 /etc/gshadow
chmod 644 /etc/passwd
chmod 644 /etc/group

# 7. SSH 加固
sed -i 's/#PermitRootLogin.*/PermitRootLogin no/' /etc/ssh/sshd_config
sed -i 's/#PasswordAuthentication.*/PasswordAuthentication no/' /etc/ssh/sshd_config
sed -i 's/#PubkeyAuthentication.*/PubkeyAuthentication yes/' /etc/ssh/sshd_config
sed -i 's/#MaxAuthTries.*/MaxAuthTries 3/' /etc/ssh/sshd_config
sed -i 's/#AllowTcpForwarding.*/AllowTcpForwarding no/' /etc/ssh/sshd_config
sed -i 's/#X11Forwarding.*/X11Forwarding no/' /etc/ssh/sshd_config
systemctl restart sshd

# 8. 自动安全更新
apt-get install -y unattended-upgrades
cat > /etc/apt/apt.conf.d/50unattended-upgrades << 'EOF'
Unattended-Upgrade::Allowed-Origins {
    "${distro_id}:${distro_codename}-security";
};
Unattended-Upgrade::AutoFixInterruptedDpkg "true";
Unattended-Upgrade::Remove-Unused-Dependencies "true";
Unattended-Upgrade::Automatic-Reboot "true";
Unattended-Upgrade::Automatic-Reboot-Time "02:00";
EOF
systemctl enable unattended-upgrades

echo "=== Host Hardening Completed ==="
echo "Please reboot to apply all changes."
```

### 文件系统加密

```bash
# 使用 LUKS 加密敏感数据分区（可选）

# 创建加密卷
cryptsetup luksFormat --type luks2 /dev/sdb1
cryptsetup open /dev/sdb1 encrypted_data

# 格式化并挂载
mkfs.ext4 /dev/mapper/encrypted_data
mkdir -p /encrypted-data
mount /dev/mapper/encrypted_data /encrypted-data

# 配置自动解锁（使用 TPM2 或密钥文件）
# 注意：生产环境建议使用硬件安全模块 (HSM) 或云 KMS
```

---

## 🔍 安全审计与合规

### CIS Benchmark 检查清单

```bash
#!/bin/bash
# cis_benchmark_check.sh - CIS Docker Benchmark 自动化检查

echo "=== CIS Docker Benchmark v1.6.0 ==="

TOTAL_CHECKS=0
PASSED_CHECKS=0
FAILED_CHECKS=0

check_result() {
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    if [ $1 -eq 0 ]; then
        PASSED_CHECKS=$((PASSED_CHECKS + 1))
        echo "[PASS] $2"
    else
        FAILED_CHECKS=$((FAILED_CHECKS + 1))
        echo "[FAIL] $2"
    fi
}

# Section 1: Host Configuration
echo ""
echo "--- Section 1: Host Configuration ---"

# 1.1 Create separate partition for containers
df -Th | grep -q "/var/lib/docker" && check_result 0 "1.1 Separate container partition" || check_result 1 "1.1 Separate container partition"

# 1.2 Harden the host kernel
sysctl net.ipv4.ip_forward | grep -q "= 1" && check_result 0 "1.2 IP forwarding configured" || check_result 1 "1.2 IP forwarding not configured"

# ... 更多检查项 ...

# Section 2: Docker Daemon Configuration
echo ""
echo "--- Section 2: Docker Daemon Configuration ---"

# 2.1 Restrict network traffic between containers
docker network ls --format '{{.Name}}' | while read net; do
    docker network inspect "$net" | grep -q '"com.docker.network.bridge.enable_ip_masquerade": false' && check_result 0 "2.1 Network isolation ($net)" || check_result 1 "2.1 Network isolation ($net)"
done

# 2.2 Set logging level
docker info | grep -q '"Logging Driver": "json-file"' && check_result 0 "2.2 Logging driver configured" || check_result 1 "2.2 Logging driver not configured"

# ... 更多检查项 ...

# Section 3: Container Images and Build Files
echo ""
echo "--- Section 3: Container Images ---"

# 3.1 Use trusted base images
docker images --format '{{.Repository}}:{{.Tag}}' | grep -vE "(ubuntu|debian|alpine|python|node)" | head -1 && check_result 1 "3.1 Untrusted base image detected" || check_result 0 "3.1 All base images are from trusted sources"

# 3.2 Do not install unnecessary packages
# （需要自定义逻辑）

# Summary
echo ""
echo "=========================================="
echo "  CIS Benchmark Results"
echo "=========================================="
echo "Total Checks: $TOTAL_CHECKS"
echo "Passed: $PASSED_CHECKS ✅"
echo "Failed: $FAILED_CHECKS ❌"
echo "Pass Rate: $(( PASSED_CHECKS * 100 / TOTAL_CHECKS ))%"
echo ""

if [ $FAILED_CHECKS -gt 0 ]; then
    exit 1
fi
```

---

## 📚 相关文档

- **[安全穹顶设计](../architecture/cupolas.md)** — Cupolas 四层防护架构
- **[安全设计指南](../../agentos/manuals/specifications/coding_standard/Security_design_guide.md)** — 安全编码规范
- **[备份恢复](backup-recovery.md)** — 数据加密备份
- **[故障排查](../troubleshooting/common-issues.md)** — 安全事件响应

---

## 🔗 外部资源

- [OWASP Top 10](https://owasp.org/www-project-top-ten/)
- [CIS Docker Benchmark](https://www.cisecurity.org/benchmark/docker)
- [NIST Cybersecurity Framework](https://www.nist.gov/cyberframework)
- [Docker Security Best Practices](https://docs.docker.com/engine/security/)

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
