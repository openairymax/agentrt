/**
 * @file platform.c
 * @brief 平台检测实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "core.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

/**
 * @brief 获取平台名称
 * @return "windows", "linux", "macos", "unknown"
 */
const char* agentos_core_get_platform(void) {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

/**
 * @brief 获取CPU核心数
 * @return CPU核心数，失败返回 -1
 */
int agentos_core_get_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int)sysinfo.dwNumberOfProcessors;
#else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return (int)nprocs;
#endif
}

/**
 * @brief 获取内存信息
 * @param out_total 总内存（字节）
 * @param out_available 可用内存（字节）
 * @param out_used 已用内存（字节）
 * @param out_percent 使用百分比（0-100）
 * @return 0 成功，-1 失败
 */
int agentos_core_get_memory_info(size_t* out_total, size_t* out_available, size_t* out_used, float* out_percent) {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) return -1;
    if (out_total) *out_total = (size_t)status.ullTotalPhys;
    if (out_available) *out_available = (size_t)status.ullAvailPhys;
    if (out_used) *out_used = (size_t)(status.ullTotalPhys - status.ullAvailPhys);
    if (out_percent) *out_percent = (float)status.dwMemoryLoad;
    return 0;
#else
    struct sysinfo info;
    if (sysinfo(&info) != 0) return -1;
    size_t total = info.totalram * info.mem_unit;
    size_t free = info.freeram * info.mem_unit;
    if (out_total) *out_total = total;
    if (out_available) *out_available = free;
    if (out_used) *out_used = total - free;
    if (out_percent) *out_percent = (float)(total - free) / total * 100.0f;
    return 0;
#endif
}