/**
 * @file stdint.h
 * @brief 标准整数类型定义 - 跨平台兼容层
 *
 * 提供C99标准<stdint.h>的兼容实现，确保uint8_t、uint32_t、uint64_t等类型在所有平台上可用。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_COMPAT_STDINT_H
#define AGENTOS_COMPAT_STDINT_H

/* 首先尝试使用系统自带的<stdint.h> */
#if defined(__has_include)
    #if __has_include(<stdint.h>)
        #include <stdint.h>
    #else
        /* 如果没有系统<stdint.h>，提供最小实现 */
        #ifdef _MSC_VER
            /* MSVC编译器 */
            typedef unsigned __int8   uint8_t;
            typedef unsigned __int16  uint16_t;
            typedef unsigned __int32  uint32_t;
            typedef unsigned __int64  uint64_t;
            typedef __int8            int8_t;
            typedef __int16           int16_t;
            typedef __int32           int32_t;
            typedef __int64           int64_t;
        #else
            /* GCC/Clang等编译器 */
            typedef unsigned char     uint8_t;
            typedef unsigned short    uint16_t;
            typedef unsigned int      uint32_t;
            typedef unsigned long long uint64_t;
            typedef signed char       int8_t;
            typedef signed short      int16_t;
            typedef signed int        int32_t;
            typedef signed long long  int64_t;
        #endif
        
        /* 其他标准类型 */
        typedef uint64_t uintmax_t;
        typedef int64_t  intmax_t;
        typedef uint64_t uintptr_t;
        typedef int64_t  intptr_t;
        
        /* 限制宏 */
        #define INT8_MIN   (-128)
        #define INT16_MIN  (-32768)
        #define INT32_MIN  (-2147483647L - 1)
        #define INT64_MIN  (-9223372036854775807LL - 1)
        
        #define INT8_MAX   127
        #define INT16_MAX  32767
        #define INT32_MAX  2147483647L
        #define INT64_MAX  9223372036854775807LL
        
        #define UINT8_MAX  255U
        #define UINT16_MAX 65535U
        #define UINT32_MAX 4294967295UL
        #define UINT64_MAX 18446744073709551615ULL
    #endif
#else
    /* 不支持__has_include的编译器，假设有<stdint.h> */
    #include <stdint.h>
#endif

#endif /* AGENTOS_COMPAT_STDINT_H */