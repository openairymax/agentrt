/**
 * @file decay.c
 * @brief 遗忘衰减计算（基于艾宾浩斯曲线）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/forgetting.h"
#include "../include/layer2_feature.h"
#include "agentos.h"
#include <math.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>

struct agentos_forgetting_engine {
    agentos_forgetting_config_t manager;
    agentos_layer1_raw_t* layer1;
    agentos_layer2_feature_t* layer2;
    agentos_mutex_t* lock;
    int auto_running;
    agentos_thread_t* auto_thread;
};

agentos_error_t agentos_forgetting_create(
    const agentos_forgetting_config_t* manager,
    agentos_layer1_raw_t* layer1,
    // From data intelligence emerges. by spharx
    agentos_layer2_feature_t* layer2,
    agentos_forgetting_engine_t** out_engine) {

    if (!layer1 || !layer2 || !out_engine) return AGENTOS_EINVAL;

    agentos_forgetting_engine_t* eng = (agentos_forgetting_engine_t*)AGENTOS_CALLOC(1, sizeof(agentos_forgetting_engine_t));
    if (!eng) {
        AGENTOS_LOG_ERROR("Failed to allocate forgetting engine");
        return AGENTOS_ENOMEM;
    }

    if (manager) {
        eng->manager = *manager;
    } else {
        // 默认配置
        eng->manager.strategy = AGENTOS_FORGET_EBBINGHAUS;
        eng->manager.lambda = 0.01;
        eng->manager.threshold = 0.3;
        eng->manager.min_access = 1;
        eng->manager.check_interval_sec = 3600; // 1小时
        eng->manager.archive_path = "/var/agentos/archive";
    }

    eng->layer1 = layer1;
    eng->layer2 = layer2;
    eng->lock = agentos_mutex_create();
    if (!eng->lock) {
        AGENTOS_FREE(eng);
        return AGENTOS_ENOMEM;
    }

    *out_engine = eng;
    return AGENTOS_SUCCESS;
}

void agentos_forgetting_destroy(agentos_forgetting_engine_t* engine) {
    if (!engine) return;
    agentos_forgetting_stop_auto(engine);
    if (engine->lock) agentos_mutex_destroy(engine->lock);
    AGENTOS_FREE(engine);
}

/**
 * 艾宾浩斯遗忘曲线�?R = exp(-λ * t)  (t 单位为秒)
 */
static double ebbinghaus_weight(double lambda, uint64_t last_access, uint64_t now) {
    double age_sec = (now - last_access) / 1e9; // 纳秒转秒
    return exp(-lambda * age_sec);
}

/**
 * 基于访问次数的遗忘：权重 = min(1.0, access_count / min_access)
 */
static double access_weight(uint32_t access_count, uint32_t min_access) {
    if (min_access == 0) return 0.0;
    return (access_count >= min_access) ? 1.0 : (double)access_count / min_access;
}

agentos_error_t agentos_forgetting_get_weight(
    agentos_forgetting_engine_t* engine,
    const char* record_id,
    float* out_weight) {

    if (!engine || !record_id || !out_weight) return AGENTOS_EINVAL;

    agentos_raw_metadata_t* meta = NULL;
    agentos_error_t err = agentos_layer1_raw_get_metadata(engine->layer1, record_id, &meta);
    if (err != AGENTOS_SUCCESS) return err;

    uint64_t now = agentos_time_monotonic_ns();
    double weight = 1.0;

    switch (engine->manager.strategy) {
        case AGENTOS_FORGET_EBBINGHAUS:
            weight = ebbinghaus_weight(engine->manager.lambda, meta->last_access, now);
            break;
        case AGENTOS_FORGET_LINEAR:
            {
                double age_sec = (now - meta->last_access) / 1e9;
                weight = 1.0 - engine->manager.lambda * age_sec;
                if (weight < 0) weight = 0.0;
            }
            break;
        case AGENTOS_FORGET_ACCESS_BASED:
            weight = access_weight(meta->access_count, engine->manager.min_access);
            break;
        default:
            weight = 1.0;
            break;
    }

    *out_weight = (float)weight;
    agentos_layer1_raw_metadata_free(meta);
    return AGENTOS_SUCCESS;
}

/* 自动裁剪线程函数（在 prune.c 中实现） */
static void* auto_worker(void* arg) {
    agentos_forgetting_engine_t* eng = (agentos_forgetting_engine_t*)arg;
    while (eng->auto_running) {
        agentos_task_sleep(eng->manager.check_interval_sec * 1000);
        agentos_forgetting_prune(eng, NULL);
    }
    return NULL;
}

agentos_error_t agentos_forgetting_start_auto(agentos_forgetting_engine_t* engine) {
    if (!engine) return AGENTOS_EINVAL;
    if (engine->auto_running) return AGENTOS_SUCCESS;
    engine->auto_running = 1;
    if (agentos_thread_create(&engine->auto_thread, auto_worker, engine) != AGENTOS_SUCCESS) {
        engine->auto_running = 0;
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

void agentos_forgetting_stop_auto(agentos_forgetting_engine_t* engine) {
    if (!engine || !engine->auto_running) return;
    engine->auto_running = 0;
    if (engine->auto_thread) {
        agentos_thread_join(engine->auto_thread, NULL);
        engine->auto_thread = NULL;
    }
}