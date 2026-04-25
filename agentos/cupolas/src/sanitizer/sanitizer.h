/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * sanitizer.h - Input Sanitizer Public Interface: Injection Attack Prevention
 *
 * Design Principles:
 * - Whitelist First: Only allow known-safe patterns
 * - Defense in Depth: Rules + length + encoding checks
 * - Configurable: Custom rules support
 */

#ifndef CUPOLAS_SANITIZER_H
#define CUPOLAS_SANITIZER_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sanitize Result */
typedef enum sanitize_result {
    SANITIZE_OK = 0,
    SANITIZE_MODIFIED,
    SANITIZE_REJECTED,
    SANITIZE_ERROR
} sanitize_result_t;

/* Sanitize Level - canonical definition from commons/types/ */
#include <sanitize_level.h>

/* Sanitize Context */
typedef struct sanitize_context {
    const char* agent_id;
    const char* input_type;
    sanitize_level_t level;
    size_t max_length;
    bool allow_html;
    bool allow_sql;
    bool allow_shell;
    bool allow_path;
} sanitize_context_t;

/* Sanitizer Handle */
typedef struct sanitizer sanitizer_t;

/**
 * @brief Create sanitizer
 * @param[in] rules_path Rules file path (optional, may be NULL)
 * @return Sanitizer handle, NULL on failure
 * @post On success, caller owns the returned handle
 * @note Thread-safe: Yes
 * @reentrant No (create/destroy must be paired)
 * @ownership Returned handle: caller owns, must call sanitizer_destroy
 */
sanitizer_t* sanitizer_create(const char* rules_path);

/**
 * @brief Destroy sanitizer
 * @param[in] sanitizer Sanitizer handle (must not be NULL)
 * @pre Handle was created by sanitizer_create
 * @post All resources are released
 * @note Thread-safe: No, ensure no other threads access sanitizer
 * @reentrant No
 * @ownership sanitizer: caller transfers ownership
 */
void sanitizer_destroy(sanitizer_t* sanitizer);

/**
 * @brief Sanitize input
 * @param[in] sanitizer Sanitizer handle (must not be NULL)
 * @param[in] input Input string to sanitize (must not be NULL)
 * @param[out] output Output buffer (must not be NULL)
 * @param[in] output_size Output buffer size in bytes
 * @param[in] ctx Sanitize context (may be NULL for default)
 * @return Sanitize result
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership input: caller retains; output: callee writes, caller owns buffer; ctx: caller retains
 */
sanitize_result_t sanitizer_sanitize(sanitizer_t* sanitizer,
                                      const char* input,
                                      char* output,
                                      size_t output_size,
                                      const sanitize_context_t* ctx);

/**
 * @brief Check if input is safe
 * @param[in] sanitizer Sanitizer handle (must not be NULL)
 * @param[in] input Input string to check (must not be NULL)
 * @param[in] ctx Sanitize context (may be NULL for default)
 * @return true if safe, false otherwise
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership input, ctx: caller retains ownership
 */
bool sanitizer_is_safe(sanitizer_t* sanitizer,
                       const char* input,
                       const sanitize_context_t* ctx);

/**
 * @brief Escape HTML special characters
 * @param[in] input Input string (must not be NULL)
 * @param[out] output Output buffer (must not be NULL)
 * @param[in] output_size Output buffer size in bytes
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership input: caller retains; output: callee writes, caller owns buffer
 */
int sanitizer_escape_html(const char* input, char* output, size_t output_size);

/**
 * @brief Escape SQL special characters
 * @param[in] input Input string (must not be NULL)
 * @param[out] output Output buffer (must not be NULL)
 * @param[in] output_size Output buffer size in bytes
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership input: caller retains; output: callee writes, caller owns buffer
 */
int sanitizer_escape_sql(const char* input, char* output, size_t output_size);

/**
 * @brief Escape Shell special characters
 * @param[in] input Input string (must not be NULL)
 * @param[out] output Output buffer (must not be NULL)
 * @param[in] output_size Output buffer size in bytes
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership input: caller retains; output: callee writes, caller owns buffer
 */
int sanitizer_escape_shell(const char* input, char* output, size_t output_size);

/**
 * @brief Escape path special characters
 * @param[in] input Input string (must not be NULL)
 * @param[out] output Output buffer (must not be NULL)
 * @param[in] output_size Output buffer size in bytes
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership input: caller retains; output: callee writes, caller owns buffer
 */
int sanitizer_escape_path(const char* input, char* output, size_t output_size);

/**
 * @brief Get default sanitize context
 * @param[out] ctx Context output (must not be NULL)
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership ctx: callee writes, caller owns
 */
void sanitizer_default_context(sanitize_context_t* ctx);

/**
 * @brief Add sanitization rule
 * @param[in] sanitizer Sanitizer handle (must not be NULL)
 * @param[in] pattern Match pattern (regex), must not be NULL
 * @param[in] replacement Replacement string (NULL to reject)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership pattern, replacement: caller retains ownership
 */
int sanitizer_add_rule(sanitizer_t* sanitizer,
                       const char* pattern,
                       const char* replacement);

/**
 * @brief Clear all rules
 * @param[in] sanitizer Sanitizer handle (must not be NULL)
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @post All custom rules are removed, default rules remain
 */
void sanitizer_clear_rules(sanitizer_t* sanitizer);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_SANITIZER_H */
