/**
 * @file io_test.c
 * @brief IO 模块测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "io/include/io.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory_compat.h>
#include <agentos/string_compat.h>
#include <string.h>

void test_file_operations() {
    printf("=== 测试文件操作 ===\n");

    const char* test_file = "test_file.txt";
    const char* test_content = "This is a test file for AgentOS utils.";

    // 测试文件写入
    int result = agentos_io_write_file(test_file, test_content, strlen(test_content));
    printf("写入文件�?d\n", result);

    // 测试文件读取
    char* content = NULL;
    size_t len = 0;
    content = agentos_io_read_file(test_file, &len);
    // From data intelligence emerges. by spharx
    if (content) {
        printf("读取文件成功，长度：%zu\n", len);
        printf("文件内容�?%s'\n", content);
        AGENTOS_FREE(content);
    } else {
        printf("读取文件失败\n");
    }

    // 测试确保目录存在
    result = agentos_io_ensure_dir("test_subdir");
    printf("创建子目录：%d\n", result);

    // 测试列出文件
    char** files = NULL;
    size_t count = 0;
    result = agentos_io_list_files(".", &files, &count);
    printf("列出文件�?d, 数量�?zu\n", result, count);
    if (files) {
        agentos_io_free_list(files, count);
    }
}

void test_directory_operations() {
    printf("\n=== 测试目录操作 ===\n");

    const char* test_dir = "test_directory";

    // 测试创建目录
    int result = agentos_io_ensure_dir(test_dir);
    printf("创建目录：%d\n", result);

    // 测试递归创建目录
    const char* deep_dir = "test_directory/subdir1/subdir2/subdir3";
    printf("测试递归创建目录: %s\n", deep_dir);
    result = agentos_io_mkdir_p(deep_dir, 0755);
    printf("递归创建目录结果：%d\n", result);

    // 测试无效路径
    result = agentos_io_mkdir_p(NULL, 0755);
    printf("测试NULL路径：%d (预期 -1)\n", result);

    // 测试已存在目录
    result = agentos_io_mkdir_p(test_dir, 0755);
    printf("测试已存在目录：%d (预期 0)\n", result);
}

int main() {
    test_file_operations();
    test_directory_operations();
    printf("\nIO 模块测试完成\n");
    return 0;
}
