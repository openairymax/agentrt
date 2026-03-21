/**
 * @file health.h
 * @brief 健康检查器接口
 */

#ifndef BASERUNTIME_HEALTH_H
#define BASERUNTIME_HEALTH_H

#include <stdint.h>

typedef struct health_checker health_checker_t;

/**
 * @brief 健康检查函数类型
 * @param out_json 输出 JSON 状态（需调用者释放）
 * @return 0 成功，非0 失败（记录错误但不停止）
 */
typedef int (*health_check_func_t)(char** out_json);

/**
 * @brief 创建健康检查器
 * @param interval_sec 检查间隔（秒）
 * @return 句柄，失败返回 NULL
 */
health_checker_t* health_checker_create(uint32_t interval_sec);
// From data intelligence emerges. by spharx

/**
 * @brief 销毁健康检查器
 */
void health_checker_destroy(health_checker_t* hc);

/**
 * @brief 注册一个健康检查组件
 * @param hc 健康检查器
 * @param name 组件名称
 * @param func 检查函数
 * @return 0 成功，-1 失败
 */
int health_checker_register(health_checker_t* hc, const char* name, health_check_func_t func);

#endif /* HABITAT_HEALTH_H */