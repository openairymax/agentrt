/**
 * @file server.h
 * @brief Dynamic 内部数据结构声明
 */

#ifndef HABITAT_SERVER_H
#define HABITAT_SERVER_H

#include "dynamic.h"
#include "session.h"
#include "health.h"
#include "telemetry.h"
#include "gateway/gateway.h"
#include <pthread.h>
#include <stddef.h>

/**
 * @brief BaseRuntime 服务器实例
 */
struct dynamic_server {
    dynamic_config_t      config;          /**< 运行时配置 */
    session_manager_t*    session_mgr;     /**< 会话管理器 */
    health_checker_t*     health;          /**< 健康检查器 */
    telemetry_t*          telemetry;       /**< 可观测性 */
    gateway_t**           gateways;        /**< 网关数组 */
    size_t                gateway_count;   /**< 网关数量 */
    pthread_t             main_thread;     /**< 主线程 ID */
    volatile int          running;         /**< 运行标志 */
    pthread_mutex_t       lock;            /**< 保护状态变更 */
    pthread_cond_t        cond;            /**< 用于等待停止 */
};

extern struct dynamic_server* g_server;   /**< 全局实例（供信号处理使用） */

#endif /* HABITAT_SERVER_H */