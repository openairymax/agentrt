/**
 * @file config.c
 * @brief MemoryRovol 配置管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供 JSON 配置加载、默认配置生成和配置资源释放功能。
 * 支持 HNSW 索引（768维默认）和艾宾浩斯遗忘策略。
 */

#include "../include/config.h"
#include "memory_compat.h"
#include "string_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CJSON
#include <cjson/cJSON.h>
#endif

static agentos_error_t config_set_defaults(agentos_memoryrov_config_t* cfg) {
    if (!cfg) return AGENTOS_EINVAL;

    cfg->raw_storage_path = AGENTOS_STRDUP("./data/memoryrovol/raw");
    cfg->index_path = AGENTOS_STRDUP("./data/memoryrovol/index");
    cfg->relation_db_path = AGENTOS_STRDUP("./data/memoryrovol/relation.db");
    cfg->pattern_storage_path = AGENTOS_STRDUP("./data/memoryrovol/patterns");

    cfg->embedding_model = AGENTOS_STRDUP("default");
    cfg->llm_model = AGENTOS_STRDUP("default");

    cfg->embedding_dim = 768;
    cfg->index_type = 2;
    cfg->hnsw_m = 16;
    cfg->ivf_nlist = 100;

    cfg->binder_q = 8;
    cfg->use_complex = 0;

    cfg->pattern_min_support = 3;
    cfg->pattern_confidence = 0.65;
    cfg->pattern_mine_interval = 300;

    cfg->retrieval_max_candidates = 50;
    cfg->retrieval_threshold = 0.3f;
    cfg->attractor_iterations = 10;
    cfg->attractor_damping = 1.0f;

    cfg->forget_strategy = 0;
    cfg->forget_lambda = 0.1;
    cfg->forget_threshold = 0.3;
    cfg->forget_check_interval = 3600;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_memoryrov_config_load(
    const char* file_path,
    agentos_memoryrov_config_t** out_config) {

    if (!file_path || !out_config) return AGENTOS_EINVAL;

    *out_config = (agentos_memoryrov_config_t*)AGENTOS_CALLOC(1, sizeof(agentos_memoryrov_config_t));
    if (!*out_config) return AGENTOS_ENOMEM;

    agentos_error_t err = config_set_defaults(*out_config);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(*out_config);
        *out_config = NULL;
        return err;
    }

#ifdef HAVE_CJSON
    FILE* fp = fopen(file_path, "r");
    if (!fp) {
        return AGENTOS_SUCCESS;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 1024 * 1024) {
        fclose(fp);
        return AGENTOS_SUCCESS;
    }

    char* json_str = (char*)AGENTOS_MALLOC((size_t)fsize + 1);
    if (!json_str) {
        fclose(fp);
        return AGENTOS_ENOMEM;
    }

    size_t nread = fread(json_str, 1, (size_t)fsize, fp);
    json_str[nread] = '\0';
    fclose(fp);

    cJSON* root = cJSON_Parse(json_str);
    AGENTOS_FREE(json_str);

    if (!root) {
        return AGENTOS_SUCCESS;
    }

    cJSON* item;

    item = cJSON_GetObjectItem(root, "raw_storage_path");
    if (item && cJSON_IsString(item)) {
        AGENTOS_FREE((*out_config)->raw_storage_path);
        (*out_config)->raw_storage_path = AGENTOS_STRDUP(item->valuestring);
    }

    item = cJSON_GetObjectItem(root, "index_path");
    if (item && cJSON_IsString(item)) {
        AGENTOS_FREE((*out_config)->index_path);
        (*out_config)->index_path = AGENTOS_STRDUP(item->valuestring);
    }

    item = cJSON_GetObjectItem(root, "embedding_model");
    if (item && cJSON_IsString(item)) {
        AGENTOS_FREE((*out_config)->embedding_model);
        (*out_config)->embedding_model = AGENTOS_STRDUP(item->valuestring);
    }

    item = cJSON_GetObjectItem(root, "embedding_dim");
    if (item && cJSON_IsNumber(item)) {
        (*out_config)->embedding_dim = (uint32_t)item->valuedouble;
    }

    item = cJSON_GetObjectItem(root, "index_type");
    if (item && cJSON_IsNumber(item)) {
        (*out_config)->index_type = (uint32_t)item->valuedouble;
    }

    item = cJSON_GetObjectItem(root, "hnsw_m");
    if (item && cJSON_IsNumber(item)) {
        (*out_config)->hnsw_m = (uint32_t)item->valuedouble;
    }

    item = cJSON_GetObjectItem(root, "forget_lambda");
    if (item && cJSON_IsNumber(item)) {
        (*out_config)->forget_lambda = item->valuedouble;
    }

    item = cJSON_GetObjectItem(root, "forget_threshold");
    if (item && cJSON_IsNumber(item)) {
        (*out_config)->forget_threshold = item->valuedouble;
    }

    cJSON_Delete(root);
#else
    (void)file_path;
#endif

    return AGENTOS_SUCCESS;
}

void agentos_memoryrov_config_free(agentos_memoryrov_config_t* cfg) {
    if (!cfg) return;

    AGENTOS_FREE(cfg->raw_storage_path);
    AGENTOS_FREE(cfg->index_path);
    AGENTOS_FREE(cfg->relation_db_path);
    AGENTOS_FREE(cfg->pattern_storage_path);
    AGENTOS_FREE(cfg->embedding_model);
    AGENTOS_FREE(cfg->llm_model);
    memset(cfg, 0, sizeof(agentos_memoryrov_config_t));
}

agentos_error_t agentos_memoryrov_config_default(
    agentos_memoryrov_config_t** out_config) {

    if (!out_config) return AGENTOS_EINVAL;

    *out_config = (agentos_memoryrov_config_t*)AGENTOS_CALLOC(1, sizeof(agentos_memoryrov_config_t));
    if (!*out_config) return AGENTOS_ENOMEM;

    return config_set_defaults(*out_config);
}
