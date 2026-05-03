/**
 * @file layer3_structure_internal.h
 * @brief L3 结构层内部实现定义（不对外暴露）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本头文件仅供 L3 结构层实现源文件使用，
 * 不得被外部模块或公共头文件包含。
 * 商业版内部结构定义，OSS 模式下不可见。
 */

#ifndef AGENTOS_LAYER3_STRUCTURE_INTERNAL_H
#define AGENTOS_LAYER3_STRUCTURE_INTERNAL_H

#include "layer3_structure.h"
#include "agentos.h"

#ifndef AGENTOS_LAYER3_STRUCTURE_SQLITE3_TYPEDEF
#define AGENTOS_LAYER3_STRUCTURE_SQLITE3_TYPEDEF
typedef struct sqlite3 sqlite3;
#endif

struct agentos_binder {
    void* data;
    agentos_mutex_t* lock;
    char* name;
    uint32_t dimension;
    float* Q;
    int use_complex;
    void (*bind_matrices)(struct agentos_binder* self, const float* input, float* output);
};

struct agentos_unbinder {
    agentos_binder_t* binder;
    agentos_mutex_t* lock;
};

struct agentos_sequence_encoder {
    void* model;
    uint32_t dimension;
    char* model_path;
    agentos_mutex_t* lock;
    float** position_vectors;
    uint32_t max_len;
    agentos_binder_t* binder;
    int position_encoding;
};

struct agentos_relation_encoder {
    void* model;
    uint32_t dimension;
    char* model_path;
    agentos_mutex_t* lock;
    sqlite3* db;
    char* db_path;
    float* role_subject;
    float* role_predicate;
    float* role_object;
    agentos_binder_t* binder;
};

#endif /* AGENTOS_LAYER3_STRUCTURE_INTERNAL_H */
