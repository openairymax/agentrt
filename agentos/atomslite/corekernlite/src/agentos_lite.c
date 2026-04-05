/**
 * @file agentos_lite.c
 * @brief AgentOS Lite 内核初始化和版本管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos_lite.h"

/* ==================== 版本信息 ==================== */

#define AGENTOS_LITE_VERSION_MAJOR 1
#define AGENTOS_LITE_VERSION_MINOR 0
#define AGENTOS_LITE_VERSION_PATCH 0
#define AGENTOS_LITE_VERSION_BUILD 6

static const char* g_version_string = "AgentOS Lite v1.0.0.6";

/* ==================== 全局状态 ==================== */

static int g_initialized = 0;

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化AgentOS Lite内核
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_init(void) {
    if (g_initialized) {
        return AGENTOS_LITE_SUCCESS;
    }
    
    /* 初始化内存子系统（使用默认配置） */
    agentos_lite_error_t err = agentos_lite_mem_init(0);
    if (err != AGENTOS_LITE_SUCCESS) {
        return err;
    }
    
    g_initialized = 1;
    return AGENTOS_LITE_SUCCESS;
}

/**
 * @brief 清理AgentOS Lite内核
 */
AGENTOS_LITE_API void agentos_lite_cleanup(void) {
    if (!g_initialized) {
        return;
    }
    
    /* 清理内存子系统 */
    agentos_lite_mem_cleanup();
    
    g_initialized = 0;
}

/**
 * @brief 获取AgentOS Lite版本信息
 */
AGENTOS_LITE_API const char* agentos_lite_version(void) {
    return g_version_string;
}