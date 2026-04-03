/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_utils.h
 * @brief 网关模块公共工具函数
 *
 * 提供各网关实现共用的辅助函数，
 * 避免代码重复和定义冲突。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef GATEWAY_UTILS_H
#define GATEWAY_UTILS_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取当前单调时钟时间（纳秒精度）
 *
 * 跨平台的高精度时间获取函数。
 * Windows 使用 FILETIME，POSIX 使用 clock_gettime。
 *
 * @return 当前时间戳（纳秒），基于 CLOCK_REALTIME/系统启动时间
 *
 * @threadsafe 安全（只读操作）
 * @note 精度：Windows ~100ns，POSIX ~1ns（取决于硬件）
 */
static inline uint64_t gateway_time_ns(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* FILETIME 是从 1601-01-01 开始的 100ns 单位数 */
    /* 转换为从 1970-01-01 开始的纳秒 */
    return (uli.QuadPart - 116444736000000000ULL) * 100;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

/**
 * @brief 跨平台 sleep 函数
 *
 * @param seconds 睡眠秒数
 */
static inline void gateway_sleep(unsigned int seconds) {
#ifdef _WIN32
    Sleep(seconds * 1000);
#else
    sleep(seconds);
#endif
}

/**
 * @brief 计算经过的时间（纳秒）
 *
 * @param start_ns 起始时间戳
 * @return 从 start_ns 到现在经过的纳秒数
 */
static inline uint64_t gateway_elapsed_ns(uint64_t start_ns) {
    return gateway_time_ns() - start_ns;
}

/**
 * @brief 将纳秒转换为毫秒
 *
 * @param ns 纳秒值
 * @return 毫秒值
 */
static inline uint64_t gateway_ns_to_ms(uint64_t ns) {
    return ns / 1000000ULL;
}

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_UTILS_H */
