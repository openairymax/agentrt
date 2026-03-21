/**
 * @file test_domes_integration.c
 * @brief Domes 集成测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "domes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief 测试 Domes 完整安全工作流
 */
void test_domes_full_workflow(void) {
    printf("=== 测试 Domes 完整安全工作流 ===\n");
    
    // 初始化 Domes
    domes_t* domain = NULL;
    int err = domes_init(NULL, &domain);
    if (err != 0 || !domain) {
        printf("初始化 Domes 失败: %d\n", err);
        return;
    }
    // From data intelligence emerges. by spharx
    printf("Domes 初始化成功\n");
    
    // 1. 测试虚拟工位创建和使用
    printf("1. 测试虚拟工位\n");
    
    char* workbench_id = NULL;
    err = domes_workbench_create(domain, "test_agent", &workbench_id);
    if (err != 0 || !workbench_id) {
        printf("创建工位失败: %d\n", err);
        domes_destroy(domain);
        return;
    }
    printf("工位创建成功: %s\n", workbench_id);
    
    // 执行安全命令
    const char* safe_argv[] = {"echo", "安全命令执行", NULL};
    char* stdout_buf = NULL;
    char* stderr_buf = NULL;
    int exit_code = 0;
    char* error = NULL;
    
    err = domes_workbench_exec(domain, workbench_id, safe_argv, 1000, &stdout_buf, &stderr_buf, &exit_code, &error);
    printf("执行安全命令: %d, 退出码: %d\n", err, exit_code);
    if (stdout_buf) {
        printf("输出: %s\n", stdout_buf);
        free(stdout_buf);
    }
    if (stderr_buf) free(stderr_buf);
    if (error) free(error);
    
    // 2. 测试权限检查
    printf("2. 测试权限检查\n");
    
    int allowed = domes_permission_check(domain, "test_agent", "file:read", "/etc/passwd", NULL);
    printf("读取 /etc/passwd 权限: %d\n", allowed);
    
    allowed = domes_permission_check(domain, "test_agent", "file:write", "/etc/passwd", NULL);
    printf("写入 /etc/passwd 权限: %d\n", allowed);
    
    allowed = domes_permission_check(domain, "test_agent", "network:access", "http://example.com", NULL);
    printf("网络访问权限: %d\n", allowed);
    
    // 3. 测试输入净化
    printf("3. 测试输入净化\n");
    
    const char* test_inputs[] = {
        "正常输入文本",
        "<script>alert('XSS')</script>",
        "SELECT * FROM users",
        "../../../etc/passwd"
    };
    
    size_t input_count = sizeof(test_inputs) / sizeof(test_inputs[0]);
    
    for (size_t i = 0; i < input_count; i++) {
        char* cleaned = NULL;
        int risk_level = 0;
        err = domes_sanitize(domain, test_inputs[i], &cleaned, &risk_level);
        printf("输入: '%s'\n", test_inputs[i]);
        printf("净化结果: %d, 风险等级: %d\n", err, risk_level);
        if (cleaned) {
            printf("净化后: '%s'\n", cleaned);
            free(cleaned);
        }
        printf("\n");
    }
    
    // 4. 测试审计功能
    printf("4. 测试审计功能\n");
    
    // 记录多个审计事件
    for (int i = 0; i < 5; i++) {
        char tool_name[32];
        char input[64];
        char output[64];
        
        snprintf(tool_name, sizeof(tool_name), "tool_%d", i);
        snprintf(input, sizeof(input), "{\"param\": %d}", i);
        snprintf(output, sizeof(output), "{\"result\": \"success\"}");
        
        err = domes_audit_record(domain, "test_agent", tool_name, input, output, 50 + i*10, 1, NULL);
        if (err != 0) {
            printf("记录审计事件 %d 失败: %d\n", i+1, err);
        }
    }
    
    // 查询审计日志
    char** events = NULL;
    size_t count = 0;
    err = domes_audit_query(domain, "test_agent", 0, 0, 10, &events, &count);
    printf("查询审计日志: %d, 找到 %zu 条事件\n", err, count);
    
    if (events) {
        for (size_t i = 0; i < count; i++) {
            printf("  事件 %zu: %s\n", i+1, events[i]);
            free(events[i]);
        }
        free(events);
    }
    
    // 5. 测试工位管理
    printf("5. 测试工位管理\n");
    
    // 列出所有工位
    char** workbench_ids = NULL;
    size_t workbench_count = 0;
    err = domes_workbench_list(domain, &workbench_ids, &workbench_count);
    printf("列出工位: %d, 数量: %zu\n", err, workbench_count);
    
    if (workbench_ids) {
        for (size_t i = 0; i < workbench_count; i++) {
            printf("  工位 %zu: %s\n", i+1, workbench_ids[i]);
            free(workbench_ids[i]);
        }
        free(workbench_ids);
    }
    
    // 销毁工位
    domes_workbench_destroy(domain, workbench_id);
    printf("销毁工位: %s\n", workbench_id);
    free(workbench_id);
    
    // 6. 测试系统恢复
    printf("6. 测试系统恢复\n");
    
    // 模拟系统重启
    domes_destroy(domain);
    printf("系统清理完成\n");
    
    // 重新初始化
    err = domes_init(NULL, &domain);
    if (err == 0 && domain) {
        printf("系统重新初始化成功\n");
        
        // 验证审计日志是否保留
        err = domes_audit_query(domain, "test_agent", 0, 0, 10, &events, &count);
        printf("重启后查询审计日志: %d, 找到 %zu 条事件\n", err, count);
        
        if (events) {
            free(events);
        }
    } else {
        printf("系统重新初始化失败: %d\n", err);
    }
    
    // 清理资源
    if (domain) {
        domes_destroy(domain);
    }
    
    printf("完整安全工作流测试完成\n\n");
}

