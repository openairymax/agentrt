/**
 * @file miner.c
 * @brief 模式挖掘器（基于持久同调和聚类）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer4_pattern.h"
#include "persistence.h"
#include "clustering.h"
#include "rules.h"
#include "validator.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#ifdef HAVE_RIPSER
#include <ripser.h>
#endif


#ifdef HAVE_HDBSCAN
#include <hdbscan/hdbscan.h>
#endif

/* 默认配置 */
#define DEFAULT_MIN_CLUSTER_SIZE 5
#define DEFAULT_PERSISTENCE_THRESHOLD 0.1
#define DEFAULT_MINING_INTERVAL 3600  /* 1小时 */

struct agentos_layer4_pattern {
    agentos_persistence_calculator_t* pers_calc;
    agentos_clustering_engine_t* cluster_engine;
    agentos_rule_generator_t* rule_gen;
    agentos_pattern_validator_t* validator;
    agentos_mutex_t* lock;
    agentos_layer4_pattern_config_t manager;
    // 自动挖掘线程
    pthread_t auto_thread;
    int auto_running;
    // 数据源回调（可扩展）
    void* data_source_ctx;
    agentos_error_t (*get_vectors_func)(void* ctx, float** out_vectors, char*** out_ids, size_t* out_count);
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 计算距离矩阵（余弦距离）
 */
static float* compute_distance_matrix(const float* vectors, size_t count, size_t dim) {
    float* dist = (float*)AGENTOS_MALLOC(count * count * sizeof(float));
    if (!dist) return NULL;
    for (size_t i = 0; i < count; i++) {
        dist[i * count + i] = 0.0f;
        for (size_t j = i + 1; j < count; j++) {
            float dot = 0.0f;
            const float* vi = vectors + i * dim;
            const float* vj = vectors + j * dim;
            for (size_t k = 0; k < dim; k++) {
                dot += vi[k] * vj[k];
            }
            float d = 1.0f - dot;  // 余弦距离
            dist[i * count + j] = d;
            dist[j * count + i] = d;
        }
    }
    return dist;
}

/**
 * @brief 执行持久同调计算
 */
static agentos_error_t compute_persistence(
    agentos_layer4_pattern_t* miner,
    const float* distance_matrix,
    size_t count,
    agentos_persistence_feature_t*** out_features,
    size_t* out_count) {

    if (!miner->pers_calc) return AGENTOS_ENOTSUP;
    return agentos_persistence_calculator_compute(
        miner->pers_calc, distance_matrix, count, out_features, out_count);
}

/**
 * @brief 执行聚类
 */
static agentos_error_t perform_clustering(
    agentos_layer4_pattern_t* miner,
    const float* vectors,
    size_t count,
    int** out_labels,
    float** out_centroids,
    int* out_num_clusters) {

    if (!miner->cluster_engine) return AGENTOS_ENOTSUP;
    return agentos_clustering_engine_cluster(
        miner->cluster_engine, vectors, count, out_labels, out_centroids, out_num_clusters);
}

/**
 * @brief 为每个聚类生成规�?
 */
static agentos_error_t generate_rules_for_clusters(
    agentos_layer4_pattern_t* miner,
    const float* vectors,
    const char** vector_ids,
    size_t count,
    const int* labels,
    int num_clusters,
    agentos_pattern_t*** out_patterns,
    size_t* out_count) {

    if (!miner->rule_gen) return AGENTOS_ENOTSUP;

    size_t dim = 384;  // 应从配置获取，这里简�?
    size_t pattern_cap = 16;
    size_t pattern_cnt = 0;
    agentos_pattern_t** patterns = (agentos_pattern_t**)AGENTOS_MALLOC(pattern_cap * sizeof(agentos_pattern_t*));
    if (!patterns) return AGENTOS_ENOMEM;

    for (int c = 0; c < num_clusters; c++) {
        // 统计该聚类的大小
        size_t cluster_size = 0;
        for (size_t i = 0; i < count; i++) {
            if (labels[i] == c) cluster_size++;
        }
        if (cluster_size < miner->manager.min_cluster_size) continue;

        // 收集该聚类的向量和ID
        float* cluster_vectors = (float*)AGENTOS_MALLOC(cluster_size * dim * sizeof(float));
        const char** cluster_ids = (const char**)AGENTOS_MALLOC(cluster_size * sizeof(const char*));
        if (!cluster_vectors || !cluster_ids) {
            AGENTOS_FREE(cluster_vectors);
            AGENTOS_FREE(cluster_ids);
            continue;
        }
        size_t idx = 0;
        for (size_t i = 0; i < count; i++) {
            if (labels[i] == c) {
                memcpy(cluster_vectors + idx * dim, vectors + i * dim, dim * sizeof(float));
                cluster_ids[idx] = vector_ids[i];
                idx++;
            }
        }

        // 生成规则
        char* rule_json = NULL;
        agentos_error_t err = agentos_rule_generator_generate(
            miner->rule_gen, cluster_vectors, cluster_ids, cluster_size, &rule_json);
        if (err == AGENTOS_SUCCESS && rule_json) {
            // 计算聚类中心（可用均值）
            float* centroid = (float*)AGENTOS_CALLOC(dim, sizeof(float));
            if (centroid) {
                for (size_t j = 0; j < cluster_size; j++) {
                    const float* v = cluster_vectors + j * dim;
                    for (size_t k = 0; k < dim; k++) {
                        centroid[k] += v[k];
                    }
                }
                for (size_t k = 0; k < dim; k++) {
                    centroid[k] /= cluster_size;
                }
            }
            agentos_pattern_t* pat = agentos_pattern_create(
                "Auto-mined pattern", rule_json, 0.8f, centroid, dim);
            if (pat) {
                if (pattern_cnt >= pattern_cap) {
                    pattern_cap *= 2;
                    agentos_pattern_t** new_pats = (agentos_pattern_t**)AGENTOS_REALLOC(patterns, pattern_cap * sizeof(agentos_pattern_t*));
                    if (!new_pats) {
                        agentos_pattern_free(pat);
                        AGENTOS_FREE(rule_json);
                        AGENTOS_FREE(cluster_vectors);
                        AGENTOS_FREE(cluster_ids);
                        AGENTOS_FREE(centroid);
                        continue;
                    }
                    patterns = new_pats;
                }
                patterns[pattern_cnt++] = pat;
            }
            AGENTOS_FREE(rule_json);
            if (centroid) AGENTOS_FREE(centroid);
        }
        AGENTOS_FREE(cluster_vectors);
        AGENTOS_FREE(cluster_ids);
    }

    *out_patterns = patterns;
    *out_count = pattern_cnt;
    return AGENTOS_SUCCESS;
}

/* ==================== 公共接口 ==================== */

agentos_error_t agentos_pattern_miner_create(
    const agentos_layer4_pattern_config_t* manager,
    agentos_layer4_pattern_t** out_miner) {

    if (!out_miner) return AGENTOS_EINVAL;

    agentos_layer4_pattern_t* miner = (agentos_layer4_pattern_t*)AGENTOS_CALLOC(1, sizeof(agentos_layer4_pattern_t));
    if (!miner) {
        AGENTOS_LOG_ERROR("Failed to allocate pattern miner");
        return AGENTOS_ENOMEM;
    }

    // 设置配置
    if (manager) {
        miner->manager = *manager;
    } else {
        miner->manager.min_cluster_size = DEFAULT_MIN_CLUSTER_SIZE;
        miner->manager.persistence_threshold = DEFAULT_PERSISTENCE_THRESHOLD;
        miner->manager.mining_interval_sec = DEFAULT_MINING_INTERVAL;
        miner->manager.pattern_storage_path = NULL;
    }

    // 创建子组�?
    agentos_error_t err;

    err = agentos_persistence_calculator_create(NULL, &miner->pers_calc);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to create persistence calculator");
        AGENTOS_FREE(miner);
        return err;
    }

