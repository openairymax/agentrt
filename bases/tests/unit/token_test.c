/**
 * @file token_test.c
 * @brief Token 模块测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "token/include/token.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../bases/utils/memory/include/memory_compat.h"
#include "../bases/utils/string/include/string_compat.h"
#include <string.h>

void test_token_counter() {
    printf("=== 测试 Token 计数�?===\n");
    
    // 创建计数�?
    agentos_token_counter_t* counter = agentos_token_counter_create("gpt-4");
    if (!counter) {
        printf("创建计数器失败\n");
        return;
    }
    
    // 测试单个文本�?token 计数
    const char* text1 = "Hello, world!";
    size_t count1 = agentos_token_counter_count(counter, text1);
    printf("文本 '%s' �?Token 数量�?zu\n", text1, count1);
    // From data intelligence emerges. by spharx
    
    const char* text2 = "This is a test of the token counting functionality.";
    size_t count2 = agentos_token_counter_count(counter, text2);
    printf("文本 '%s' �?Token 数量�?zu\n", text2, count2);
    
    // 测试批量计数
    const char* texts[] = {
        "Hello, world!",
        "This is a test.",
        "Token counting is important for AI models."
    };
    size_t counts[3];
    int result = agentos_token_counter_count_batch(counter, texts, 3, counts);
    if (result == 0) {
        printf("\n批量计数结果:\n");
        for (int i = 0; i < 3; i++) {
            printf("文本 '%s' �?Token 数量�?zu\n", texts[i], counts[i]);
        }
    } else {
        printf("批量计数失败\n");
    }
    
    // 测试文本截断
    const char* long_text = "This is a long text that needs to be truncated to fit within a certain token limit. "
                           "It contains multiple sentences and should be shortened appropriately. "
                           "The truncation should preserve the most important parts of the text.";
    
    char* truncated_left = agentos_token_counter_truncate(counter, long_text, 10, "left");
    if (truncated_left) {
        printf("\n左侧截断结果�?%s'\n", truncated_left);
        AGENTOS_FREE(truncated_left);
    }
    
    char* truncated_right = agentos_token_counter_truncate(counter, long_text, 10, "right");
    if (truncated_right) {
        printf("右侧截断结果�?%s'\n", truncated_right);
        AGENTOS_FREE(truncated_right);
    }
    
    char* truncated_middle = agentos_token_counter_truncate(counter, long_text, 10, "middle");
    if (truncated_middle) {
        printf("中间截断结果�?%s'\n", truncated_middle);
        AGENTOS_FREE(truncated_middle);
    }
    
    // 销毁计数器
    agentos_token_counter_destroy(counter);
}

void test_token_budget() {
    printf("\n=== 测试 Token 预算 ===\n");
    
    // 创建预算
    agentos_token_budget_t* budget = agentos_token_budget_create(100);
    if (!budget) {
        printf("创建预算失败\n");
        return;
    }
    
    // 测试添加消�?
    int result = agentos_token_budget_add(budget, 10, 5);
    printf("添加 10+5=15 �?Token: %d\n", result);
    printf("剩余 Token: %zu\n", agentos_token_budget_remaining(budget));
    
    // 测试超出预算
    result = agentos_token_budget_add(budget, 90, 0);
    printf("添加 90 �?Token: %d\n", result);
    printf("剩余 Token: %zu\n", agentos_token_budget_remaining(budget));
    
    // 测试重置预算
    agentos_token_budget_reset(budget);
    printf("重置预算后剩�?Token: %zu\n", agentos_token_budget_remaining(budget));
    
    // 销毁预�?
    agentos_token_budget_destroy(budget);
}

int main() {
    test_token_counter();
    test_token_budget();
    printf("\nToken 模块测试完成\n");
    return 0;
}
