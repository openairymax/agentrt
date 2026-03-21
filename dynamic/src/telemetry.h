/**
 * @file telemetry.h
 * @brief 可观测性集成（指标、追踪）
 */

#ifndef BASERUNTIME_TELEMETRY_H
#define BASERUNTIME_TELEMETRY_H

typedef struct telemetry telemetry_t;

/**
 * @brief 创建可观测性实例
 * @return 句柄，失败返回 NULL
 */
telemetry_t* telemetry_create(void);

/**
 * @brief 销毁可观测性实例
 */
void telemetry_destroy(telemetry_t* tel);

/**
 * @brief 导出当前指标为 JSON 字符串
 * @param tel 句柄
 * @return 字符串，需调用者 free，失败返回 NULL
 */
char* telemetry_export_metrics(telemetry_t* tel);

/**
 * @brief 导出当前追踪为 JSON 字符串
 * @return 字符串，需调用者 free，失败返回 NULL
 */
char* telemetry_export_traces(void);

#endif /* HABITAT_TELEMETRY_H */