/**
 * @file time.c
 * @brief AgentOS Lite 时间管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 提供跨平台的高精度时间管理功能。
 */

#include "../include/time.h"
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    
    static LARGE_INTEGER g_qpc_freq = {0};
    static int g_time_initialized = 0;
    
    AGENTOS_LITE_API agentos_lite_error_t agentos_lite_time_init(void) {
        if (g_time_initialized) {
            return AGENTOS_LITE_SUCCESS;
        }
        
        if (!QueryPerformanceFrequency(&g_qpc_freq)) {
            return AGENTOS_LITE_EIO;
        }
        
        g_time_initialized = 1;
        return AGENTOS_LITE_SUCCESS;
    }
    
    AGENTOS_LITE_API void agentos_lite_time_cleanup(void) {
        g_time_initialized = 0;
        memset(&g_qpc_freq, 0, sizeof(g_qpc_freq));
    }
    
    AGENTOS_LITE_API uint64_t agentos_lite_time_get_ns(void) {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return (uint64_t)((counter.QuadPart * 1000000000ULL) / g_qpc_freq.QuadPart);
    }
    
    AGENTOS_LITE_API uint64_t agentos_lite_time_get_us(void) {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return (uint64_t)((counter.QuadPart * 1000000ULL) / g_qpc_freq.QuadPart);
    }
    
    AGENTOS_LITE_API uint64_t agentos_lite_time_get_ms(void) {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return (uint64_t)((counter.QuadPart * 1000ULL) / g_qpc_freq.QuadPart);
    }
    
    AGENTOS_LITE_API uint64_t agentos_lite_time_get_unix(void) {
        FILETIME ft;
        ULARGE_INTEGER uli;
        
        GetSystemTimeAsFileTime(&ft);
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        
        return (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;
    }
    
#else
    #include <time.h>
    #include <sys/time.h>
    
    static int g_time_initialized = 0;
    
    AGENTOS_LITE_API agentos_lite_error_t agentos_lite_time_init(void) {
        if (g_time_initialized) {
            return AGENTOS_LITE_SUCCESS;
        }
        g_time_initialized = 1;
        return AGENTOS_LITE_SUCCESS;
    }
    
    AGENTOS_LITE_API void agentos_lite_time_cleanup(void) {
        g_time_initialized = 0;
    }
    
    AGENTOS_LITE_API uint64_t agentos_lite_time_get_ns(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
    
    AGENTOS_LITE_API uint64_t agentos_lite_time_get_us(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
    }
    
    AGENTOS_LITE_API uint64_t agentos_lite_time_get_ms(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
    }
    
    AGENTOS_LITE_API uint64_t agentos_lite_time_get_unix(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t)tv.tv_sec;
    }
    
#endif

AGENTOS_LITE_API uint64_t agentos_lite_time_diff_ns(uint64_t start, uint64_t end) {
    if (end >= start) {
        return end - start;
    }
    return 0;
}

AGENTOS_LITE_API uint64_t agentos_lite_time_diff_us(uint64_t start, uint64_t end) {
    if (end >= start) {
        return end - start;
    }
    return 0;
}

AGENTOS_LITE_API uint64_t agentos_lite_time_diff_ms(uint64_t start, uint64_t end) {
    if (end >= start) {
        return end - start;
    }
    return 0;
}