    err = agentos_clustering_engine_create("hdbscan", "{\"min_cluster_size\":5}", &miner->cluster_engine);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to create clustering engine");
        agentos_persistence_calculator_destroy(miner->pers_calc);
        AGENTOS_FREE(miner);
        return err;
    }

    err = agentos_rule_generator_create(NULL, &miner->rule_gen);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to create rule generator");
        agentos_persistence_calculator_destroy(miner->pers_calc);
        agentos_clustering_engine_destroy(miner->cluster_engine);
        AGENTOS_FREE(miner);
        return err;
    }

    err = agentos_pattern_validator_create(NULL, &miner->validator);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to create pattern validator");
        agentos_persistence_calculator_destroy(miner->pers_calc);
        agentos_clustering_engine_destroy(miner->cluster_engine);
        agentos_rule_generator_destroy(miner->rule_gen);
        AGENTOS_FREE(miner);
        return err;
    }

    miner->lock = agentos_mutex_create();
    if (!miner->lock) {
        AGENTOS_LOG_ERROR("Failed to create mutex");
        agentos_persistence_calculator_destroy(miner->pers_calc);
        agentos_clustering_engine_destroy(miner->cluster_engine);
        agentos_rule_generator_destroy(miner->rule_gen);
        agentos_pattern_validator_destroy(miner->validator);
        AGENTOS_FREE(miner);
        return AGENTOS_ENOMEM;
    }

    *out_miner = miner;
    return AGENTOS_SUCCESS;
}

