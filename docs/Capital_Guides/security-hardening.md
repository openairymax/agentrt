Copyright (c) 2026 SPHARX. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 瀹夊叏鍔犲浐鎸囧崡

**鐗堟湰**: 1.0.0
**鏈€鍚庢洿鏂?*: 2026-04-06
**鐘舵€?*: 鐢熶骇灏辩华
**鍚堣鏍囧噯**: CIS Benchmark, NIST, SOC2

---

## 馃搵 瀹夊叏鍔犲浐姒傝

AgentOS 閬靛惊**瀹夊叏鍐呯敓鍘熷垯 (E-1)**锛屽疄鐜扮旱娣遍槻寰★細

```
鈹屸攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?          缃戠粶灞傚畨鍏?(Network)                鈹?鈹?  TLS 1.3 路 IP 鐧藉悕鍗?路 DDoS 闃叉姢           鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?          搴旂敤灞傚畨鍏?(Application)            鈹?鈹?  璁よ瘉鎺堟潈 路 杈撳叆鍑€鍖?路 CSRF 闃叉姢            鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?          瀹瑰櫒灞傚畨鍏?(Container)              鈹?鈹?  鏈€灏忔潈闄?路 鍙鏂囦欢绯荤粺 路 鑳藉姏闄愬埗         鈹?鈹溾攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?鈹?          涓绘満灞傚畨鍏?(Host)                   鈹?鈹?  鍐呮牳鍔犲浐 路 鏂囦欢鏉冮檺 路 瀹¤鏃ュ織            鈹?鈹斺攢鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹?```

---

## 馃寪 缃戠粶灞傚姞鍥?
### TLS/SSL 閰嶇疆

```nginx
# nginx/tls.conf - 寮哄寲鐨?TLS 閰嶇疆

server {
    listen 443 ssl http2;
    server_name agentos.spharx.cn;

    # 浠呬娇鐢?TLS 1.3锛堢鐢ㄦ棫鐗堟湰锛?    ssl_protocols TLSv1.3;
    ssl_prefer_server_ciphers off;

    # 寮哄瘑鐮佸浠讹紙TLS 1.3锛?    ssl_ciphers 'TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256';

    # OCSP Stapling
    ssl_stapling on;
    ssl_stapling_verify on;
    ssl_trusted_certificate /etc/ssl/certs/chain.pem;

    # 浼氳瘽缂撳瓨
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 1d;
    ssl_session_tickets off;  # 绂佺敤浼氳瘽绁紙鍓嶅悜淇濆瘑锛?
    # HSTS锛堜弗鏍间紶杈撳畨鍏級
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains; preload" always;

    # 鍏朵粬瀹夊叏澶?    add_header X-Content-Type-Options "nosniff" always;
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;
    add_header Content-Security-Policy "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline';" always;

    # 璇佷功閰嶇疆
    ssl_certificate /etc/ssl/certs/fullchain.pem;
    ssl_certificate_key /etc/ssl/private/privkey.pem;

    # 璇佷功鑷姩缁湡锛圠et's Encrypt 鎴?ACME 鍗忚锛?}
```

### 闃茬伀澧欒鍒?
```bash
#!/bin/bash
# firewall_rules.sh - iptables/UFW 闃茬伀澧欓厤缃?
# 閲嶇疆瑙勫垯
iptables -F
iptables -t nat -F

# 榛樿绛栫暐锛氭嫆缁濇墍鏈夊叆绔欙紝鍏佽鍑虹珯
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# 鍏佽鍥炵幆鎺ュ彛
iptables -A INPUT -i lo -j ACCEPT
iptables -A OUTPUT -o lo -j ACCEPT

# 鍏佽宸插缓绔嬬殑杩炴帴
iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT

# 鍏佽 SSH锛堥檺鍒舵潵婧?IP锛?iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 22 -m state --state NEW -j ACCEPT

# AgentOS 鏈嶅姟绔彛锛堜粎鍏佽鍐呯綉璁块棶锛?iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 8080 -j ACCEPT   # IPC API
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 8001 -j ACCEPT   # LLM Service
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 8002 -j ACCEPT   # Market Service
iptables -A INPUT -p tcp -s 10.0.0.0/8 --dport 8003 -j Accept   # Monitor Service
iptables -A INPUT -p tcp -s 172.16.0.0/12 --dport 5432 -j ACCEPT # PostgreSQL
iptables -A INPUT -p tcp -s 172.16.0.0/12 --dport 6379 -j ACCEPT # Redis

# Prometheus 鐩戞帶绔彛锛堜粎鐩戞帶鏈嶅姟鍣ㄥ彲璁块棶锛?iptables -A INPUT -p tcp -s <MONITOR_IP> --dport 9090 -j ACCEPT
iptables -A INPUT -p tcp -s <MONITOR_IP> --dport 9100 -j ACCEPT

# 鍋ュ悍妫€鏌ョ鐐癸紙鍏佽璐熻浇鍧囪　鍣ㄨ闂級
iptables -A INPUT -p tcp -s <LB_IP> --dport 8080 -m comment --comment "health-check" -j ACCEPT

# 閫熺巼闄愬埗锛堥槻姝㈡毚鍔涚牬瑙ｏ級
iptables -A INPUT -p tcp --dport 22 -m connlimit --connlimit-above 5 -j DROP
iptables -A INPUT -p tcp --dport 8080 -m connlimit --connlimit-above 100 -j DROP

# 鏃ュ織璁板綍琚嫆缁濈殑鍖?iptables -A INPUT -m limit --limit 5/min -j LOG --log-prefix "[DROPPED] "

# 淇濆瓨瑙勫垯
iptables-save > /etc/iptables/rules.v4
```

