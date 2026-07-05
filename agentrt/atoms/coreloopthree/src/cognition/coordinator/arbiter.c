/**
 * @file arbiter.c
 * @brief 外部仲裁策略（调用仲裁器模型或人工接口）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * P2.7: 桩消除 — 当 arbiter_model 配置且 base->llm 可用时，
 * 真正调用 LLM 进行仲裁（构建包含所有候选输入的 prompt，
 * 让 LLM 选择最佳结果），而非直接返回 inputs[0]。
 * 降级路径：LLM 不可用时保守返回 inputs[0] 并记录 WARN。
 */

#include "agentrt.h"
#include "strategy.h"

/* P2.7: 调用 agentrt_llm_service_t 进行 LLM 仲裁
 *
 * 注意：不直接 #include "llm_client.h"，因为该头与 coordinator/strategy.h
 * 对 agentrt_llm_service_t 有冲突的 typedef 定义（strategy.h 定义为
 * struct llm_service；llm_client.h 定义为 struct agentrt_llm_service）。
 * base->llm 字段类型是 struct agentrt_llm_service *（与 llm_client.h 一致），
 * 因此这里前向声明所需函数，使用 struct tag 避免 typedef 冲突。
 * 使用 agentrt_llm_service_call（简单 prompt API）而非 agentrt_llm_complete
 * （需要 request/response struct 类型，会引入签名不兼容问题）。
 * arbiter_model 通过 prompt 上下文传递给 LLM provider，由 daemon 侧路由。 */
struct agentrt_llm_service;

int agentrt_llm_service_is_available(const struct agentrt_llm_service *service);
agentrt_error_t agentrt_llm_service_call(struct agentrt_llm_service *service,
                                         const char *prompt, char **out_response);

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 外部仲裁私有数据
 */
typedef struct arbiter_data {
    char *arbiter_model; /**< 仲裁模型名称（可为 NULL，表示人工） */
    agentrt_mutex_t *lock;
    void (*human_callback)(const char *question, char *answer, size_t max_len); /**< 人工回调 */
} arbiter_data_t;

static void arbiter_destroy(agentrt_coordinator_base_t *base)
{
    if (!base)
        return;
    arbiter_data_t *data = (arbiter_data_t *)base->data;
    if (data) {
        if (data->arbiter_model)
            AGENTRT_FREE(data->arbiter_model);
        if (data->lock)
            agentrt_mutex_free(data->lock);
        AGENTRT_FREE(data);
    }
    AGENTRT_FREE(base);
}

/**
 * @brief P2.7: 从 LLM 仲裁响应中提取选择的索引（1-based）
 *
 * LLM 可能返回纯数字、"Choice: N"、"index: N"、JSON {"choice":N} 等。
 * 解析失败返回 -1。
 */
static int parse_arbiter_choice(const char *response, size_t input_count)
{
    if (!response)
        return -1;

    /* 跳过前导空白 */
    while (*response && isspace((unsigned char)*response))
        response++;

    /* 尝试解析前缀数字（"1", "2.", "Choice: 2", "index: 3"） */
    for (const char *p = response; *p; p++) {
        if (isdigit((unsigned char)*p)) {
            int val = atoi(p);
            if (val >= 1 && (size_t)val <= input_count) {
                return val;
            }
            /* 数字超出范围 — 继续扫描下一个数字 */
        }
    }
    return -1;
}

/**
 * @brief P2.7: LLM 仲裁核心 — 构造 prompt 并调用 LLM 选择最佳候选
 *
 * @param llm LLM 服务句柄（borrowed，类型为 struct agentrt_llm_service*）
 * @param arbiter_model 仲裁模型名（可为 NULL，通过 prompt 上下文传递）
 * @param inputs 候选输入数组
 * @param input_count 候选数量
 * @param out_choice [out] 选择的索引（1-based，-1 表示解析失败）
 * @return AGENTRT_SUCCESS 或错误码
 */
