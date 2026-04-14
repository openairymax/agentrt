// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file zero_trust_integration.h
 * @brief Zero Trust Architecture Integration for AgentOS
 *
 * 零信任架构集成模块，实现"永不信任，始终验证"的安全原则。
 * 与Cupolas安全穹顶和SafetyGuard守卫框架深度集成。
 *
 * 核心原则:
 * 1. 永不信任 — 所有访问请求必须经过验证
 * 2. 最小权限 — 仅授予完成任务所需的最小权限
 * 3. 假设违规 — 始终假设存在安全威胁
 * 4. 显式验证 — 每次访问都进行身份验证和授权
 * 5. 微分段 — 细粒度的网络和资源隔离
 *
 * @since 2.0.0
 */

#ifndef AGENTOS_ZERO_TRUST_INTEGRATION_H
#define AGENTOS_ZERO_TRUST_INTEGRATION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZTA_MAX_IDENTITIES      512
#define ZTA_MAX_POLICIES        256
#define ZTA_MAX_SESSIONS        1024
#define ZTA_MAX_TRUST_LEVELS    5

typedef enum {
    ZTA_TRUST_NONE = 0,
    ZTA_TRUST_LOW = 1,
    ZTA_TRUST_MEDIUM = 2,
    ZTA_TRUST_HIGH = 3,
    ZTA_TRUST_FULL = 4
} zta_trust_level_t;

typedef enum {
    ZTA_AUTH_NONE = 0,
    ZTA_AUTH_TOKEN,
    ZTA_AUTH_CERTIFICATE,
    ZTA_AUTH_MUTUAL_TLS,
    ZTA_AUTH_MULTI_FACTOR
} zta_auth_method_t;

typedef struct {
    char id[64];
    char name[128];
    char type[32];
    zta_trust_level_t trust_level;
    zta_auth_method_t auth_method;
    char* attributes_json;
    char* certificate_pem;
    uint64_t last_verified;
    uint64_t verification_expiry;
} zta_identity_t;

typedef struct {
    char id[64];
    char session_token[256];
    char identity_id[64];
    zta_trust_level_t trust_level;
    char* context_json;
    uint64_t created_at;
    uint64_t expires_at;
    uint64_t last_activity;
    bool is_valid;
} zta_session_t;

typedef struct {
    char id[64];
    char name[128];
    char description[256];
    zta_trust_level_t min_trust_level;
    char subject_pattern[256];
    char action_pattern[128];
    char resource_pattern[256];
    char condition_json[512];
    char* required_attributes_json;
    bool require_mfa;
    uint32_t max_session_duration_ms;
} zta_policy_t;

typedef struct {
    char id[64];
    char name[128];
    zta_trust_level_t trust_level;
    char* allowed_actions_json;
    char* allowed_resources_json;
    char* network_segments_json;
} zta_segment_t;

typedef struct zta_context_s zta_context_t;

typedef bool (*zta_verify_identity_fn)(const zta_identity_t* identity,
                                        const char* challenge,
                                        char** response,
                                        void* user_data);

typedef void (*zta_session_callback_t)(const zta_session_t* session,
                                         const char* event_type,
                                         void* user_data);

zta_context_t* zta_context_create(void);
void zta_context_destroy(zta_context_t* ctx);

int zta_register_identity(zta_context_t* ctx, const zta_identity_t* identity);
int zta_unregister_identity(zta_context_t* ctx, const char* identity_id);
const zta_identity_t* zta_get_identity(zta_context_t* ctx, const char* identity_id);

int zta_add_policy(zta_context_t* ctx, const zta_policy_t* policy);
int zta_remove_policy(zta_context_t* ctx, const char* policy_id);

int zta_add_segment(zta_context_t* ctx, const zta_segment_t* segment);
int zta_remove_segment(zta_context_t* ctx, const char* segment_id);

int zta_authenticate(zta_context_t* ctx,
                       const char* identity_id,
                       zta_auth_method_t method,
                       const char* credentials_json,
                       zta_session_t** session);

int zta_authorize(zta_context_t* ctx,
                    const char* session_token,
                    const char* action,
                    const char* resource,
                    bool* allowed);

int zta_validate_session(zta_context_t* ctx, const char* session_token, bool* valid);
int zta_invalidate_session(zta_context_t* ctx, const char* session_token);
int zta_refresh_session(zta_context_t* ctx, const char* session_token, zta_session_t** session);

zta_trust_level_t zta_evaluate_trust(zta_context_t* ctx,
                                       const char* identity_id,
                                       const char* context_json);

int zta_escalate_trust(zta_context_t* ctx,
                         const char* session_token,
                         zta_auth_method_t method,
                         const char* evidence_json);

int zta_set_verify_fn(zta_context_t* ctx, zta_verify_identity_fn fn, void* user_data);
int zta_set_session_callback(zta_context_t* ctx, zta_session_callback_t callback, void* user_data);

size_t zta_get_identity_count(zta_context_t* ctx);
size_t zta_get_session_count(zta_context_t* ctx);
size_t zta_get_policy_count(zta_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ZERO_TRUST_INTEGRATION_H */
