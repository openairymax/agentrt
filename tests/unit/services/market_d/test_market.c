/**
 * @file test_market.c
 * @brief 市场服务单元测试
 * @details 测试市场服务的核心功能，包括 Agent 和 Skill 的注册、发现、安装和管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../../../backs/market_d/include/market_service.h"

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
        .enable_remote_registry = true,
        .enable_auto_update = true
    };

    // 创建市场服务
    market_service_t* service = NULL;
    int ret = market_service_create(&config, &service);
    if (ret != 0) {
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 销毁市场服务
    ret = market_service_destroy(service);
    if (ret != 0) {
        printf("失败: 销毁服务返回 %d\n", ret);
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
        printf("失败: 创建服务返回 %d\n", ret);
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
        printf("失败: 注册 Agent 返回 %d\n", ret);
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
        printf("失败: 搜索 Agent 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    if (count != 1) {
        printf("失败: Agent 数量不正确，期望 1，实际 %zu\n", count);
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
 * @brief 测试 Skill 注册
 */
int test_register_skill() {
    printf("测试 Skill 注册...");

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
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 注册 Skill
    skill_info_t skill = {
        .skill_id = "skill-001",
        .name = "天气查询",
        .version = "1.0.0",
        .description = "查询天气信息",
        .type = SKILL_TYPE_TOOL,
        .author = "SPHARX",
        .repository = "https://github.com/spharx/skill-001",
        .dependencies = "none",
        .rating = 4.8,
        .download_count = 2000,
        .last_updated = (uint64_t)time(NULL) * 1000
    };

    ret = market_service_register_skill(service, &skill);
    if (ret != 0) {
        printf("失败: 注册 Skill 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    // 搜索 Skill
    search_params_t search_params = {
        .query = "天气",
        .agent_type = AGENT_TYPE_COUNT,
        .skill_type = SKILL_TYPE_TOOL,
        .only_installed = false,
        .sort_by_rating = true,
        .sort_by_download = false,
        .limit = 10,
        .offset = 0
    };

    skill_info_t** skills = NULL;
    size_t count = 0;
    ret = market_service_search_skills(service, &search_params, &skills, &count);
    if (ret != 0) {
        printf("失败: 搜索 Skill 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    if (count != 1) {
        printf("失败: Skill 数量不正确，期望 1，实际 %zu\n", count);
        if (skills) {
            free(skills);
        }
        market_service_destroy(service);
        return -1;
    }

    if (skills) {
        free(skills);
    }

    market_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试 Agent 安装
 */
int test_install_agent() {
    printf("测试 Agent 安装...");

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
        printf("失败: 创建服务返回 %d\n", ret);
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
        printf("失败: 注册 Agent 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    // 安装 Agent
    install_request_t request = {
        .id = "agent-001",
        .version = NULL,
        .force_update = false,
        .install_path = NULL
    };

    install_result_t* result = NULL;
    ret = market_service_install_agent(service, &request, &result);
    if (ret != 0) {
        printf("失败: 安装 Agent 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    if (!result) {
        printf("失败: 安装结果为空\n");
        market_service_destroy(service);
        return -1;
    }

    if (!result->success) {
        printf("失败: 安装失败: %s\n", result->message);
        if (result->message) {
            free(result->message);
        }
        if (result->installed_version) {
            free(result->installed_version);
        }
        if (result->install_path) {
            free(result->install_path);
        }
        free(result);
        market_service_destroy(service);
        return -1;
    }

    // 检查已安装的 Agent
    agent_info_t** agents = NULL;
    size_t count = 0;
    ret = market_service_get_installed_agents(service, &agents, &count);
    if (ret != 0) {
        printf("失败: 获取已安装 Agent 返回 %d\n", ret);
        if (result->message) {
            free(result->message);
        }
        if (result->installed_version) {
            free(result->installed_version);
        }
        if (result->install_path) {
            free(result->install_path);
        }
        free(result);
        market_service_destroy(service);
        return ret;
    }

    if (count != 1) {
        printf("失败: 已安装 Agent 数量不正确，期望 1，实际 %zu\n", count);
        if (agents) {
            free(agents);
        }
        if (result->message) {
            free(result->message);
        }
        if (result->installed_version) {
            free(result->installed_version);
        }
        if (result->install_path) {
            free(result->install_path);
        }
        free(result);
        market_service_destroy(service);
        return -1;
    }

    if (agents) {
        free(agents);
    }

    if (result->message) {
        free(result->message);
    }
    if (result->installed_version) {
        free(result->installed_version);
    }
    if (result->install_path) {
        free(result->install_path);
    }
    free(result);

    market_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试 Skill 安装
 */
int test_install_skill() {
    printf("测试 Skill 安装...");

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
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 注册 Skill
    skill_info_t skill = {
        .skill_id = "skill-001",
        .name = "天气查询",
        .version = "1.0.0",
        .description = "查询天气信息",
        .type = SKILL_TYPE_TOOL,
        .author = "SPHARX",
        .repository = "https://github.com/spharx/skill-001",
        .dependencies = "none",
        .rating = 4.8,
        .download_count = 2000,
        .last_updated = (uint64_t)time(NULL) * 1000
    };

    ret = market_service_register_skill(service, &skill);
    if (ret != 0) {
        printf("失败: 注册 Skill 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    // 安装 Skill
    install_request_t request = {
        .id = "skill-001",
        .version = NULL,
        .force_update = false,
        .install_path = NULL
    };

    install_result_t* result = NULL;
    ret = market_service_install_skill(service, &request, &result);
    if (ret != 0) {
        printf("失败: 安装 Skill 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    if (!result) {
        printf("失败: 安装结果为空\n");
        market_service_destroy(service);
        return -1;
    }

    if (!result->success) {
        printf("失败: 安装失败: %s\n", result->message);
        if (result->message) {
            free(result->message);
        }
        if (result->installed_version) {
            free(result->installed_version);
        }
        if (result->install_path) {
            free(result->install_path);
        }
        free(result);
        market_service_destroy(service);
        return -1;
    }

    if (result->message) {
        free(result->message);
    }
    if (result->installed_version) {
        free(result->installed_version);
    }
    if (result->install_path) {
        free(result->install_path);
    }
    free(result);

    market_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试 Agent 卸载
 */
int test_uninstall_agent() {
    printf("测试 Agent 卸载...");

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
        printf("失败: 创建服务返回 %d\n", ret);
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
        printf("失败: 注册 Agent 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    // 卸载 Agent
    ret = market_service_uninstall_agent(service, "agent-001");
    if (ret != 0) {
        printf("失败: 卸载 Agent 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    market_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试 Skill 卸载
 */
int test_uninstall_skill() {
    printf("测试 Skill 卸载...");

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
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 注册 Skill
    skill_info_t skill = {
        .skill_id = "skill-001",
        .name = "天气查询",
        .version = "1.0.0",
        .description = "查询天气信息",
        .type = SKILL_TYPE_TOOL,
        .author = "SPHARX",
        .repository = "https://github.com/spharx/skill-001",
        .dependencies = "none",
        .rating = 4.8,
        .download_count = 2000,
        .last_updated = (uint64_t)time(NULL) * 1000
    };

    ret = market_service_register_skill(service, &skill);
    if (ret != 0) {
        printf("失败: 注册 Skill 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    // 卸载 Skill
    ret = market_service_uninstall_skill(service, "skill-001");
    if (ret != 0) {
        printf("失败: 卸载 Skill 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    market_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试检查更新
 */
int test_check_update() {
    printf("测试检查更新...");

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
        printf("失败: 创建服务返回 %d\n", ret);
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
        printf("失败: 注册 Agent 返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    // 检查更新
    bool has_update = false;
    char* latest_version = NULL;
    ret = market_service_check_update(service, "agent-001", &has_update, &latest_version);
    if (ret != 0) {
        printf("失败: 检查更新返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    if (latest_version) {
        free(latest_version);
    }

    market_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试配置重载
 */
int test_reload_config() {
    printf("测试配置重载...");

    // 初始配置
    market_config_t config1 = {
        .registry_url = "http://registry.agentos.org",
        .storage_path = "./test_market",
        .sync_interval_ms = 60000,
        .cache_ttl_ms = 300000,
        .enable_remote_registry = true,
        .enable_auto_update = true
    };

    // 创建市场服务
    market_service_t* service = NULL;
    int ret = market_service_create(&config1, &service);
    if (ret != 0) {
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 新配置
    market_config_t config2 = {
        .registry_url = "http://new-registry.agentos.org",
        .storage_path = "./new_test_market",
        .sync_interval_ms = 30000,
        .cache_ttl_ms = 150000,
        .enable_remote_registry = false,
        .enable_auto_update = false
    };

    // 重载配置
    ret = market_service_reload_config(service, &config2);
    if (ret != 0) {
        printf("失败: 重载配置返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
    }

    market_service_destroy(service);
    printf("成功\n");
    return 0;
}

/**
 * @brief 测试同步注册中心
 */
int test_sync_registry() {
    printf("测试同步注册中心...");

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
        printf("失败: 创建服务返回 %d\n", ret);
        return ret;
    }

    // 同步注册中心
    ret = market_service_sync_registry(service);
    if (ret != 0) {
        printf("失败: 同步注册中心返回 %d\n", ret);
        market_service_destroy(service);
        return ret;
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
        test_register_agent,
        test_register_skill,
        test_install_agent,
        test_install_skill,
        test_uninstall_agent,
        test_uninstall_skill,
        test_check_update,
        test_reload_config,
        test_sync_registry
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
    printf("测试完成: %zu 个测试，%d 个通过，%zu 个失败\n", test_count, passed, test_count - passed);

    if (passed == test_count) {
        printf("所有测试通过！\n");
        return 0;
    } else {
        printf("有测试失败\n");
        return 1;
    }
}
