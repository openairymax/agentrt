/**
 * @file clustering.h
 * @brief L4 聚类引擎接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_CLUSTERING_H
#define AGENTOS_CLUSTERING_H

#include "layer4_pattern.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct agentos_clustering_engine {
    char method[32];
    void* params;
    agentos_mutex_t* lock;
};
typedef struct agentos_clustering_engine agentos_clustering_engine_t;

agentos_error_t agentos_clustering_engine_create(
    const char* method,
    const char* manager,
    agentos_clustering_engine_t** out_engine);

void agentos_clustering_engine_destroy(agentos_clustering_engine_t* engine);

agentos_error_t agentos_clustering_engine_cluster(
    agentos_clustering_engine_t* engine,
    const float* vectors,
    size_t count,
    int** out_labels,
    float** out_centroids,
    int* out_num_clusters);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CLUSTERING_H */
