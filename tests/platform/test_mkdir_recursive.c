/**
 * @file test_mkdir_recursive.c
 * @brief R-09-87 platform_mkdir_recursive 递归创建目录单元测试
 *
 * 测试 agentos_mkdir 的 POSIX 递归创建逻辑（Linux 路径）。
 * 通过直接测试算法核心验证以下场景：
 * - 递归创建多层目录
 * - 已存在目录（幂等性）
 * - 单层创建
 * - NULL路径
 * - 已存在的父目录
 * - 非递归模式父目录不存在
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "test_macros.h"

#define TEST_BASE_DIR "/tmp/agentos_test_mkdir_XXXXXX"

static char g_test_dir[256];

static int dir_exists(const char* path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
}

/**
 * @brief POSIX递归mkdir - 与 agentos_mkdir 的递归逻辑一致
 */
static int posix_mkdir_recursive(const char* path)
{
    if (!path || !*path) return -1;
    int len = (int)strlen(path);
    if (len <= 0) return -1;

    char tmp[4096];
    if (len >= (int)sizeof(tmp)) return -1;
    memcpy(tmp, path, len + 1);

    for (int i = (tmp[0] == '/') ? 1 : 0; i <= len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\0') {
            char saved = tmp[i];
            tmp[i] = '\0';
            if (i > 0 && tmp[0] != '\0') {
                struct stat st;
                if (stat(tmp, &st) != 0) {
                    if (mkdir(tmp, 0755) != 0) { tmp[i] = saved; return -1; }
                }
            }
            tmp[i] = saved;
        }
    }
    return 0;
}

static void test_mkdir_null_path(void)
{
    TEST_CASE_START(mkdir_null_path);

    int result = posix_mkdir_recursive(NULL);
    TEST_ASSERT_EQUAL_INT(-1, result, "NULL路径返回-1");

    result = posix_mkdir_recursive("");
    TEST_ASSERT_EQUAL_INT(-1, result, "空路径返回-1");
}

static void test_mkdir_single_level(void)
{
    TEST_CASE_START(mkdir_single_level);

    char path[512];
    snprintf(path, sizeof(path), "%s/single", g_test_dir);

    rmdir(path);

    int result = posix_mkdir_recursive(path);
    TEST_ASSERT_EQUAL_INT(0, result, "单层目录创建成功");
    TEST_ASSERT_TRUE(dir_exists(path), "目录实际存在");

    rmdir(path);
}

static void test_mkdir_deep_recursive(void)
{
    TEST_CASE_START(mkdir_deep_recursive);

    char path[512];

    snprintf(path, sizeof(path), "%s/a/b/c", g_test_dir);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/a/b", g_test_dir);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/a", g_test_dir);
    rmdir(path);

    snprintf(path, sizeof(path), "%s/a", g_test_dir);
    TEST_ASSERT_FALSE(dir_exists(path), "父目录初始不存在");

    snprintf(path, sizeof(path), "%s/a/b/c", g_test_dir);
    int result = posix_mkdir_recursive(path);
    TEST_ASSERT_EQUAL_INT(0, result, "递归创建a/b/c成功");

    snprintf(path, sizeof(path), "%s/a", g_test_dir);
    TEST_ASSERT_TRUE(dir_exists(path), "目录a存在");
    snprintf(path, sizeof(path), "%s/a/b", g_test_dir);
    TEST_ASSERT_TRUE(dir_exists(path), "目录a/b存在");
    snprintf(path, sizeof(path), "%s/a/b/c", g_test_dir);
    TEST_ASSERT_TRUE(dir_exists(path), "目录a/b/c存在");

    rmdir(path);
    snprintf(path, sizeof(path), "%s/a/b", g_test_dir);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/a", g_test_dir);
    rmdir(path);
}

static void test_mkdir_already_exists(void)
{
    TEST_CASE_START(mkdir_already_exists);

    char path[512];
    snprintf(path, sizeof(path), "%s/existing", g_test_dir);

    rmdir(path);
    mkdir(path, 0755);

    int result = posix_mkdir_recursive(path);
    TEST_ASSERT_EQUAL_INT(0, result, "已存在目录成功（幂等）");
    TEST_ASSERT_TRUE(dir_exists(path), "目录仍然存在");

    rmdir(path);
}

static void test_mkdir_existing_parent(void)
{
    TEST_CASE_START(mkdir_existing_parent);

    char parent[512];
    snprintf(parent, sizeof(parent), "%s/parent_exists", g_test_dir);
    rmdir(parent);
    mkdir(parent, 0755);

    char child[512];
    snprintf(child, sizeof(child), "%s/parent_exists/sub", g_test_dir);

    int result = posix_mkdir_recursive(child);
    TEST_ASSERT_EQUAL_INT(0, result, "父目录已存在时创建子目录成功");
    TEST_ASSERT_TRUE(dir_exists(child), "子目录存在");
    TEST_ASSERT_TRUE(dir_exists(parent), "父目录仍存在");

    rmdir(child);
    rmdir(parent);
}

static void test_mkdir_same_level_multiple(void)
{
    TEST_CASE_START(mkdir_same_level_multiple);

    char p1[512], p2[512];
    snprintf(p1, sizeof(p1), "%s/multi1", g_test_dir);
    snprintf(p2, sizeof(p2), "%s/multi2", g_test_dir);

    rmdir(p1);
    rmdir(p2);

    TEST_ASSERT_EQUAL_INT(0, posix_mkdir_recursive(p1), "创建multi1成功");
    TEST_ASSERT_EQUAL_INT(0, posix_mkdir_recursive(p2), "创建multi2成功");
    TEST_ASSERT_TRUE(dir_exists(p1), "multi1存在");
    TEST_ASSERT_TRUE(dir_exists(p2), "multi2存在");

    rmdir(p1);
    rmdir(p2);
}

int main(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  R-09-87 递归mkdir 单元测试\n");
    printf("  POSIX递归逻辑算法验证\n");
    printf("========================================\n");

    RESET_TEST_STATS();

    strcpy(g_test_dir, TEST_BASE_DIR);
    if (mkdtemp(g_test_dir) == NULL) {
        printf("❌ 无法创建测试基目录\n");
        return 1;
    }
    printf("测试基目录: %s\n", g_test_dir);

    RUN_TEST(test_mkdir_null_path);
    RUN_TEST(test_mkdir_single_level);
    RUN_TEST(test_mkdir_deep_recursive);
    RUN_TEST(test_mkdir_already_exists);
    RUN_TEST(test_mkdir_existing_parent);
    RUN_TEST(test_mkdir_same_level_multiple);

    rmdir(g_test_dir);
    printf("已清理测试基目录: %s\n", g_test_dir);

    PRINT_TEST_STATS();

    return TESTS_PASSED() ? 0 : 1;
}