void agentos_pattern_miner_destroy(agentos_layer4_pattern_t* miner) {
    if (!miner) return;

    // 停止自动线程
    agentos_pattern_miner_stop_auto(miner);

    agentos_mutex_lock(miner->lock);

    if (miner->pers_calc) agentos_persistence_calculator_destroy(miner->pers_calc);
    if (miner->cluster_engine) agentos_clustering_engine_destroy(miner->cluster_engine);
    if (miner->rule_gen) agentos_rule_generator_destroy(miner->rule_gen);
    if (miner->validator) agentos_pattern_validator_destroy(miner->validator);

    agentos_mutex_unlock(miner->lock);
    agentos_mutex_destroy(miner->lock);
    AGENTOS_FREE(miner);
}

agentos_error_t agentos_pattern_miner_mine(
    agentos_layer4_pattern_t* miner,
    const float* vectors,
    const char** vector_ids,
    size_t count,
    agentos_pattern_t*** out_patterns,
    size_t* out_count) {

    if (!miner || !vectors || !vector_ids || count == 0 || !out_patterns || !out_count)
        return AGENTOS_EINVAL;

    agentos_mutex_lock(miner->lock);

    // 1. 计算距离矩阵
    size_t dim = 384; // 实际应从上下文获取，这里简�?
    float* distance_matrix = compute_distance_matrix(vectors, count, dim);
    if (!distance_matrix) {
        agentos_mutex_unlock(miner->lock);
        return AGENTOS_ENOMEM;
    }

    // 2. 计算持久特征（可选，用于后续过滤�?
    agentos_persistence_feature_t** features = NULL;
    size_t feature_count = 0;
    compute_persistence(miner, distance_matrix, count, &features, &feature_count);
    if (features) {
        agentos_persistence_features_free(features, feature_count);
    }
    AGENTOS_FREE(distance_matrix);

    // 3. 执行聚类
    int* labels = NULL;
    float* centroids = NULL;
    int num_clusters = 0;
    agentos_error_t err = perform_clustering(miner, vectors, count, &labels, &centroids, &num_clusters);
    if (err != AGENTOS_SUCCESS) {
        agentos_mutex_unlock(miner->lock);
        return err;
    }

    // 4. 生成规则
    agentos_pattern_t** patterns = NULL;
    size_t pattern_count = 0;
    err = generate_rules_for_clusters(miner, vectors, vector_ids, count, labels, num_clusters,
                                       &patterns, &pattern_count);

    AGENTOS_FREE(labels);
    if (centroids) AGENTOS_FREE(centroids);

    agentos_mutex_unlock(miner->lock);

    if (err != AGENTOS_SUCCESS) {
        if (patterns) {
            for (size_t i = 0; i < pattern_count; i++) agentos_pattern_free(patterns[i]);
            AGENTOS_FREE(patterns);
        }
        return err;
    }

    *out_patterns = patterns;
    *out_count = pattern_count;
    return AGENTOS_SUCCESS;
}