/**
 * @brief 测试 Domes 性能
 */
void test_domes_performance(void) {
    printf("=== 测试 Domes 性能 ===\n");
    
    // 初始化 Domes
    domes_t* domain = NULL;
    int err = domes_init(NULL, &domain);
    if (err != 0 || !domain) {
        printf("初始化失败: %d\n", err);
        return;
    }
    
    // 测试权限检查性能
    const int test_count = 1000;
    clock_t start = clock();
    
    for (int i = 0; i < test_count; i++) {
        domes_permission_check(domain, "test_agent", "file:read", "/etc/passwd", NULL);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("%d 次权限检查耗时: %.2f 秒, 平均每次: %.6f 秒\n", test_count, elapsed, elapsed/test_count);
    
    // 测试输入净化性能
    const char* test_input = "Hello, World! This is a test input for sanitization.";
    start = clock();
    
    for (int i = 0; i < test_count; i++) {
        char* cleaned = NULL;
        int risk_level = 0;
        domes_sanitize(domain, test_input, &cleaned, &risk_level);
        if (cleaned) {
            free(cleaned);
        }
    }
    
    end = clock();
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("%d 次输入净化耗时: %.2f 秒, 平均每次: %.6f 秒\n", test_count, elapsed, elapsed/test_count);
    
    // 测试审计记录性能
    start = clock();
    
    for (int i = 0; i < test_count; i++) {
        char input[32];
        snprintf(input, sizeof(input), "{\"test\": %d}", i);
        domes_audit_record(domain, "test_agent", "test_tool", input, "{\"result\": \"ok\"}", 10, 1, NULL);
    }
    
    end = clock();
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("%d 次审计记录耗时: %.2f 秒, 平均每次: %.6f 秒\n", test_count, elapsed, elapsed/test_count);
    
    // 清理资源
    domes_destroy(domain);
    
    printf("性能测试完成\n\n");
}

/**
 * @brief 测试 Domes 错误处理
 */
void test_domes_error_handling(void) {
    printf("=== 测试 Domes 错误处理 ===\n");
    
    // 测试空指针参数
    domes_t* domain = NULL;
    int err = domes_init(NULL, NULL);
    printf("初始化（空输出指针）: %d\n", err);
    
    // 测试无效配置
    domes_config_t invalid_config = {
        .workbench_type = "invalid_type",  // 无效类型
        .workbench_memory_bytes = 0,        // 无效内存限制
    };
    
    err = domes_init(&invalid_config, &domain);
    printf("初始化（无效配置）: %d\n", err);
    
    // 测试空句柄操作
    if (domain) {
        domes_destroy(domain);
    }
    
    // 测试空句柄调用其他函数
    char* workbench_id = NULL;
    err = domes_workbench_create(NULL, "test_agent", &workbench_id);
    printf("创建工位（空句柄）: %d\n", err);
    
    int allowed = domes_permission_check(NULL, "test_agent", "file:read", "/etc/passwd", NULL);
    printf("权限检查（空句柄）: %d\n", allowed);
    
    char* cleaned = NULL;
    int risk_level = 0;
    err = domes_sanitize(NULL, "test", &cleaned, &risk_level);
    printf("输入净化（空句柄）: %d\n", err);
    
    err = domes_audit_record(NULL, "test_agent", "test_tool", "input", "output", 10, 1, NULL);
    printf("记录审计事件（空句柄）: %d\n", err);
    
    printf("错误处理测试完成\n\n");
}

int main(void) {
    printf("开始 Domes 集成测试\n\n");
    
    // 运行各项测试
    test_domes_full_workflow();
    test_domes_performance();
    test_domes_error_handling();
    
    printf("Domes 集成测试完成\n");
    return 0;
}
