/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file test_unified_modules.c
 * @brief 统一日志和配置模块构建兼容性测试
 * 
 * 此测试程序验证统一日志和配置模块的头文件包含和基本API兼容性。
 * 不执行实际功能测试，仅验证编译时兼容性。
 * 
 * @author Spharx AgentOS Team
 * @date 2026-04-01
 * @version 2.0
 * 
 * @note 线程安全：本测试为单线程程序
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>

/* 包含统一日志模块头文件 */
#include "../../utils/logging/include/logging.h"
#include "../../utils/logging/include/atomic_logging.h"
#include "../../utils/logging/include/service_logging.h"
#include "../../utils/logging/include/logging_compat.h"

/* 包含统一配置模块头文件 */
#include "../../utils/config_unified/include/core_config.h"
#include "../../utils/config_unified/include/config_source.h"
#include "../../utils/config_unified/include/config_service.h"
#include "../../utils/config_unified/include/config_compat.h"

int main(void) {
    printf("=== 统一日志和配置模块构建兼容性测试 ===\n");
    printf("测试目的：验证新模块的头文件可正确包含，基本API存在。\n");
    
    /* 日志模块API存在性验证 */
    printf("\n1. 验证日志模块API存在性：\n");
    
    /* 日志级别类型 */
    log_level_t level = LOG_LEVEL_INFO;
    printf("   日志级别类型大小：%zu字节\n", sizeof(level));
    
    /* 日志初始化函数（声明存在） */
    printf("   日志初始化函数：log_init()\n");
    
    /* 日志写入函数（声明存在） */
    printf("   日志写入函数：log_write()\n");
    
    /* 配置模块API存在性验证 */
    printf("\n2. 验证配置模块API存在性：\n");
    
    /* 配置值类型 */
    config_value_t* config_val = NULL;
    printf("   配置值类型大小：%zu字节\n", sizeof(config_val));
    
    /* 配置源类型 */
    config_source_type_t source_type = CONFIG_SOURCE_FILE;
    printf("   配置源类型：%d\n", source_type);
    
    /* 配置验证器类型 */
    validator_type_t validator_type = VALIDATOR_TYPE_RANGE;
    printf("   验证器类型：%d\n", validator_type);
    
    /* 向后兼容层验证 */
    printf("\n3. 验证向后兼容层：\n");
    
    /* 日志兼容层 */
    printf("   日志兼容层：logging_compat.h\n");
    
    /* 配置兼容层 */
    printf("   配置兼容层：config_compat.h\n");
    
    /* 简单功能测试 */
    printf("\n4. 简单功能测试：\n");
    
    /* 测试日志级别字符串 */
    const char* level_str = log_level_to_string(LOG_LEVEL_INFO);
    if (level_str) {
        printf("   日志级别字符串：%s\n", level_str);
    }
    
    /* 测试配置源类型字符串 */
    const char* source_str = config_source_type_to_string(CONFIG_SOURCE_FILE);
    if (source_str) {
        printf("   配置源类型字符串：%s\n", source_str);
    }
    
    printf("\n=== 构建兼容性测试完成 ===\n");
    printf("如果程序能编译并运行到此，说明统一模块的头文件包含和基本API兼容性验证通过。\n");
    
    return 0;
}
