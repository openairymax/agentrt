Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
"From data intelligence emerges."

# AgentOS 备份与恢复指南

**版本**: Doc V1.8
**最后更新**: 2026-04-09
**状态**: 生产就绪

---

## 📋 概述

本文档提供 AgentOS 系统的数据备份与灾难恢复方案，确保业务连续性。

### 备份策略矩阵

| 数据类型 | 备份频率 | 保留期 | RTO | RPO |
|----------|---------|--------|-----|-----|
| **L1 原始记忆** | 实时 (WAL) | 10年 | < 1小时 | < 5分钟 |
| **L2 向量索引** | 每日 | 30天 | < 4小时 | < 24小时 |
| **L3 关系图谱** | 每周 | 90天 | < 8小时 | < 7天 |
| **L4 模式库** | 每月 | 1年 | < 24小时 | < 30天 |
| **配置文件** | 变更时触发 | 永久 | < 15分钟 | 即时 |
| **PostgreSQL** | 每6小时 | 30天 | < 2小时 | < 6小时 |
| **Redis** | 每15分钟（RDB+AOF） | 7天 | < 1小时 | < 15分钟 |

---

## 💾 备份方案

### 1. L1 原始卷备份

L1 采用**仅追加写入**模式，适合增量备份：

```bash
#!/bin/bash
# backup_l1.sh - L1 记忆数据备份脚本

set -e

SOURCE_DIR="/app/data/memory/l1"
BACKUP_DIR="/backup/agentos/l1"
DATE=$(date +%Y%m%d_%H%M%S)
RETENTION_DAYS=3650  # 10年保留

echo "[$(date)] Starting L1 backup..."

# 创建按日期分层的备份结构
BACKUP_PATH="$BACKUP_DIR/$DATE"
mkdir -p "$BACKUP_PATH"

# 使用 rsync 增量备份（硬链接减少存储）
rsync -a --link-dest="$BACKUP_DIR/latest" \
      "$SOURCE_DIR/" "$BACKUP_PATH/"

# 更新 latest 符号链接
rm -f "$BACKUP_DIR/latest"
ln -s "$DATE" "$BACKUP_DIR/latest"

# 创建校验和清单
find "$BACKUP_PATH" -type f -exec sha256sum {} \; > "$BACKUP_PATH/checksums.sha256"

# 验证备份完整性
cd "$BACKUP_PATH"
sha256sum -c checksums.sha256 || { echo "ERROR: Checksum verification failed!"; exit 1; }

# 清理过期备份
find "$BACKUP_DIR" -maxdepth 1 -type d -mtime +$RETENTION_DAYS ! -name "latest" -exec rm -rf {} \;

# 统计信息
TOTAL_SIZE=$(du -sh "$BACKUP_PATH" | cut -f1)
FILE_COUNT=$(find "$BACKUP_PATH" -type f | wc -l)

echo "[$(date)] L1 backup completed:"
echo "  - Path: $BACKUP_PATH"
echo "  - Size: $TOTAL_SIZE"
echo "  - Files: $FILE_COUNT"

# 上传到远程存储（可选）
if [ -n "$REMOTE_BACKUP_S3_BUCKET" ]; then
    aws s3 sync "$BACKUP_PATH/" "s3://$REMOTE_BACKUP_S3_BUCKET/l1/$DATE/"
    echo "[$(date)] Uploaded to S3: s3://$REMOTE_BACKUP_S3_BUCKET/l1/$DATE/"
fi
```

#### 定时任务配置

```bash
# crontab -e
# 每日凌晨 2 点执行全量备份，每小时执行增量同步
0 2 * * * /opt/agentos/scripts/backup_l1.sh >> /var/log/agentos/backup.log 2>&1
0 * * * * /opt/agentos/scripts/backup_l1_incremental.sh >> /var/log/agentos/backup.log 2>&1
```

### 2. PostgreSQL 数据库备份

