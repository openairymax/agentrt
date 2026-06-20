-- Rollback: V002 → V001
-- Description: Drop V002 additions, preserve core data
-- Created: 2026-06-17T00:00:00Z
--
-- Backward-compatible: drops new columns/table, does NOT lose core data
--   - agents: tags column dropped (original data preserved)
--   - turns: retry_count column dropped (original data preserved)
--   - config_history: table dropped (new data, no existing dependency)
--   - sessions: GIN index dropped (original data preserved)

BEGIN;

-- Drop GIN index on sessions (safe)
DROP INDEX IF EXISTS idx_sessions_metadata_gin;

-- Drop config_history table (safe — no FK references)
DROP TABLE IF EXISTS config_history;

-- Drop retry_count from turns (safe — defaults to 0, no FK)
ALTER TABLE turns
    DROP COLUMN IF EXISTS retry_count;

-- Drop tags from agents (safe — defaults to '[]', no FK)
ALTER TABLE agents
    DROP COLUMN IF EXISTS tags;

COMMIT;