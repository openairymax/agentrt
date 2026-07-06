/**
 * @file test_syscall_extended.c
 * @brief 系统调用扩展单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 测试系统调用表、系统调用入口、熔断器、限流器等核心功能
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8 可测试性原则
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "memory_compat.h"
#include "string_compat.h"
#include "syscalls.h"
#include "agentrt.h"

int test_syscall_table_lookup(void) {
    printf("  测试系统调用表查找...\n");

    const agentrt_syscall_entry_t* entry = agentrt_syscall_table_lookup(AGENTRT_SYSCALL_TASK_CREATE);
    if (entry == NULL) {
        printf("    系统调用表查找失败\n");
        return 1;
    }

    if (entry->name == NULL) {
        printf("    系统调用名称为空\n");
        return 1;
    }

    printf("    找到系统调用: %s\n", entry->name);
    printf("    系统调用表查找测试通过\n");
    return 0;
}

int test_syscall_table_invalid_index(void) {
    printf("  测试无效系统调用号...\n");

    const agentrt_syscall_entry_t* entry = agentrt_syscall_table_lookup(9999);
    if (entry != NULL) {
        printf("    无效系统调用号应返回NULL\n");
        return 1;
    }

    printf("    无效系统调用号测试通过\n");
    return 0;
}

int test_syscall_table_count(void) {
    printf("  测试系统调用表大小...\n");

    int count = agentrt_syscall_table_count();
    if (count <= 0) {
        printf("    系统调用表大小应大于0，实际: %d\n", count);
        return 1;
    }

    printf("    系统调用表大小: %d\n", count);
    printf("    系统调用表大小测试通过\n");
    return 0;
}

int test_syscall_table_iterate(void) {
    printf("  测试系统调用表遍历...\n");

    int count = agentrt_syscall_table_count();
    int valid_entries = 0;

    for (int i = 0; i < count; i++) {
        const agentrt_syscall_entry_t* entry = agentrt_syscall_table_lookup(i);
        if (entry != NULL && entry->name != NULL) {
            valid_entries++;
        }
    }

    if (valid_entries == 0) {
        printf("    未找到有效的系统调用条目\n");
        return 1;
    }

    printf("    遍历找到 %d 个有效系统调用\n", valid_entries);
    printf("    系统调用表遍历测试通过\n");
    return 0;
}

int test_rate_limiter_creation(void) {
    printf("  测试限流器创建...\n");

    agentrt_rate_limiter_t* limiter = NULL;
    agentrt_error_t err = agentrt_rate_limiter_create(100, 10, &limiter);

    if (err != AGENTRT_SUCCESS || limiter == NULL) {
        printf("    限流器创建失败\n");
        return 1;
    }

    agentrt_rate_limiter_destroy(limiter);
    printf("    限流器创建测试通过\n");
    return 0;
}

int test_rate_limiter_basic(void) {
    printf("  测试限流器基本功能...\n");

    agentrt_rate_limiter_t* limiter = NULL;
    agentrt_rate_limiter_create(10, 2, &limiter);

    for (int i = 0; i < 5; i++) {
        int allowed = agentrt_rate_limiter_allow(limiter, "test_key");
        if (i < 2 && !allowed) {
            printf("    前2次请求应被允许\n");
            agentrt_rate_limiter_destroy(limiter);
            return 1;
        }
    }

    agentrt_rate_limiter_destroy(limiter);
    printf("    限流器基本功能测试通过\n");
    return 0;
}

int test_rate_limiter_refill(void) {
    printf("  测试限流器令牌补充...\n");

    agentrt_rate_limiter_t* limiter = NULL;
    agentrt_rate_limiter_create(2, 2, &limiter);

    agentrt_rate_limiter_allow(limiter, "key1");
    agentrt_rate_limiter_allow(limiter, "key2");
    agentrt_rate_limiter_allow(limiter, "key3");

    printf("    限流器令牌补充测试通过\n");
    agentrt_rate_limiter_destroy(limiter);
    return 0;
}

int test_circuit_breaker_creation(void) {
    printf("  测试熔断器创建...\n");

    agentrt_circuit_breaker_t* cb = NULL;
    agentrt_error_t err = agentrt_circuit_breaker_create(
        5,
        30000,
        60000,
        &cb
    );

    if (err != AGENTRT_SUCCESS || cb == NULL) {
        printf("    熔断器创建失败\n");
        return 1;
    }

    agentrt_circuit_breaker_destroy(cb);
    printf("    熔断器创建测试通过\n");
    return 0;
}

int test_circuit_breaker_state_transitions(void) {
    printf("  测试熔断器状态转换...\n");

    agentrt_circuit_breaker_t* cb = NULL;
    agentrt_circuit_breaker_create(3, 1000, 5000, &cb);

    agentrt_circuit_state_t state = agentrt_circuit_breaker_get_state(cb);
    if (state != AGENTRT_CIRCUIT_STATE_CLOSED) {
        printf("    初始状态应为CLOSED\n");
        agentrt_circuit_breaker_destroy(cb);
        return 1;
    }

    agentrt_circuit_breaker_destroy(cb);
    printf("    熔断器状态转换测试通过\n");
    return 0;
}

int test_circuit_breaker_record_success(void) {
    printf("  测试熔断器记录成功...\n");

    agentrt_circuit_breaker_t* cb = NULL;
    agentrt_circuit_breaker_create(3, 1000, 5000, &cb);

    agentrt_circuit_breaker_record_success(cb);
    agentrt_circuit_breaker_record_success(cb);

    agentrt_circuit_state_t state = agentrt_circuit_breaker_get_state(cb);
    if (state != AGENTRT_CIRCUIT_STATE_CLOSED) {
        printf("    成功记录后状态仍应为CLOSED\n");
        agentrt_circuit_breaker_destroy(cb);
        return 1;
    }

    agentrt_circuit_breaker_destroy(cb);
    printf("    熔断器记录成功测试通过\n");
    return 0;
}

int test_telemetry_initialization(void) {
    printf("  测试遥测初始化...\n");

    agentrt_error_t err = agentrt_telemetry_init();
    if (err != AGENTRT_SUCCESS) {
        printf("    遥测初始化失败\n");
        return 1;
    }

    agentrt_telemetry_shutdown();
    printf("    遥测初始化测试通过\n");
    return 0;
}

int test_telemetry_counter(void) {
    printf("  测试遥测计数器...\n");

    agentrt_telemetry_init();

    agentrt_telemetry_counter_inc("test_counter");
    agentrt_telemetry_counter_add("test_counter", 5);

    agentrt_telemetry_shutdown();
    printf("    遥测计数器测试通过\n");
    return 0;
}

int test_telemetry_histogram(void) {
    printf("  测试遥测直方图...\n");

    agentrt_telemetry_init();

    agentrt_telemetry_histogram_observe("test_histogram", 100.5);
    agentrt_telemetry_histogram_observe("test_histogram", 200.3);
    agentrt_telemetry_histogram_observe("test_histogram", 50.1);

    agentrt_telemetry_shutdown();
    printf("    遥测直方图测试通过\n");
    return 0;
}

int test_sandbox_initialization(void) {
    printf("  测试沙箱初始化...\n");

    agentrt_sandbox_t* sandbox = NULL;
    agentrt_error_t err = agentrt_sandbox_create(&sandbox);

    if (err != AGENTRT_SUCCESS || sandbox == NULL) {
        printf("    沙箱创建失败\n");
        return 1;
    }

    agentrt_sandbox_destroy(sandbox);
    printf("    沙箱初始化测试通过\n");
    return 0;
}

int test_sandbox_permission_check(void) {
    printf("  测试沙箱权限检查...\n");

    agentrt_sandbox_t* sandbox = NULL;
    agentrt_sandbox_create(&sandbox);

    int allowed = agentrt_sandbox_check_permission(sandbox, AGENTRT_SYSCALL_FILE_READ, AGENTRT_TMP_DIR "/test.txt");
    if (allowed < 0) {
        printf("    权限检查失败\n");
        agentrt_sandbox_destroy(sandbox);
        return 1;
    }

    agentrt_sandbox_destroy(sandbox);
    printf("    沙箱权限检查测试通过\n");
    return 0;
}

int test_sandbox_resource_limits(void) {
    printf("  测试沙箱资源限制...\n");

    agentrt_sandbox_t* sandbox = NULL;
    agentrt_sandbox_create(&sandbox);

    agentrt_error_t err = agentrt_sandbox_set_resource_limit(sandbox, AGENTRT_RESOURCE_MEMORY, 1024 * 1024);
    if (err != AGENTRT_SUCCESS) {
        printf("    设置资源限制失败\n");
        agentrt_sandbox_destroy(sandbox);
        return 1;
    }

    agentrt_sandbox_destroy(sandbox);
    printf("    沙箱资源限制测试通过\n");
    return 0;
}

int main(void) {
    printf("开始运行 syscall 扩展单元测试...\n");

    int failures = 0;

    failures |= test_syscall_table_lookup();
    failures |= test_syscall_table_invalid_index();
    failures |= test_syscall_table_count();
    failures |= test_syscall_table_iterate();
    failures |= test_rate_limiter_creation();
    failures |= test_rate_limiter_basic();
    failures |= test_rate_limiter_refill();
    failures |= test_circuit_breaker_creation();
    failures |= test_circuit_breaker_state_transitions();
    failures |= test_circuit_breaker_record_success();
    failures |= test_telemetry_initialization();
    failures |= test_telemetry_counter();
    failures |= test_telemetry_histogram();
    failures |= test_sandbox_initialization();
    failures |= test_sandbox_permission_check();
    failures |= test_sandbox_resource_limits();

    if (failures == 0) {
        printf("\n所有syscall扩展测试通过！\n");
        return 0;
    } else {
        printf("\n%d 个syscall扩展测试失败\n", failures);
        return 1;
    }
}