```bash
#!/bin/bash
# backup_postgres.sh - PostgreSQL 备份脚本

set -e

DB_HOST="localhost"
DB_PORT="5432"
DB_NAME="agentos"
DB_USER="agentos"
BACKUP_DIR="/backup/agentos/postgres"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p "$BACKUP_DIR"

echo "[$(date)] Starting PostgreSQL backup..."

# 方法1: pg_dump 逻辑备份（推荐用于小数据库）
pg_dump -h $DB_HOST -p $DB_PORT -U $DB_USER -d $DB_NAME \
    --format=custom \
        --compress=9 \
        --file="$BACKUP_DIR/agentos_$DATE.dump"

# 方法2: pg_basebackup 物理备份（推荐用于大数据库）
# pg_basebackup -h $DB_HOST -p $DB_PORT -U $DB_USER \
#     -D "$BACKUP_DIR/base_$DATE" -Ft -z -P -Xs

# 验证备份文件
pg_restore --list "$BACKUP_DIR/agentos_$DATE.dump" > /dev/null && echo "Backup valid"

# 加密备份（可选）
gpg --symmetric --cipher-algo AES256 --batch --passphrase "$ENCRYPTION_KEY" \
    --output "$BACKUP_DIR/agentos_$DATE.dump.gpg" \
    "$BACKUP_DIR/agentos_$DATE.dump"

# 清理未加密文件
rm "$BACKUP_DIR/agentos_$DATE.dump"

# 保留最近 30 天备份
find "$BACKUP_DIR" -name "*.dump.gpg" -mtime +30 -delete

echo "[$(date)] PostgreSQL backup completed: agentos_$DATE.dump.gpg"
```

### 3. Redis 缓存备份

```bash
#!/bin/bash
# backup_redis.sh - Redis 备份脚本

REDIS_HOST="localhost"
REDIS_PORT="6379"
REDIS_PASSWORD="${REDIS_PASSWORD}"
BACKUP_DIR="/backup/agentos/redis"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p "$BACKUP_DIR"

echo "[$(date)] Starting Redis backup..."

# 触发 BGSAVE（后台保存）
redis-cli -h $REDIS_HOST -p $REDIS_PORT -a "$REDIS_PASSWORD" BGSAVE

# 等待 BGSAVE 完成
while [ $(redis-cli -h $REDIS_HOST -p $REDIS_PORT -a "$REDIS_PASSWORD" LASTSAVE) -lt $(date +%s) ]; do
    sleep 1
done

# 获取 RDB 文件路径
RDB_PATH=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT -a "$REDIS_PASSWORD" CONFIG GET dir | tail -n1)
RDB_FILE=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT -a "$REDIS_PASSWORD" CONFIG GET dbfilename | tail -n1)

# 复制 RDB 文件
cp "$RDB_PATH/$RDB_FILE" "$BACKUP_DIR/dump_$DATE.rdb"

# 同时备份 AOF 文件（如果启用）
AOF_ENABLED=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT -a "$REDIS_PASSWORD" CONFIG GET appendonly | tail -n1)
if [ "$AOF_ENABLED" = "yes" ]; then
    AOF_FILE=$(redis-cli -h $REDIS_HOST -p $REDIS_PORT -a "$REDIS_PASSWORD" CONFIG GET appendfilename | tail -n1)
    cp "$RDB_PATH/$AOF_FILE" "$BACKUP_DIR/appendonly_$DATE.aof"
fi

# 压缩备份
gzip "$BACKUP_DIR/dump_$DATE.rdb"
[ -f "$BACKUP_DIR/appendonly_$DATE.aof" ] && gzip "$BACKUP_DIR/appendonly_$DATE.aof"

# 保留最近 7 天
find "$BACKUP_DIR" -name "*.rdb.gz" -mtime +7 -delete
find "$BACKUP_DIR" -name "*.aof.gz" -mtime +7 -delete

echo "[$(date)] Redis backup completed"
```

### 4. FAISS 索引备份

