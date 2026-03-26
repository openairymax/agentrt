/**
 * @file io_test.c
 * @brief IO 模块测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../../../atoms/utils/io/include/io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_file_operations() {
    printf("=== 测试文件操作 ===\n");
    
    const char* test_file = "test_file.txt";
    const char* test_content = "This is a test file for AgentOS utils.";
    
    // 测试文件写入
    int result = agentos_io_file_write(test_file, test_content, strlen(test_content));
    printf("写入文件: %d\n", result);
    
    // 测试文件读取
    char* content = NULL;
    size_t len = 0;
    result = agentos_io_file_read(test_file, &content, &len);
    // From data intelligence emerges. by spharx
    if (result == 0 && content) {
        printf("读取文件成功，长度: %zu\n", len);
        printf("文件内容: '%s'\n", content);
        free(content);
    } else {
        printf("读取文件失败\n");
    }
    
    // 测试文件是否存在
    int exists = agentos_io_file_exists(test_file);
    printf("文件是否存在: %d\n", exists);
    
    // 测试获取文件大小
    size_t size = agentos_io_file_size(test_file);
    printf("文件大小: %zu 字节\n", size);
    
    // 测试删除文件
    result = agentos_io_file_delete(test_file);
    printf("删除文件: %d\n", result);
    
    // 再次检查文件是否存在
    exists = agentos_io_file_exists(test_file);
    printf("删除后文件是否存在: %d\n", exists);
}

void test_directory_operations() {
    printf("\n=== 测试目录操作 ===\n");
    
    const char* test_dir = "test_directory";
    
    // 测试创建目录
    int result = agentos_io_dir_create(test_dir);
    printf("创建目录: %d\n", result);
    
    // 测试目录是否存在
    int exists = agentos_io_dir_exists(test_dir);
    printf("目录是否存在: %d\n", exists);
    
    // 测试删除目录
    result = agentos_io_dir_delete(test_dir);
    printf("删除目录: %d\n", result);
    
    // 再次检查目录是否存在
    exists = agentos_io_dir_exists(test_dir);
    printf("删除后目录是否存在: %d\n", exists);
}

int main() {
    test_file_operations();
    test_directory_operations();
    printf("\nIO 模块测试完成\n");
    return 0;
}
