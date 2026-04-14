/**
 * @file test_registry.c
 * @brief Tool 注册表单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "registry.h"

/**
 * @brief 测试注册表创建和销毁
 */
static void test_registry_create_destroy(void) {
    printf("  test_registry_create_destroy...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试工具注册
 */
static void test_registry_add(void) {
    printf("  test_registry_add...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "test_tool_001";
    meta.name = "Test Tool";
    meta.description = "A test tool for unit testing";
    meta.executable = "/usr/bin/echo";
    meta.timeout_sec = 30;
    meta.cacheable = 1;

    int ret = tool_registry_add(reg, &meta);
    assert(ret == 0);

    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试工具重复注册
 */
static void test_registry_add_duplicate(void) {
    printf("  test_registry_add_duplicate...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "duplicate_tool";
    meta.name = "Duplicate Tool";
    meta.executable = "/usr/bin/echo";

    int ret = tool_registry_add(reg, &meta);
    assert(ret == 0);

    ret = tool_registry_add(reg, &meta);
    assert(ret != 0);

    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试工具获取
 */
static void test_registry_get(void) {
    printf("  test_registry_get...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "get_test_tool";
    meta.name = "Get Test Tool";
    meta.description = "Tool for get testing";
    meta.executable = "/usr/bin/cat";

    tool_registry_add(reg, &meta);

    tool_metadata_t* retrieved = tool_registry_get(reg, "get_test_tool");
    assert(retrieved != NULL);
    assert(strcmp(retrieved->id, "get_test_tool") == 0);
    assert(strcmp(retrieved->name, "Get Test Tool") == 0);

    tool_metadata_free(retrieved);
    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试工具获取不存在
 */
static void test_registry_get_nonexistent(void) {
    printf("  test_registry_get_nonexistent...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    tool_metadata_t* retrieved = tool_registry_get(reg, "nonexistent_tool");
    assert(retrieved == NULL);

    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试工具移除
 */
static void test_registry_remove(void) {
    printf("  test_registry_remove...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "remove_test_tool";
    meta.name = "Remove Test Tool";
    meta.executable = "/usr/bin/ls";

    tool_registry_add(reg, &meta);

    int ret = tool_registry_remove(reg, "remove_test_tool");
    assert(ret == 0);

    tool_metadata_t* retrieved = tool_registry_get(reg, "remove_test_tool");
    assert(retrieved == NULL);

    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试工具移除不存在
 */
static void test_registry_remove_nonexistent(void) {
    printf("  test_registry_remove_nonexistent...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    int ret = tool_registry_remove(reg, "nonexistent_tool");
    assert(ret != 0);

    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试工具列表JSON
 */
static void test_registry_list_json(void) {
    printf("  test_registry_list_json...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    tool_metadata_t meta1;
    memset(&meta1, 0, sizeof(meta1));
    meta1.id = "json_tool_1";
    meta1.name = "JSON Tool 1";
    meta1.executable = "/usr/bin/echo";

    tool_metadata_t meta2;
    memset(&meta2, 0, sizeof(meta2));
    meta2.id = "json_tool_2";
    meta2.name = "JSON Tool 2";
    meta2.executable = "/usr/bin/cat";

    tool_registry_add(reg, &meta1);
    tool_registry_add(reg, &meta2);

    char* json = tool_registry_list_json(reg);
    assert(json != NULL);
    assert(strstr(json, "json_tool_1") != NULL);
    assert(strstr(json, "json_tool_2") != NULL);

    free(json);
    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试空注册表列表JSON
 */
static void test_registry_list_json_empty(void) {
    printf("  test_registry_list_json_empty...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    char* json = tool_registry_list_json(reg);
    assert(json != NULL);
    assert(strcmp(json, "[]") == 0);

    free(json);
    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试注册表空参数
 */
static void test_registry_null_param(void) {
    printf("  test_registry_null_param...\n");

    int ret = tool_registry_add(NULL, NULL, NULL);
    assert(ret != 0);

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = NULL;

    tool_registry_t* reg = tool_registry_create(NULL);
    ret = tool_registry_add(reg, &meta);
    assert(ret != 0);

    tool_metadata_t* retrieved = tool_registry_get(NULL, "test");
    assert(retrieved == NULL);

    retrieved = tool_registry_get(reg, NULL);
    assert(retrieved == NULL);

    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

/**
 * @brief 测试带参数的工具
 */
static void test_registry_tool_with_params(void) {
    printf("  test_registry_tool_with_params...\n");

    tool_registry_t* reg = tool_registry_create(NULL);
    assert(reg != NULL);

    tool_param_t params[2];
    memset(params, 0, sizeof(params));
    params[0].name = "input_file";
    params[0].schema = "{\"type\": \"string\"}";
    params[1].name = "output_file";
    params[1].schema = "{\"type\": \"string\"}";

    tool_metadata_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.id = "param_tool";
    meta.name = "Param Tool";
    meta.executable = "/usr/bin/cp";
    meta.params = params;
    meta.param_count = 2;

    int ret = tool_registry_add(reg, &meta);
    assert(ret == 0);

    tool_metadata_t* retrieved = tool_registry_get(reg, "param_tool");
    assert(retrieved != NULL);
    assert(retrieved->param_count == 2);
    assert(strcmp(retrieved->params[0].name, "input_file") == 0);
    assert(strcmp(retrieved->params[1].name, "output_file") == 0);

    tool_metadata_free(retrieved);
    tool_registry_destroy(reg);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Tool Registry Unit Tests\n");
    printf("=========================================\n");

    test_registry_create_destroy();
    test_registry_add();
    test_registry_add_duplicate();
    test_registry_get();
    test_registry_get_nonexistent();
    test_registry_remove();
    test_registry_remove_nonexistent();
    test_registry_list_json();
    test_registry_list_json_empty();
    test_registry_null_param();
    test_registry_tool_with_params();

    printf("\n✅ All tool registry tests PASSED\n");
    return 0;
}
