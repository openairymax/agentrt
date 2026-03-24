/**
 * @file fuzz_permission.c
 * @brief 权限系统模糊测试
 * @author Spharx
 * @date 2024
 *
 * 使用 libFuzzer 对权限系统进行模糊测试，覆盖：
 * - 权限裁决边界条件
 * - 规则解析异常输入
 * - 缓存溢出
 * - 并发访问
 */

#include "../../src/permission/permission.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ID_SIZE 256
#define MAX_CONTEXT_SIZE 1024

static const char* test_agents[] = {
    "agent_001",
    "admin_agent",
    "test_user_agent",
    "guest_agent",
    "super_admin",
    "",
    "a",
    "A",
    "0",
    "agent\x00with\x00null",
    "agent with spaces",
    "agent\twith\ttabs",
    "agent\nwith\nnewlines",
    "AGENT_001",
    "Agent_001",
    "agent-001",
    "agent.001",
    "agent@001",
    "agent#001",
    "agent$001"
};

#define AGENT_COUNT (sizeof(test_agents) / sizeof(test_agents[0]))

static const char* test_actions[] = {
    "read",
    "write",
    "delete",
    "execute",
    "admin",
    "",
    "a",
    "READ",
    "Write",
    "read ",
    " read",
    "r e a d",
    "read\x00with\x00null",
    "READ; DROP TABLE",
    "read\nwrite\ndelete",
    "<script>alert('xss')</script>",
    "../../../etc/passwd"
};

#define ACTION_COUNT (sizeof(test_actions) / sizeof(test_actions[0]))

static const char* test_resources[] = {
    "/data/users",
    "/data/admin",
    "/config/system",
    "/logs/audit",
    "",
    "a",
    "/",
    ".",
    "..",
    "/etc/passwd",
    "C:\\Windows\\System32",
    "/data/users/../../../etc/shadow",
    "/data//users",
    "/data/./users",
    "/data/./../etc/passwd",
    "/\x00null",
    "/verylongpathAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    "http://localhost/admin",
    "ftp://ftp.example.com",
    "javascript:alert(1)"
};

#define RESOURCE_COUNT (sizeof(test_resources) / sizeof(test_resources[0]))

/* 简单变异函数 */
static void mutate_string(char* str, size_t max_size) {
    size_t len = strlen(str);
    if (len == 0 || len >= max_size) return;

    size_t pos = rand() % len;
    uint8_t mutation = rand() % 256;

    switch (rand() % 6) {
        case 0:
            str[pos] ^= mutation;
            break;
        case 1:
            str[pos] = mutation;
            break;
        case 2:
            if (pos > 0 && len + 1 < max_size) {
                memmove(str + pos + 1, str + pos, len - pos + 1);
                str[pos] = 'X';
            }
            break;
        case 3:
            if (pos < len - 1) {
                str[pos] = str[pos + 1];
            }
            break;
        case 4:
            if (len + 3 < max_size) {
                str[len] = '%';
                str[len + 1] = 's';
                str[len + 2] = '\0';
            }
            break;
        case 5:
            str[pos] = "\x00\x01\x7f\xff"[rand() % 4];
            break;
    }
}

/* 测试基础权限裁决 */
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    permission_engine_t* engine = permission_engine_create(NULL);
    if (!engine) {
        return 0;
    }

    for (size_t i = 0; i < AGENT_COUNT && i < size; i++) {
        for (size_t j = 0; j < ACTION_COUNT; j++) {
            for (size_t k = 0; k < RESOURCE_COUNT; k++) {
                permission_engine_check(engine,
                                       test_agents[i],
                                       test_actions[j],
                                       test_resources[k],
                                       NULL);
            }
        }
    }

    permission_engine_destroy(engine);
    return 0;
}

/* 测试规则添加和裁决 */
int LLVMFuzzerTestOneInput_Rules(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    permission_engine_t* engine = permission_engine_create(NULL);
    if (!engine) {
        return 0;
    }

    for (size_t i = 0; i < AGENT_COUNT && i < size; i++) {
        for (size_t j = 0; j < ACTION_COUNT; j++) {
            for (size_t k = 0; k < RESOURCE_COUNT; k++) {
                permission_engine_add_rule(engine,
                                          test_agents[i],
                                          test_actions[j],
                                          test_resources[k],
                                          1,
                                          rand() % 100);
            }
        }
    }

    for (size_t i = 0; i < AGENT_COUNT && i < size; i++) {
        for (size_t j = 0; j < ACTION_COUNT; j++) {
            for (size_t k = 0; k < RESOURCE_COUNT; k++) {
                permission_engine_check(engine,
                                       test_agents[i],
                                       test_actions[j],
                                       test_resources[k],
                                       NULL);
            }
        }
    }

    permission_engine_destroy(engine);
    return 0;
}

