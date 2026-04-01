/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * sanitizer_rules.h - Sanitizer Rules Manager Internal Interface
 */

#ifndef CUPOLAS_SANITIZER_RULES_H
#define CUPOLAS_SANITIZER_RULES_H

#include "../platform/platform.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sanitizer rules manager structure
 * 
 * Design principles:
 * - Pattern-based rule matching
 * - Priority-based rule application
 * - Thread-safe rule management
 */
typedef struct sanitizer_rules sanitizer_rules_t;

/**
 * @brief Create rules manager and load rules from file
 * @param[in] rules_path Path to rules configuration file
 * @return Rules manager handle, NULL on failure
 * @note Thread-safe: Safe to call from multiple threads (initialization only)
 * @reentrant No
 * @ownership Returns owned pointer: caller must call sanitizer_rules_destroy()
 * @ownership rules_path: caller retains ownership
 */
sanitizer_rules_t* sanitizer_rules_create(const char* rules_path);

/**
 * @brief Destroy rules manager and free all resources
 * @param[in] rules Rules manager handle (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership rules: transferred to this function, will be freed
 */
void sanitizer_rules_destroy(sanitizer_rules_t* rules);

/**
 * @brief Add a sanitization rule
 * @param[in] rules Rules manager handle
 * @param[in] pattern Match pattern (regex or literal)
 * @param[in] replacement Replacement string
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 * @ownership pattern and replacement: caller retains ownership
 */
int sanitizer_rules_add(sanitizer_rules_t* rules, const char* pattern, const char* replacement);

/**
 * @brief Apply rules to sanitize input string
 * @param[in] rules Rules manager handle
 * @param[in] input Input string to sanitize
 * @param[out] output Output buffer for sanitized result
 * @param[in] output_size Size of output buffer
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership input: caller retains ownership
 * @ownership output: caller provides buffer, function writes to it
 */
int sanitizer_rules_apply(sanitizer_rules_t* rules, const char* input, char* output, size_t output_size);

/**
 * @brief Clear all rules
 * @param[in] rules Rules manager handle
 * @note Thread-safe: Safe to call from multiple threads (but not concurrently with other operations)
 * @reentrant No
 */
void sanitizer_rules_clear(sanitizer_rules_t* rules);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_SANITIZER_RULES_H */
