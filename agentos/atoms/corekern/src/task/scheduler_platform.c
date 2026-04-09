/**
 * @file scheduler_platform.c
 * @brief ﻟﺍﮒﭦ۵ﮒ۷ﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﺏ۷ﮒﻛﺕﻝ؟۰ﻝ
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * ﮔ؛ﮔ۷۰ﮒﮒ؟ﻝﺍﮒﺗﺏﮒﺍﻠﻠﮒ۷ﻝﮔﺏ۷ﮒﻙﻝ؟۰ﻝﮒﻟ۹ﮒ۷ﮒﮒ۶ﮒﮒﻟﺛﻙ? * ﮔ ﺗﮔ؟ﻝﺙﻟﺁﮒﺗﺏﮒﺍﻟ۹ﮒ۷ﻠﮔ۸WindowsﮔPOSIXﻠﻠﮒ۷ﺅﺙﮒﺗﭘﮔﻛﺝﻝﭨﻛﺕﻝﮔ۴ﮒ۲ﻟ؟ﺟﻠ؟ﻙ? */

#include "scheduler_platform.h"
#include <stddef.h>

/* ==================== ﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠﮒ۲ﺍﮔ ==================== */

/* Windowsﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠﮒ۲ﺍﮔ */
#if AGENTOS_PLATFORM_WINDOWS
extern const scheduler_platform_ops_t* scheduler_platform_get_windows_ops(void);
#endif

/* POSIXﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠﮒ۲ﺍﮔ */
#if AGENTOS_PLATFORM_POSIX
extern const scheduler_platform_ops_t* scheduler_platform_get_posix_ops(void);
#endif

/* ==================== ﻠﮔﮒ۷ﮒﺎﻝﭘﮔ?==================== */

/** @brief ﮒﺛﮒﮔﺏ۷ﮒﻝﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠ */
static const scheduler_platform_ops_t* g_current_platform_ops = NULL;

/** @brief ﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮒﮒ۶ﮒﻝﭘﮔ?*/
static int g_platform_initialized = 0;

/* ==================== ﮒﻠ۷ﻟﺝﮒ۸ﮒﺛﮔﺍ ==================== */

/**
 * @brief ﻟ۹ﮒ۷ﮔ۲ﮔﭖﮒﺗﺏﮒﺍﮒﺗﭘﻟﺟﮒﮒﺁﺗﮒﭦﻝﻠﻠﮒ۷ﮔﻛﺛﻠ
 * 
 * ﮔ ﺗﮔ؟ﻝﺙﻟﺁﮔﭘﮒ؟ﻛﺗﻝﮒﺗﺏﮒﺍﮒ؟ﺅﺙﻟﺟﮒﮒﺁﺗﮒﭦﻝﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠﻙ? * 
 * @return ﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠﮔﻠﺅﺙﮒ۳ﺎﻟﺑ۴ﻟﺟﮒNULL
 */
static const scheduler_platform_ops_t* detect_platform_ops(void)
{
#if AGENTOS_PLATFORM_WINDOWS
    return scheduler_platform_get_windows_ops();
#elif AGENTOS_PLATFORM_POSIX
    return scheduler_platform_get_posix_ops();
#else
    /* ﮔ۹ﻝ۴ﮒﺗﺏﮒﺍ */
    return NULL;
#endif
}

/* ==================== ﮒ؛ﮒﺎAPIﮒ؟ﻝﺍ ==================== */

/**
 * @brief ﮔﺏ۷ﮒﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠ
 * 
 * ﮔﺏ۷ﮒﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠﺅﺙﻟ۵ﻝﻟ۹ﮒ۷ﮔ۲ﮔﭖﻝﻝﭨﮔﻙ? * ﻠﮒﺕﺕﮒ۷ﻝﺏﭨﻝﭨﮒﮒ۶ﮒﮔﭘﻟﺍﻝ۷ﻙ? * 
 * @param ops ﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠ
 */
void scheduler_platform_register_ops(const scheduler_platform_ops_t* ops)
{
    /* ﮒ۵ﮔﮒﺓﺎﻝﭨﮒﮒ۶ﮒﺅﺙﮒﮔﻝﭨﮔﺏ۷ﮒﮔﺍﻝﮔﻛﺛﻠ */
    if (g_platform_initialized) {
        return;
    }
    
    g_current_platform_ops = ops;
}

/**
 * @brief ﻟﺓﮒﮒﺛﮒﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠ
 * 
 * ﻟﺟﮒﮒﺛﮒﮔﺏ۷ﮒﻝﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠﻙ? * ﮒ۵ﮔﮔ۹ﮔﺝﮒﺙﮔﺏ۷ﮒﺅﺙﮒﻟﺟﮒﻟ۹ﮒ۷ﮔ۲ﮔﭖﮒﺍﻝﮔﻛﺛﻠﻙ? * 
 * @return ﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠﺅﺙﮔ۹ﮔﺏ۷ﮒﻟﺟﮒNULL
 */