/* ==================== 自动挖掘线程 ==================== */

static void* auto_miner_thread_func(void* arg) {
    agentos_layer4_pattern_t* miner = (agentos_layer4_pattern_t*)arg;

    while (miner->auto_running) {
        // 等待配置的间�?
        sleep(miner->manager.mining_interval_sec);

        if (!miner->auto_running) break;

        // 需要获取向量数�?
        if (!miner->get_vectors_func) {
            AGENTOS_LOG_WARN("No vector source set for auto mining");
            continue;
        }

        float* vectors = NULL;
        char** ids = NULL;
        size_t count = 0;
        agentos_error_t err = miner->get_vectors_func(miner->data_source_ctx, &vectors, &ids, &count);
        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_ERROR("Failed to get vectors for auto mining");
            continue;
        }

        if (count == 0) {
            if (vectors) AGENTOS_FREE(vectors);
            if (ids) {
                for (size_t i = 0; i < count; i++) AGENTOS_FREE(ids[i]);
                AGENTOS_FREE(ids);
            }
            continue;
        }

        agentos_pattern_t** patterns = NULL;
        size_t pattern_count = 0;
        err = agentos_pattern_miner_mine(miner, vectors, (const char**)ids, count, &patterns, &pattern_count);
        if (err == AGENTOS_SUCCESS && pattern_count > 0) {
            AGENTOS_LOG_INFO("Auto mining discovered %zu patterns", pattern_count);
            // 这里可以通知进化委员会或存储模式
            // 暂不实现
            agentos_patterns_free(patterns, pattern_count);
        }

        AGENTOS_FREE(vectors);
        for (size_t i = 0; i < count; i++) AGENTOS_FREE(ids[i]);
        AGENTOS_FREE(ids);
    }
    return NULL;
}

agentos_error_t agentos_pattern_miner_start_auto(agentos_layer4_pattern_t* miner) {
    if (!miner) return AGENTOS_EINVAL;
    if (miner->manager.mining_interval_sec == 0) {
        AGENTOS_LOG_WARN("Auto mining interval is 0, not starting thread");
        return AGENTOS_SUCCESS;
    }
    if (miner->auto_running) return AGENTOS_SUCCESS;

    miner->auto_running = 1;
    if (pthread_create(&miner->auto_thread, NULL, auto_miner_thread_func, miner) != 0) {
        miner->auto_running = 0;
        AGENTOS_LOG_ERROR("Failed to create auto mining thread");
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

void agentos_pattern_miner_stop_auto(agentos_layer4_pattern_t* miner) {
    if (!miner || !miner->auto_running) return;
    miner->auto_running = 0;
    pthread_join(miner->auto_thread, NULL);
}

/* ==================== 设置数据源回�?==================== */

void agentos_pattern_miner_set_data_source(
    agentos_layer4_pattern_t* miner,
    void* ctx,
    agentos_error_t (*get_vectors)(void*, float**, char***, size_t*)) {

    if (!miner) return;
    agentos_mutex_lock(miner->lock);
    miner->data_source_ctx = ctx;
    miner->get_vectors_func = get_vectors;
    agentos_mutex_unlock(miner->lock);
}
