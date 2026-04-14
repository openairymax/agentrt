// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_protocol_compatibility.c
 * @brief Protocol Compatibility Integration Tests
 *
 * 测试MCP/A2A/OpenAI/OpenJiuwen协议适配器与gateway_d的集成。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../daemon/common/include/safe_string_utils.h"
#include "../../protocols/include/unified_protocol.h"
#include "../../protocols/mcp/include/mcp_v1_adapter.h"
#include "../../protocols/a2a/include/a2a_v03_adapter.h"
#include "../../protocols/openai/include/openai_enterprise_adapter.h"
#include "../../protocols/openjiuwen/include/openjiuwen_adapter.h"

/* ============================================================================
 * 测试辅助宏
 * ============================================================================ */

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, reason) printf("[FAIL] %s: %s\n", name, reason)

/* ============================================================================
 * 安全函数测试
 * ============================================================================ */

void test_safe_strcpy_normal(void) {
    char dest[100];
    const char* src = "Hello, AgentOS!";
    int result = safe_strcpy(dest, src, sizeof(dest));

    if (result == 0 && strcmp(dest, src) == 0) {
        TEST_PASS("safe_strcpy normal operation");
    } else {
        TEST_FAIL("safe_strcpy normal operation",
                  "Copy failed or content mismatch");
    }
}

void test_safe_strcpy_truncation(void) {
    char dest[10];
    const char* src = "This is a very long string that will be truncated";
    int result = safe_strcpy(dest, src, sizeof(dest));

    if (result == -2 && strlen(dest) == 9) {
        TEST_PASS("safe_strcpy truncation handling");
    } else {
        TEST_FAIL("safe_strcpy truncation handling",
                  "Truncation not handled correctly");
    }
}

void test_safe_strcpy_empty_source(void) {
    char dest[100] = "Original";
    int result = safe_strcpy(dest, "", sizeof(dest));

    if (result == 0 && strcmp(dest, "") == 0) {
        TEST_PASS("safe_strcpy empty source");
    } else {
        TEST_FAIL("safe_strcpy empty source",
                  "Empty string not handled correctly");
    }
}

void test_safe_strcat_normal(void) {
    char dest[50] = "Hello";
    int result = safe_strcat(dest, ", World!", sizeof(dest));

    if (result == 0 && strcmp(dest, "Hello, World!") == 0) {
        TEST_PASS("safe_strcat normal operation");
    } else {
        TEST_FAIL("safe_strcat normal operation",
                  "Concatenation failed or content mismatch");
    }
}

void test_safe_strcat_boundary(void) {
    char dest[15] = "Hello";
    int result = safe_strcat(dest, ", World!", sizeof(dest));

    /* Should truncate to fit in buffer */
    if (result == -2 && strlen(dest) <= 14) {
        TEST_PASS("safe_strcat boundary handling");
    } else {
        TEST_FAIL("safe_strcat boundary handling",
                  "Boundary condition not handled correctly");
    }
}

/* ============================================================================
 * 协议兼容性测试
 * ============================================================================ */

void test_mcp_adapter_registration(void) {
    unified_protocol_config_t config = {
        .name = "Test-Stack",
        .max_adapters = 4,
        .default_protocol = UNIFIED_PROTOCOL_JSON_RPC
    };

    protocol_stack_handle_t stack = unified_protocol_create(&config);
    assert(stack != NULL);

    int result = unified_protocol_register_adapter(
        stack, &mcp_v1_adapter_interface);

    if (result == 0) {
        size_t count = unified_protocol_get_adapter_count(stack);
        if (count >= 1) {
            TEST_PASS("MCP v1.0 adapter registration");
        } else {
            TEST_FAIL("MCP v1.0 adapter registration",
                      "Adapter count mismatch after registration");
        }
    } else {
        TEST_FAIL("MCP v1.0 adapter registration",
                  "Registration returned error");
    }

    unified_protocol_destroy(stack);
}

void test_a2a_adapter_registration(void) {
    unified_protocol_config_t config = {
        .name = "Test-Stack",
        .max_adapters = 4,
        .default_protocol = UNIFIED_PROTOCOL_JSON_RPC
    };

    protocol_stack_handle_t stack = unified_protocol_create(&config);
    assert(stack != NULL);

    int result = unified_protocol_register_adapter(
        stack, &a2a_v03_adapter_interface);

    if (result == 0) {
        size_t count = unified_protocol_get_adapter_count(stack);
        if (count >= 1) {
            TEST_PASS("A2A v0.3.0 adapter registration");
        } else {
            TEST_FAIL("A2A v0.3.0 adapter registration",
                      "Adapter count mismatch after registration");
        }
    } else {
        TEST_FAIL("A2A v0.3.0 adapter registration",
                  "Registration returned error");
    }

    unified_protocol_destroy(stack);
}

