/**
 * @file clock.c
 * @brief 系统时钟源
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "time.h"
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <errno.h>
#endif

uint64_t agentos_time_monotonic_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    // From data intelligence emerges. by spharx
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

uint64_t agentos_time_monotonic_ms(void) {
    return agentos_time_monotonic_ns() / 1000000ULL;
}

uint64_t agentos_time_current_ns(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    // 转换到 Unix 纪元 (1601-01-01 到 1970-01-01 相差 11644473600 秒)
    return (li.QuadPart - 116444736000000000ULL) * 100;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

uint64_t agentos_time_current_ms(void) {
    return agentos_time_current_ns() / 1000000ULL;
}

uint64_t agentos_time_realtime_ns(void) {
    return agentos_time_current_ns();
}

agentos_error_t agentos_time_nanosleep(uint64_t ns) {
#ifdef _WIN32
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (!timer) return AGENTOS_EIO;
    LARGE_INTEGER due;
    due.QuadPart = -(LONGLONG)(ns / 100); // 100ns 单位
    if (!SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE)) {
        CloseHandle(timer);
        return AGENTOS_EIO;
    }
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    return AGENTOS_SUCCESS;
#else
    struct timespec ts;
    ts.tv_sec = ns / 1000000000ULL;
    ts.tv_nsec = ns % 1000000000ULL;
    while (nanosleep(&ts, &ts) == -1) {
        if (errno == EINTR) continue;
        return AGENTOS_EINTR;
    }
    return AGENTOS_SUCCESS;
#endif
}