/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * fuzz_sanitizer.c - Input Sanitizer Fuzz Testing
 */

/**
 * @file fuzz_sanitizer.c
 * @brief Input Sanitizer Fuzz Testing
 * @author Spharx AgentOS Team
 * @date 2024
 *
 * Fuzz testing for input sanitizer using libFuzzer, covering:
 * - SQL injection detection
 * - XSS cross-site scripting detection
 * - Command injection detection
 * - Path traversal detection
 * - Special character handling
 */

#include "../../src/sanitizer/sanitizer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_SIZE 4096
#define MAX_OUTPUT_SIZE 4096
