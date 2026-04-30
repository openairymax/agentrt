/**
 * @file confidence_calibrator.c
 * @brief 置信度校准器实现 - IMP-04认知层双思考功能
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "confidence_calibrator.h"
#include "memory_compat.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

confidence_calibrator_t* confidence_calibrator_create(double decay_factor) {
    confidence_calibrator_t* cb = (confidence_calibrator_t*)AGENTOS_CALLOC(1, sizeof(confidence_calibrator_t));
    if (!cb) return NULL;

    cb->decay_factor = (decay_factor > 0.0 && decay_factor < 1.0) ? decay_factor : CC_DEFAULT_DECAY_FACTOR;

    for (int i = 0; i < CC_MAX_DIMENSIONS; i++) {
        cb->historical_bias[i] = 0.0;
        cb->calibration_factors[i] = 1.0;
        cb->sample_count[i] = 0;
        cb->recent_index[i] = 0;
        for (int j = 0; j < 5; j++) {
            cb->recent_raw[i][j] = 0.0;
            cb->recent_calibrated[i][j] = 0.0;
        }
    }

    return cb;
}

void confidence_calibrator_destroy(confidence_calibrator_t* calibrator) {
    if (calibrator) AGENTOS_FREE(calibrator);
}

double confidence_calibrator_calibrate(
    confidence_calibrator_t* calibrator,
    double raw_score,
    cc_dimension_t dimension) {
    if (!calibrator) return raw_score;
    if (dimension < 0 || dimension >= CC_DIM_COUNT) return raw_score;

    double bias = calibrator->historical_bias[dimension];
    double factor = calibrator->calibration_factors[dimension];
    double calibrated = raw_score * (1.0 - bias) * factor;

    if (calibrated < 0.0) calibrated = 0.0;
    if (calibrated > 1.0) calibrated = 1.0;

    uint32_t idx = calibrator->recent_index[dimension];
    calibrator->recent_raw[dimension][idx % 5] = raw_score;
    calibrator->recent_calibrated[dimension][idx % 5] = calibrated;
    calibrator->recent_index[dimension] = idx + 1;

    return calibrated;
}

void confidence_calibrator_update(
    confidence_calibrator_t* calibrator,
    double actual_accuracy,
    double predicted_confidence,
    cc_dimension_t dimension) {
    if (!calibrator) return;
    if (dimension < 0 || dimension >= CC_DIM_COUNT) return;

    double error = fabs(actual_accuracy - predicted_confidence);

    calibrator->historical_bias[dimension] =
        calibrator->decay_factor * calibrator->historical_bias[dimension] +
        (1.0 - calibrator->decay_factor) * error;

    if (error > 0.1) {
        calibrator->calibration_factors[dimension] *= 0.95;
    } else if (error < 0.05) {
        double new_factor = calibrator->calibration_factors[dimension] * 1.05;
        if (new_factor < 1.1) {
            calibrator->calibration_factors[dimension] = new_factor;
        } else {
            calibrator->calibration_factors[dimension] = 1.1;
        }
    }

    calibrator->sample_count[dimension]++;
}

double confidence_calibrator_get_bias(
    confidence_calibrator_t* calibrator,
    cc_dimension_t dimension) {
    if (!calibrator || dimension < 0 || dimension >= CC_DIM_COUNT) return 0.0;
    return calibrator->historical_bias[dimension];
}

int confidence_calibrator_check_convergence(
    confidence_calibrator_t* calibrator,
    cc_dimension_t dimension,
    double threshold) {
    if (!calibrator || dimension < 0 || dimension >= CC_DIM_COUNT) return 0;

    uint32_t idx = calibrator->recent_index[dimension];
    if (idx < 5) return 0;

    double max_diff = 0.0;
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            double diff = fabs(calibrator->recent_calibrated[dimension][i] -
                               calibrator->recent_calibrated[dimension][j]);
            if (diff > max_diff) max_diff = diff;
        }
    }

    return max_diff < threshold ? 1 : 0;
}

void confidence_calibrator_reset(confidence_calibrator_t* calibrator) {
    if (!calibrator) return;

    for (int i = 0; i < CC_MAX_DIMENSIONS; i++) {
        calibrator->historical_bias[i] = 0.0;
        calibrator->calibration_factors[i] = 1.0;
        calibrator->sample_count[i] = 0;
        calibrator->recent_index[i] = 0;
        for (int j = 0; j < 5; j++) {
            calibrator->recent_raw[i][j] = 0.0;
            calibrator->recent_calibrated[i][j] = 0.0;
        }
    }
}
