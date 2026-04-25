/**
 * @file confidence_calibrator.h
 * @brief 置信度校准器 - IMP-04认知层双思考功能
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供多维度偏差校准和指数衰减算法，替代启发式评分。
 */

#ifndef AGENTOS_CONFIDENCE_CALIBRATOR_H
#define AGENTOS_CONFIDENCE_CALIBRATOR_H

#include "agentos.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CC_MAX_DIMENSIONS 5
#define CC_DEFAULT_DECAY_FACTOR 0.95
#define CC_BIAS_THRESHOLD 0.05

typedef enum {
    CC_DIM_ACCURACY = 0,
    CC_DIM_RELEVANCE = 1,
    CC_DIM_COMPLETENESS = 2,
    CC_DIM_CONSISTENCY = 3,
    CC_DIM_CONFIDENCE = 4,
    CC_DIM_COUNT = 5
} cc_dimension_t;

typedef struct {
    double historical_bias[CC_MAX_DIMENSIONS];
    double calibration_factors[CC_MAX_DIMENSIONS];
    uint32_t sample_count[CC_MAX_DIMENSIONS];
    double decay_factor;
    double recent_raw[CC_MAX_DIMENSIONS][5];
    double recent_calibrated[CC_MAX_DIMENSIONS][5];
    uint32_t recent_index[CC_MAX_DIMENSIONS];
} confidence_calibrator_t;

confidence_calibrator_t* confidence_calibrator_create(double decay_factor);

void confidence_calibrator_destroy(confidence_calibrator_t* calibrator);

double confidence_calibrator_calibrate(
    confidence_calibrator_t* calibrator,
    double raw_score,
    cc_dimension_t dimension);

void confidence_calibrator_update(
    confidence_calibrator_t* calibrator,
    double actual_accuracy,
    double predicted_confidence,
    cc_dimension_t dimension);

double confidence_calibrator_get_bias(
    confidence_calibrator_t* calibrator,
    cc_dimension_t dimension);

int confidence_calibrator_check_convergence(
    confidence_calibrator_t* calibrator,
    cc_dimension_t dimension,
    double threshold);

void confidence_calibrator_reset(confidence_calibrator_t* calibrator);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CONFIDENCE_CALIBRATOR_H */