static agentrt_error_t arbiter_llm_arbitrate(struct agentrt_llm_service *llm,
                                             const char *arbiter_model,
                                             const char **inputs, size_t input_count,
                                             int *out_choice)
{
    if (!llm || !inputs || !out_choice)
        return AGENTRT_EINVAL;
    *out_choice = -1;

    /* 检查 LLM 可用性 */
    if (!agentrt_llm_service_is_available(llm)) {
        AGENTRT_LOG_WARN("arbiter: LLM service unavailable, falling back to inputs[0]");
        return AGENTRT_ENOTSUP;
    }

    /* 构造仲裁 prompt：列出所有候选，要求 LLM 选择最佳 */
    /* 每个 candidate 最多截断 256 字节，前缀 + 后缀约 200 字节 */
    size_t prompt_sz = 512 + input_count * (256 + 32);
    char *prompt = (char *)AGENTRT_MALLOC(prompt_sz);
    if (!prompt)
        return AGENTRT_ENOMEM;

    int wn = snprintf(prompt, prompt_sz,
                      "You are an impartial arbiter%s%s. "
                      "Multiple models produced inconsistent responses. "
                      "Select the BEST response by replying with ONLY its number (1-%zu).\n\n",
                      arbiter_model ? " using model " : "",
                      arbiter_model ? arbiter_model : "",
                      input_count);
    if (wn <= 0 || (size_t)wn >= prompt_sz) {
        AGENTRT_FREE(prompt);
        return AGENTRT_EINVAL;
    }

    for (size_t i = 0; i < input_count; i++) {
        const char *cand = inputs[i] ? inputs[i] : "(empty)";
        size_t cand_len = strlen(cand);
        size_t truncate = cand_len > 256 ? 256 : cand_len;

        char buf[320];
        int bn = snprintf(buf, sizeof(buf), "Candidate %zu:\n%.*s\n\n",
                          i + 1, (int)truncate, cand);
        if (bn <= 0)
            continue;

        /* 追加到 prompt（保证不溢出） */
        size_t cur_len = strlen(prompt);
        size_t remaining = prompt_sz - cur_len - 1;
        if ((size_t)bn < remaining) {
            strncat(prompt, buf, remaining);
        }
    }

    /* 追加最终指令 */
    size_t remaining = prompt_sz - strlen(prompt) - 1;
    strncat(prompt, "Reply with ONLY the number of the best candidate (1-", remaining);
    remaining = prompt_sz - strlen(prompt) - 1;
    char numbuf[32];
    snprintf(numbuf, sizeof(numbuf), "%zu).", input_count);
    strncat(prompt, numbuf, remaining);

    /* 调用 LLM (agentrt_llm_service_call — 简单 prompt API) */
    char *llm_response = NULL;
    agentrt_error_t err = agentrt_llm_service_call(llm, prompt, &llm_response);
    AGENTRT_FREE(prompt);

    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_WARN("arbiter: LLM service_call failed (err=%d)", (int)err);
        return err;
    }

    if (!llm_response) {
        AGENTRT_LOG_WARN("arbiter: LLM returned empty response");
        return AGENTRT_ESERVICE;
    }

    int choice = parse_arbiter_choice(llm_response, input_count);
    AGENTRT_LOG_DEBUG("arbiter: LLM response='%s' parsed_choice=%d",
                      llm_response, choice);

    AGENTRT_FREE(llm_response);

    if (choice < 1) {
        AGENTRT_LOG_WARN("arbiter: failed to parse choice from LLM response, falling back");
        return AGENTRT_ESERVICE;
    }

    *out_choice = choice;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 外部仲裁函数 — P2.7: 真正调用 LLM 仲裁
 */
