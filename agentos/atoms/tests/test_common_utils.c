/**
 * @file test_common_utils.c
 * @brief 公共工具函数单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 测试公共工具函数的正确性、边界条件和异常处理
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include "../../../agentos/commons/utils/quality/agentos_quality.h"

#define TEST_BUFFER_SIZE 256

int test_safe_strcpy_basic(void) {
    printf("  测试安全字符串复制基本功能...\n");

    char dest[TEST_BUFFER_SIZE];
    const char* src = "Hello, AgentOS!";

    int result = agentos_safe_strcpy(dest, sizeof(dest), src);
    if (result != 0) {
        printf("    复制失败\n");
        return 1;
    }

    if (strcmp(dest, src) != 0) {
        printf("    复制结果不匹配\n");
        return 1;
    }

    printf("    安全字符串复制基本功能测试通过\n");
    return 0;
}

int test_safe_strcpy_null_src(void) {
    printf("  测试空源字符串处理...\n");

    char dest[TEST_BUFFER_SIZE] = "original";
    int result = agentos_safe_strcpy(dest, sizeof(dest), NULL);

    if (result == 0) {
        printf("    空源应返回错误\n");
        return 1;
    }

    printf("    空源字符串处理测试通过\n");
    return 0;
}

int test_safe_strcpy_null_dest(void) {
    printf("  测试空目标缓冲区...\n");

    int result = agentos_safe_strcpy(NULL, 100, "test");

    if (result == 0) {
        printf("    空目标应返回错误\n");
        return 1;
    }

    printf("    空目标缓冲区测试通过\n");
    return 0;
}

int test_safe_strcpy_overflow(void) {
    printf("  测试缓冲区溢出保护...\n");

    char dest[10];
    const char* long_src = "This is a very long string that exceeds the buffer";

    int result = agentos_safe_strcpy(dest, sizeof(dest), long_src);

    if (result != 0) {
        printf("    溢出时返回错误\n");
        return 1;
    }

    if (strlen(dest) >= sizeof(dest)) {
        printf("    缓冲区应被截断\n");
        return 1;
    }

    printf("    缓冲区溢出保护测试通过\n");
    return 0;
}

int test_safe_strcat_basic(void) {
    printf("  测试安全字符串拼接基本功能...\n");

    char dest[TEST_BUFFER_SIZE] = "Hello";
    const char* src = ", World!";

    int result = agentos_safe_strcat(dest, sizeof(dest), src);
    if (result != 0) {
        printf("    拼接失败\n");
        return 1;
    }

    if (strcmp(dest, "Hello, World!") != 0) {
        printf("    拼接结果不匹配: %s\n", dest);
        return 1;
    }

    printf("    安全字符串拼接基本功能测试通过\n");
    return 0;
}

int test_safe_strcat_empty_dest(void) {
    printf("  测试空目标拼接...\n");

    char dest[TEST_BUFFER_SIZE];
    memset(dest, 0, sizeof(dest));

    int result = agentos_safe_strcat(dest, sizeof(dest), "test");
    if (result != 0 || strcmp(dest, "test") != 0) {
        printf("    空目标拼接失败\n");
        return 1;
    }

    printf("    空目标拼接测试通过\n");
    return 0;
}

int test_validate_non_negative(void) {
    printf("  测试非负整数验证...\n");

    if (!agentos_validate_non_negative(0)) { return 1; }
    if (!agentos_validate_non_negative(100)) { return 1; }
    if (agentos_validate_non_negative(-1)) { return 1; }
    if (agentos_validate_non_negative(INT32_MIN)) { return 1; }

    printf("    非负整数验证测试通过\n");
    return 0;
}

int test_validate_positive(void) {
    printf("  测试正整数验证...\n");

    if (agentos_validate_positive(0)) { return 1; }
    if (!agentos_validate_positive(1)) { return 1; }
    if (!agentos_validate_positive(100)) { return 1; }
    if (agentos_validate_positive(-1)) { return 1; }

    printf("    正整数验证测试通过\n");
    return 0;
}

int test_validate_percentage(void) {
    printf("  测试百分比验证...\n");

    if (!agentos_validate_percentage(0.0f)) { return 1; }
    if (!agentos_validate_percentage(50.5f)) { return 1; }
    if (!agentos_validate_percentage(100.0f)) { return 1; }
    if (agentos_validate_percentage(-1.0f)) { return 1; }
    if (agentos_validate_percentage(101.0f)) { return 1; }

    printf("    百分比验证测试通过\n");
    return 0;
}

int test_validate_probability(void) {
    printf("  测试概率值验证...\n");

    if (!agentos_validate_probability(0.0f)) { return 1; }
    if (!agentos_validate_probability(0.5f)) { return 1; }
    if (!agentos_validate_probability(1.0f)) { return 1; }
    if (agentos_validate_probability(-0.1f)) { return 1; }
    if (agentos_validate_probability(1.1f)) { return 1; }

    printf("    概率值验证测试通过\n");
    return 0;
}

int test_validate_priority(void) {
    printf("  测试优先级验证...\n");

    if (!agentos_validate_priority(5, 0, 10)) { return 1; }
    if (!agentos_validate_priority(0, 0, 10)) { return 1; }
    if (!agentos_validate_priority(10, 0, 10)) { return 1; }
    if (agentos_validate_priority(-1, 0, 10)) { return 1; }
    if (agentos_validate_priority(11, 0, 10)) { return 1; }

    printf("    优先级验证测试通过\n");
    return 0;
}

int test_timestamp_ns(void) {
    printf("  测试纳秒时间戳...\n");

    uint64_t ts1 = agentos_get_timestamp_ns();
    
    for (volatile int i = 0; i < 10000; i++);
    
    uint64_t ts2 = agentos_get_timestamp_ns();

    if (ts2 <= ts1) {
        printf("    时间戳应单调递增\n");
        return 1;
    }

    if ((ts2 - ts1) > 100000000ULL) {
        printf("    时间差过大（可能时钟精度问题）\n");
    }

    printf("    纳秒时间戳测试通过 (diff=%llu ns)\n", 
           (unsigned long long)(ts2 - ts1));
    return 0;
}

int test_timestamp_ms(void) {
    printf("  测试毫秒时间戳...\n");

    uint64_t ts1 = agentos_get_timestamp_ms();
    
    for (volatile int i = 0; i < 100000; i++);
    
    uint64_t ts2 = agentos_get_timestamp_ms();

    if (ts2 < ts1) {
        printf("    时间戳应单调递增\n");
        return 1;
    }

    printf("    毫秒时间戳测试通过 (diff=%llu ms)\n",
           (unsigned long long)(ts2 - ts1));
    return 0;
}

int test_atomic_operations(void) {
    printf("  测试原子操作...\n");

    volatile int32_t counter = 0;

    int32_t old = agentos_atomic_fetch_add(&counter, 10);
    if (old != 0 || counter != 10) {
        printf("    原子加法结果错误\n");
        return 1;
    }

    old = agentos_atomic_fetch_add(&counter, 5);
    if (old != 10 || counter != 15) {
        printf("    原子加法结果错误\n");
        return 1;
    }

    bool success = agentos_atomic_compare_exchange(&counter, 15, 20);
    if (!success || counter != 20) {
        printf("    原子比较交换结果错误\n");
        return 1;
    }

    success = agentos_atomic_compare_exchange(&counter, 15, 25);
    if (success) {
        printf("    不应交换成功\n");
        return 1;
    }

    printf("    原子操作测试通过\n");
    return 0;
}

int test_log_message(void) {
    printf("  测试日志消息...\n");

    AGENTOS_LOG_DEBUG("Debug message: %d", 42);
    AGENTOS_LOG_INFO("Info message: %s", "test");
    AGENTOS_LOG_WARN("Warning message: %f", 3.14);
    AGENTOS_LOG_ERROR("Error message: %s", "error");
    AGENTOS_LOG_FATAL("Fatal message: %d", 999);

    printf("    日志消息测试通过\n");
    return 0;
}

int main(void) {
    printf("开始运行 atoms 公共工具函数单元测试...\n");

    int failures = 0;

    failures |= test_safe_strcpy_basic();
    failures |= test_safe_strcpy_null_src();
    failures |= test_safe_strcpy_null_dest();
    failures |= test_safe_strcpy_overflow();
    failures |= test_safe_strcat_basic();
    failures |= test_safe_strcat_empty_dest();
    failures |= test_validate_non_negative();
    failures |= test_validate_positive();
    failures |= test_validate_percentage();
    failures |= test_validate_probability();
    failures |= test_validate_priority();
    failures |= test_timestamp_ns();
    failures |= test_timestamp_ms();
    failures |= test_atomic_operations();
    failures |= test_log_message();

    if (failures == 0) {
        printf("\n所有公共工具函数测试通过！\n");
        return 0;
    } else {
        printf("\n%d 个公共工具函数测试失败\n", failures);
        return 1;
    }
}
