#!/usr/bin/env bash
# Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
# db-migrate.sh — AgentRT heapstore 数据库迁移管理工具
# P3.20: 生产数据库迁移策略
#
# 功能:
#   - Schema 版本化管理（V001, V002, ...）
#   - 前向兼容（forward-only）迁移
#   - 后向兼容（rollback）迁移（需显式 --force 确认）
#   - 迁移状态追踪表 `_migrations`
#   - 支持 --dry-run 预览
#
# 用法:
#   bash scripts/ci/pipeline/db-migrate.sh [COMMAND] [OPTIONS]
#
# 命令:
#   status    查看当前迁移状态
#   migrate   执行待处理的迁移
#   rollback  回滚最近一次迁移（需 --force）
#   new       创建新的迁移文件模板

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
MIGRATIONS_DIR="${PROJECT_ROOT}/agentos/heapstore/migrations"
DB_HOST="${AGENTOS_POSTGRES_HOST:-localhost}"
DB_PORT="${AGENTOS_POSTGRES_PORT:-5432}"
DB_NAME="${AGENTOS_POSTGRES_DB:-agentrt}"
DB_USER="${AGENTOS_POSTGRES_USER:-agentrt}"
DB_PASSWORD="${AGENTOS_POSTGRES_PASSWORD:-}"

# 命令行参数
COMMAND="${1:-status}"
DRY_RUN=false
FORCE=false

shift || true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run)  DRY_RUN=true; shift ;;
        --force)    FORCE=true; shift ;;
        --host)     DB_HOST="$2"; shift 2 ;;
        --port)     DB_PORT="$2"; shift 2 ;;
        --db)       DB_NAME="$2"; shift 2 ;;
        --user)     DB_USER="$2"; shift 2 ;;
        --password) DB_PASSWORD="$2"; shift 2 ;;
        *)          echo "Unknown option: $1"; exit 1 ;;
    esac
done

# PostgreSQL 连接命令
PG_CMD=(
    psql
    -h "${DB_HOST}"
    -p "${DB_PORT}"
    -d "${DB_NAME}"
    -U "${DB_USER}"
    -v ON_ERROR_STOP=1
    --no-psqlrc
    -q
)

if [[ -n "${DB_PASSWORD}" ]]; then
    export PGPASSWORD="${DB_PASSWORD}"
fi

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'

log_ok()    { echo -e "${GREEN}[MIGRATE]${NC} $*"; }
log_info()  { echo -e "[MIGRATE] $*"; }
log_warn()  { echo -e "${YELLOW}[MIGRATE]${NC} $*"; }
log_error() { echo -e "${RED}[MIGRATE]${NC} $*" >&2; }