const scheduler_platform_ops_t* scheduler_platform_get_ops(void)
{
    /* ﮒ۵ﮔﮔ۹ﮔﺝﮒﺙﮔﺏ۷ﮒﺅﺙﮒﺍﻟﺁﻟ۹ﮒ۷ﮔ۲ﮔﭖ?*/
    if (!g_current_platform_ops) {
        g_current_platform_ops = detect_platform_ops();
    }
    
    return g_current_platform_ops;
}

/**
 * @brief ﮒﮒ۶ﮒﮒﺗﺏﮒﺍﻠﻠﮒ۷ﺅﺙﻟ۹ﮒ۷ﻠﮔ۸ﺅﺙ? * 
 * ﻟ۹ﮒ۷ﮔ۲ﮔﭖﮒﺗﺏﮒﺍﮒﺗﭘﮒﮒ۶ﮒﮒﺁﺗﮒﭦﻝﻠﻠﮒ۷ﻙ? * ﮒ۵ﮔﮒﺓﺎﻝﭨﮒﮒ۶ﮒﺅﺙﮒﻝﺑﮔ۴ﻟﺟﮒﮔﮒﻙ? * 
 * @return 0 ﮔﮒﺅﺙ?1 ﮒ۳ﺎﻟﺑ۴
 */
int scheduler_platform_auto_init(void)
{
    /* ﮒ۵ﮔﮒﺓﺎﻝﭨﮒﮒ۶ﮒﺅﺙﻝﺑﮔ۴ﻟﺟﮒﮔﮒ */
    if (g_platform_initialized) {
        return 0;
    }
    
    /* ﻟﺓﮒﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﻛﺛﻠ */
    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (!ops) {
        /* ﮔ ﮔﺏﮔ۲ﮔﭖﮒﺍﮒﺗﺏﮒﺍﻠﻠﮒ?*/
        return -1;
    }
    
    /* ﮔ۲ﮔ۴ﮔﻛﺛﻠﮒ؟ﮔﺑﮔ?*/
    if (!ops->init || !ops->cleanup || !ops->thread_create || 
        !ops->thread_join || !ops->thread_set_priority ||
        !ops->get_current_thread_id || !ops->get_thread_system_id ||
        !ops->thread_sleep || !ops->thread_yield ||
        !ops->cleanup_platform_resources || !ops->get_name) {
        /* ﮔﻛﺛﻠﻛﺕﮒ؟ﮔﺑ */
        return -1;
    }
    
    /* ﮒﮒ۶ﮒﮒﺗﺏﮒﺍﻠﻠﮒ?*/
    if (ops->init() != 0) {
        return -1;
    }
    
    /* ﮔ ﻟ؟ﺍﻛﺕﭦﮒﺓﺎﮒﮒ۶ﮒ?*/
    g_platform_initialized = 1;
    
    return 0;
}

/**
 * @brief ﮔ۲ﮔ۴ﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﺁﮒ۵ﮒﺓﺎﮒﮒ۶ﮒ? * 
 * ﮔ۲ﮔ۴ﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮔﺁﮒ۵ﮒﺓﺎﻝﭨﮔﮒﮒﮒ۶ﮒﻙ? * 
 * @return 1 ﮒﺓﺎﮒﮒ۶ﮒﺅﺙ? ﮔ۹ﮒﮒ۶ﮒ
 */
int scheduler_platform_is_initialized(void)
{
    return g_platform_initialized;
}

/**
 * @brief ﮔﺕﻝﮒﺗﺏﮒﺍﻠﻠﮒ? * 
 * ﮔﺕﻝﮒﺗﺏﮒﺍﻠﻠﮒ۷ﻟﭖﮔﭦﻙ? * ﮒ۵ﮔﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮒﺓﺎﮔﺏ۷ﮒﺅﺙﮒﻟﺍﻝ۷ﮒﭘcleanupﮔﻛﺛﻙ? */
void scheduler_platform_cleanup(void)
{
    if (!g_platform_initialized) {
        return;
    }
    
    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (ops && ops->cleanup) {
        ops->cleanup();
    }
    
    g_platform_initialized = 0;
    g_current_platform_ops = NULL;
}

/**
 * @brief ﻟﺓﮒﮒﺛﮒﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮒﻝ۶? * 
 * ﻟﺓﮒﮒﺛﮒﮒﺗﺏﮒﺍﻠﻠﮒ۷ﻝﮒﻝ۶ﺍﮒ­ﻝ؛۵ﻛﺕﺎﻙ? * 
 * @return ﮒﺗﺏﮒﺍﻠﻠﮒ۷ﮒﻝ۶ﺍﮒ­ﻝ؛۵ﻛﺕﺎﺅﺙﮔ۹ﮒﮒ۶ﮒﻟﺟﮒ?unknown"
 */
const char* scheduler_platform_get_name(void)
{
    const scheduler_platform_ops_t* ops = scheduler_platform_get_ops();
    if (ops && ops->get_name) {
        return ops->get_name();
    }
    
    return "unknown";
}