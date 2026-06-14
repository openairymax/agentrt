/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file prometheus_exporter.h
 * @brief C-L10: Prometheus scrape endpoint for monit_d
 *
 * 提供 /metrics HTTP 端点暴露 Prometheus 格式指标，
 * 包含 14 项必需指标的注册和导出。
 */

#ifndef AGENTOS_PROMETHEUS_EXPORTER_H
#define AGENTOS_PROMETHEUS_EXPORTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 生命周期 ==================== */

/**
 * @brief C-L10: 初始化 Prometheus exporter
 *
 * 初始化统一指标收集器并注册 14 项必需指标。
 * @param service_name 服务名称（如 "monit_d"）
 * @return 0 成功，非 0 失败
 */
int prometheus_exporter_init(const char *service_name);

/**
 * @brief C-L10: 关闭 Prometheus exporter
 */
void prometheus_exporter_shutdown(void);

/* ==================== 指标注册 ==================== */

/**
 * @brief C-L10: 注册 14 项必需指标
 *
 * 注册工程标准规范手册 16.1 定义的 10 项核心指标 +
 * 内存可观测性 4 项指标。
 * @return 0 成功，非 0 失败
 */
int prometheus_exporter_register_required_metrics(void);

/* ==================== HTTP 端点处理 ==================== */

/**
 * @brief C-L10: 处理 Prometheus scrape HTTP 请求
 *
 * 检测是否为 "GET /metrics" HTTP 请求，如果是则响应 Prometheus 格式指标。
 *
 * @param request 接收到的请求数据
 * @param request_len 请求数据长度
 * @param response 输出参数，HTTP 响应数据（需调用者释放）
 * @param response_len 输出参数，响应数据长度
 * @return 0 表示已处理（是 /metrics 请求），-1 表示非 Prometheus 请求（需其他处理）
 */
int prometheus_exporter_handle_http(const char *request, size_t request_len,
                                    char **response, size_t *response_len);

/* ==================== 指标值更新 ==================== */

/**
 * @brief 递增计数器指标
 */
void prometheus_counter_inc(const char *name, double value);

/**
 * @brief 设置 Gauge 指标值
 */
void prometheus_gauge_set(const char *name, double value);

/**
 * @brief 记录 Histogram 观察值
 */
void prometheus_histogram_observe(const char *name, double value);

/**
 * @brief 获取 Prometheus 格式的完整指标导出
 * @return Prometheus 格式字符串（需调用者释放），失败返回 NULL
 */
char *prometheus_exporter_get_metrics(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PROMETHEUS_EXPORTER_H */