### DDoS 闃叉姢

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
    block_list_update_interval: 300  # 5鍒嗛挓鏇存柊榛戝悕鍗?    allow_list:
      - "10.0.0.0/8"
      - "172.16.0.0/12"

  challenge_response:
    enabled: true
    threshold: 100  # 瓒呰繃姝ら槇鍊艰Е鍙戦獙璇佺爜
    provider: "cloudflare"  # 鎴?reCAPTCHA/hCaptcha
```

---

## 馃攼 搴旂敤灞傚畨鍏?
### 璁よ瘉涓庢巿鏉冨己鍖?
```python
# security_config.py - 瀹夊叏閰嶇疆绀轰緥

from datetime import timedelta
from cryptography.fernet import Fernet

class SecurityConfig:
    """瀹夊叏閰嶇疆绫?""

    # JWT 閰嶇疆
    JWT_SECRET_KEY = os.environ.get("JWT_SECRET_KEY")  # 蹇呴』 >= 256 bits
    JWT_ALGORITHM = "HS256"
    JWT_ACCESS_TOKEN_EXPIRE = timedelta(minutes=15)
    JWT_REFRESH_TOKEN_EXPIRE = timedelta(days=7)

    # 瀵嗙爜绛栫暐
    PASSWORD_POLICY = {
        "min_length": 12,
        "require_uppercase": True,
        "require_lowercase": True,
        "require_digits": True,
        "require_special_chars": True,
        "max_length": 128,
        "forbidden_patterns": ["password", "123456", "qwerty"],
        "history_count": 24,  # 涓嶈兘浣跨敤鏈€杩?24 娆＄殑瀵嗙爜
        "lockout_threshold": 5,  # 杩炵画澶辫触 5 娆￠攣瀹?        "lockout_duration": 1800  # 閿佸畾 30 鍒嗛挓
    }

    # API Key 绠＄悊
    API_KEY_LENGTH = 64  # 64 瀛楃闅忔満瀵嗛挜
    API_KEY_ROTATION_DAYS = 90  # 姣?90 澶╄疆鎹?    API_KEY_MAX_KEYS_PER_USER = 5

    # 浼氳瘽绠＄悊
    SESSION_TIMEOUT_MINUTES = 30
    MAX_SESSIONS_PER_USER = 10
    CONCURRENT_LOGIN_PREVENTION = True  # 鍗曠偣鐧诲綍

    # 鍔犲瘑閰嶇疆
    ENCRYPTION_KEY = Fernet.generate_key()  # AES-256-GCM
    SENSITIVE_FIELDS_ENCRYPTED = [
        "api_key",
        "database_password",
        "llm_api_key"
    ]

    # CORS 閰嶇疆
    CORS_ALLOWED_ORIGINS = [
        "https://app.spharx.cn",
        "https://admin.spharx.cn"
    ]
    CORS_ALLOW_CREDENTIALS = True
    CORS_MAX_AGE = 3600

    # 瀹夊叏澶?    SECURITY_HEADERS = {
        "Strict-Transport-Security": "max-age=31536000; includeSubDomains",
        "X-Content-Type-Options": "nosniff",
        "X-Frame-Options": "DENY",
        "Content-Security-Policy": "default-src 'self'",
        "Referrer-Policy": "strict-origin-when-cross-origin",
        "Permissions-Policy": "camera=(), microphone=(), geolocation=()"
    }
```

### 杈撳叆鍑€鍖栨渶浣冲疄璺?
```python
# input_sanitizer.py - 寮哄寲鐗堣緭鍏ュ噣鍖?
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
    """鐢熶骇鐜杈撳叆鍑€鍖栧櫒"""

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
        鏍规嵁璇锋眰 schema 鍑€鍖栬緭鍏ユ暟鎹?        """
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

            # 绫诲瀷寮哄埗杞崲鍜岄獙璇?            try:
                sanitized_value = self._type_validate(value, expected_type)
                result.sanitized_data[field_name] = sanitized_value
            except ValueError as e:
                result.warnings.append(f"Field '{field_name}': {str(e)}")
                result.is_safe = False
                result.blocked = True
                return result

            # 鐗瑰畾绫诲瀷鐨勫畨鍏ㄦ鏌?            if expected_type == str:
                check_result = self._sanitize_string(field_name, str(sanitized_value))
                if not check_result.is_safe:
                    result.warnings.extend(check_result.warnings)
                    result.is_safe = False
                    if check_result.blocked:
                        result.blocked = True
                        return result

                    result.sanitized_data[field_name] = check_result.sanitized_value

            # 闀垮害闄愬埗
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
        """瀛楃涓插瓧娈电殑澶氬眰鍑€鍖?""
        result = SanitizationResult(is_safe=True, sanitized_value=value, warnings=[], blocked=False)

        # SQL 娉ㄥ叆妫€娴?        for pattern in self.sql_patterns:
            if re.search(pattern, value):
                result.is_safe = False
                result.warnings.append(f"Potential SQL injection in '{field_name}'")
                result.sanitized_value = re.sub(pattern, "[REMOVED]", value, flags=re.IGNORECASE)
                result.blocked = True
                return result

        # XSS 妫€娴?        decoded = html.unescape(value)
        for pattern in self.xss_patterns:
            if re.search(pattern, decoded, re.IGNORECASE | re.DOTALL):
                result.is_safe = False
                result.warnings.append(f"Potential XSS in '{field_name}'")
                result.sanitized_value = html.escape(value)
                break

        # 鍛戒护娉ㄥ叆妫€娴?        for pattern in self.command_injection_patterns:
            if re.search(pattern, value):
                result.is_safe = False
                result.warnings.append(f"Potential command injection in '{field_name}'")
                result.blocked = True
                return result

        return result
```

---

## 馃惓 瀹瑰櫒灞傚畨鍏?
### Dockerfile 瀹夊叏鏈€浣冲疄璺?
```dockerfile
# Dockerfile.secure - 瀹夊叏鍔犲浐鐗?Dockerfile

# 浣跨敤鏈€灏忓熀纭€闀滃儚
FROM python:3.11-slim AS production

# 鍒涘缓闈?root 鐢ㄦ埛锛堝繀椤诲湪瀹夎浠讳綍涓滆タ涔嬪墠锛?RUN groupadd -r appgroup && useradd -r -g appgroup -d /app -s /sbin/nologin appuser

# 瀹夎鏈€灏忚繍琛屾椂渚濊禆
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libssl3 \
    curl \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean \
    && rm -rf /tmp/* /var/tmp/*

# 澶嶅埗搴旂敤浠ｇ爜
COPY --chown=appuser:appgroup . /app/

WORKDIR /app

# 璁剧疆瀹夊叏鐨勭洰褰曟潈闄?RUN chmod 755 /app && \
    chmod 700 /app/config && \
    chmod 600 /app/config/*.key 2>/dev/null || true

# 鍒囨崲鍒伴潪 root 鐢ㄦ埛
USER appuser

# 鍙毚闇插繀瑕佺鍙?EXPOSE 8000

# 鍋ュ悍妫€鏌?HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8000/health || exit 1

# 瀹夊叏鐜鍙橀噺
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=off \
    PIP_DISABLE_PIP_VERSION_CHECK=on

# 浣跨敤 exec 褰㈠紡锛圥ID 1 姝ｇ‘鎺ユ敹淇″彿锛?CMD ["gunicorn", "--bind", "0.0.0.0:8000", "--workers", "4", "--threads", "2", \
     "--access-logfile", "-", "--error-logfile", "-", \
     "--timeout", "120", "--graceful-timeout", "30", \
     "main:app"]
```

### Docker Compose 瀹夊叏閰嶇疆

```yaml
# docker-compose.security.yml
version: '3.8'

services:
  kernel:
    build:
      context: ..
      dockerfile: docker/Dockerfile.kernel
      target: runtime

    # 瀹夊叏閫夐」
    security_opt:
      - no-new-privileges:true          # 绂佹鎻愭潈
      - seccomp:seccomp-profile.json    # seccomp 绯荤粺璋冪敤杩囨护

    # 绉婚櫎鎵€鏈夎兘鍔涳紝浠呮坊鍔犲繀瑕佺殑
    cap_drop:
      - ALL
    cap_add:
      - NET_BIND_SERVICE               # 浠呭厑璁哥粦瀹氱鍙?
    # 鍙鏂囦欢绯荤粺锛堥厤鍚?tmpfs锛?    read_only: true
    tmpfs:
      - /tmp:size=512M,mode=1777,uid=1000,gid=1000,noexec,nosuid,nodev
      - /run:size=64M,mode=1755,uid=1000,gid=1000,noexec,nosuid,nodev

    # 璧勬簮闄愬埗
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 4G
          pids: 500                     # 鏈€澶ц繘绋嬫暟
        reservations:
          cpus: '2'
          memory: 2G

    # 鐢ㄦ埛鍛藉悕绌洪棿闅旂锛堝鏋滃唴鏍告敮鎸侊級
    userns_mode: "host"

    # PID 鍛藉悕绌洪棿鍏变韩锛堝厑璁告煡鐪嬪鍣ㄨ繘绋嬶級
    pid: "host"

    # 绂佹娣诲姞棰濆鐗规潈
    privileged: false

    # 璁惧鐧藉悕鍗曪紙榛樿涓嶅厑璁镐换浣曡澶囷級
    devices: []

    # 缃戠粶妯″紡
    networks:
      - agentos-internal              # 鍐呴儴缃戠粶锛屼笉鏆撮湶鍒板閮?
    # 鏍囩鐢ㄤ簬瀹¤
    labels:
      security.scan: "true"
      compliance.cis: "level-1"
      owner: "security-team"
```

### 瀹瑰櫒闀滃儚鎵弿

```bash
#!/bin/bash
# container_scan.sh - 瀹瑰櫒闀滃儚瀹夊叏鎵弿鑴氭湰

IMAGE_NAME=$1

if [ -z "$IMAGE_NAME" ]; then
    echo "Usage: $0 <image_name>"
    exit 1
fi

echo "=== Scanning image: $IMAGE_NAME ==="

# 1. Trivy 鎵弿锛堝紑婧愭紡娲炴壂鎻忓櫒锛?echo ""
echo "[1/4] Running Trivy vulnerability scan..."
trivy image --severity HIGH,CRITICAL --exit-code 1 "$IMAGE_NAME"

# 2. Docker Scout 鎵弿锛堝畼鏂瑰伐鍏凤級
echo ""
echo "[2/4] Running Docker Scout CVE scan..."
docker scout cves "$IMAGE_NAME"

# 3. 妫€鏌ラ暅鍍忛厤缃畨鍏ㄦ€?echo ""
echo "[3/4] Checking image configuration..."
docker inspect --format='{{json .Config.User}}' "$IMAGE_NAME" | grep -q root && echo "鈿狅笍  WARNING: Container runs as root!" || echo "鉁?OK: Non-root user"
docker inspect --format='{{json .Config.ReadOnly}}' "$IMAGE_NAME" | grep -q true && echo "鉁?OK: Read-only filesystem" || echo "鈿狅笍  WARNING: Writable filesystem"

# 4. 妫€鏌ユ晱鎰熶俊鎭硠闇?echo ""
echo "[4/4] Checking for secrets exposure..."
docker history --no-trunc "$IMAGE_NAME" | grep -iE "(password|secret|key|token)" && echo "鈿狅笍  WARNING: Potential secrets found in layers!" || echo "鉁?OK: No obvious secrets"

echo ""
echo "=== Scan completed ==="
```

---

## 馃枼锔?涓绘満灞傚畨鍏?
### 鎿嶄綔绯荤粺鍔犲浐

```bash
#!/bin/bash
# host_hardening.sh - Linux 涓绘満鍔犲浐鑴氭湰

set -e

echo "=== Starting Host Hardening ==="

# 1. 鏇存柊绯荤粺骞跺畨瑁呭畨鍏ㄥ伐鍏?apt-get update && apt-get upgrade -y
apt-get install -y fail2ban auditd aide rkhunter clamav

# 2. 閰嶇疆 AIDE锛堟枃浠跺畬鏁存€ф娴嬶級
aide --init
mv /var/lib/aide/aide.db.new /var/lib/aide/aide.db
(crontab -l 2>/dev/null; echo "0 3 * * * /usr/bin/aide --check | mail -s 'AIDE Report' admin@spharx.cn") | crontab -

# 3. 閰嶇疆 Fail2Ban锛堥槻鏆村姏鐮磋В锛?cat > /etc/fail2ban/jail.local << 'EOF'
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

# 4. 閰嶇疆瀹¤瀛愮郴缁燂紙auditd锛?cat > /etc/audit/rules.d/agentos.rules << 'EOF'
## 鐩戞帶鍏抽敭鏂囦欢鍙樻洿
-w /etc/passwd -p wa -k identity
-w /etc/group -p wa -k identity
-w /etc/shadow -p wa -k identity
-w /etc/sudoers -p wa -k privilege
-w /etc/ssh/sshd_config -p wa -k ssh_config

## 鐩戞帶鍏抽敭鐩綍
-a always,exit -F dir=/app/data -F perm=wa -k agentos_data
-a always,exit -F dir=/app/config -F perm=wa -k agentos_config

## 鐩戞帶绯荤粺璋冪敤
-a always,exit -F arch=b64 -S mount -k mounts
-a always,exit -F arch=b64 -S unlink -rmdir -S rename -S link -S symlink -k delete

## 鐩戞帶缃戠粶鐩稿叧鎿嶄綔
-a always,exit -F arch=b64 -S bind -S connect -k network
EOF
systemctl restart auditd

# 5. 鍐呮牳鍙傛暟鍔犲浐
cat > /etc/sysctl.d/99-security-hardening.conf << 'EOF'
# 绂佺敤 IPv6锛堝鏋滀笉浣跨敤锛?net.ipv6.conf.all.disable_ipv6 = 1

# 鍚敤 SYN Cookie 闃插尽 SYN Flood
net.ipv4.tcp_syncookies = 1

# 绂佺敤 ICMP 閲嶅畾鍚?net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.default.accept_redirects = 0

# 鍚敤婧愯矾鐢变繚鎶?net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.default.rp_filter = 1

# 绂佺敤婧愯矾鐢辨暟鎹寘
net.ipv4.conf.all.accept_source_route = 0
net.ipv4.conf.default.accept_source_route = 0

# 绂佺敤 ICMP 閲嶅畾鍚戝彂閫?net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.default.send_redirects = 0

# 鍐呮牳鍦板潃绌洪棿甯冨眬闅忔満鍖?(ASLR)
kernel.randomize_va_space = 2

# 绂佺敤鏍稿績杞偍锛堥槻姝俊鎭硠闇诧級
fs.suid_dumpable = 0

# 闄愬埗 dmesg 璁块棶锛堜粎 root 鍙锛?kernel.dmesg_restrict = 1

# 闄愬埗 ptrace 鑼冨洿锛堜粎鍏佽璋冭瘯瀛愯繘绋嬶級
kernel.yama.ptrace_scope = 1

# 闄愬埗绗﹀彿閾炬帴璺熼殢
fs.protected_symlinks_create = 1
fs.protected_regular = 2

# 绂佺敤妯″潡鍔犺浇锛堝彲閫夛紝鏍规嵁闇€姹傦級
# kernel.modules_disabled = 1
EOF
sysctl -p /etc/sysctl.d/99-security-hardening.conf

# 6. 鏂囦欢鏉冮檺鍔犲浐
chmod 700 /root
chmod 600 /etc/shadow
chmod 600 /etc/gshadow
chmod 644 /etc/passwd
chmod 644 /etc/group

# 7. SSH 鍔犲浐
sed -i 's/#PermitRootLogin.*/PermitRootLogin no/' /etc/ssh/sshd_config
sed -i 's/#PasswordAuthentication.*/PasswordAuthentication no/' /etc/ssh/sshd_config
sed -i 's/#PubkeyAuthentication.*/PubkeyAuthentication yes/' /etc/ssh/sshd_config
sed -i 's/#MaxAuthTries.*/MaxAuthTries 3/' /etc/ssh/sshd_config
sed -i 's/#AllowTcpForwarding.*/AllowTcpForwarding no/' /etc/ssh/sshd_config
sed -i 's/#X11Forwarding.*/X11Forwarding no/' /etc/ssh/sshd_config
systemctl restart sshd

# 8. 鑷姩瀹夊叏鏇存柊
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

### 鏂囦欢绯荤粺鍔犲瘑

```bash
# 浣跨敤 LUKS 鍔犲瘑鏁忔劅鏁版嵁鍒嗗尯锛堝彲閫夛級

# 鍒涘缓鍔犲瘑鍗?cryptsetup luksFormat --type luks2 /dev/sdb1
cryptsetup open /dev/sdb1 encrypted_data

# 鏍煎紡鍖栧苟鎸傝浇
mkfs.ext4 /dev/mapper/encrypted_data
mkdir -p /encrypted-data
mount /dev/mapper/encrypted_data /encrypted-data

# 閰嶇疆鑷姩瑙ｉ攣锛堜娇鐢?TPM2 鎴栧瘑閽ユ枃浠讹級
# 娉ㄦ剰锛氱敓浜х幆澧冨缓璁娇鐢ㄧ‖浠跺畨鍏ㄦā鍧?(HSM) 鎴栦簯 KMS
```

---

## 馃攳 瀹夊叏瀹¤涓庡悎瑙?
### CIS Benchmark 妫€鏌ユ竻鍗?
```bash
#!/bin/bash
# cis_benchmark_check.sh - CIS Docker Benchmark 鑷姩鍖栨鏌?
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

# ... 鏇村妫€鏌ラ」 ...

# Section 2: Docker Daemon Configuration
echo ""
echo "--- Section 2: Docker Daemon Configuration ---"

# 2.1 Restrict network traffic between containers
docker network ls --format '{{.Name}}' | while read net; do
    docker network inspect "$net" | grep -q '"com.docker.network.bridge.enable_ip_masquerade": false' && check_result 0 "2.1 Network isolation ($net)" || check_result 1 "2.1 Network isolation ($net)"
done

# 2.2 Set logging level
docker info | grep -q '"Logging Driver": "json-file"' && check_result 0 "2.2 Logging driver configured" || check_result 1 "2.2 Logging driver not configured"

# ... 鏇村妫€鏌ラ」 ...

# Section 3: Container Images and Build Files
echo ""
echo "--- Section 3: Container Images ---"

# 3.1 Use trusted base images
docker images --format '{{.Repository}}:{{.Tag}}' | grep -vE "(ubuntu|debian|alpine|python|node)" | head -1 && check_result 1 "3.1 Untrusted base image detected" || check_result 0 "3.1 All base images are from trusted sources"

# 3.2 Do not install unnecessary packages
# 锛堥渶瑕佽嚜瀹氫箟閫昏緫锛?
# Summary
echo ""
echo "=========================================="
echo "  CIS Benchmark Results"
echo "=========================================="
echo "Total Checks: $TOTAL_CHECKS"
echo "Passed: $PASSED_CHECKS 鉁?
echo "Failed: $FAILED_CHECKS 鉂?
echo "Pass Rate: $(( PASSED_CHECKS * 100 / TOTAL_CHECKS ))%"
echo ""

if [ $FAILED_CHECKS -gt 0 ]; then
    exit 1
fi
```

---

## 馃摎 鐩稿叧鏂囨。

- **[瀹夊叏绌归《璁捐](../architecture/cupolas.md)** 鈥?Cupolas 鍥涘眰闃叉姢鏋舵瀯
- **[瀹夊叏璁捐鎸囧崡](../../agentos/docs/specifications/coding_standard/Security_design_guide.md)** 鈥?瀹夊叏缂栫爜瑙勮寖
- **[澶囦唤鎭㈠](backup-recovery.md)** 鈥?鏁版嵁鍔犲瘑澶囦唤
- **[鏁呴殰鎺掓煡](../troubleshooting/common-issues.md)** 鈥?瀹夊叏浜嬩欢鍝嶅簲

---

## 馃敆 澶栭儴璧勬簮

- [OWASP Top 10](https://owasp.org/www-project-top-ten/)
- [CIS Docker Benchmark](https://www.cisecurity.org/benchmark/docker)
- [NIST Cybersecurity Framework](https://www.nist.gov/cyberframework)
- [Docker Security Best Practices](https://docs.docker.com/engine/security/)

---

**漏 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
