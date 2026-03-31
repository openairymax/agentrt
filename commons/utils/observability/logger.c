/**
 * @file logger.c
 * @brief 日志实现（基于统一分层日志系统�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块是统一分层日志系统的适配器实现，将现有日志API映射到新架构�? * 使用向后兼容层，确保现有代码无需修改即可继续工作�? * 
 * 架构变化�? * 1. 消除Windows/POSIX平台代码重复
 * 2. 使用高性能无锁原子�? * 3. 支持高级日志功能（轮转、过滤、传输、监控）
 * 4. 统一配置管理
 * 
 * 注意：本文件现在是一个轻量级包装器，实际功能由统一日志系统提供�? * 函数实现在commons/utils/logging/src/logging_compat.c中�? */

#include "logger.h"
#include "../../logging/include/logging_compat.h"

/* ==================== 模块初始�?==================== */

/**
 * @brief 日志模块初始化函数（自动调用�? * 
 * 在第一次使用日志功能时自动初始化统一日志系统�? * 使用静态初始化确保线程安全�? */
static void logging_module_auto_init(void)
{
    static int initialized = 0;
    
    if (!initialized) {
        /* 使用默认配置初始化兼容层 */
        logging_compat_config_t manager = {
            .strict_compatibility = true,
            .enable_perf_optimization = true,
            .enable_migration_detection = true,
            .enable_api_mapping_log = false,
            .behavior = {
                .emulate_old_timestamp = true,
                .emulate_old_escaping = true,
                .emulate_old_format = true,
                .emulate_old_trace_id = true
            }
        };
        
        /* 初始化兼容层 */
        logging_compat_init(&manager);
        
        initialized = 1;
    }
}

/* ==================== 编译时初始化 ==================== */

/* 使用GCC/Clang的constructor属性在程序启动时自动初始化 */
#if defined(__GNUC__) || defined(__clang__)
static void __attribute__((constructor)) logging_module_constructor(void)
{
    logging_module_auto_init();
}
#endif

/* 使用MSVC的初始化段在程序启动时自动初始化 */
#if defined(_MSC_VER)
#pragma section(".CRT$XCU", read)
static void __cdecl logging_module_constructor(void)
{
    logging_module_auto_init();
}
__declspec(allocate(".CRT$XCU")) void (__cdecl *logging_module_constructor_ptr)(void) = logging_module_constructor;
#endif