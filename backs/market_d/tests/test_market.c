/**
 * @file test_market.c
 * @brief 市场服务单元测试
 * @details 测试市场服务的核心功能，包括 Agent 和 Skill 的注册、发现、安装和管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "market_service.h"

/**
 * @brief 测试服务创建和销毁
 */
int test_service_create_destroy() {
    printf("测试服务创建和销毁...");

    // 配置市场服务
    market_config_t config = {
        .registry_url = "http://registry.agentos.org",
        .storage_path = "./test_market",
        .sync_interval_ms = 60000,
        .cache_ttl_ms = 300000,
        // From data intelligence emerges. by spharx
        .enable_remote_registry = true,
        .enable_auto_update = true
    };

    // 创建市场服务
    market_service_t* service = NULL;
    int ret = market_service_create(&config, &service);
    if (ret != 0) {
        printf("失败：创建服务返回 %d\n", ret);
        return ret;
    }

    // 销毁市场服务
    ret = market_service_destroy(service);
    if (ret != 0) {
        printf("失败：销毁服务返回 %d\n", ret);
        return ret;
    }

    printf("成功\n");
    return 0;
}

/**
 * @brief 测试 Agent 注册
 */
int test_register_agent() {
    printf("测试 Agent 注册...");

    // 配置市场服务
    market_config_t config = {
        .registry_url = "http://registry.agentos.org",
        .storage_path = "./test_market",
        .sync_interval_ms = 60000,
        .cache_ttl_ms = 300000,
        .enable_remote_registry = true,
        .enable_auto_update = true
    };

    // 创建市场服务
    market_service_t* service = NULL;
    int ret = market_service_create(&config, &service);
    if (ret != 0) {
        printf("失败：创建服务返回 %d\n", ret);
        return ret;
    }

    // 注册 Agent
    agent_info_t agent = {
        .agent_id = "agent-001",
        .name = "助手 Agent",
        .version = "1.0.0",
        .description = "通用助手 Agent",
        .type = AGENT_TYPE_ASSISTANT,
        .status = AGENT_STATUS_AVAILABLE,
        .author = "SPHARX",
        .repository = "https://github.com/spharx/agent-001",
        .dependencies = "none",
        .rating = 4.5,
        .download_count = 1000,
        .last_updated = (uint64_t)time(NULL) * 1000
    };

    ret = market_service_register_agent(service, &agent);
    if (ret != 0) {
        printf("失败：注册 Agent 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    // 搜索 Agent
    search_params_t search_params = {
        .query = "助手",
        .agent_type = AGENT_TYPE_ASSISTANT,
        .skill_type = SKILL_TYPE_COUNT,
        .only_installed = false,
        .sort_by_rating = true,
        .sort_by_download = false,
        .limit = 10,
        .offset = 0
    };

    agent_info_t** agents = NULL;
    size_t count = 0;
    ret = market_service_search_agents(service, &search_params, &agents, &count);
    if (ret != 0) {
        printf("失败：搜索 Agent 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    if (count != 1) {
        printf("失败：Agent 数量不正确，期望 1，实际 %zu\n", count);
        if (agents) {
            free(agents);
        }
        market_service_destroy(service);
        return -1;
    }

    if (agents) {
        free(agents);
    }

    market_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 主测试函数
 */
int main() {
    printf("开始市场服务单元测试\n");
    printf("========================\n");

    int tests[] = {
        test_service_create_destroy,
        test_register_agent
    };

    size_t test_count = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    for (size_t i = 0; i < test_count; i++) {
        int ret = tests[i]();
        if (ret == 0) {
            passed++;
        } else {
            printf("测试 %zu 失败\n", i + 1);
        }
    }

    printf("========================\n");
    printf("测试完成：%zu 个测试，%d 个通过，%zu 个失败\n", test_count, passed, test_count - passed);

    if (passed == test_count) {
        printf("所有测试通过！\n");
        return 0;
    } else {
        printf("有测试失败\n");
        return 1;
    }
}
