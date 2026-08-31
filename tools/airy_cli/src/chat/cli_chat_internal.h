// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_internal.h
 * @brief 聊天域内部共享头（域拆分：classify/stream/finalize/reply 主流程）。
 *
 * 原 cli_chat.c（802 行）按功能域拆分后，跨文件共享的流式折叠状态与
 * 内部函数声明统一收敛于此；cli_internal.h 的公共 API 不变
 * （cli_classify_input / cli_chat_t1p_cached 等公共原型仍在 cli_internal.h）。
 * 此头仅限 airy_cli/src 内部使用。
 */

#ifndef AIRY_CLI_CHAT_INTERNAL_H
#define AIRY_CLI_CHAT_INTERNAL_H

#include "cli_internal.h"

/* ---- stream 域（cli_chat_stream.c）：流式归一化 / 思考进度回调 / 模型槽缓存 ---- */
void cli_stream_norm_flush_carry(void);
void cli_chat_stream_cb(const char *chunk, void *user_data);
void cli_chat_reasoning_cb(const char *delta, void *user_data);
void cli_chat_reasoning_clear(void);
int cli_chat_stream_round(llm_svc_adapter_t *adapter, const llm_request_config_t *cfg,
                          llm_response_t **out_resp);
const char *cli_chat_t1f_cached(void);
cli_actor_t cli_chat_think_actor(void);

/* finalize 域（cli_chat_finalize.c）：最终回复渲染 / 历史写入 */
void cli_chat_reply_finalize(llm_response_t *final_resp, const char *input,
                             int stream_mode, int tool_rounds);

#endif /* AIRY_CLI_CHAT_INTERNAL_H */
