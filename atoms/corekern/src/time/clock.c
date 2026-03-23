/**
 * @file clock.c
 * @brief 跨平台时钟源实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "time.h"
#include <stdint.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

static uint64_t get_qpc_freq_ns(void) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (uint64_t)(1000000000ULL / freq.QuadPart);
}

uint64_t agentos_time_monotonic_ns(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)counter.QuadPart * get_qpc_freq_ns();
}

uint64_t agentos_time_monotonic_ms(void) {
    return agentos_time_monotonic_ns() / 1000000ULL;
}

uint64_t agentos_time_current_ns(void) {
    FILETIME ft;
    GetSystemTimePreciseAsFileTime(&ft);
    uint64_t ns = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    ns -= 116444736000000000ULL;
    return ns * 100;
}

uint64_t agentos_time_current_ms(void) {
    return agentos_time_current_ns() / 1000000ULL;
}

agentos_error_t agentos_time_nanosleep(uint64_t ns) {
    HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (!timer) return AGENTOS_EIO;

    LARGE_INTEGER due;
    due.QuadPart = -(int64_t)(ns / 100);
    if (!SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE)) {
        CloseHandle(timer);
        return AGENTOS_EIO;
    }

    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
    return AGENTOS_SUCCESS;
}

#else

#include <time.h>

uint64_t agentos_time_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

uint64_t agentos_time_monotonic_ms(void) {
    return agentos_time_monotonic_ns() / 1000000ULL;
}

uint64_t agentos_time_current_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

uint64_t agentos_time_current_ms(void) {
    return agentos_time_current_ns() / 1000000ULL;
}

agentos_error_t agentos_time_nanosleep(uint64_t ns) {
    struct timespec ts;
    ts.tv_sec = ns / 1000000000ULL;
    ts.tv_nsec = ns % 1000000000ULL;
    if (nanosleep(&ts, NULL) != 0) return AGENTOS_EINTR;
    return AGENTOS_SUCCESS;
}

#endif
