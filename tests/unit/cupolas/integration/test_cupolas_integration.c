/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_cupolas_integration.c - cupolas Module Integration Tests
 */

/**
 * @file test_cupolas_integration.c
 * @brief cupolas Module Integration Tests
 * @author Spharx AgentOS Team
 * @date 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/cupolas.h"

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)
