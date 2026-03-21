/**
 * @file test_domes_core.c
 * @brief Domes 核心功能单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "domes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 测试 Domes 初始化和清理
 */
void test_domes_init_cleanup(void) {
    printf("=== 测试 Domes 初始化和清理 ===\n");
    
    // 测试默认配置初始化
    domes_t* domain = NULL;
    int err = domes_init(NULL, &domain);
    printf("默认配置初始化: %d\n", err);
    
    if (err == 0 && domain) {
        // 清理资源
        domes_destroy(domain);
        // From data intelligence emerges. by spharx
        printf("系统清理完成\n");
    }
    
    // 测试自定义配置初始化
    domes_config_t config = {
        .workbench_type = "process",
        .workbench_memory_bytes = 1024 * 1024 * 100,  // 100MB
        .workbench_cpu_quota = 1.0,
        .workbench_network = 0,
        .workbench_rootfs = NULL,
        .permission_rules_path = NULL,
        .permission_cache_ttl_ms = 3600000,  // 1小时
        .audit_log_path = NULL,
        .audit_max_size_bytes = 1024 * 1024 * 10,  // 10MB
        .audit_max_files = 5,
        .audit_format = "json",
        .sanitizer_max_input_len = 1024 * 1024,  // 1MB
        .sanitizer_rules_path = NULL
    };
    
    err = domes_init(&config, &domain);
    printf("自定义配置初始化: %d\n", err);
    
    if (err == 0 && domain) {
        domes_destroy(domain);
        printf("自定义配置系统清理完成\n");
    }
    
    printf("初始化和清理测试完成\n\n");
}

/**
 * @brief 测试虚拟工位功能
 */
void test_workbench(void) {
    printf("=== 测试虚拟工位功能 ===\n");
    
    // 初始化 Domes
    domes_t* domain = NULL;
    int err = domes_init(NULL, &domain);
    if (err != 0 || !domain) {
        printf("初始化 Domes 失败: %d\n", err);
        return;
    }
    
    // 测试创建工位
    char* workbench_id = NULL;
    err = domes_workbench_create(domain, "test_agent", &workbench_id);
    printf("创建工位: %d\n", err);
    
    if (err == 0 && workbench_id) {
        printf("工位ID: %s\n", workbench_id);
        
        // 测试执行命令
        const char* argv[] = {"echo", "Hello, Domes!", NULL};
        char* stdout_buf = NULL;
        char* stderr_buf = NULL;
        int exit_code = 0;
        char* error = NULL;
        
        err = domes_workbench_exec(domain, workbench_id, argv, 1000, &stdout_buf, &stderr_buf, &exit_code, &error);
        printf("执行命令: %d, 退出码: %d\n", err, exit_code);
        
        if (stdout_buf) {
            printf("标准输出: %s\n", stdout_buf);
            free(stdout_buf);
        }
        
        if (stderr_buf) {
            free(stderr_buf);
        }
        
        if (error) {
            free(error);
        }
        
        // 测试列出工位
        char** workbench_ids = NULL;
        size_t count = 0;
        err = domes_workbench_list(domain, &workbench_ids, &count);
        printf("列出工位: %d, 数量: %zu\n", err, count);
        
        if (workbench_ids) {
            for (size_t i = 0; i < count; i++) {
                printf("  工位 %zu: %s\n", i+1, workbench_ids[i]);
                free(workbench_ids[i]);
            }
            free(workbench_ids);
        }
        
        // 销毁工位
        domes_workbench_destroy(domain, workbench_id);
        printf("销毁工位完成\n");
        
        free(workbench_id);
    }
    
    // 清理资源
    domes_destroy(domain);
    printf("虚拟工位测试完成\n\n");
}

/**
 * @brief 测试权限裁决功能
 */
void test_permission(void) {
    printf("=== 测试权限裁决功能 ===\n");
    
    // 初始化 Domes
    domes_t* domain = NULL;
    int err = domes_init(NULL, &domain);
    if (err != 0 || !domain) {
        printf("初始化 Domes 失败: %d\n", err);
        return;
    }
    
    // 测试权限检查
    int allowed = domes_permission_check(domain, "test_agent", "file:read", "/etc/passwd", NULL);
    printf("权限检查 (file:read /etc/passwd): %d\n", allowed);
    
    allowed = domes_permission_check(domain, "test_agent", "file:write", "/etc/passwd", NULL);
    printf("权限检查 (file:write /etc/passwd): %d\n", allowed);
    
    allowed = domes_permission_check(domain, "test_agent", "network:access", "http://example.com", NULL);
    printf("权限检查 (network:access example.com): %d\n", allowed);
    
    // 测试重新加载权限规则
    err = domes_permission_reload(domain);
    printf("重新加载权限规则: %d\n", err);
    
    // 清理资源
    domes_destroy(domain);
    printf("权限裁决测试完成\n\n");
}

/**
 * @brief 测试审计功能
 */
void test_audit(void) {
    printf("=== 测试审计功能 ===\n");
    
    // 初始化 Domes
    domes_t* domain = NULL;
    int err = domes_init(NULL, &domain);
    if (err != 0 || !domain) {
        printf("初始化 Domes 失败: %d\n", err);
        return;
    }
    
    // 测试记录审计事件
    err = domes_audit_record(domain, "test_agent", "test_tool", "{\"input\": \"test\"}", "{\"output\": \"result\"}", 100, 1, NULL);
    printf("记录审计事件: %d\n", err);
    
    // 测试查询审计日志
    char** events = NULL;
    size_t count = 0;
    err = domes_audit_query(domain, "test_agent", 0, 0, 10, &events, &count);
    printf("查询审计日志: %d, 事件数量: %zu\n", err, count);
    
    if (events) {
        for (size_t i = 0; i < count; i++) {
            printf("  事件 %zu: %s\n", i+1, events[i]);
            free(events[i]);
        }
        free(events);
    }
    
    // 清理资源
    domes_destroy(domain);
    printf("审计功能测试完成\n\n");
}

/**
 * @brief 测试输入净化功能
 */
void test_sanitizer(void) {
    printf("=== 测试输入净化功能 ===\n");
    
    // 初始化 Domes
    domes_t* domain = NULL;
    int err = domes_init(NULL, &domain);
    if (err != 0 || !domain) {
        printf("初始化 Domes 失败: %d\n", err);
        return;
    }
    
    // 测试净化正常输入
    const char* normal_input = "Hello, World!";
    char* cleaned = NULL;
    int risk_level = 0;
    err = domes_sanitize(domain, normal_input, &cleaned, &risk_level);
    printf("净化正常输入: %d, 风险等级: %d\n", err, risk_level);
    if (cleaned) {
        printf("净化结果: %s\n", cleaned);
        free(cleaned);
    }
    
    // 测试净化可能的恶意输入
    const char* malicious_input = "<script>alert('XSS')</script>";
    err = domes_sanitize(domain, malicious_input, &cleaned, &risk_level);
    printf("净化恶意输入: %d, 风险等级: %d\n", err, risk_level);
    if (cleaned) {
        printf("净化结果: %s\n", cleaned);
        free(cleaned);
    }
    
    // 清理资源
    domes_destroy(domain);
    printf("输入净化测试完成\n\n");
}

int main(void) {
    printf("开始 Domes 单元测试\n\n");
    
    // 运行各项测试
    test_domes_init_cleanup();
    test_workbench();
    test_permission();
    test_audit();
    test_sanitizer();
    
    printf("Domes 单元测试完成\n");
    return 0;
}
