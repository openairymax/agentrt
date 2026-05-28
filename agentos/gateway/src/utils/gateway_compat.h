/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_compat.h
 * @brief Gateway utility 兼容层 - AGENTOS 标准错误码备用定义
 *
 * 供不包含 gateway_internal.h 的独立 utility 模块使用。
 * 如果 types.h 在 include path 中已提供定义，则使用其定义；
 * 否则使用此处的 fallback。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef GATEWAY_COMPAT_H
#define GATEWAY_COMPAT_H

#ifndef AGENTOS_SUCCESS
#define AGENTOS_SUCCESS 0
#endif

#ifndef AGENTOS_EFAIL
#define AGENTOS_EFAIL (-17)
#endif

#ifndef AGENTOS_ENOMEM
#define AGENTOS_ENOMEM (-12)
#endif

#ifndef AGENTOS_EINVAL
#define AGENTOS_EINVAL (-1)
#endif

#endif /* GATEWAY_COMPAT_H */