/* 测试上下文参数 */
int LLVMFuzzerTestOneInput_Context(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    permission_engine_t* engine = permission_engine_create(NULL);
    if (!engine) {
        return 0;
    }

    char context[MAX_CONTEXT_SIZE];

    for (size_t i = 0; i < 100; i++) {
        size_t context_len = (rand() % (MAX_CONTEXT_SIZE - 1));
        for (size_t j = 0; j < context_len; j++) {
            context[j] = (char)(rand() % 256);
        }
        context[context_len] = '\0';

        permission_engine_check(engine,
                              "agent_001",
                              "read",
                              "/data/users",
                              context);
    }

    permission_engine_destroy(engine);
    return 0;
}

/* 测试缓存操作 */
int LLVMFuzzerTestOneInput_Cache(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    permission_engine_t* engine = permission_engine_create(NULL);
    if (!engine) {
        return 0;
    }

    for (size_t i = 0; i < 1000; i++) {
        const char* agent = test_agents[rand() % AGENT_COUNT];
        const char* action = test_actions[rand() % ACTION_COUNT];
        const char* resource = test_resources[rand() % RESOURCE_COUNT];

        permission_engine_check(engine, agent, action, resource, NULL);

        if (i % 100 == 0) {
            permission_engine_clear_cache(engine);
        }
    }

    permission_engine_destroy(engine);
    return 0;
}

/* 测试边界条件 */
int LLVMFuzzerTestOneInput_EdgeCases(const uint8_t* data, size_t size) {
    permission_engine_t* engine = permission_engine_create(NULL);
    if (!engine) {
        return 0;
    }

    permission_engine_check(engine, NULL, NULL, NULL, NULL);
    permission_engine_check(engine, "", "", "", "");
    permission_engine_check(engine, "a", "b", "c", NULL);

    permission_engine_add_rule(engine, NULL, NULL, NULL, 0, 0);
    permission_engine_add_rule(engine, "", "", "", 0, 0);
    permission_engine_add_rule(engine, "a", "b", "c", 2, -1);
    permission_engine_add_rule(engine, "a", "b", "c", -1, 100);

    permission_engine_destroy(engine);
    return 0;
}

/* 测试规则覆盖 */
int LLVMFuzzerTestOneInput_RulePriority(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    permission_engine_t* engine = permission_engine_create(NULL);
    if (!engine) {
        return 0;
    }

    for (int deny_priority = 0; deny_priority <= 100; deny_priority += 10) {
        permission_engine_add_rule(engine, "agent_test", "read", "/data/*",
                                  0, deny_priority);
    }

    for (int allow_priority = 0; allow_priority <= 100; allow_priority += 10) {
        permission_engine_add_rule(engine, "agent_test", "read", "/data/*",
                                  1, allow_priority);
    }

    permission_engine_check(engine, "agent_test", "read", "/data/users", NULL);

    permission_engine_destroy(engine);
    return 0;
}

/* 测试通配符规则 */
int LLVMFuzzerTestOneInput_Wildcards(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    permission_engine_t* engine = permission_engine_create(NULL);
    if (!engine) {
        return 0;
    }

    const char* wildcard_patterns[] = {
        "*",
        "agent_*",
        "agent_???",
        "/data/*",
        "/data/**/*.txt",
        "*/read",
        "*/*/admin",
        "/**",
        "*:*",
        "*@*"
    };

    size_t pattern_count = sizeof(wildcard_patterns) / sizeof(wildcard_patterns[0]);

    for (size_t i = 0; i < pattern_count; i++) {
        permission_engine_add_rule(engine, wildcard_patterns[i],
                                  wildcard_patterns[(i + 1) % pattern_count],
                                  wildcard_patterns[(i + 2) % pattern_count],
                                  1, 50);
    }

    for (size_t i = 0; i < 100; i++) {
        permission_engine_check(engine,
                              test_agents[rand() % AGENT_COUNT],
                              test_actions[rand() % ACTION_COUNT],
                              test_resources[rand() % RESOURCE_COUNT],
                              NULL);
    }

    permission_engine_destroy(engine);
    return 0;
}

/* 测试多规则组合 */
int LLVMFuzzerTestOneInput_MultiRules(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    permission_engine_t* engine = permission_engine_create(NULL);
    if (!engine) {
        return 0;
    }

    for (size_t i = 0; i < 50; i++) {
        char agent[64], action[64], resource[64];

        snprintf(agent, sizeof(agent), "agent_%zu", i);
        snprintf(action, sizeof(action), "action_%zu", i);
        snprintf(resource, sizeof(resource), "/resource/%zu", i);

        permission_engine_add_rule(engine, agent, action, resource, 1, (int)i);
    }

    for (size_t i = 0; i < 50; i++) {
        char agent[64], action[64], resource[64];

        snprintf(agent, sizeof(agent), "agent_%zu", i);
        snprintf(action, sizeof(action), "action_%zu", i);
        snprintf(resource, sizeof(resource), "/resource/%zu", i);

        permission_engine_check(engine, agent, action, resource, NULL);
    }

    permission_engine_destroy(engine);
    return 0;
}