###############################################################################
# 迁移追踪表初始化
###############################################################################
init_migration_table() {
    log_info "Initializing migration tracking table..."
    "${PG_CMD[@]}" <<'SQL'
CREATE TABLE IF NOT EXISTS _migrations (
    id          SERIAL PRIMARY KEY,
    version     VARCHAR(16) NOT NULL UNIQUE,
    description TEXT NOT NULL,
    checksum    VARCHAR(64) NOT NULL,
    applied_at  TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    applied_by  TEXT NOT NULL DEFAULT CURRENT_USER,
    duration_ms INTEGER,
    success     BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE INDEX IF NOT EXISTS idx_migrations_version ON _migrations(version);
CREATE INDEX IF NOT EXISTS idx_migrations_applied_at ON _migrations(applied_at);
SQL
    log_ok "Migration table ready."
}

###############################################################################
# 状态查看
###############################################################################
do_status() {
    log_info "AgentRT Heapstore Migration Status"
    echo ""

    # 检查连接
    if ! "${PG_CMD[@]}" -c "SELECT 1" >/dev/null 2>&1; then
        log_error "Cannot connect to PostgreSQL at ${DB_HOST}:${DB_PORT}/${DB_NAME}"
        log_info "Set AGENTOS_POSTGRES_HOST/PORT/DB/USER/PASSWORD env vars"
        exit 1
    fi

    # 初始化迁移表
    init_migration_table

    # 已应用的迁移
    echo "Applied migrations:"
    "${PG_CMD[@]}" -c "
        SELECT version, description, applied_at,
               CASE WHEN success THEN 'OK' ELSE 'FAIL' END as status
        FROM _migrations ORDER BY id;
    " 2>/dev/null || echo "  (none)"

    echo ""

    # 待处理的迁移
    local pending=()
    local applied versions
    applied=$("${PG_CMD[@]}" -t -c "SELECT version FROM _migrations WHERE success = TRUE ORDER BY version;" 2>/dev/null | tr -d ' ' || true)

    if [[ -d "${MIGRATIONS_DIR}" ]]; then
        for migration in "${MIGRATIONS_DIR}"/V*.sql; do
            [[ -f "$migration" ]] || continue
            local ver
            ver=$(basename "$migration" .sql | grep -oP '^V\d+')
            if [[ -n "$ver" ]]; then
                if ! echo "$applied" | grep -q "$ver"; then
                    local desc
                    desc=$(head -3 "$migration" | grep "^--" | tail -1 | sed 's/^-- *//')
                    pending+=("${ver}: ${desc}")
                fi
            fi
        done
    fi

    if [[ ${#pending[@]} -gt 0 ]]; then
        echo "Pending migrations:"
        for p in "${pending[@]}"; do
            echo "  - $p"
        done
    else
        echo "No pending migrations."
    fi
}

###############################################################################
# 执行迁移
###############################################################################
do_migrate() {
    log_info "Running pending migrations..."

    if ! "${PG_CMD[@]}" -c "SELECT 1" >/dev/null 2>&1; then
        log_error "Cannot connect to PostgreSQL"
        exit 1
    fi

    init_migration_table

    local applied
    applied=$("${PG_CMD[@]}" -t -c "SELECT version FROM _migrations WHERE success = TRUE ORDER BY version;" 2>/dev/null | tr -d ' ' || true)

    local migrated=0
    local failed=0

    if [[ ! -d "${MIGRATIONS_DIR}" ]]; then
        log_warn "No migrations directory: ${MIGRATIONS_DIR}"
        exit 1
    fi

    for migration in $(ls "${MIGRATIONS_DIR}"/V*.sql 2>/dev/null | sort); do
        local ver
        ver=$(basename "$migration" .sql | grep -oP '^V\d+')
        local description
        description=$(head -3 "$migration" | grep "^-- Description:" | sed 's/^-- Description: *//' || echo "No description")

        if echo "$applied" | grep -q "$ver"; then
            continue  # 已应用，跳过
        fi

        local checksum
        checksum=$(sha256sum "$migration" | awk '{print $1}')

        log_info "Applying ${ver}: ${description}..."

        if [[ "${DRY_RUN}" == "true" ]]; then
            log_info "  [DRY-RUN] Would execute: $(basename "$migration")"
            continue
        fi

        local start_ms
        start_ms=$(date +%s%3N)

        if "${PG_CMD[@]}" -f "$migration" 2>/tmp/migrate_err.$$; then
            local end_ms
            end_ms=$(date +%s%3N)
            local duration=$((end_ms - start_ms))

            # 记录成功
            "${PG_CMD[@]}" -c "
                INSERT INTO _migrations (version, description, checksum, duration_ms, success)
                VALUES ('${ver}', '${description}', '${checksum}', ${duration}, TRUE)
                ON CONFLICT (version) DO UPDATE SET success = TRUE, duration_ms = ${duration};
            " >/dev/null 2>&1

            log_ok "  ${ver} applied (${duration}ms)"
            ((migrated++)) || true
        else
            # 记录失败
            "${PG_CMD[@]}" -c "
                INSERT INTO _migrations (version, description, checksum, success)
                VALUES ('${ver}', '${description}', '${checksum}', FALSE)
                ON CONFLICT (version) DO UPDATE SET success = FALSE;
            " >/dev/null 2>&1

            log_error "  ${ver} FAILED"
            cat /tmp/migrate_err.$$ 2>/dev/null || true
            ((failed++)) || true
            break  # 不继续执行后续迁移
        fi

        rm -f /tmp/migrate_err.$$
    done

    echo ""
    log_info "Migration complete: ${migrated} applied, ${failed} failed"

    if [[ $failed -gt 0 ]]; then
        exit 1
    fi
}

###############################################################################
# 回滚
###############################################################################
do_rollback() {
    if [[ "${FORCE}" != "true" ]]; then
        log_error "Rollback requires --force flag for safety."
        log_info "Usage: $0 rollback --force"
        exit 1
    fi

    log_warn "Rolling back most recent migration..."

    if ! "${PG_CMD[@]}" -c "SELECT 1" >/dev/null 2>&1; then
        log_error "Cannot connect to PostgreSQL"
        exit 1
    fi

    local last_version
    last_version=$("${PG_CMD[@]}" -t -c "
        SELECT version FROM _migrations WHERE success = TRUE
        ORDER BY id DESC LIMIT 1;
    " 2>/dev/null | tr -d ' ' || true)

    if [[ -z "$last_version" ]]; then
        log_warn "No migrations to rollback."
        exit 0
    fi

    local rollback_file="${MIGRATIONS_DIR}/${last_version}_rollback.sql"

    if [[ -f "$rollback_file" ]]; then
        log_info "Executing rollback: ${rollback_file}"
        if "${PG_CMD[@]}" -f "$rollback_file" 2>/dev/null; then
            "${PG_CMD[@]}" -c "
                UPDATE _migrations SET success = FALSE WHERE version = '${last_version}';
            " >/dev/null 2>&1
            log_ok "Rolled back ${last_version}."
        else
            log_error "Rollback failed for ${last_version}."
            exit 1
        fi
    else
        log_warn "No rollback script found for ${last_version}."
        log_info "Marking as not-applied anyway..."
        "${PG_CMD[@]}" -c "
            UPDATE _migrations SET success = FALSE WHERE version = '${last_version}';
        " >/dev/null 2>&1
    fi
}

###############################################################################
# 创建新迁移文件
###############################################################################
do_new() {
    local next_version="V001"

    if [[ -d "${MIGRATIONS_DIR}" ]]; then
        local last
        last=$(ls "${MIGRATIONS_DIR}"/V*.sql 2>/dev/null | sort | tail -1 | grep -oP 'V\d+' || echo "V000")
        local num
        num=$(echo "${last}" | grep -oP '\d+')
        num=$((10#$num + 1))
        next_version=$(printf "V%03d" "$num")
    else
        mkdir -p "${MIGRATIONS_DIR}"
    fi

    local migration_file="${MIGRATIONS_DIR}/${next_version}_initial_schema.sql"

    cat > "$migration_file" << 'TMPEOF'
-- Migration: V001
-- Description: Initial schema
-- Created: $(date -u +"%Y-%m-%dT%H:%M:%SZ")
-- Forward-only: true
--
-- Guidelines:
--   1. Always use IF NOT EXISTS / IF EXISTS for idempotency
--   2. Use transactional DDL where possible
--   3. Avoid long-running locks (VACUUM FULL, etc.)
--   4. Test on a staging DB before production

BEGIN;

-- TODO: Add your schema changes here

COMMIT;
TMPEOF

    # 替换日期和版本号
    local now
    now=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    sed -i "s/V001/${next_version}/g" "$migration_file"
    sed -i "s/\$(date -u +\"%Y-%m-%dT%H:%M:%SZ\")/${now}/" "$migration_file"

    log_ok "Created migration: ${migration_file}"
    log_info "Edit this file to add your schema changes."
}

###############################################################################
# 入口
###############################################################################
case "${COMMAND}" in
    status)   do_status ;;
    migrate)  do_migrate ;;
    rollback) do_rollback ;;
    new)      do_new ;;
    -h|--help)
        echo "AgentRT DB Migration Tool"
        echo ""
        echo "Commands:"
        echo "  status       Show migration status"
        echo "  migrate      Apply pending migrations"
        echo "  rollback     Rollback last migration (requires --force)"
        echo "  new          Create a new migration file"
        echo ""
        echo "Options:"
        echo "  --dry-run    Preview without executing"
        echo "  --force      Required for rollback"
        echo "  --host HOST  PostgreSQL host (env: AGENTOS_POSTGRES_HOST)"
        echo "  --port PORT  PostgreSQL port (env: AGENTOS_POSTGRES_PORT)"
        exit 0
        ;;
    *)
        log_error "Unknown command: ${COMMAND}"
        log_info "Available: status, migrate, rollback, new"
        exit 1
        ;;
esac