#ifndef OBSERVABILITY_COMPAT_H
#define OBSERVABILITY_COMPAT_H

#include "observability.h"

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 类型别名 */
typedef agentrt_metrics_t agentrt_observability_t;

/* 指标类型常量（兼容旧API）- 供需要整数值的API使用 */
#ifndef AGENTRT_OBSERVABILITY_METRIC_MACROS_DEFINED
#define AGENTRT_OBSERVABILITY_METRIC_MACROS_DEFINED
#define AGENTRT_METRIC_COUNTER 0
#define AGENTRT_METRIC_GAUGE 1
#define AGENTRT_METRIC_HISTOGRAM 2
#endif /* AGENTRT_OBSERVABILITY_METRIC_MACROS_DEFINED */

/* 函数映射：创建 */
static inline agentrt_observability_t *agentrt_observability_create(void)
{
    return agentrt_metrics_create();
}

/* 函数映射：销毁 */
static inline void agentrt_observability_destroy(agentrt_observability_t *obs)
{
    agentrt_metrics_destroy(obs);
}

/* 函数映射：注册指标（新API不需要注册，直接忽略） */
static inline int agentrt_observability_register_metric(agentrt_observability_t *obs,
                                                        const char *name, int type,
                                                        const char *desc)
{
    if (!obs || !name)
        return AGENTRT_EINVAL;
    (void)desc;
    if (type == AGENTRT_METRIC_COUNTER) {
        agentrt_metrics_increment(obs, name, 0);
    } else if (type == AGENTRT_METRIC_GAUGE) {
        agentrt_metrics_gauge(obs, name, 0.0);
    }
    return AGENTRT_SUCCESS;
}

/* 函数映射：计数器增加 */
static inline void agentrt_observability_increment_counter(agentrt_observability_t *obs,
                                                           const char *label, int64_t value)
{
    agentrt_metrics_increment(obs, label, (uint64_t)value);
}

/* 函数映射：直方图记录（用timing替代） */
static inline void agentrt_observability_record_histogram(agentrt_observability_t *obs,
                                                          const char *name, double value)
{
    agentrt_metrics_timing(obs, name, value);
}

/* 函数映射：获取单调时间（纳秒） */
static inline uint64_t agentrt_get_monotonic_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#ifdef __cplusplus
}
#endif

#endif /* OBSERVABILITY_COMPAT_H */