static agentrt_error_t
arbiter_coordinate(agentrt_coordinator_base_t *base,
                   const agentrt_coordination_context_t __attribute__((unused)) * context,
                   const char **inputs, size_t input_count, char **out_result)
{
    if (!base || !out_result)
        return AGENTRT_EINVAL;

    arbiter_data_t *data = (arbiter_data_t *)base->data;
    if (!data)
        return AGENTRT_EINVAL;

    agentrt_mutex_lock(data->lock);

    /* 输入为空 — 返回 no_input */
    if (input_count == 0 || !inputs) {
        *out_result = AGENTRT_STRDUP("no_input");
        if (!*out_result) {
            agentrt_mutex_unlock(data->lock);
            return AGENTRT_ENOMEM;
        }
        agentrt_mutex_unlock(data->lock);
        return AGENTRT_SUCCESS;
    }

    /* 路径 1：人工仲裁（保持原逻辑） */
    if (data->human_callback) {
        char question[1024];
        snprintf(question, sizeof(question), "多个模型输出不一致，请选择最佳结果：\n");

        for (size_t i = 0; i < input_count && i < 5; i++) {
            char option[256];
            snprintf(option, sizeof(option), "%zu. %s\n", i + 1, inputs[i]);
            strncat(question, option, sizeof(question) - strlen(question) - 1);
        }

        char answer[512];
        data->human_callback(question, answer, sizeof(answer));

        int choice = atoi(answer);
        if (choice >= 1 && choice <= (int)input_count) {
            *out_result = AGENTRT_STRDUP(inputs[choice - 1]);
        } else {
            *out_result = AGENTRT_STRDUP("invalid_choice");
        }
        if (!*out_result) {
            agentrt_mutex_unlock(data->lock);
            return AGENTRT_ENOMEM;
        }
        agentrt_mutex_unlock(data->lock);
        return AGENTRT_SUCCESS;
    }

    /* P2.7 路径 2：LLM 仲裁 — arbiter_model 配置 + base->llm 可用时调用 LLM */
    if (data->arbiter_model && base->llm) {
        int choice = -1;
        agentrt_error_t err = arbiter_llm_arbitrate(base->llm, data->arbiter_model,
                                                    inputs, input_count, &choice);
        if (err == AGENTRT_SUCCESS && choice >= 1 && (size_t)choice <= input_count) {
            AGENTRT_LOG_INFO("arbiter: LLM selected candidate %d/%zu",
                             choice, input_count);
            *out_result = AGENTRT_STRDUP(inputs[choice - 1]);
            if (!*out_result) {
                agentrt_mutex_unlock(data->lock);
                return AGENTRT_ENOMEM;
            }
            agentrt_mutex_unlock(data->lock);
            return AGENTRT_SUCCESS;
        }
        AGENTRT_LOG_WARN("arbiter: LLM arbitration failed (err=%d choice=%d), "
                         "falling back to inputs[0]", (int)err, choice);
        /* 降级到下面的 inputs[0] 路径 */
    } else if (data->arbiter_model && !base->llm) {
        AGENTRT_LOG_WARN("arbiter: arbiter_model configured but base->llm is NULL "
                         "(create() 未注入 LLM 句柄), falling back to inputs[0]");
    }

    /* 降级路径：返回 inputs[0]（仅当 LLM 不可用时） */
    *out_result = AGENTRT_STRDUP(inputs[0]);
    if (!*out_result) {
        agentrt_mutex_unlock(data->lock);
        return AGENTRT_ENOMEM;
    }

    agentrt_mutex_unlock(data->lock);
    return AGENTRT_SUCCESS;
}

/**
 * @brief 创建外部仲裁协调器
 */
agentrt_error_t agentrt_coordinator_arbiter_create(const char *arbiter_model,
                                                   void (*human_callback)(const char *question,
                                                                          char *answer,
                                                                          size_t max_len),
                                                   agentrt_coordinator_base_t **out_base)
{
    if (!out_base)
        return AGENTRT_EINVAL;

    agentrt_coordinator_base_t *base =
        (agentrt_coordinator_base_t *)AGENTRT_CALLOC(1, sizeof(agentrt_coordinator_base_t));
    if (!base)
        return AGENTRT_ENOMEM;

    arbiter_data_t *data = (arbiter_data_t *)AGENTRT_CALLOC(1, sizeof(arbiter_data_t));
    if (!data) {
        AGENTRT_FREE(base);
        return AGENTRT_ENOMEM;
    }

    data->lock = agentrt_mutex_create();
    if (!data->lock) {
        AGENTRT_FREE(data);
        AGENTRT_FREE(base);
        return AGENTRT_ENOMEM;
    }

    if (arbiter_model) {
        data->arbiter_model = AGENTRT_STRDUP(arbiter_model);
        if (!data->arbiter_model) {
            agentrt_mutex_free(data->lock);
            AGENTRT_FREE(data);
            AGENTRT_FREE(base);
            return AGENTRT_ENOMEM;
        }
    }

    data->human_callback = human_callback;

    base->data = data;
    base->llm = NULL; /* 由 agentrt_arbiter_model_create 注入 */
    base->coordinate = arbiter_coordinate;
    base->destroy = arbiter_destroy;

    *out_base = base;
    return AGENTRT_SUCCESS;
}
