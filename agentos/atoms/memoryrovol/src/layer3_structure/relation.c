/**
 * @file relation.c
 * @brief L3 结构层关系编码器（支持持久化）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer3_structure.h"
#include "agentos.h"
#include "logger.h"
#include <sqlite3.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <math.h>



static float* random_unit_vector(size_t dim) {
    float* vec = (float*)AGENTOS_MALLOC(dim * sizeof(float));
    if (!vec) return NULL;
    float norm = 0;
    for (size_t i = 0; i < dim; i++) {
        vec[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
        norm += vec[i] * vec[i];
    }
    if (norm > 0) {
        float inv = 1.0f / sqrtf(norm);
        for (size_t i = 0; i < dim; i++) vec[i] *= inv;
    }
    return vec;
}

static const char* create_relations_table_sql =
    "CREATE TABLE IF NOT EXISTS relations ("
    "from_id TEXT,"
    "to_id TEXT,"
    "type INTEGER,"
    "weight REAL,"
    "metadata TEXT,"
    "PRIMARY KEY (from_id, to_id, type)"
    ");";

agentos_relation_encoder_t* agentos_relation_encoder_create(
    agentos_binder_t* binder,
    const agentos_layer3_structure_config_t* manager) {

    if (!binder) return NULL;

    agentos_relation_encoder_t* enc = (agentos_relation_encoder_t*)AGENTOS_CALLOC(1, sizeof(agentos_relation_encoder_t));
    if (!enc) {
        AGENTOS_LOG_ERROR("Failed to allocate relation encoder");
        return NULL;
    }

    enc->binder = binder;
    enc->lock = agentos_mutex_create();
    if (!enc->lock) {
        AGENTOS_FREE(enc);
        return NULL;
    }

    size_t dim = binder->dimension;
    enc->role_subject = random_unit_vector(dim);
    enc->role_predicate = random_unit_vector(dim);
    enc->role_object = random_unit_vector(dim);

    if (!enc->role_subject || !enc->role_predicate || !enc->role_object) {
        if (enc->role_subject) AGENTOS_FREE(enc->role_subject);
        if (enc->role_predicate) AGENTOS_FREE(enc->role_predicate);
        if (enc->role_object) AGENTOS_FREE(enc->role_object);
        agentos_mutex_destroy(enc->lock);
        AGENTOS_FREE(enc);
        return NULL;
    }

    // 打开数据�?
    if (manager && manager->db_path) {
        enc->db_path = AGENTOS_STRDUP(manager->db_path);
        int rc = sqlite3_open(manager->db_path, &enc->db);
        if (rc != SQLITE_OK) {
            AGENTOS_LOG_ERROR("Failed to open relation database: %s", sqlite3_errmsg(enc->db));
            AGENTOS_FREE(enc->role_subject);
            AGENTOS_FREE(enc->role_predicate);
            AGENTOS_FREE(enc->role_object);
            agentos_mutex_destroy(enc->lock);
            AGENTOS_FREE(enc);
            return NULL;
        }
        char* errmsg = NULL;
        rc = sqlite3_exec(enc->db, create_relations_table_sql, NULL, NULL, &errmsg);
        if (rc != SQLITE_OK) {
            AGENTOS_LOG_ERROR("Failed to create relations table: %s", errmsg);
            sqlite3_free(errmsg);
            sqlite3_close(enc->db);
            AGENTOS_FREE(enc->role_subject);
            AGENTOS_FREE(enc->role_predicate);
            AGENTOS_FREE(enc->role_object);
            agentos_mutex_destroy(enc->lock);
            AGENTOS_FREE(enc);
            return NULL;
        }
    }

    return enc;
}

void agentos_relation_encoder_destroy(agentos_relation_encoder_t* enc) {
    if (!enc) return;
    AGENTOS_FREE(enc->role_subject);
    AGENTOS_FREE(enc->role_predicate);
    AGENTOS_FREE(enc->role_object);
    if (enc->db) sqlite3_close(enc->db);
    if (enc->db_path) AGENTOS_FREE(enc->db_path);
    if (enc->lock) agentos_mutex_destroy(enc->lock);
    AGENTOS_FREE(enc);
}

agentos_error_t agentos_relation_encode_triple(
    agentos_relation_encoder_t* enc,
    const float* subject,
    const float* predicate,
    const float* object,
    float** out_relation) {

    if (!enc || !subject || !predicate || !object || !out_relation) return AGENTOS_EINVAL;

    size_t dim = enc->binder->dimension;
    float* bound_subj = NULL;
    float* bound_pred = NULL;
    float* bound_obj = NULL;
    agentos_error_t err;

    const float* subj_pair[2] = {enc->role_subject, subject};
    err = agentos_binder_bind(enc->binder, subj_pair, 2, &bound_subj);
    if (err != AGENTOS_SUCCESS) return err;

    const float* pred_pair[2] = {enc->role_predicate, predicate};
    err = agentos_binder_bind(enc->binder, pred_pair, 2, &bound_pred);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(bound_subj);
        return err;
    }

    const float* obj_pair[2] = {enc->role_object, object};
    err = agentos_binder_bind(enc->binder, obj_pair, 2, &bound_obj);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(bound_subj);
        AGENTOS_FREE(bound_pred);
        return err;
    }

    float* result = (float*)AGENTOS_MALLOC(dim * sizeof(float));
    if (!result) {
        AGENTOS_FREE(bound_subj);
        AGENTOS_FREE(bound_pred);
        AGENTOS_FREE(bound_obj);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < dim; i++) {
        result[i] = bound_subj[i] + bound_pred[i] + bound_obj[i];
    }

    AGENTOS_FREE(bound_subj);
    AGENTOS_FREE(bound_pred);
    AGENTOS_FREE(bound_obj);

    *out_relation = result;
    return AGENTOS_SUCCESS;
}

// 注意：relation_encode_event 需要从记忆系统获取向量，这里保持原有实现（略）
agentos_error_t agentos_relation_encode_event(
    agentos_relation_encoder_t* enc,
    const agentos_relation_t** relations,
    size_t count,
    float** out_event) {
    return AGENTOS_ENOTSUP;
}

agentos_relation_t* agentos_relation_copy(const agentos_relation_t* src) {
    if (!src) return NULL;
    agentos_relation_t* dst = (agentos_relation_t*)AGENTOS_MALLOC(sizeof(agentos_relation_t));
    if (!dst) return NULL;
    dst->from_id = src->from_id ? AGENTOS_STRDUP(src->from_id) : NULL;
    dst->to_id = src->to_id ? AGENTOS_STRDUP(src->to_id) : NULL;
    dst->type = src->type;
    dst->weight = src->weight;
    dst->metadata_json = src->metadata_json ? AGENTOS_STRDUP(src->metadata_json) : NULL;
    if ((src->from_id && !dst->from_id) || (src->to_id && !dst->to_id) || (src->metadata_json && !dst->metadata_json)) {
        if (dst->from_id) AGENTOS_FREE(dst->from_id);
        if (dst->to_id) AGENTOS_FREE(dst->to_id);
        if (dst->metadata_json) AGENTOS_FREE(dst->metadata_json);
        AGENTOS_FREE(dst);
        return NULL;
    }
    return dst;
}

void agentos_relations_free(agentos_relation_t** relations, size_t count) {
    if (!relations) return;
    for (size_t i = 0; i < count; i++) {
        if (relations[i]) {
            if (relations[i]->from_id) AGENTOS_FREE(relations[i]->from_id);
            if (relations[i]->to_id) AGENTOS_FREE(relations[i]->to_id);
            if (relations[i]->metadata_json) AGENTOS_FREE(relations[i]->metadata_json);
            AGENTOS_FREE(relations[i]);
        }
    }
    AGENTOS_FREE(relations);
}
