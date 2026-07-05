/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_monitoring.h - Monitoring Interface: Prometheus / OpenTelemetry Export
 */

#ifndef CUPOLAS_MONITORING_H
#define CUPOLAS_MONITORING_H

#include "cupolas_metrics.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* SP06 解耦：cupolas 不再反向依赖 gateway 模块。
 * 以下类型定义了通用的 HTTP 端点请求/响应抽象，
 * 由调用方（如 gateway_d）负责适配到具体的 HTTP 服务器实现。 */

/** HTTP 端点请求（cupolas 视角的抽象） */
typedef struct cupolas_endpoint_request {
    void *user_data;  /**< 调用方在注册时传入的 user_data */
} cupolas_endpoint_request_t;

/** HTTP 端点响应（cupolas 视角的抽象） */
typedef struct cupolas_endpoint_response {
    int status_code;       /**< HTTP 状态码 */
    const char *content_type; /**< Content-Type 头 */
    char *body;            /**< 响应体（调用方负责释放） */
    size_t body_len;       /**< 响应体长度 */
} cupolas_endpoint_response_t;

/** HTTP 端点处理函数指针 */
typedef int (*cupolas_endpoint_handler_t)(const cupolas_endpoint_request_t *req,
                                          cupolas_endpoint_response_t *resp);

/** HTTP 端点注册函数指针（由调用方注入，如 gateway 适配器） */
typedef int (*cupolas_endpoint_register_fn_t)(void *server_handle,
                                              const char *method,
                                              const char *path,
                                              cupolas_endpoint_handler_t handler,
                                              void *user_data);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Monitoring backend types
 *
 * Design Principles:
 * - Standard protocols: Prometheus / OpenTelemetry / StatsD support
 * - Low overhead: Minimal impact on core logic
 * - High availability: Local cache + remote sync
 * - Async reporting: Non-blocking main business logic
 */
typedef enum monitoring_backend {
    MONITORING_BACKEND_NONE = 0,      /**< No monitoring backend */
    MONITORING_BACKEND_PROMETHEUS,    /**< Prometheus pull mode (/metrics endpoint) */
    MONITORING_BACKEND_OPENTELEMETRY, /**< OpenTelemetry push mode (OTLP protocol) */
    MONITORING_BACKEND_STATSD,        /**< StatsD traditional metrics collection */
    MONITORING_BACKEND_ALL            /**< Enable all backends */
} monitoring_backend_t;

/**
 * @brief Monitoring configuration structure
 */
typedef struct monitoring_config {
    monitoring_backend_t backend; /**< Active monitoring backend(s) */

    struct {
        const char *listen_addr;   /**< Listen address for HTTP server */
        uint16_t port;             /**< Port number for metrics endpoint */
        const char *endpoint;      /**< Metrics endpoint path */
        bool enable_tls;           /**< Enable TLS for metrics server */
        const char *tls_cert_file; /**< TLS certificate file path */
        const char *tls_key_file;  /**< TLS private key file path */
    } prometheus;

    struct {
        const char *endpoint;          /**< OTLP collector endpoint URL */
        const char *service_name;      /**< Service name for identification */
        const char *service_namespace; /**< Service namespace */
        const char *auth_token;        /**< Authentication token */
        bool enable_tls;               /**< Enable TLS for OTLP connection */
    } opentelemetry;

    struct {
        const char *host;   /**< StatsD server host */
        uint16_t port;      /**< StatsD server port */
        const char *prefix; /**< Metric name prefix */
    } statsd;

    uint32_t reporting_interval_ms; /**< Push reporting interval (milliseconds) */
    uint32_t buffer_size;           /**< Buffer size for metric data */
    bool enable_caching;            /**< Enable local metric caching */
} monitoring_config_t;

/**
 * @brief Monitoring status enumeration
 */
typedef enum monitoring_status {
    MONITORING_STATUS_STOPPED = 0, /**< Monitoring stopped */
    MONITORING_STATUS_STARTING,    /**< Monitoring starting up */
    MONITORING_STATUS_RUNNING,     /**< Monitoring active and running */
    MONITORING_STATUS_ERROR,       /**< Monitoring in error state */
    MONITORING_STATUS_STOPPING     /**< Monitoring shutting down */
} monitoring_status_t;

/**
 * @brief Health check result structure
 */
typedef struct health_check_result {
    bool healthy;          /**< Health status (true=healthy) */
    const char *component; /**< Component name being checked */
    const char *message;   /**< Status message or error description */
    uint64_t timestamp_ns; /**< Check timestamp in nanoseconds */
} health_check_result_t;

/**
 * @brief Monitoring handle (opaque)
 */
typedef struct cupolas_monitoring cupolas_monitoring_t;

