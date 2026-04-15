/**
 * @file stdbool.h
 * @brief 标准布尔类型定义 - 跨平台兼容层
 *
 * 提供C99标准<stdbool.h>的兼容实现，确保bool、true、false在所有平台上可用。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_COMPAT_STDBOOL_H
#define AGENTOS_COMPAT_STDBOOL_H

/* 首先尝试使用系统自带的<stdbool.h> */
#if defined(__has_include)
    #if __has_include(<stdbool.h>)
        #include <stdbool.h>
    #else
        /* 如果没有系统<stdbool.h>，提供最小实现 */
        #ifdef __cplusplus
            /* C++已有bool类型 */
        #else
            /* C语言需要定义bool类型 */
            #ifndef __bool_true_false_are_defined
                #define __bool_true_false_are_defined 1
                
                /* 使用_Bool作为基础类型（C99标准） */
                #ifndef __cplusplus
                    #define bool _Bool
                #else
                    /* C++中bool是内置类型 */
                    typedef bool _Bool;
                #endif
                
                #define true 1
                #define false 0
            #endif
        #endif
    #endif
#else
    /* 不支持__has_include的编译器，假设有<stdbool.h> */
    #include <stdbool.h>
#endif

#endif /* AGENTOS_COMPAT_STDBOOL_H */