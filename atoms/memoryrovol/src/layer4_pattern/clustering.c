/**
 * @file clustering.c
 * @brief L4 模式层聚类引擎（使用HDBSCAN�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer4_pattern.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../bases/utils/memory/include/memory_compat.h"
#include "../../../bases/utils/string/include/string_compat.h"
#include <string.h>
#include <math.h>

#ifdef HAVE_HDBSCAN
#include <hdbscan/hdbscan.h>
#endif

struct agentos_clustering_engine {
    char method[32];
    void* params;
    agentos_mutex_t* lock;
};

agentos_error_t agentos_clustering_engine_create(
    const char* method,
    const char* manager,
    agentos_clustering_engine_t** out_engine) {
    // From data intelligence emerges. by spharx

    if (!method || !out_engine) return AGENTOS_EINVAL;

    agentos_clustering_engine_t* eng = (agentos_clustering_engine_t*)AGENTOS_CALLOC(1, sizeof(agentos_clustering_engine_t));
    if (!eng) return AGENTOS_ENOMEM;

    strncpy(eng->method, method, sizeof(eng->method) - 1);
    eng->lock = agentos_mutex_create();
    if (!eng->lock) {
        AGENTOS_FREE(eng);
        return AGENTOS_ENOMEM;
    }

    // 解析配置（这里简单处理，实际可用cJSON�?
    eng->params = AGENTOS_MALLOC(8); // 占位
    if (!eng->params) {
        agentos_mutex_destroy(eng->lock);
        AGENTOS_FREE(eng);
        return AGENTOS_ENOMEM;
    }

    *out_engine = eng;
    return AGENTOS_SUCCESS;
}

void agentos_clustering_engine_destroy(agentos_clustering_engine_t* engine) {
    if (!engine) return;
    if (engine->params) AGENTOS_FREE(engine->params);
    if (engine->lock) agentos_mutex_destroy(engine->lock);
    AGENTOS_FREE(engine);
}

agentos_error_t agentos_clustering_engine_cluster(
    agentos_clustering_engine_t* engine,
    const float* vectors,
    size_t count,
    int** out_labels,
    float** out_centroids,
    int* out_num_clusters) {

    if (!engine || !vectors || count == 0 || !out_labels || !out_num_clusters)
        return AGENTOS_EINVAL;

    size_t dim = 384; // 实际应从参数获取

#ifdef HAVE_HDBSCAN
    // 使用HDBSCAN�?
    hdbscan_input input;
    input.data = (double*)vectors; // 假设可以转换
    input.num_points = count;
    input.dim = dim;
    input.min_cluster_size = 5;
    input.min_samples = 1;

    hdbscan_output output;
    hdbscan_cluster(&input, &output);

    int* labels = (int*)AGENTOS_MALLOC(count * sizeof(int));
    if (!labels) {
        hdbscan_free_output(&output);
        return AGENTOS_ENOMEM;
    }
    memcpy(labels, output.labels, count * sizeof(int));

    // 计算聚类中心（可选，HDBSCAN不直接提供）
    float* centroids = NULL;
    int num_clusters = output.num_clusters;

    hdbscan_free_output(&output);
#else
    // 无HDBSCAN时，返回�?标签（单聚类�?
    int* labels = (int*)AGENTOS_MALLOC(count * sizeof(int));
    if (!labels) return AGENTOS_ENOMEM;
    for (size_t i = 0; i < count; i++) labels[i] = 0;
    float* centroids = NULL;
    int num_clusters = 1;
#endif

    *out_labels = labels;
    *out_centroids = centroids;
    *out_num_clusters = num_clusters;
    return AGENTOS_SUCCESS;
}