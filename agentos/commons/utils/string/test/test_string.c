/**
 * @file test_string.c
 * @brief 统一字符串处理模块单元测�? * 
 * 测试字符串模块的基本功能：安全复制、连接、比较、格式化等�? * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>
#include <assert.h>
#include "../../include/string.h"

/**
 * @brief 测试安全字符串复制功�? * 
 * @return 成功返回0，失败返�?
 */
static int test_string_copy(void) {
    printf("测试安全字符串复�?..\n");
    
    char dest[32];
    
    // 测试正常复制
    int result = string_copy(dest, "Hello", sizeof(dest));
    if (result != 5) {
        printf("  错误：正常复制失败，返回 %d\n", result);
        return 1;
    }
    if (strcmp(dest, "Hello") != 0) {
        printf("  错误：复制结果不匹配: %s\n", dest);
        return 1;
    }
    
    // 测试缓冲区溢出保�?    result = string_copy(dest, "This is a very long string that should be truncated", sizeof(dest));
    if (result != -1) {
        printf("  错误：缓冲区溢出未检测到，返�?%d\n", result);
        return 1;
    }
    
    // 测试空字符串
    result = string_copy(dest, "", sizeof(dest));
    if (result != 0) {
        printf("  错误：空字符串复制失败，返回 %d\n", result);
        return 1;
    }
    if (dest[0] != '\0') {
        printf("  错误：空字符串复制结果不正确\n");
        return 1;
    }
    
    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试安全字符串连接功�? * 
 * @return 成功返回0，失败返�?
 */
static int test_string_concat(void) {
    printf("测试安全字符串连�?..\n");
    
    char dest[32] = "Hello";
    
    // 测试正常连接
    int result = string_concat(dest, " World", sizeof(dest));
    if (result != 11) {
        printf("  错误：正常连接失败，返回 %d\n", result);
        return 1;
    }
    if (strcmp(dest, "Hello World") != 0) {
        printf("  错误：连接结果不匹配: %s\n", dest);
        return 1;
    }
    
    // 测试缓冲区溢出保�?    char small_dest[10] = "Test";
    result = string_concat(small_dest, "VeryLongString", sizeof(small_dest));
    if (result != -1) {
        printf("  错误：缓冲区溢出未检测到，返�?%d\n", result);
        return 1;
    }
    if (strcmp(small_dest, "Test") != 0) {
        printf("  错误：源字符串被修改\n");
        return 1;
    }
    
    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试字符串比较功�? * 
 * @return 成功返回0，失败返�?
 */
static int test_string_compare(void) {
    printf("测试字符串比�?..\n");
    
    // 测试区分大小写比�?    int result = string_compare("Hello", "Hello", STRING_COMPARE_CASE_SENSITIVE);
    if (result != 0) {
        printf("  错误：相同字符串比较失败，返�?%d\n", result);
        return 1;
    }
    
    result = string_compare("Hello", "hello", STRING_COMPARE_CASE_SENSITIVE);
    if (result == 0) {
        printf("  错误：大小写敏感比较错误\n");
        return 1;
    }
    
    // 测试不区分大小写比较
    result = string_compare("Hello", "hello", STRING_COMPARE_CASE_INSENSITIVE);
    if (result != 0) {
        printf("  错误：大小写不敏感比较失败，返回 %d\n", result);
        return 1;
    }
    
    result = string_compare("Hello", "World", STRING_COMPARE_CASE_INSENSITIVE);
    if (result == 0) {
        printf("  错误：不同字符串比较错误\n");
        return 1;
    }
    
    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试字符串格式化功能
 * 
 * @return 成功返回0，失败返�?
 */
static int test_string_format(void) {
    printf("测试字符串格式化...\n");
    
    char buffer[64];
    
    // 测试简单格式化
    int result = string_format(buffer, sizeof(buffer), "Hello %s!", "World");
    if (result != 12) {
        printf("  错误：简单格式化失败，返�?%d\n", result);
        return 1;
    }
    if (strcmp(buffer, "Hello World!") != 0) {
        printf("  错误：格式化结果不匹�? %s\n", buffer);
        return 1;
    }
    
    // 测试数字格式�?    result = string_format(buffer, sizeof(buffer), "Number: %d", 42);
    if (result != 11) {
        printf("  错误：数字格式化失败，返�?%d\n", result);
        return 1;
    }
    if (strcmp(buffer, "Number: 42") != 0) {
        printf("  错误：数字格式化结果不匹�? %s\n", buffer);
        return 1;
    }
    
    // 测试缓冲区溢出保�?    char small_buffer[10];
    result = string_format(small_buffer, sizeof(small_buffer), "This is a very long string %d", 12345);
    if (result != -1) {
        printf("  错误：缓冲区溢出未检测到，返�?%d\n", result);
        return 1;
    }
    
    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试字符串查找功�? * 
 * @return 成功返回0，失败返�?
 */
static int test_string_find(void) {
    printf("测试字符串查�?..\n");
    
    const char* str = "Hello World";
    
    // 测试查找子字符串
    const char* result = string_find(str, "World", STRING_COMPARE_CASE_SENSITIVE);
    if (result == NULL) {
        printf("  错误：未找到子字符串\n");
        return 1;
    }
    if (strcmp(result, "World") != 0) {
        printf("  错误：查找结果不正确: %s\n", result);
        return 1;
    }
    
    // 测试查找不存在的子字符串
    result = string_find(str, "Universe", STRING_COMPARE_CASE_SENSITIVE);
    if (result != NULL) {
        printf("  错误：找到了不存在的子字符串\n");
        return 1;
    }
    
    // 测试查找字符
    result = string_find_char(str, 'o');
    if (result == NULL) {
        printf("  错误：未找到字符\n");
        return 1;
    }
    if (*result != 'o') {
        printf("  错误：字符查找结果不正确\n");
        return 1;
    }
    
    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试字符串分割功�? * 
 * @return 成功返回0，失败返�?
 */
static int test_string_split(void) {
    printf("测试字符串分�?..\n");
    
    const char* str = "apple,banana,cherry";
    
    // 测试分割字符�?    string_list_t list = {0};
    if (!string_split(str, ",", 0, &list)) {
        printf("  错误：字符串分割失败\n");
        return 1;
    }
    
    if (list.count != 3) {
        printf("  错误：分割结果数量不正确: %zu\n", list.count);
        string_list_clear(&list);
        return 1;
    }
    
    // 验证分割结果
    if (strncmp(list.items[0].data, "apple", list.items[0].length) != 0) {
        printf("  错误：第一个分割结果不正确\n");
        string_list_clear(&list);
        return 1;
    }
    
    if (strncmp(list.items[1].data, "banana", list.items[1].length) != 0) {
        printf("  错误：第二个分割结果不正确\n");
        string_list_clear(&list);
        return 1;
    }
    
    if (strncmp(list.items[2].data, "cherry", list.items[2].length) != 0) {
        printf("  错误：第三个分割结果不正确\n");
        string_list_clear(&list);
        return 1;
    }
    
    string_list_clear(&list);
    printf("  通过\n");
    return 0;
}

/**
 * @brief 测试字符串缓冲区功能
 * 
 * @return 成功返回0，失败返�?
 */
static int test_string_buffer(void) {
    printf("测试字符串缓冲区...\n");
    
    string_buffer_t buffer = {0};
    
    // 初始化缓冲区
    if (!string_buffer_init(&buffer, 32, STRING_ENCODING_UTF8)) {
        printf("  错误：缓冲区初始化失败\n");
        return 1;
    }
    
    // 测试追加字符�?    if (!string_buffer_append(&buffer, "Hello")) {
        printf("  错误：追加字符串失败\n");
        string_buffer_clear(&buffer);
        return 1;
    }
    
    if (buffer.length != 5) {
        printf("  错误：缓冲区长度不正�? %zu\n", buffer.length);
        string_buffer_clear(&buffer);
        return 1;
    }
    
    if (strcmp(buffer.data, "Hello") != 0) {
        printf("  错误：缓冲区内容不正�? %s\n", buffer.data);
        string_buffer_clear(&buffer);
        return 1;
    }
    
    // 测试追加更多内容
    if (!string_buffer_append(&buffer, " World")) {
        printf("  错误：追加更多内容失败\n");
        string_buffer_clear(&buffer);
        return 1;
    }
    
    if (strcmp(buffer.data, "Hello World") != 0) {
        printf("  错误：最终缓冲区内容不正�? %s\n", buffer.data);
        string_buffer_clear(&buffer);
        return 1;
    }
    
    string_buffer_clear(&buffer);
    printf("  通过\n");
    return 0;
}

/**
 * @brief 主测试函�? * 
 * @return 成功返回0，失败返�?
 */
int main(void) {
    printf("开始统一字符串模块单元测试\n");
    printf("==========================\n");
    
    int total_failures = 0;
    
    // 运行所有测�?    total_failures += test_string_copy();
    total_failures += test_string_concat();
    total_failures += test_string_compare();
    total_failures += test_string_format();
    total_failures += test_string_find();
    total_failures += test_string_split();
    total_failures += test_string_buffer();
    
    printf("==========================\n");
    if (total_failures == 0) {
        printf("所有测试通过！\n");
        return 0;
    } else {
        printf("测试失败�?d 个测试未通过\n", total_failures);
        return 1;
    }
}