/**
 * @brief Health check callback function type
 * @return true if healthy, false if unhealthy
 */
typedef bool (*health_check_fn_t)(void);

/* ============================================================================
 * Core API Functions
 * ============================================================================ */

/**
 * @brief Create monitoring manager instance
 * @param[in] config Monitoring configuration (NULL for default config)
 * @return Monitoring manager handle, NULL on failure
 * @note Thread-safe: Safe to call from main thread during initialization
 * @reentrant No
 * @ownership Returns owned pointer: caller must call cupolas_monitoring_destroy()
 * @ownership config: caller retains ownership
 */
cupolas_monitoring_t *cupolas_monitoring_create(const monitoring_config_t *config);

/**
 * @brief Destroy monitoring manager
 * @param[in] mgr Monitoring manager handle (may be NULL)
 * @note Thread-safe: Safe to call from main thread only
 * @reentrant No
 * @ownership mgr: transferred to this function, will be freed
 */
void cupolas_monitoring_destroy(cupolas_monitoring_t *mgr);

/**
 * @brief Start monitoring services
 * @param[in] mgr Monitoring manager handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from main thread only
 * @reentrant No
 */
int cupolas_monitoring_start(cupolas_monitoring_t *mgr);

/**
 * @brief Stop monitoring services
 * @param[in] mgr Monitoring manager handle
 * @note Thread-safe: Safe to call from main thread only
 * @reentrant No
 */
void cupolas_monitoring_stop(cupolas_monitoring_t *mgr);

/**
 * @brief Get current monitoring status
 * @param[in] mgr Monitoring manager handle
 * @return Current monitoring status
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
monitoring_status_t cupolas_monitoring_get_status(cupolas_monitoring_t *mgr);

/**
 * @brief Report metrics to configured backend (push mode)
 * @param[in] mgr Monitoring manager handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No (should be called periodically by single thread)
 */
int cupolas_monitoring_report(cupolas_monitoring_t *mgr);

/**
 * @brief Export metrics in Prometheus exposition format (pull mode)
 * @param[in] mgr Monitoring manager handle
 * @param[out] buffer Output buffer for Prometheus format metrics
 * @param[in] size Size of output buffer
 * @return Number of bytes written to buffer, or 0 on error
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership buffer: caller provides buffer, function writes to it
 *
 * @details
 * Exports metrics in standard Prometheus text exposition format:
 *
 * # HELP cupolas_permission_checks_total Total permission checks
 * # TYPE cupolas_permission_checks_total counter
 * cupolas_permission_checks_total{result="allowed"} 1234
 *
 * # HELP cupolas_sanitize_operations_total Total sanitize operations
 * # TYPE cupolas_sanitize_operations_total counter
 * cupolas_sanitize_operations_total{level="strict"} 5678
 *
 * # HELP cupolas_audit_events_total Total audit events logged
 * # TYPE cupolas_audit_events_total counter
 * cupolas_audit_events_total{service="tool_d"} 9012
 */
size_t cupolas_monitoring_export(cupolas_monitoring_t *mgr, char *buffer, size_t size);

/**
 * @brief Export metrics in OpenTelemetry OTLP JSON format
 * @param[in] mgr Monitoring manager handle
 * @param[out] buffer Output buffer for OTLP JSON format
 * @param[in] size Size of output buffer
 * @return Number of bytes written to buffer, or 0 on error
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership buffer: caller provides buffer, function writes to it
 */
size_t cupolas_monitoring_export_otlp(cupolas_monitoring_t *mgr, char *buffer, size_t size);

/**
 * @brief Register custom health check callback
 * @param[in] mgr Monitoring manager handle
 * @param[in] name Unique name for the health check
 * @param[in] callback Function pointer to health check implementation
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from main thread only
 * @reentrant No
 * @ownership name and callback: caller retains ownership
 */
int cupolas_monitoring_register_health_check(cupolas_monitoring_t *mgr, const char *name,
                                             health_check_fn_t callback);

/**
 * @brief Execute all registered health checks
 * @param[in] mgr Monitoring manager handle
 * @param[out] results Array of health check results (caller-allocated)
 * @param[in] max_results Maximum number of results to store
 * @return Actual number of results, negative on failure
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 * @ownership results: caller provides buffer, function writes to it
 */
int cupolas_monitoring_check_health(cupolas_monitoring_t *mgr, health_check_result_t *results,
                                    size_t max_results);

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * @brief Get Prometheus metrics endpoint address
 * @param[in] mgr Monitoring manager handle
 * @return Address string (e.g., "127.0.0.1:9090"), NULL if not running
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char *cupolas_monitoring_get_listen_addr(cupolas_monitoring_t *mgr);

