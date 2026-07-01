/**
 * @file tc3_llm_callbacks.h
 * @brief ThinkDual (双思考系统) LLM 驱动的 tc3 回调实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 为 triple_coordinator (tc3) 提供 LLM 驱动的三个回调：
 *   - s2_generate: T2 主思考生成（首轮复用 engine 的 LLM seed，后续轮次独立调用 LLM）
 *   - s1_verify:   T1-F 快思考验证（LLM 评估准确性/完整性/清晰度/任务对齐度）
 *   - s1_expert:   T1-P 专业思考仲裁（LLM 扮演领域专家给出最终判决）
 *
 * 设计目标（用户需求："既要思考的逻辑正确，又要不偏离任务"）：
 *   1. 逻辑正确性: s1_verify 让 LLM 检查推理链是否有逻辑错误、事实错误
 *   2. 任务不偏离: s1_verify 将原始任务纳入 prompt，显式检查内容是否偏题
 *   3. 专业仲裁:   s1_expert 在 MAJOR_FIX 时升级，由 LLM 给出权威判决
 *
 * 线程安全: 回调通过 engine->lock 保护 LLM 句柄的读取。LLM 调用本身
 * 在锁外执行（避免长时间持锁阻塞其他认知线程）。
 *
 * 内存约定: 所有出参的堆分配字符串 (output/critique/opinion) 由调用方
 * (tc3_coordinator) 负责释放。回调内部用 AGENTOS_MALLOC 分配。
 */

#ifndef AGENTOS_TC3_LLM_CALLBACKS_H
#define AGENTOS_TC3_LLM_CALLBACKS_H

#include "agentos.h"
#include "triple_coordinator.h"

/* C-L02: llm_d → CoreLoopThree */
#include "llm_service.h"
#include "llm_svc_adapter.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LLM 回调上下文 — 持有 engine 的 LLM 句柄（borrowed）和 seed 文本
 *
 * 所有指针均为 borrowed（不拥有所有权），由 engine.c 管理生命周期。
 * 该结构体通常在 engine.c 的栈上构造，传入 tc3_config_t 的 user_data 字段。
 */
typedef struct tc3_llm_ctx {
    /* LLM 句柄（borrowed from engine）— 优先 IPC adapter，回退直接 service */
    llm_svc_adapter_t *llm_adapter;
    llm_service_t *llm_svc;

    /* engine 互斥锁（borrowed）— 用于线程安全地读取 LLM 句柄 */
    agentos_mutex_t *lock;

    /* LLM 请求的 max_tokens 上限 */
    uint32_t max_tokens;

    /* Seed: engine.c 第一次 LLM 调用的结果（borrowed，不拥有）。
     * s2_generate 首次调用时返回此 seed 的副本（避免重复 LLM 调用），
     * 后续（修正轮次）调用 LLM 独立生成。
     * 若 engine 未做首次调用（seed 不可用），此字段为 NULL。 */
    const char *seed_text;
    size_t seed_len;
    int seed_consumed;

    /* 原始任务输入（borrowed）— 传入 s1_verify 用于任务偏离检测 */
    const char *original_input;
    size_t original_input_len;
} tc3_llm_ctx_t;

/**
 * @brief T2 主思考生成回调
 *
 * 首次调用: 若 seed_text 可用且未消费，返回其副本（复用 engine 已做的 LLM 调用）。
 * 后续调用（修正轮次）: 以 input（build_correction_prompt 构建的修正提示）为
 *   user message 调用 LLM，返回生成结果。
 *
 * @param input      输入文本（首轮=原始任务；修正轮=修正提示）
 * @param input_len  输入长度
 * @param output     [out] 生成的文本（AGENTOS_MALLOC 分配，调用方释放）
 * @param output_len [out] 生成文本长度
 * @param user_data  tc3_llm_ctx_t 指针
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t tc3_llm_s2_generate(const char *input, size_t input_len, char **output,
                                    size_t *output_len, void *user_data);

/**
 * @brief T1-F 快思考验证回调
 *
 * 构造包含原始任务和待验证内容的评估 prompt，调用 LLM 返回 JSON 格式的
 * 评分 (0.0-1.0)、可接受性 (true/false) 和具体批判意见。
 * 重点检查: 逻辑正确性、事实准确性、完整性、清晰度、任务对齐度（不偏离）。
 *
 * LLM 不可用或解析失败时回退到启发式评分（保证 tc3 循环不中断）。
 *
 * @param content        待验证内容
 * @param content_len    内容长度
 * @param out_score      [out] 评分 0.0-1.0
 * @param out_acceptable [out] 是否可接受 (0/1)
 * @param out_critique   [out] 批判意见（AGENTOS_MALLOC 分配，调用方释放）
 * @param out_critique_len [out] 批判意见长度
 * @param user_data      tc3_llm_ctx_t 指针
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t tc3_llm_s1_verify(const char *content, size_t content_len, float *out_score,
                                  int *out_acceptable, char **out_critique,
                                  size_t *out_critique_len, void *user_data);

/**
 * @brief T1-P 专业思考仲裁回调
 *
 * 在 MAJOR_FIX 时升级调用。构造包含内容和批判意见的仲裁 prompt，调用 LLM
 * 扮演领域专家，返回最终判决 (accept/minor_fix/major_fix/reject)、评分和
 * 专家意见。
 *
 * LLM 不可用或解析失败时回退到保守判决（MAJOR_FIX，保持原 score）。
 *
 * @param content          待仲裁内容
 * @param content_len      内容长度
 * @param critique         T1-F 的批判意见
 * @param critique_len     批判意见长度
 * @param out_score        [out] 专家评分 0.0-1.0
 * @param out_verdict      [out] 专家判决
 * @param out_expert_opinion [out] 专家意见（AGENTOS_MALLOC 分配，调用方释放）
 * @param out_opinion_len  [out] 专家意见长度
 * @param user_data        tc3_llm_ctx_t 指针
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t tc3_llm_s1_expert(const char *content, size_t content_len, const char *critique,
                                  size_t critique_len, float *out_score,
                                  tc3_verdict_t *out_verdict, char **out_expert_opinion,
                                  size_t *out_opinion_len, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TC3_LLM_CALLBACKS_H */
