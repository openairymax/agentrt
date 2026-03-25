/**
 * @file persistence.c
 * @brief L4 模式层持久性计算器（基于Ripser）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer4_pattern.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_RIPSER
#include <ripser.h>
#endif

struct agentos_persistence_calculator {
    double noise_factor;
    int max_dim;
    agentos_mutex_t* lock;
};

typedef struct {
    agentos_persistence_feature_t** features;
    size_t count;
    size_t capacity;
    // From data intelligence emerges. by spharx
} feature_collector_t;

static void collector_add(feature_collector_t* col, int dim, double birth, double death) {
    if (col->count >= col->capacity) {
        col->capacity = (col->capacity == 0) ? 16 : col->capacity * 2;
        agentos_persistence_feature_t** new_f = (agentos_persistence_feature_t**)realloc(
            col->features, col->capacity * sizeof(agentos_persistence_feature_t*));
        if (!new_f) return;
        col->features = new_f;
    }
    agentos_persistence_feature_t* f = (agentos_persistence_feature_t*)malloc(sizeof(agentos_persistence_feature_t));
    if (!f) return;
    f->dimension = dim;
    f->birth = birth;
    f->death = death;
    f->persistence = death - birth;
    f->confidence = 1.0;
    col->features[col->count++] = f;
}

#ifdef HAVE_RIPSER
static void ripser_callback(double birth, double death, int dim, void* user) {
    feature_collector_t* col = (feature_collector_t*)user;
    collector_add(col, dim, birth, death);
}
#endif

agentos_error_t agentos_persistence_calculator_create(
    const void* config,
    agentos_persistence_calculator_t** out_calc) {

    if (!out_calc) return AGENTOS_EINVAL;
    agentos_persistence_calculator_t* calc = (agentos_persistence_calculator_t*)calloc(1, sizeof(agentos_persistence_calculator_t));
    if (!calc) return AGENTOS_ENOMEM;

    calc->noise_factor = 3.0;
    calc->max_dim = 2;
    calc->lock = agentos_mutex_create();
    if (!calc->lock) {
        free(calc);
        return AGENTOS_ENOMEM;
    }

    *out_calc = calc;
    return AGENTOS_SUCCESS;
}

void agentos_persistence_calculator_destroy(agentos_persistence_calculator_t* calc) {
    if (!calc) return;
    if (calc->lock) agentos_mutex_destroy(calc->lock);
    free(calc);
}

agentos_error_t agentos_persistence_calculator_compute(
    agentos_persistence_calculator_t* calc,
    const float* distance_matrix,
    size_t n,
    agentos_persistence_feature_t*** out_features,
    size_t* out_count) {

    if (!calc || !distance_matrix || n == 0 || !out_features || !out_count)
        return AGENTOS_EINVAL;

    feature_collector_t collector = {0};

#ifdef HAVE_RIPSER
    // 配置Ripser
    ripser_parameters params;
    params.coeff = 2;
    params.maxdim = calc->max_dim;
    params.threshold = 1e30;
    params.do_cocycles = 0;
    params.do_barcodes = 0;

    // 调用Ripser
    ripser_diagrams(distance_matrix, n, &params, ripser_callback, &collector);
#else
    // 无Ripser时返回空结果
    *out_features = NULL;
    *out_count = 0;
    return AGENTOS_SUCCESS;
#endif

    *out_features = collector.features;
    *out_count = collector.count;
    return AGENTOS_SUCCESS;
}

void agentos_persistence_features_free(
    agentos_persistence_feature_t** features,
    size_t count) {

    if (!features) return;
    for (size_t i = 0; i < count; i++) free(features[i]);
    free(features);
}

double agentos_persistence_noise_threshold(
    agentos_persistence_calculator_t* calc,
    const float* distances,
    size_t count) {

    if (!calc || !distances || count == 0) return 0.1;

    double sum = 0;
    for (size_t i = 0; i < count; i++) sum += distances[i];
    double mean = sum / count;
    double var = 0;
    for (size_t i = 0; i < count; i++) var += (distances[i] - mean) * (distances[i] - mean);
    double std = sqrt(var / count);
    return mean + calc->noise_factor * std;
}