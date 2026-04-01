/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * fuzz_permission.c - Permission System Fuzz Testing
 */

/**
 * @file fuzz_permission.c
 * @brief Permission System Fuzz Testing
 * @author Spharx
 * @date 2024
 *
 * Fuzz testing for permission system using libFuzzer, covering:
 * - Permission decision boundary conditions
 * - Rule parsing exception input
 * - Cache overflow
 * - Concurrent access
 */

#include "../../src/permission/permission.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ID_SIZE 256
#define MAX_CONTEXT_SIZE 1024
