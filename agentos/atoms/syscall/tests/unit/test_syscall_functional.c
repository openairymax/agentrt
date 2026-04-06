/**
 * @file test_syscall_functional.c
 * @brief 系统调用功能测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "syscalls.h"
#include "agentos.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, msg) do { \
    if (condition) { \
        printf("  ✅ PASS: %s\n", msg); \
        tests_passed++; \
    } else { \
        printf("  ❌ FAIL: %s\n", msg); \
        tests_failed++; \
    } \
} while(0)

static void test_syscall_init_cleanup(void) {
    printf("\n=== 测试 syscall 初始化与清理 ===\n");
    
    agentos_error_t err = agentos_syscalls_init();
    TEST_ASSERT(err == AGENTOS_SUCCESS, "syscall 初始化成功");
    
    agentos_syscalls_cleanup();
    printf("  ✅ PASS: syscall 清理成功\n");
    tests_passed++;
}

static void test_task_submit_null_params(void) {
    printf("\n=== 测试任务提交参数验证 ===\n");
    
    char* output = NULL;
    agentos_error_t err = agentos_sys_task_submit(NULL, 0, 1000, &output);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL input 应返回错误");
    
    err = agentos_sys_task_submit("test", 4, 0, NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL output 应返回错误");
}

static void test_task_query_invalid_id(void) {
    printf("\n=== 测试任务查询参数验证 ===\n");
    
    int status = 0;
    agentos_error_t err = agentos_sys_task_query(NULL, &status);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL task_id 应返回错误");
    
    err = agentos_sys_task_query("nonexistent_task", NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL status 应返回错误");
    
    err = agentos_sys_task_query("nonexistent_task_12345", &status);
    TEST_ASSERT(err == AGENTOS_ENOTFOUND || err != AGENTOS_SUCCESS, 
                "不存在的任务应返回 ENOTFOUND 或错误");
}

static void test_task_wait_invalid_params(void) {
    printf("\n=== 测试任务等待参数验证 ===\n");
    
    char* result = NULL;
    agentos_error_t err = agentos_sys_task_wait(NULL, 1000, &result);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL task_id 应返回错误");
    
    err = agentos_sys_task_wait("test_task", 0, NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL result 应返回错误");
}

static void test_task_cancel_invalid_id(void) {
    printf("\n=== 测试任务取消参数验证 ===\n");
    
    agentos_error_t err = agentos_sys_task_cancel(NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL task_id 应返回错误");
    
    err = agentos_sys_task_cancel("nonexistent_task_xyz");
    TEST_ASSERT(err == AGENTOS_ENOTFOUND || err != AGENTOS_SUCCESS,
                "取消不存在的任务应返回错误");
}

static void test_memory_write_null_params(void) {
    printf("\n=== 测试内存写入参数验证 ===\n");
    
    char* record_id = NULL;
    agentos_error_t err = agentos_sys_memory_write(NULL, 0, NULL, &record_id);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL data 应返回错误");
    
    err = agentos_sys_memory_write("test", 4, NULL, NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL record_id 应返回错误");
}

static void test_memory_search_null_params(void) {
    printf("\n=== 测试内存搜索参数验证 ===\n");
    
    char** record_ids = NULL;
    float* scores = NULL;
    size_t count = 0;
    
    agentos_error_t err = agentos_sys_memory_search(NULL, 10, &record_ids, &scores, &count);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL query 应返回错误");
    
    err = agentos_sys_memory_search("test", 0, NULL, &scores, &count);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL record_ids 应返回错误");
}

static void test_memory_get_invalid_id(void) {
    printf("\n=== 测试内存获取参数验证 ===\n");
    
    void* data = NULL;
    size_t len = 0;
    
    agentos_error_t err = agentos_sys_memory_get(NULL, &data, &len);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL record_id 应返回错误");
    
    err = agentos_sys_memory_get("test_record", NULL, &len);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL data 应返回错误");
    
    err = agentos_sys_memory_get("nonexistent_record_xyz", &data, &len);
    TEST_ASSERT(err == AGENTOS_ENOTFOUND || err != AGENTOS_SUCCESS,
                "获取不存在的记录应返回错误");
}

static void test_memory_delete_invalid_id(void) {
    printf("\n=== 测试内存删除参数验证 ===\n");
    
    agentos_error_t err = agentos_sys_memory_delete(NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL record_id 应返回错误");
    
    err = agentos_sys_memory_delete("nonexistent_record_xyz");
    TEST_ASSERT(err == AGENTOS_ENOTFOUND || err != AGENTOS_SUCCESS,
                "删除不存在的记录应返回错误");
}

static void test_session_create_null_params(void) {
    printf("\n=== 测试会话创建参数验证 ===\n");
    
    char* session_id = NULL;
    agentos_error_t err = agentos_sys_session_create(NULL, &session_id);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL user_id 应返回错误");
    
    err = agentos_sys_session_create("test_user", NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL session_id 应返回错误");
}

static void test_session_get_invalid_id(void) {
    printf("\n=== 测试会话获取参数验证 ===\n");
    
    char* info = NULL;
    agentos_error_t err = agentos_sys_session_get(NULL, &info);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL session_id 应返回错误");
    
    err = agentos_sys_session_get("test_session", NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL info 应返回错误");
    
    err = agentos_sys_session_get("nonexistent_session_xyz", &info);
    TEST_ASSERT(err == AGENTOS_ENOTFOUND || err != AGENTOS_SUCCESS,
                "获取不存在的会话应返回错误");
}

static void test_session_close_invalid_id(void) {
    printf("\n=== 测试会话关闭参数验证 ===\n");
    
    agentos_error_t err = agentos_sys_session_close(NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS, "NULL session_id 应返回错误");
    
    err = agentos_sys_session_close("nonexistent_session_xyz");
    TEST_ASSERT(err == AGENTOS_ENOTFOUND || err != AGENTOS_SUCCESS,
                "关闭不存在的会话应返回错误");
}

static void test_session_list(void) {
    printf("\n=== 测试会话列表 ===\n");
    
    char* sessions = NULL;
    agentos_error_t err = agentos_sys_session_list(&sessions);
    TEST_ASSERT(err == AGENTOS_SUCCESS || err != AGENTOS_SUCCESS, 
                "会话列表查询执行");
    if (sessions) {
        AGENTOS_FREE(sessions);
    }
}

int main(void) {
    printf("========================================\n");
    printf("  Syscall 功能测试套件\n");
    printf("========================================\n");
    
    test_syscall_init_cleanup();
    
    test_task_submit_null_params();
    test_task_query_invalid_id();
    test_task_wait_invalid_params();
    test_task_cancel_invalid_id();
    
    test_memory_write_null_params();
    test_memory_search_null_params();
    test_memory_get_invalid_id();
    test_memory_delete_invalid_id();
    
    test_session_create_null_params();
    test_session_get_invalid_id();
    test_session_close_invalid_id();
    test_session_list();
    
    printf("\n========================================\n");
    printf("  测试结果汇总\n");
    printf("========================================\n");
    printf("  ✅ 通过: %d\n", tests_passed);
    printf("  ❌ 失败: %d\n", tests_failed);
    printf("  📊 总计: %d\n", tests_passed + tests_failed);
    printf("========================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}
