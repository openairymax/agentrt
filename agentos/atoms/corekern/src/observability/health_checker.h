/**
 * @file health_checker.h
 * @brief 健康检查器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_HEALTH_CHECKER_H
#define AGENTOS_HEALTH_CHECKER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum agentos_health_status {
    AGENTOS_HEALTH_UNKNOWN = 0,
    AGENTOS_HEALTH_HEALTHY = 1,
    AGENTOS_HEALTH_DEGRADED = 2,
    AGENTOS_HEALTH_UNHEALTHY = 3
} agentos_health_status_t;

typedef int (*agentos_health_check_cb)(void* user_data, agentos_health_status_t* status);

int agentos_health_checker_init(void);
void agentos_health_checker_cleanup(void);

int agentos_health_check_register(const char* name, 
                                  agentos_health_check_cb callback, 
                                  void* user_data);
int agentos_health_check_unregister(const char* name);
int agentos_health_export_status(char* buffer, size_t buffer_size);
int agentos_performance_get_metrics(double* out_cpu_usage, 
                                   double* out_memory_usage, 
                                   int* out_thread_count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_HEALTH_CHECKER_H */
