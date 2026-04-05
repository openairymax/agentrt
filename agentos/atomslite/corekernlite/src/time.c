/**
 * @file time.c
 * @brief 轻量级时间管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "time.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#endif

/* ==================== 内部辅助函数 ==================== */

#ifdef _WIN32
/**
 * @brief 获取高精度计数器频率（Windows）
 */
static uint64_t get_frequency(void) {
    static LARGE_INTEGER frequency = {0};
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    return frequency.QuadPart;
}
#endif

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 获取当前时间（纳秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_ns(void) {
#ifdef _WIN32
    /* Windows：使用高精度性能计数器 */
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    
    uint64_t freq = get_frequency();
    if (freq == 0) return 0;
    
    /* 转换为纳秒 */
    uint64_t seconds = counter.QuadPart / freq;
    uint64_t fraction = counter.QuadPart % freq;
    return seconds * 1000000000ULL + (fraction * 1000000000ULL) / freq;
#else
    /* POSIX：使用 clock_gettime，CLOCK_MONOTONIC */
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    }
    
    /* 回退：使用 gettimeofday（实时时钟，可能受系统时间调整影响） */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000000ULL + (uint64_t)tv.tv_usec * 1000ULL;
#endif
}

/**
 * @brief 获取当前时间（微秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_us(void) {
    return agentos_lite_time_get_ns() / 1000ULL;
}

/**
 * @brief 获取当前时间（毫秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_ms(void) {
    return agentos_lite_time_get_ns() / 1000000ULL;
}

/**
 * @brief 获取Unix时间戳（秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_unix(void) {
#ifdef _WIN32
    /* Windows：使用 GetSystemTimeAsFileTime */
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    
    /* 转换为Unix时间戳（从1601-01-01到1970-01-01的100纳秒间隔数） */
    uint64_t windows_time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    uint64_t unix_time = windows_time / 10000000ULL - 11644473600ULL;
    return unix_time;
#else
    /* POSIX：使用 time() */
    return (uint64_t)time(NULL);
#endif
}

/**
 * @brief 计算时间差（纳秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_diff_ns(uint64_t start, uint64_t end) {
    return end - start;
}

/**
 * @brief 计算时间差（微秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_diff_us(uint64_t start, uint64_t end) {
    return (end - start) / 1000ULL;
}

/**
 * @brief 计算时间差（毫秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_diff_ms(uint64_t start, uint64_t end) {
    return (end - start) / 1000000ULL;
}