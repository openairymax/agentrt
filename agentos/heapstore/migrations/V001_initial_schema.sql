-- Migration: V001
-- Description: AgentRT Heapstore Initial Schema
-- Created: 2026-06-16T00:00:00Z
-- Forward-only: true

BEGIN;

-- ===========================================================================
-- Agent 注册表
-- ===========================================================================
CREATE TABLE IF NOT EXISTS agents (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name            VARCHAR(128) NOT NULL,
    namespace       VARCHAR(128) NOT NULL DEFAULT 'default',
    version         VARCHAR(32) NOT NULL DEFAULT '0.1.0',
    type            VARCHAR(32) NOT NULL DEFAULT 'generic',
    description     TEXT,
    config          JSONB DEFAULT '{}',
    enabled         BOOLEAN NOT NULL DEFAULT TRUE,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(name, namespace)
);

CREATE INDEX IF NOT EXISTS idx_agents_namespace ON agents(namespace);
CREATE INDEX IF NOT EXISTS idx_agents_enabled ON agents(enabled);
CREATE INDEX IF NOT EXISTS idx_agents_type ON agents(type);

-- ===========================================================================
-- Agent 会话表
-- ===========================================================================
CREATE TABLE IF NOT EXISTS sessions (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    agent_id        UUID NOT NULL REFERENCES agents(id) ON DELETE CASCADE,
    user_id         VARCHAR(256),
    status          VARCHAR(32) NOT NULL DEFAULT 'active',
    context         JSONB DEFAULT '{}',
    metadata        JSONB DEFAULT '{}',
    started_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    ended_at        TIMESTAMPTZ,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_sessions_agent ON sessions(agent_id);
CREATE INDEX IF NOT EXISTS idx_sessions_status ON sessions(status);
CREATE INDEX IF NOT EXISTS idx_sessions_user ON sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_sessions_started ON sessions(started_at);

-- ===========================================================================
-- Agent 对话轮次表
-- ===========================================================================
CREATE TABLE IF NOT EXISTS turns (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    session_id      UUID NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    turn_number     INTEGER NOT NULL,
    role            VARCHAR(32) NOT NULL,
    content         TEXT,
    tool_calls      JSONB,
    tool_results    JSONB,
    tokens_in       INTEGER DEFAULT 0,
    tokens_out      INTEGER DEFAULT 0,
    cost_usd        NUMERIC(10,6) DEFAULT 0,
    duration_ms     INTEGER DEFAULT 0,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(session_id, turn_number)
);

CREATE INDEX IF NOT EXISTS idx_turns_session ON turns(session_id);
CREATE INDEX IF NOT EXISTS idx_turns_created ON turns(created_at);

-- ===========================================================================
-- Checkpoint / 状态快照表
-- ===========================================================================
CREATE TABLE IF NOT EXISTS checkpoints (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    session_id      UUID NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    turn_number     INTEGER NOT NULL,
    state           JSONB NOT NULL,
    checksum        VARCHAR(64) NOT NULL,
    size_bytes      INTEGER DEFAULT 0,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(session_id, turn_number)
);

CREATE INDEX IF NOT EXISTS idx_checkpoints_session ON checkpoints(session_id);

-- ===========================================================================
-- LLM 调用日志表
-- ===========================================================================
CREATE TABLE IF NOT EXISTS llm_calls (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    turn_id         UUID REFERENCES turns(id) ON DELETE SET NULL,
    provider        VARCHAR(64) NOT NULL,
    model           VARCHAR(128) NOT NULL,
    request_json    JSONB,
    response_json   JSONB,
    tokens_in       INTEGER DEFAULT 0,
    tokens_out      INTEGER DEFAULT 0,
    cost_usd        NUMERIC(10,6) DEFAULT 0,
    latency_ms      INTEGER DEFAULT 0,
    status          VARCHAR(32) NOT NULL DEFAULT 'ok',
    error_message   TEXT,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_llm_calls_turn ON llm_calls(turn_id);
CREATE INDEX IF NOT EXISTS idx_llm_calls_created ON llm_calls(created_at);
CREATE INDEX IF NOT EXISTS idx_llm_calls_provider ON llm_calls(provider);
CREATE INDEX IF NOT EXISTS idx_llm_calls_model ON llm_calls(model);

-- ===========================================================================
-- 工具调用日志表
-- ===========================================================================
CREATE TABLE IF NOT EXISTS tool_calls (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    turn_id         UUID REFERENCES turns(id) ON DELETE SET NULL,
    tool_name       VARCHAR(128) NOT NULL,
    arguments       JSONB,
    result          JSONB,
    approved        BOOLEAN NOT NULL DEFAULT FALSE,
    safety_check    JSONB,
    duration_ms     INTEGER DEFAULT 0,
    status          VARCHAR(32) NOT NULL DEFAULT 'pending',
    error_message   TEXT,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_tool_calls_turn ON tool_calls(turn_id);
CREATE INDEX IF NOT EXISTS idx_tool_calls_tool ON tool_calls(tool_name);
CREATE INDEX IF NOT EXISTS idx_tool_calls_created ON tool_calls(created_at);

-- ===========================================================================
-- 事件日志表
-- ===========================================================================
CREATE TABLE IF NOT EXISTS events (
    id              BIGSERIAL PRIMARY KEY,
    event_type      VARCHAR(64) NOT NULL,
    source          VARCHAR(128) NOT NULL,
    severity        VARCHAR(16) NOT NULL DEFAULT 'INFO',
    payload         JSONB,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_events_type ON events(event_type);
CREATE INDEX IF NOT EXISTS idx_events_created ON events(created_at);
CREATE INDEX IF NOT EXISTS idx_events_severity ON events(severity);

-- ===========================================================================
-- 成本追踪表
-- ===========================================================================
CREATE TABLE IF NOT EXISTS cost_tracking (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    agent_id        UUID REFERENCES agents(id) ON DELETE SET NULL,
    provider        VARCHAR(64) NOT NULL,
    model           VARCHAR(128) NOT NULL,
    tokens_in       BIGINT DEFAULT 0,
    tokens_out      BIGINT DEFAULT 0,
    cost_usd        NUMERIC(12,6) DEFAULT 0,
    period_start    DATE NOT NULL,
    period_end      DATE NOT NULL,
    budget_limit    NUMERIC(12,6),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE(agent_id, provider, model, period_start)
);

CREATE INDEX IF NOT EXISTS idx_cost_agent ON cost_tracking(agent_id);
CREATE INDEX IF NOT EXISTS idx_cost_period ON cost_tracking(period_start, period_end);

-- ===========================================================================
-- 迁移版本追踪表
-- ===========================================================================
CREATE TABLE IF NOT EXISTS _migrations (
    id              SERIAL PRIMARY KEY,
    version         VARCHAR(16) NOT NULL UNIQUE,
    description     TEXT NOT NULL,
    checksum        VARCHAR(64) NOT NULL,
    applied_at      TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    applied_by      TEXT NOT NULL DEFAULT CURRENT_USER,
    duration_ms     INTEGER,
    success         BOOLEAN NOT NULL DEFAULT TRUE
);

CREATE INDEX IF NOT EXISTS idx_migrations_version ON _migrations(version);

COMMIT;