```python
#!/usr/bin/env python3
# backup_faiss.py - FAISS 向量索引备份

import faiss
import os
import shutil
from datetime import datetime
import hashlib

def backup_faiss_index(index_path: str, backup_dir: str):
    """备份 FAISS 索引"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_path = os.path.join(backup_dir, f"faiss_{timestamp}")

    print(f"[{datetime.now()}] Loading FAISS index from {index_path}")
    index = faiss.read_index(index_path)

    print(f"[{datetime.now()}] Index stats: {index.ntotal} vectors")

    # 保存到备份目录
    os.makedirs(backup_path, exist_ok=True)
    backup_file = os.path.join(backup_path, "faiss_index.index")
    faiss.write_index(index, backup_file)

    # 生成元数据
    metadata = {
        'original_path': index_path,
        'vector_count': index.ntotal,
        'dimension': index.d,
        'backup_time': timestamp.isoformat(),
        'checksum': file_checksum(backup_file),
        'size_bytes': os.path.getsize(backup_file)
    }

    import json
    with open(os.path.join(backup_path, 'metadata.json'), 'w') as f:
        json.dump(metadata, f, indent=2)

    # 验证备份完整性
    verify_index = faiss.read_index(backup_file)
    assert verify_index.ntotal == index.ntotal, "Vector count mismatch!"

    print(f"[{datetime.now()}] Backup verified and saved to {backup_path}")
    return backup_path

def file_checksum(filepath: str) -> str:
    """计算文件 SHA256 校验和"""
    sha256 = hashlib.sha256()
    with open(filepath, 'rb') as f:
        for chunk in iter(lambda: f.read(8192), b''):
            sha256.update(chunk)
    return sha256.hexdigest()

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print("Usage: python backup_faiss.py <index_path> <backup_dir>")
        sys.exit(1)

    backup_faiss_index(sys.argv[1], sys.argv[2])
```

### 5. 配置文件备份

```bash
#!/bin/bash
# backup_configs.sh - 配置文件备份脚本

CONFIG_DIRS=(
    "/app/config"
    "/etc/agentos"
    "/opt/agentos/conf"
)

BACKUP_DIR="/backup/agentos/configs"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p "$BACKUP_DIR"

echo "[$(date)] Backing up configuration files..."

for config_dir in "${CONFIG_DIRS[@]}"; do
    if [ -d "$config_dir" ]; then
        dir_name=$(basename "$config_dir")
        tar czf "$BACKUP_DIR/${dir_name}_$DATE.tar.gz" -C "$(dirname "$config_dir")" "$(basename "$config_dir")"
        echo "  - Backed up: $config_dir"
    fi done

# Git 版本控制配置变更（推荐）
cd /app/config
git add -A
git commit -m "Config backup: $DATE" || true

echo "[$(date)] Configuration backup completed"
```

---

## 🔄 恢复流程

### 场景 1: 单点故障恢复 (RTO < 1小时)

```bash
#!/bin/bash
# restore_single_service.sh - 单服务恢复脚本

SERVICE_NAME=$1  # kernel | postgres | redis | openlab

case $SERVICE_NAME in
    kernel)
        echo "Restoring Kernel service..."
        docker compose -f docker/docker-compose.yml stop kernel
        # 恢复内核数据卷
        tar xzf /backup/latest/kernel-data.tar.gz -C /var/lib/docker/volumes/
        docker compose -f docker/docker-compose.yml start kernel
        ;;

    postgres)
        echo "Restoring PostgreSQL..."
        BACKUP_FILE=$(ls -t /backup/agentos/postgres/*.dump.gpg | head -1)
        gpg --decrypt --batch --passphrase "$ENCRYPTION_KEY" "$BACKUP_FILE" | \
            pg_restore -h localhost -U agentos -d agentos --clean --if-exists
        ;;

    redis)
        echo "Restoring Redis..."
        BACKUP_FILE=$(ls -t /backup/agentos/redis/*.rdb.gz | head -1)
        gunzip -c "$BACKUP_FILE" > /var/lib/redis/dump.rdb
        systemctl restart redis-server
        ;;

    *)
        echo "Unknown service: $SERVICE_NAME"
        exit 1
        ;;
esac

echo "[$(date)] Service $SERVICE_NAME restored successfully"
```

### 场景 2: 完整系统恢复 (RTO < 4小时)

