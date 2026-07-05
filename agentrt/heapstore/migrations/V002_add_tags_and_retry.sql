-- Migration: V002
-- Description: Add agent tags, retry tracking, and config history
-- Created: 2026-06-17T00:00:00Z
-- Forward-only: true
--
-- Forward-compatible changes (non-destructive):
--   1. Add tags column to agents table (JSONB, default '[]')
--   2. Add retry_count to turns table (INTEGER, default 0)
--   3. Add config_history table for config versioning
--   4. Add session metadata index for faster queries
--
-- Rollback: V002_rollback.sql (drops new columns, does NOT lose core data)

BEGIN;

-- ===========================================================================
-- 1. Add tags to agents (non-destructive: new column with default)
-- ===========================================================================
ALTER TABLE agents
    ADD COLUMN IF NOT EXISTS tags JSONB DEFAULT '[]';

COMMENT ON COLUMN agents.tags IS 'Flexible agent tags for categorization and filtering';

-- ===========================================================================
-- 2. Add retry_count to turns (non-destructive: new column with default)
-- ===========================================================================
ALTER TABLE turns
    ADD COLUMN IF NOT EXISTS retry_count INTEGER DEFAULT 0;

COMMENT ON COLUMN turns.retry_count IS 'Number of retries for this turn';

-- ===========================================================================
-- 3. Config history table (new table, no impact on existing data)
-- ===========================================================================
CREATE TABLE IF NOT EXISTS config_history (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    config_key      VARCHAR(256) NOT NULL,
    config_value    JSONB,
    previous_value  JSONB,
    changed_by      TEXT,
    change_reason   TEXT,
    version         INTEGER NOT NULL DEFAULT 1,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_config_history_key ON config_history(config_key);
CREATE INDEX IF NOT EXISTS idx_config_history_created ON config_history(created_at);

COMMENT ON TABLE config_history IS 'Audit trail for configuration changes';

-- ===========================================================================
-- 4. Add session metadata index for JSONB queries
-- ===========================================================================
CREATE INDEX IF NOT EXISTS idx_sessions_metadata_gin
    ON sessions USING GIN (metadata jsonb_path_ops);

COMMIT;