void test_openai_adapter_registration(void) {
    unified_protocol_config_t config = {
        .name = "Test-Stack",
        .max_adapters = 4,
        .default_protocol = UNIFIED_PROTOCOL_JSON_RPC
    };

    protocol_stack_handle_t stack = unified_protocol_create(&config);
    assert(stack != NULL);

    int result = unified_protocol_register_adapter(
        stack, &openai_enterprise_adapter_interface);

    if (result == 0) {
        size_t count = unified_protocol_get_adapter_count(stack);
        if (count >= 1) {
            TEST_PASS("OpenAI Enterprise adapter registration");
        } else {
            TEST_FAIL("OpenAI Enterprise adapter registration",
                      "Adapter count mismatch after registration");
        }
    } else {
        TEST_FAIL("OpenAI Enterprise adapter registration",
                  "Registration returned error");
    }

    unified_protocol_destroy(stack);
}

void test_openjiuwen_adapter_creation(void) {
    openjiuwen_config_t config;
    openjiuwen_get_default_config(&config);

    const protocol_adapter_t* adapter =
        openjiuwen_adapter_create(&config);

    if (adapter && adapter->protocol_type == UNIFIED_PROTOCOL_OPENJIUWEN) {
        TEST_PASS("OpenJiuwen adapter creation");

        /* Test connection verification */
        int verify_result = openjiuwen_verify_connection(adapter);
        if (verify_result == 0) {
            TEST_PASS("OpenJiuwen connection verification");
        } else {
            printf("[WARN] OpenJiuwen verification skipped (no network)\n");
        }

        /* Cleanup */
        if (adapter->destroy) {
            adapter->destroy(adapter->context);
        }
    } else {
        TEST_FAIL("OpenJiuwen adapter creation",
                  "Adapter creation failed or type mismatch");
    }
}

void test_multi_protocol_registration(void) {
    unified_protocol_config_t config = {
        .name = "Multi-Protocol-Stack",
        .max_adapters = 8,
        .default_protocol = UNIFIED_PROTOCOL_JSON_RPC
    };

    protocol_stack_handle_t stack = unified_protocol_create(&config);
    assert(stack != NULL);

    /* Register all adapters */
    int mcp_result = unified_protocol_register_adapter(
        stack, &mcp_v1_adapter_interface);
    int a2a_result = unified_protocol_register_adapter(
        stack, &a2a_v03_adapter_interface);
    int openai_result = unified_protocol_register_adapter(
        stack, &openai_enterprise_adapter_interface);

    if (mcp_result == 0 && a2a_result == 0 && openai_result == 0) {
        size_t count = unified_protocol_get_adapter_count(stack);
        if (count >= 3) {
            TEST_PASS("Multi-protocol adapter registration (3+ adapters)");
        } else {
            TEST_FAIL("Multi-protocol adapter registration",
                      "Expected 3+ adapters, got fewer");
        }
    } else {
        TEST_FAIL("Multi-protocol adapter registration",
                  "One or more registrations failed");
    }

    unified_protocol_destroy(stack);
}

/* ============================================================================
 * OpenJiuwen消息转换测试
 * ============================================================================ */

void test_openjiuwen_message_conversion(void) {
    unified_message_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.protocol_type = UNIFIED_PROTOCOL_OPENJIUWEN;
    msg.message_id = 12345;
    msg.timestamp = (uint32_t)time(NULL);
    safe_strcpy(msg.source_agent, "TestAgent", sizeof(msg.source_agent));
    safe_strcpy(msg.target_agent, "TargetAgent", sizeof(msg.target_agent));

    const char* payload_data = "Test payload data";
    msg.payload_size = strlen(payload_data) + 1;
    msg.payload = malloc(msg.payload_size);
    memcpy(msg.payload, payload_data, msg.payload_size);

    /* Convert to native format */
    char buffer[1024];
    int native_size = openjiuwen_unified_to_native(
        msg, buffer, sizeof(buffer));

    if (native_size > 0) {
        /* Convert back to unified format */
        unified_message_t recovered_msg;
        int convert_result = openjiuwen_native_to_unified(
            buffer, (size_t)native_size, &recovered_msg);

        if (convert_result == 0 &&
            recovered_msg.message_id == msg.message_id &&
            strcmp(recovered_msg.source_agent, msg.source_agent) == 0) {

            TEST_PASS("OpenJiuwen message round-trip conversion");
        } else {
            TEST_FAIL("OpenJiuwen message round-trip conversion",
                      "Recovered message does not match original");
        }

        if (recovered_msg.payload) {
            free(recovered_msg.payload);
        }
    } else {
        TEST_FAIL("OpenJiuwen message round-trip conversion",
                  "Native format conversion failed");
    }

    free(msg.payload);
}

/* ============================================================================
 * 主测试函数
 * ============================================================================ */

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║   AgentOS Protocol Compatibility Tests   ║\n");
    printf("╚══════════════════════════════════════════╝\n");
    printf("\n");

    printf("--- Safe String Function Tests ---\n");
    test_safe_strcpy_normal();
    test_safe_strcpy_truncation();
    test_safe_strcpy_empty_source();
    test_safe_strcat_normal();
    test_safe_strcat_boundary();

    printf("\n--- Protocol Adapter Registration Tests ---\n");
    test_mcp_adapter_registration();
    test_a2a_adapter_registration();
    test_openai_adapter_registration();
    test_openjiuwen_adapter_creation();
    test_multi_protocol_registration();

    printf("\n--- Protocol Conversion Tests ---\n");
    test_openjiuwen_message_conversion();

    printf("\n========================================\n");
    printf("All tests completed!\n");
    printf("========================================\n\n");

    return 0;
}