```bash
#!/bin/bash
# disaster_recovery.sh - 灾难恢复主脚本

set -e

BACKUP_DATE=$1  # 格式: YYYYMMDD_HHMMSS

if [ -z "$BACKUP_DATE" ]; then
    echo "Usage: $0 <YYYYMMDD_HHMMSS>"
    echo "Available backups:"
    ls -la /backup/agentos/l1/ | grep "^d"
    exit 1
fi

echo "=========================================="
echo "  AgentOS Disaster Recovery Procedure"
echo "  Backup Date: $BACKUP_DATE"
echo "=========================================="

# Step 1: 停止所有服务
echo "[Step 1/7] Stopping all services..."
docker compose -f docker/docker-compose.prod.yml down

# Step 2: 恢复 PostgreSQL
echo "[Step 2/7] Restoring PostgreSQL database..."
docker run -d --name postgres-restore \
    -v /backup/agentos/postgres:/backup \
    -e POSTGRES_DB=agentos -e POSTGRES_USER=agentos \
    postgres:15-alpine &
sleep 10
# 执行恢复命令...

# Step 3: 恢复 Redis
echo "[Step 3/7] Restoring Redis cache..."
cp /backup/agentos/redis/dump_${BACKUP_DATE}.rdb.gz /tmp/
gunzip /tmp/dump_${BACKUP_DATE}.rdb.gz

# Step 4: 恢复 L1 记忆数据
echo "[Step 4/7] Restoring L1 memory data..."
rm -rf /data/kernel/memory/l1/*
rsync -av /backup/agentos/l1/${BACKUP_DATE}/ /data/kernel/memory/l1/

# Step 5: 恢复 FAISS 索引
echo "[Step 5/7] Restoring L2 FAISS index..."
cp /backup/agentos/faiss/faiss_${BACKUP_DATE}/faiss_index.index /data/kernel/memory/l2/

# Step 6: 恢复配置文件
echo "[Step 6/7] Restoring configuration files..."
tar xzf /backup/agentos/configs/config_${BACKUP_DATE}.tar.gz -C /

# Step 7: 启动所有服务并验证
echo "[Step 7/7] Starting all services..."
docker compose -f docker/docker-compose.prod.yml up -d

# 等待服务健康检查通过
sleep 60

# 自动化健康验证
echo ""
echo "=== Health Check Results ==="
curl -sf http://localhost:8080/health | jq .
curl -sf http://localhost:8001/api/v1/health | jq .

echo ""
echo "=========================================="
echo "  Disaster Recovery Completed!"
echo "=========================================="
```

### 场景 3: 时间点恢复 (Point-in-Time Recovery)

```sql
-- PostgreSQL PITR 示例
-- 1. 停止 PostgreSQL
-- 2. 从基础备份恢复
pg_restore -U agentos -D /var/lib/postgresql/data base_backup.dump

-- 3. 配置 recovery.conf
echo "restore_command = 'cp /wal_archive/%f %p'" >> /var/lib/postgresql/data/recovery.conf
echo "recovery_target_time = '2026-04-06 14:30:00'" >> /var/lib/postgresql/data/recovery.conf
echo "recovery_target_action = 'promote'" >> /var/lib/postgresql/data/recovery.conf

-- 4. 启动 PostgreSQL（进入恢复模式）
pg_ctl start -D /var/lib/postgresql/data

-- 5. 监控恢复进度
SELECT * FROM pg_stat_recovery;
```

---

## 🧪 备份验证

### 自动化验证脚本

