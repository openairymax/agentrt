#ifndef OBSERVABILITY_COMPAT_H
#define OBSERVABILITY_COMPAT_H

#include "observability.h"
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 类型别名 */
typedef agentos_metrics_t agentos_observability_t;

/* 指标类型常量（兼容旧API）- 供需要整数值的API使用 */
#ifndef AGENTOS_OBSERVABILITY_METRIC_MACROS_DEFINED
#define AGENTOS_OBSERVABILITY_METRIC_MACROS_DEFINED
#define AGENTOS_METRIC_COUNTER   0
#define AGENTOS_METRIC_GAUGE     1
#define AGENTOS_METRIC_HISTOGRAM 2
#endif /* AGENTOS_OBSERVABILITY_METRIC_MACROS_DEFINED */

/* 函数映射：创建 */
static inline agentos_observability_t* agentos_observability_create(void) {
    return agentos_metrics_create();
}

/* 函数映射：销毁 */
static inline void agentos_observability_destroy(agentos_observability_t* obs) {
    agentos_metrics_destroy(obs);
}

/* 函数映射：注册指标（新API不需要注册，直接忽略） */
static inline int agentos_observability_register_metric(
    agentos_observability_t* obs, const char* name, int type, const char* desc) {
    (void)obs; (void)name; (void)type; (void)desc;
    return 0;
}

/* 函数映射：计数器增加 */
static inline void agentos_observability_increment_counter(
    agentos_observability_t* obs, const char* label, int64_t value) {
    agentos_metrics_increment(obs, label, (uint64_t)value);
}

/* 函数映射：直方图记录（用timing替代） */
static inline void agentos_observability_record_histogram(
    agentos_observability_t* obs, const char* name, double value) {
    agentos_metrics_timing(obs, name, value);
}

/* 函数映射：获取单调时间（纳秒） */
static inline uint64_t agentos_get_monotonic_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#ifdef __cplusplus
}
#endif

#endif /* OBSERVABILITY_COMPAT_H */
