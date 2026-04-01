/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file stdio_gateway.h
 * @brief Stdio网关接口
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_GATEWAY_STDIO_H
#define AGENTOS_GATEWAY_STDIO_H

#include "gateway.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建Stdio网关
 *
 * @return 网关实例，失败返回NULL
 *
 * @ownership 调用者需通过gateway_destroy()释放
 */
gateway_t* stdio_gateway_create(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_STDIO_H */
