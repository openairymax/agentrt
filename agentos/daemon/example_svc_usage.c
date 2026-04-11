// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file example_svc_usage.c
 * @brief 服务管理框架使用示例
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 展示如何使用 svc_common.h 中定义的服务管理框架 API。
 * 此示例演示了服务的完整生命周期管理。
 *
 * 编译命令（Linux/macOS）:
 *   gcc -o example_svc_usage example_svc_usage.c -I./common/include -L./build/daemon/common -lsvc_common -lagentos_common -lpthread
 *
 * 编译命令（Windows）:
 *   cl example_svc_usage.c /I./common/include /link svc_common.lib agentos_common.lib
 */

#include <stdio.h>
#include <string.h>
#include "svc_common.h"

/* ==================== 示例服务实现 ==================== */

/**
 * @brief 示例服务上下文
 */
typedef struct {
    int counter;
    char name[64];
} example_service_context_t;

/**
 * @brief 示例服务初始化函数
 */
static agentos_error_t example_service_init(
    agentos_service_t svc,
    const agentos_svc_config_t* config) {

    printf("示例服务初始化: %s\n", agentos_service_get_name(svc));

    /* 分配服务上下文 */
    example_service_context_t* ctx =
        (example_service_context_t*)AGENTOS_MALLOC(sizeof(example_service_context_t));
    if (!ctx) {
        return AGENTOS_ENOMEM;
    }

    memset(ctx, 0, sizeof(example_service_context_t));
    strncpy(ctx->name, config->name, sizeof(ctx->name) - 1);
    ctx->counter = 0;

    /* 存储上下文到服务 */
    /* 注意：实际实现中应使用 agentos_service_set_user_data 等接口 */

    return AGENTOS_SUCCESS;
}

/**
 * @brief 示例服务启动函数
 */
static agentos_error_t example_service_start(agentos_service_t svc) {
    printf("示例服务启动: %s\n", agentos_service_get_name(svc));
    return AGENTOS_SUCCESS;
}

/**
 * @brief 示例服务停止函数
 */
static agentos_error_t example_service_stop(
    agentos_service_t svc,
    bool force) {

    printf("示例服务停止: %s (强制: %s)\n",
           agentos_service_get_name(svc),
           force ? "是" : "否");
    return AGENTOS_SUCCESS;
}

/**
 * @brief 示例服务销毁函数
 */
static void example_service_destroy(agentos_service_t svc) {
    printf("示例服务销毁: %s\n", agentos_service_get_name(svc));
}

/**
 * @brief 示例服务健康检查函数
 */