/**
 * @brief Set metric filter patterns
 * @param[in] mgr Monitoring manager handle
 * @param[in] include_patterns Array of include patterns (NULL = include all)
 * @param[in] exclude_patterns Array of exclude patterns (NULL = exclude none)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from main thread only
 * @reentrant No
 */
int cupolas_monitoring_set_filter(cupolas_monitoring_t *mgr, const char **include_patterns,
                                  const char **exclude_patterns);

/**
 * @brief Get total number of registered metrics
 * @param[in] mgr Monitoring manager handle
 * @return Number of metrics being tracked
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
size_t cupolas_monitoring_get_metric_count(cupolas_monitoring_t *mgr);

/**
 * @brief Get timestamp of last successful report
 * @param[in] mgr Monitoring manager handle
 * @return Timestamp in nanoseconds since epoch
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
uint64_t cupolas_monitoring_get_last_report_time(cupolas_monitoring_t *mgr);

/**
 * @brief Get last error message
 * @param[in] mgr Monitoring manager handle
 * @return Error message string (static, do not free), NULL if no error
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char *cupolas_monitoring_get_last_error(cupolas_monitoring_t *mgr);

/* ============================================================================
 * Convenience Functions - Configuration Builders
 * ============================================================================ */

/**
 * @brief Create default Prometheus configuration
 * @param[in] port Port number for metrics HTTP server
 * @return Configuration structure (caller must free with monitoring_config_destroy())
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant Yes
 * @ownership Returns owned pointer: caller must call monitoring_config_destroy()
 */
monitoring_config_t *monitoring_config_create_prometheus(uint16_t port);

/**
 * @brief Create default OpenTelemetry configuration
 * @param[in] endpoint OTLP collector endpoint URL
 * @param[in] service_name Service name for identification
 * @return Configuration structure (caller must free with monitoring_config_destroy())
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant Yes
 * @ownership Returns owned pointer: caller must call monitoring_config_destroy()
 * @ownership endpoint and service_name: caller retains ownership
 */
monitoring_config_t *monitoring_config_create_opentelemetry(const char *endpoint,
                                                            const char *service_name);

/**
 * @brief Destroy configuration structure
 * @param[in] config Configuration to destroy (may be NULL)
 * @note Thread-safe: Safe to call from multiple threads
 * @reentrant No
 * @ownership config: transferred to this function, will be freed
 */
void monitoring_config_destroy(monitoring_config_t *config);

/* ============================================================================
 * String Conversion Functions
 * ============================================================================ */

/**
 * @brief Get backend type name string
 * @param[in] backend Backend type enumeration value
 * @return Backend name string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char *monitoring_backend_string(monitoring_backend_t backend);

/**
 * @brief Get status name string
 * @param[in] status Status enumeration value
 * @return Status name string (static, do not free)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
const char *monitoring_status_string(monitoring_status_t status);

/* ============================================================================
 * Singleton Pattern Functions
 * ============================================================================ */

/**
 * @brief Get singleton monitoring instance
 * @return Global monitoring instance (NULL if not initialized)
 * @note Thread-safe: Safe to call from multiple threads concurrently
 * @reentrant Yes
 */
cupolas_monitoring_t *cupolas_monitoring_get_instance(void);

/**
 * @brief Initialize singleton monitoring instance
 * @param[in] config Configuration (NULL for defaults)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Safe to call from main thread only
 * @reentrant No
 * @ownership config: caller retains ownership
 */
int cupolas_monitoring_init_instance(const monitoring_config_t *config);

/**
 * @brief Shutdown singleton monitoring instance
 * @note Thread-safe: Safe to call from main thread only
 * @reentrant No
 */
void cupolas_monitoring_shutdown_instance(void);

/**
 * @brief Register monitoring endpoints with an HTTP server
 *
 * Registers /metrics, /health, and /monitoring endpoints with the
 * specified HTTP server via a caller-injected registration callback.
 * After registration, the server's HTTP handler will serve monitoring
 * data, eliminating the need for a separate HTTP server in the monitoring
 * module.
 *
 * @param[in] mgr Monitoring manager handle
 * @param[in] server_handle Opaque server handle (e.g. gateway_t *)
 * @param[in] register_fn Endpoint registration callback injected by caller
 * @return 0 on success, negative on failure
 *
 * @note Must be called after cupolas_monitoring_start() and before server start
 * @note Thread-safe: Safe to call from main thread only
 * @reentrant No
 * @ownership mgr and server_handle: caller retains ownership
 *
 * @since 0.1.0
 * @changed SP06 (0.1.1): gateway_t *gw → void *server_handle + register_fn callback
 */
int cupolas_monitoring_register_endpoints(cupolas_monitoring_t *mgr,
                                           void *server_handle,
                                           cupolas_endpoint_register_fn_t register_fn);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_MONITORING_H */
