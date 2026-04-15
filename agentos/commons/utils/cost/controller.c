/**
 * @file controller.c
 * @brief 预算控制器实现（跨平台）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现成本预算控制功能：
 * - 支持周期性能耗统�?
 * - 提供预算预警和限�?
 * - 线程安全的预算操�?
 */

#include "cost.h"
#include <stdlib.h>

#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <time.h>

#include "atomic_compat.h"

/* 平台特定头文件 */
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

/**
 * @brief 跨平台互斥锁类型
 */
#ifdef _WIN32
typedef CRITICAL_SECTION budget_ctrl_mutex_t;
#else
typedef pthread_mutex_t budget_ctrl_mutex_t;
#endif

/**
 * @brief 初始化互斥锁
 */
static int budget_ctrl_mutex_init(budget_ctrl_mutex_t* mutex) {
#ifdef _WIN32
    InitializeCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

/**
 * @brief 销毁互斥锁
 */
static void budget_ctrl_mutex_destroy(budget_ctrl_mutex_t* mutex) {
#ifdef _WIN32
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

/**
 * @brief 加锁
 */
static void budget_ctrl_mutex_lock(budget_ctrl_mutex_t* mutex) {
#ifdef _WIN32
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

/**
 * @brief 解锁
 */
static void budget_ctrl_mutex_unlock(budget_ctrl_mutex_t* mutex) {
#ifdef _WIN32
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

/**
 * @brief 预算控制器内部结�?
 */
struct agentos_budget_controller {
    double max_cost_usd;                     /**< 最大成本预算（美元�?*/
    double warning_threshold;                /**< 警告阈值（百分比） */
    atomic_double consumed_cost;             /**< 已消耗成�?*/
    atomic_double period_cost;              /**< 周期内消�?*/
    atomic_uint64_t request_count;         /**< 请求计数 */
    atomic_uint64_t denied_count;          /**< 拒绝计数 */
    budget_ctrl_mutex_t mutex;                 /**< 互斥�?*/
    time_t period_start;                   /**< 周期开始时�?*/
    uint32_t period_seconds;               /**< 周期时长（秒�?*/
    double average_cost;                   /**< 平均成本 */
};

/**
 * @brief 获取当前时间
 */
static time_t get_current_time(void) {
    return time(NULL);
}

/**
 * @brief 检查并重置周期
 */
static int check_and_reset_period(agentos_budget_controller_t* controller) {
    if (!controller) {
        return -1;
    }

    time_t now = get_current_time();

    if (now >= controller->period_start + (time_t)controller->period_seconds) {
        budget_ctrl_mutex_lock(&controller->mutex);

        if (now >= controller->period_start + (time_t)controller->period_seconds) {
            atomic_store(&controller->period_cost, 0.0);
            controller->period_start = now;
        }

        budget_ctrl_mutex_unlock(&controller->mutex);

        return 1;
    }

    return 0;
}

agentos_budget_controller_t* agentos_budget_controller_create(double max_cost_usd, uint32_t period_seconds) {
    if (max_cost_usd <= 0) {
        return NULL;
    }

    agentos_budget_controller_t* controller = (agentos_budget_controller_t*)AGENTOS_MALLOC(
        sizeof(agentos_budget_controller_t));
    if (!controller) {
        return NULL;
    }

    memset(controller, 0, sizeof(agentos_budget_controller_t));

    controller->max_cost_usd = max_cost_usd;
    controller->warning_threshold = 0.8;
    atomic_init(&controller->consumed_cost, 0.0);
    atomic_init(&controller->period_cost, 0.0);
    atomic_init(&controller->request_count, 0);
    atomic_init(&controller->denied_count, 0);

    if (budget_ctrl_mutex_init(&controller->mutex) != 0) {
        AGENTOS_FREE(controller);
        return NULL;
    }

    controller->period_start = get_current_time();
    controller->period_seconds = period_seconds;
    controller->average_cost = 0.0;

    return controller;
}

void agentos_budget_controller_destroy(agentos_budget_controller_t* controller) {
    if (!controller) {
        return;
    }

    budget_ctrl_mutex_destroy(&controller->mutex);
    AGENTOS_FREE(controller);
}

int agentos_budget_controller_consume(agentos_budget_controller_t* controller, double cost_usd) {
    if (!controller || cost_usd < 0) {
        return -1;
    }

    check_and_reset_period(controller);

    double current_period = atomic_load(&controller->period_cost);
    double current_total = atomic_load(&controller->consumed_cost);

    if (current_period + cost_usd > controller->max_cost_usd) {
        atomic_fetch_add(&controller->denied_count, 1);
        return -1;
    }

    if (current_total + cost_usd > controller->max_cost_usd * 100) {
        atomic_fetch_add(&controller->denied_count, 1);
        return -1;
    }

    double new_period = atomic_fetch_add_double(&controller->period_cost, cost_usd, memory_order_relaxed) + cost_usd;
    double new_total = atomic_fetch_add_double(&controller->consumed_cost, cost_usd, memory_order_relaxed) + cost_usd;
    atomic_fetch_add(&controller->request_count, 1);

    budget_ctrl_mutex_lock(&controller->mutex);

    uint64_t requests = atomic_load(&controller->request_count);
    if (requests > 0) {
        controller->average_cost = new_total / (double)requests;
    }

    budget_ctrl_mutex_unlock(&controller->mutex);

    if (new_period > controller->max_cost_usd * controller->warning_threshold) {
        return 1;
    }

    return 0;
}

double agentos_budget_controller_remaining(agentos_budget_controller_t* controller) {
    if (!controller) {
        return 0.0;
    }

    check_and_reset_period(controller);

    double period_cost = atomic_load(&controller->period_cost);
    double remaining = controller->max_cost_usd - period_cost;

    return remaining > 0 ? remaining : 0.0;
}

double agentos_budget_controller_consumed(agentos_budget_controller_t* controller) {
    if (!controller) {
        return 0.0;
    }

    return atomic_load(&controller->consumed_cost);
}

double agentos_budget_controller_period_consumed(agentos_budget_controller_t* controller) {
    if (!controller) {
        return 0.0;
    }

    check_and_reset_period(controller);

    return atomic_load(&controller->period_cost);
}

uint64_t agentos_budget_controller_requests(agentos_budget_controller_t* controller) {
    if (!controller) {
        return 0;
    }

    return atomic_load(&controller->request_count);
}

uint64_t agentos_budget_controller_denied(agentos_budget_controller_t* controller) {
    if (!controller) {
        return 0;
    }

    return atomic_load(&controller->denied_count);
}

int agentos_budget_controller_set_warning(agentos_budget_controller_t* controller, double threshold) {
    if (!controller || threshold <= 0 || threshold > 1.0) {
        return -1;
    }

    budget_ctrl_mutex_lock(&controller->mutex);
    controller->warning_threshold = threshold;
    budget_ctrl_mutex_unlock(&controller->mutex);

    return 0;
}

int agentos_budget_controller_reset_period(agentos_budget_controller_t* controller) {
    if (!controller) {
        return -1;
    }

    budget_ctrl_mutex_lock(&controller->mutex);
    atomic_store(&controller->period_cost, 0.0);
    controller->period_start = get_current_time();
    budget_ctrl_mutex_unlock(&controller->mutex);

    return 0;
}

double agentos_budget_controller_average(agentos_budget_controller_t* controller) {
    if (!controller) {
        return 0.0;
    }

    budget_ctrl_mutex_lock(&controller->mutex);
    double avg = controller->average_cost;
    budget_ctrl_mutex_unlock(&controller->mutex);

    return avg;
}

int agentos_budget_controller_get_status(agentos_budget_controller_t* controller) {
    if (!controller) {
        return -1;
    }

    check_and_reset_period(controller);

    double period_cost = atomic_load(&controller->period_cost);
    double percentage = (period_cost / controller->max_cost_usd) * 100.0;

    if (percentage >= 100.0) {
        return 2;
    } else if (percentage >= controller->warning_threshold * 100.0) {
        return 1;
    }

    return 0;
}