```python
#!/usr/bin/env python3
# verify_backups.py - 备份完整性验证

import os
import json
import hashlib
from datetime import datetime, timedelta

def verify_backup_integrity(backup_dir: str):
    """验证备份完整性和可恢复性"""

    results = {
        'timestamp': datetime.now().isoformat(),
        'checks': [],
        'overall_status': 'PASS'
    }

    # 检查 1: L1 备份存在性
    l1_latest = os.path.join(backup_dir, 'l1', 'latest')
    if os.path.islink(l1_latest):
        actual_path = os.readlink(l1_latest)
        results['checks'].append({
            'name': 'L1 Backup Exists',
            'status': 'PASS',
            'detail': f'Latest: {actual_path}'
        })
    else:
        results['checks'].append({'name': 'L1 Backup Exists', 'status': 'FAIL'})
        results['overall_status'] = 'FAIL'

    # 检查 2: 校验和验证
    checksum_file = os.path.join(l1_latest, 'checksums.sha256')
    if os.path.exists(checksum_file):
        os.chdir(l1_latest)
        ret = os.system('sha256sum -c checksums.sha256 > /dev/null 2>&1')
        status = 'PASS' if ret == 0 else 'FAIL'
        results['checks'].append({'name': 'L1 Checksum Verification', 'status': status})
        if status == 'FAIL':
            results['overall_status'] = 'FAIL'

    # 检查 3: PostgreSQL 备份
    pg_backup_dir = os.path.join(backup_dir, 'postgres')
    pg_backups = sorted([f for f in os.listdir(pg_backup_dir) if f.endswith('.dump.gpg')])
    if len(pg_backups) >= 1:
        latest_pg = pg_backups[-1]
        age = datetime.now() - datetime.fromtimestamp(
            os.path.getmtime(os.path.join(pg_backup_dir, latest_pg))
        )
        results['checks'].append({
            'name': 'PostgreSQL Backup',
            'status': 'PASS' if age < timedelta(hours=24) else 'WARNING',
            'detail': f'Latest: {latest_pg}, Age: {age}'
        })
    else:
        results['checks'].append({'name': 'PostgreSQL Backup', 'status': 'FAIL'})
        results['overall_status'] = 'FAIL'

    # 检查 4: Redis 备份
    redis_backup_dir = os.path.join(backup_dir, 'redis')
    redis_backups = sorted([f for f in os.listdir(redis_backup_dir) if f.endswith('.rdb.gz')])
    if len(redis_backups) >= 1:
        latest_redis = redis_backups[-1]
        age = datetime.now() - datetime.fromtimestamp(
            os.path.getmtime(os.path.join(redis_backup_dir, latest_redis))
        )
        results['checks'].append({
            'name': 'Redis Backup',
            'status': 'PASS' if age < timedelta(hours=1) else 'WARNING',
            'detail': f'Latest: {latest_redis}, Age: {age}'
        })

    # 输出结果
    print(json.dumps(results, indent=2))

    # 发送告警（如果失败）
    if results['overall_status'] != 'PASS':
        send_alert(f"Backup verification FAILED: {results}")

    return results

def send_alert(message: str):
    """发送告警通知"""
    import requests
    # Slack webhook / Email / PagerDuty
    requests.post(os.environ.get('ALERT_WEBHOOK_URL'), json={'text': message})

if __name__ == "__main__":
    backup_dir = os.environ.get("BACKUP_DIR", "/backup/agentos")
    verify_backup_integrity(backup_dir)
```

---

## 📊 备份监控指标

### Prometheus 告警规则

```yaml
groups:
  - name: backup_alerts
    rules:
      - alert: BackupFailed
        expr: agentos_backup_success{job="agentos-backup"} == 0
        for: 1h
        labels:
          severity: critical
        annotations:
          summary: "AgentOS backup failed"
          description: "Last backup of {{ $labels.backup_type }} failed at {{ $value }}"

      - alert: BackupStale
        expr: time() - agentos_backup_timestamp_seconds > 86400
        for: 1h
        labels:
          severity: warning
        annotations:
          summary: "Backup is stale (>24 hours old)"
          description: "{{ $labels.backup_type }} backup has not been updated in over 24 hours"

      - alert: BackupSizeAnomaly
        expr |
          abs(agentos_backup_size_bytes -
              avg_over_time(agentos_backup_size_bytes[7d])) / avg_over_time(agentos_backup_size_bytes[7d]) > 0.5
        labels:
          severity: warning
        annotations:
          summary: "Backup size anomaly detected"
          description: "Backup size changed by more than 50% compared to 7-day average"
```

---

## 📚 相关文档

- **[部署指南](../guides/deployment.md)** — 部署架构说明
- **[性能调优](performance-tuning.md)** — I/O 性能优化
- **[安全加固](security-hardening.md)** — 备份加密与访问控制
- **[故障排查](../troubleshooting/common-issues.md)** — 恢复失败诊断

---

**© 2026 SPHARX Ltd. All Rights Reserved.**

*"From data intelligence emerges."*