static agentos_error_t example_service_healthcheck(agentos_service_t svc) {
    printf("示例服务健康检查: %s\n", agentos_service_get_name(svc));
    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建示例服务接口
 */
static agentos_svc_interface_t create_example_service_interface(void) {
    agentos_svc_interface_t iface = {
        .init = example_service_init,
        .start = example_service_start,
        .stop = example_service_stop,
        .destroy = example_service_destroy,
        .healthcheck = example_service_healthcheck
    };
    return iface;
}

/* ==================== 主程序 ==================== */

int main(void) {
    printf("=== AgentOS 服务管理框架使用示例 ===\n\n");

    agentos_error_t err = AGENTOS_SUCCESS;
    agentos_service_t service = NULL;

    /* 1. 创建服务配置 */
    agentos_svc_config_t config = {
        .name = "example-service",
        .version = "1.0.0",
        .capabilities = AGENTOS_SVC_CAP_ASYNC | AGENTOS_SVC_CAP_PAUSEABLE,
        .max_concurrent = 10,
        .timeout_ms = 5000,
        .priority = 5,
        .auto_start = true,
        .enable_metrics = true,
        .enable_tracing = false
    };

    /* 2. 创建服务接口 */
    agentos_svc_interface_t iface = create_example_service_interface();

    /* 3. 创建服务实例 */
    printf("1. 创建服务...\n");
    err = agentos_service_create(&service, "example-service", &iface, &config);
    if (err != AGENTOS_SUCCESS) {
        printf("  创建服务失败: %d\n", err);
        return 1;
    }
    printf("  服务创建成功: %s\n", agentos_service_get_name(service));

    /* 4. 初始化服务 */
    printf("\n2. 初始化服务...\n");
    err = agentos_service_init(service);
    if (err != AGENTOS_SUCCESS) {
        printf("  服务初始化失败: %d\n", err);
        agentos_service_destroy(service);
        return 1;
    }
    printf("  服务初始化成功，状态: %s\n",
           agentos_svc_state_to_string(agentos_service_get_state(service)));

    /* 5. 启动服务 */
    printf("\n3. 启动服务...\n");
    err = agentos_service_start(service);
    if (err != AGENTOS_SUCCESS) {
        printf("  服务启动失败: %d\n", err);
        agentos_service_destroy(service);
        return 1;
    }
    printf("  服务启动成功，状态: %s\n",
           agentos_svc_state_to_string(agentos_service_get_state(service)));

    /* 6. 检查服务状态 */
    printf("\n4. 检查服务状态...\n");
    printf("  服务名称: %s\n", agentos_service_get_name(service));
    printf("  服务版本: %s\n", agentos_service_get_version(service));
    printf("  服务状态: %s\n",
           agentos_svc_state_to_string(agentos_service_get_state(service)));
    printf("  是否就绪: %s\n", agentos_service_is_ready(service) ? "是" : "否");
    printf("  是否运行: %s\n", agentos_service_is_running(service) ? "是" : "否");

    /* 7. 执行健康检查 */
    printf("\n5. 执行健康检查...\n");
    err = agentos_service_healthcheck(service);
    if (err != AGENTOS_SUCCESS) {
        printf("  健康检查失败: %d\n", err);
    } else {
        printf("  健康检查通过\n");
    }

    /* 8. 获取服务统计 */
    printf("\n6. 获取服务统计...\n");
    agentos_svc_stats_t stats;
    err = agentos_service_get_stats(service, &stats);
    if (err != AGENTOS_SUCCESS) {
        printf("  获取统计失败: %d\n", err);
    } else {
        printf("  请求总数: %llu\n", (unsigned long long)stats.request_count);
        printf("  成功次数: %llu\n", (unsigned long long)stats.success_count);
        printf("  错误次数: %llu\n", (unsigned long long)stats.error_count);
    }

    /* 9. 暂停和恢复服务（如果支持） */
    printf("\n7. 测试暂停/恢复功能...\n");
    if (agentos_service_has_capability(service, AGENTOS_SVC_CAP_PAUSEABLE)) {
        printf("  服务支持暂停功能\n");

        err = agentos_service_pause(service);
        if (err != AGENTOS_SUCCESS) {
            printf("  暂停失败: %d\n", err);
        } else {
            printf("  服务已暂停，状态: %s\n",
                   agentos_svc_state_to_string(agentos_service_get_state(service)));

            err = agentos_service_resume(service);
            if (err != AGENTOS_SUCCESS) {
                printf("  恢复失败: %d\n", err);
            } else {
                printf("  服务已恢复，状态: %s\n",
                       agentos_svc_state_to_string(agentos_service_get_state(service)));
            }
        }
    } else {
        printf("  服务不支持暂停功能\n");
    }

    /* 10. 停止服务 */
    printf("\n8. 停止服务...\n");
    err = agentos_service_stop(service, false);  /* 正常停止 */
    if (err != AGENTOS_SUCCESS) {
        printf("  服务停止失败: %d，尝试强制停止...\n", err);
        err = agentos_service_stop(service, true);  /* 强制停止 */
        if (err != AGENTOS_SUCCESS) {
            printf("  强制停止也失败: %d\n", err);
        }
    }
    printf("  服务停止成功，状态: %s\n",
           agentos_svc_state_to_string(agentos_service_get_state(service)));

    /* 11. 销毁服务 */
    printf("\n9. 销毁服务...\n");
    agentos_service_destroy(service);
    printf("  服务销毁完成\n");

    /* 12. 查询注册表中的服务数量 */
    printf("\n10. 注册表统计...\n");
    uint32_t service_count = agentos_service_count();
    printf("  注册表中的服务数量: %u\n", service_count);

    printf("\n=== 示例程序完成 ===\n");

    return 0;
}
