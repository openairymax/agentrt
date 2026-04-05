/**
 * @file energy.c
 * @brief 能量函数计算（用于吸引子网络和遗忘）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/retrieval.h"
#include "../include/layer2_feature.h"
#include "../include/forgetting.h"
#include "agentos.h"
#include <math.h>

float agentos_energy_hopfield(
    const float* state,
    const float* patterns,
    size_t n,
    size_t dim) {

    // Hopfield 能量: E = -0.5 * Σ_μ ( (s·m^μ)^2 )
    float energy = 0.0f;
    for (size_t mu = 0; mu < n; mu++) {
        const float* pat = patterns + mu * dim;
        float dot = 0.0f;
        for (size_t i = 0; i < dim; i++) {
            dot += pat[i] * state[i];
        }
        energy += dot * dot;
    }
    return -0.5f * energy;
}

float agentos_energy_memory(
    agentos_forgetting_engine_t* forgetting,
    const char* record_id) {

    if (!forgetting || !record_id) return 0.0f;
    float weight = 0.0f;
    if (agentos_forgetting_get_weight(forgetting, record_id, &weight) == AGENTOS_SUCCESS) {
        // 能量 = 1 - 权重（权重高则能量低，稳定）
        return 1.0f - weight;
    }
    return 0.5f; // 默认
}
