/**
 * @file binder.c
 * @brief L3 绑定器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/layer3_structure.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"
#include <string.h>

typedef struct binder_config {
    float similarity_threshold;
    int max_bindings_per_entity;
} binder_config_t;

typedef struct binding_context {
    agentos_knowledge_graph_t* kg;
    binder_config_t manager;
} binding_context_t;

static binding_context_t* g_binding_ctx = NULL;

static agentos_error_t ensure_binding_context(void) {
    if (g_binding_ctx) return AGENTOS_SUCCESS;

    g_binding_ctx = (binding_context_t*)AGENTOS_CALLOC(1, sizeof(binding_context_t));
    if (!g_binding_ctx) return AGENTOS_ENOMEM;

    agentos_error_t err = agentos_knowledge_graph_create(&g_binding_ctx->kg);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(g_binding_ctx);
        g_binding_ctx = NULL;
        return err;
    }

    g_binding_ctx->manager.similarity_threshold = 0.7f;
    g_binding_ctx->manager.max_bindings_per_entity = 100;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer3_bind_entities(
    const char* entity_a,
    const char* entity_b,
    agentos_relation_type_t relation_type,
    float weight) {
    if (!entity_a || !entity_b) return AGENTOS_EINVAL;

    agentos_error_t err = ensure_binding_context();
    if (err != AGENTOS_SUCCESS) return err;

    return agentos_knowledge_graph_add_relation(
        g_binding_ctx->kg,
        entity_a,
        entity_b,
        relation_type,
        weight);
}
