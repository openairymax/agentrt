/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * sanitize_level.h - Canonical Sanitize Level Type Definition
 *
 * Single source of truth for sanitize_level_t across all modules.
 * All modules MUST include this file instead of defining sanitize_level_t locally.
 *
 * Design Principles (per Engineering Standards Manual v13.0):
 * - TYPE-001 fix: Eliminates redefinition conflict between
 *   cupolas/sanitizer.h (enum) and daemon_security.h (typedef int)
 * - DRY Principle: One definition, used everywhere
 */

#ifndef SANITIZE_LEVEL_H
#define SANITIZE_LEVEL_H

#include <stdint.h>

/**
 * @brief Input sanitization strictness level
 *
 * Usage:
 * - STRICT  = Maximum sanitization, reject anything suspicious
 * - NORMAL  = Balanced sanitization for typical agent interactions
 * - RELAXED = Minimal sanitization for trusted internal channels
 */
typedef enum sanitize_level {
    SANITIZE_LEVEL_STRICT = 0,
    SANITIZE_LEVEL_NORMAL,
    SANITIZE_LEVEL_RELAXED
} sanitize_level_t;

#endif /* SANITIZE_LEVEL_H */
