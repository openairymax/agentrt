/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cupolas_metrics.c - cupolas Metrics Module Unit Tests
 */

/**
 * @file test_cupolas_metrics.c
 * @brief cupolas Metrics Module Unit Tests
 * @author Spharx
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/cupolas_metrics.h"
#include "../../src/platform/platform.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) do { printf("[FAIL] %s: %s\n", name, msg); return 1; } while(0)
