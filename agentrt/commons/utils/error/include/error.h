/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file error.h
 * @brief 统一错误处理框架
 *
 * 设计原则：
 * 1. 所有错误码为负值，成功为0
 * 2. 错误码分段管理，避免冲突
 * 3. 支持错误链追踪
 * 4. 线程安全的错误信息存储
 * 5. 支持结构化错误上下文
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-03-30
 * @version 2.0
 *
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-6 错误可追溯原则
 */

#ifndef AGENTRT_UTILS_ERROR_H
#define AGENTRT_UTILS_ERROR_H

#include "../../types/include/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 错误码类型 ==================== */

/* agentrt_error_t 已在 agentrt_types.h 中定义（BAN-196 权威源），此处不再重复定义 */

/* ==================== 成功/失败基础 ==================== */

#ifndef AGENTRT_OK
#define AGENTRT_OK 0
#endif
#ifndef AGENTRT_ERR_UNKNOWN
#define AGENTRT_ERR_UNKNOWN (-1)
#endif

/* ==================== 向后兼容别名 ==================== */
/*
 * 权威源说明（G2.1 统一错误码表）：
 *
 * POSIX 风格错误码（AGENTRT_EINVAL / AGENTRT_ENOMEM / AGENTRT_EBUSY 等）
 * 的权威定义位于 agentrt_types.h（硬定义，无 #ifndef 保护）。
 *
 * 本文件原先用 #ifndef 为上述 POSIX 码提供"别名到 AGENTRT_ERR_* 扩展码"
 * 的兼容层，但因 types.h 总是先被 include（本文件 line 27），#ifndef 恒为
 * false，这些别名均为死代码。已于 G2.1 清理。
 *
 * 下方仅保留两类活跃定义：
 *   1. types.h 未定义的扩展别名（EINTR/EBADF/ERESOURCE/ESECURITY/ESANITIZE/
 *      ECANCELED/ENOTDIR/ENAMETOOLONG）——本文件为唯一源
 *   2. #undef AGENTRT_EPERM 重定义——使 EPERM 与 ERR_PERMISSION_DENIED 数值
 *      一致（-10），消除语义歧义
 *
 * 已知技术债（计划 0.1.2 消除）：types.h POSIX 码（-1 到 -29）与本文件
 * AGENTRT_ERR_* 扩展码（-1 到 -19）在数值区间存在重叠（如 EINVAL=-1 与
 * ERR_UNKNOWN=-1）。调用方应始终使用语义宏，严禁与字面量直接比较。
 */
#ifndef AGENTRT_ECANCELED
#define AGENTRT_ECANCELED AGENTRT_ERR_CANCELED
#endif
#ifndef AGENTRT_EINTR
#define AGENTRT_EINTR AGENTRT_ERR_INTERRUPTED
#endif
#ifndef AGENTRT_EBADF
#define AGENTRT_EBADF AGENTRT_ERR_SYS_FILE
#endif
#ifndef AGENTRT_ERESOURCE
#define AGENTRT_ERESOURCE AGENTRT_ERR_SYS_RESOURCE
#endif
#ifndef AGENTRT_ESECURITY
#define AGENTRT_ESECURITY AGENTRT_ERR_ESECURITY
#endif
#ifndef AGENTRT_ESANITIZE
#define AGENTRT_ESANITIZE AGENTRT_ERR_ESANITIZE
#endif
#undef AGENTRT_EPERM
#define AGENTRT_EPERM AGENTRT_ERR_PERMISSION_DENIED
#ifndef AGENTRT_ENOTDIR
#define AGENTRT_ENOTDIR (-28)
#endif
#ifndef AGENTRT_ENAMETOOLONG
#define AGENTRT_ENAMETOOLONG (-29)
#endif

/* ==================== 错误码分段 ==================== */
/*
 * 错误码分段规划：
 *   -1 到 -99:      通用基础错误
 *   -100 到 -999:   系统与平台错误
 *   -1000 到 -1999: 内核层错误
 *   -2000 到 -2999: 服务层错误
 *   -3000 到 -3999: LLM/AI服务错误
 *   -4000 到 -4999: 执行/工具错误
 *   -5000 到 -5999: 调度错误
 *   -6000 到 -6999: 记忆/存储错误
 *   -7000 到 -7999: 安全/沙箱错误
 */

/* 通用基础错误 (-1 到 -99) */
#ifndef AGENTRT_ERR_INVALID_PARAM
#define AGENTRT_ERR_INVALID_PARAM (-2)
#endif
#ifndef AGENTRT_ERR_NULL_POINTER
#define AGENTRT_ERR_NULL_POINTER (-3)
#endif
#ifndef AGENTRT_ERR_OUT_OF_MEMORY
#define AGENTRT_ERR_OUT_OF_MEMORY (-4)
#endif
#ifndef AGENTRT_ERR_BUFFER_TOO_SMALL
#define AGENTRT_ERR_BUFFER_TOO_SMALL (-5)
#endif
#ifndef AGENTRT_ERR_NOT_FOUND
#define AGENTRT_ERR_NOT_FOUND (-6)
#endif
#ifndef AGENTRT_ERR_ALREADY_EXISTS
#define AGENTRT_ERR_ALREADY_EXISTS (-7)
#endif
#ifndef AGENTRT_ERR_TIMEOUT
#define AGENTRT_ERR_TIMEOUT (-8)
#endif
#ifndef AGENTRT_ERR_NOT_SUPPORTED
#define AGENTRT_ERR_NOT_SUPPORTED (-9)
#endif
#ifndef AGENTRT_ERR_PERMISSION_DENIED
#define AGENTRT_ERR_PERMISSION_DENIED (-10)
#endif
#ifndef AGENTRT_ERR_IO
#define AGENTRT_ERR_IO (-11)
#endif
#ifndef AGENTRT_ERR_PARSE_ERROR
#define AGENTRT_ERR_PARSE_ERROR (-12)
#endif
#ifndef AGENTRT_ERR_STATE_ERROR
#define AGENTRT_ERR_STATE_ERROR (-13)
#endif
#ifndef AGENTRT_ERR_OVERFLOW
#define AGENTRT_ERR_OVERFLOW (-14)
#endif
#ifndef AGENTRT_ERR_UNDERFLOW
#define AGENTRT_ERR_UNDERFLOW (-15)
#endif
#ifndef AGENTRT_ERR_CANCELED
#define AGENTRT_ERR_CANCELED (-16)
#endif
#ifndef AGENTRT_ERR_BUSY
#define AGENTRT_ERR_BUSY (-17)
#endif
#ifndef AGENTRT_ERR_WOULD_BLOCK
#define AGENTRT_ERR_WOULD_BLOCK (-18)
#endif
#ifndef AGENTRT_ERR_INTERRUPTED
#define AGENTRT_ERR_INTERRUPTED (-19)
#endif

#ifndef AGENTRT_ERR_NOT_IMPLEMENTED
#define AGENTRT_ERR_NOT_IMPLEMENTED (-30)
#endif
#ifndef AGENTRT_ERR_FAIL
#define AGENTRT_ERR_FAIL (-31)
#endif

/* 系统与平台错误 (-100 到 -199) */
#ifndef AGENTRT_ERR_SYS_BASE
#define AGENTRT_ERR_SYS_BASE (-100)
#endif
#ifndef AGENTRT_ERR_SYS_NOT_INIT
#define AGENTRT_ERR_SYS_NOT_INIT (-101)
#endif
#ifndef AGENTRT_ERR_SYS_RESOURCE
#define AGENTRT_ERR_SYS_RESOURCE (-102)
#endif
#ifndef AGENTRT_ERR_SYS_DEADLOCK
#define AGENTRT_ERR_SYS_DEADLOCK (-103)
#endif
#ifndef AGENTRT_ERR_SYS_THREAD
#define AGENTRT_ERR_SYS_THREAD (-104)
#endif
#ifndef AGENTRT_ERR_SYS_MUTEX
#define AGENTRT_ERR_SYS_MUTEX (-105)
#endif
#ifndef AGENTRT_ERR_SYS_SEMAPHORE
#define AGENTRT_ERR_SYS_SEMAPHORE (-106)
#endif
#ifndef AGENTRT_ERR_SYS_CONDITION
#define AGENTRT_ERR_SYS_CONDITION (-107)
#endif
#ifndef AGENTRT_ERR_SYS_ATOMIC
#define AGENTRT_ERR_SYS_ATOMIC (-108)
#endif
#ifndef AGENTRT_ERR_SYS_SOCKET
#define AGENTRT_ERR_SYS_SOCKET (-109)
#endif
#ifndef AGENTRT_ERR_SYS_PIPE
#define AGENTRT_ERR_SYS_PIPE (-110)
#endif
#ifndef AGENTRT_ERR_SYS_PROCESS
#define AGENTRT_ERR_SYS_PROCESS (-111)
#endif
#ifndef AGENTRT_ERR_SYS_FILE
#define AGENTRT_ERR_SYS_FILE (-112)
#endif
#ifndef AGENTRT_ERR_SYS_TIME
#define AGENTRT_ERR_SYS_TIME (-113)
#endif

/* 内核层错误 (-200 到 -299) */
#ifndef AGENTRT_ERR_KERN_BASE
#define AGENTRT_ERR_KERN_BASE (-200)
#endif
#ifndef AGENTRT_ERR_KERN_IPC
#define AGENTRT_ERR_KERN_IPC (-201)
#endif
#ifndef AGENTRT_ERR_KERN_TASK
#define AGENTRT_ERR_KERN_TASK (-202)
#endif
#ifndef AGENTRT_ERR_KERN_SYNC
#define AGENTRT_ERR_KERN_SYNC (-203)
#endif
#ifndef AGENTRT_ERR_KERN_LOCK
#define AGENTRT_ERR_KERN_LOCK (-204)
#endif
#ifndef AGENTRT_ERR_KERN_MEM
#define AGENTRT_ERR_KERN_MEM (-205)
#endif
#ifndef AGENTRT_ERR_KERN_SCHED
#define AGENTRT_ERR_KERN_SCHED (-206)
#endif
#ifndef AGENTRT_ERR_KERN_TIMER
#define AGENTRT_ERR_KERN_TIMER (-207)
#endif
#ifndef AGENTRT_ERR_KERN_INTERRUPT
#define AGENTRT_ERR_KERN_INTERRUPT (-208)
#endif

/* 服务层错误 (-300 到 -399) */
#ifndef AGENTRT_ERR_SVC_BASE
#define AGENTRT_ERR_SVC_BASE (-300)
#endif
#ifndef AGENTRT_ERR_SVC_NOT_READY
#define AGENTRT_ERR_SVC_NOT_READY (-301)
#endif
#ifndef AGENTRT_ERR_SVC_BUSY
#define AGENTRT_ERR_SVC_BUSY (-302)
#endif
#ifndef AGENTRT_ERR_SVC_STOPPED
#define AGENTRT_ERR_SVC_STOPPED (-303)
#endif
#ifndef AGENTRT_ERR_SVC_CONFIG
#define AGENTRT_ERR_SVC_CONFIG (-304)
#endif
#ifndef AGENTRT_ERR_SVC_DEPENDENCY
#define AGENTRT_ERR_SVC_DEPENDENCY (-305)
#endif
#ifndef AGENTRT_ERR_SVC_HEALTH
#define AGENTRT_ERR_SVC_HEALTH (-306)
#endif
#ifndef AGENTRT_ERR_SVC_LOADBALANCE
#define AGENTRT_ERR_SVC_LOADBALANCE (-307)
#endif

/* LLM/AI服务错误 (-400 到 -499) */
#ifndef AGENTRT_ERR_LLM_BASE
#define AGENTRT_ERR_LLM_BASE (-400)
#endif
#ifndef AGENTRT_ERR_LLM_NO_PROVIDER
#define AGENTRT_ERR_LLM_NO_PROVIDER (-401)
#endif
#ifndef AGENTRT_ERR_LLM_PROVIDER_FAIL
#define AGENTRT_ERR_LLM_PROVIDER_FAIL (-402)
#endif
#ifndef AGENTRT_ERR_LLM_RATE_LIMIT
#define AGENTRT_ERR_LLM_RATE_LIMIT (-403)
#endif
#ifndef AGENTRT_ERR_LLM_CONTEXT_LEN
#define AGENTRT_ERR_LLM_CONTEXT_LEN (-404)
#endif
#ifndef AGENTRT_ERR_LLM_INVALID_MODEL
#define AGENTRT_ERR_LLM_INVALID_MODEL (-405)
#endif
#ifndef AGENTRT_ERR_LLM_AUTH_FAIL
#define AGENTRT_ERR_LLM_AUTH_FAIL (-406)
#endif
#ifndef AGENTRT_ERR_LLM_TOKEN_LIMIT
#define AGENTRT_ERR_LLM_TOKEN_LIMIT (-407)
#endif
#ifndef AGENTRT_ERR_LLM_PARSE_RESP
#define AGENTRT_ERR_LLM_PARSE_RESP (-408)
#endif
#ifndef AGENTRT_ERR_LLM_EMPTY_RESP
#define AGENTRT_ERR_LLM_EMPTY_RESP (-409)
#endif
#ifndef AGENTRT_ERR_LLM_COST_EXCEED
#define AGENTRT_ERR_LLM_COST_EXCEED (-410)
#endif

/* 执行/工具错误 (-500 到 -599) */
#ifndef AGENTRT_ERR_EXEC_BASE
#define AGENTRT_ERR_EXEC_BASE (-500)
#endif
#ifndef AGENTRT_ERR_EXEC_NOT_FOUND
#define AGENTRT_ERR_EXEC_NOT_FOUND (-501)
#endif
#ifndef AGENTRT_ERR_EXEC_FAIL
#define AGENTRT_ERR_EXEC_FAIL (-502)
#endif
#ifndef AGENTRT_ERR_EXEC_TIMEOUT
#define AGENTRT_ERR_EXEC_TIMEOUT (-503)
#endif
#ifndef AGENTRT_ERR_EXEC_VALIDATION
#define AGENTRT_ERR_EXEC_VALIDATION (-504)
#endif
#ifndef AGENTRT_ERR_EXEC_SANDBOX
#define AGENTRT_ERR_EXEC_SANDBOX (-505)
#endif
#ifndef AGENTRT_ERR_EXEC_PERMISSION
#define AGENTRT_ERR_EXEC_PERMISSION (-506)
#endif
#ifndef AGENTRT_ERR_EXEC_ARGS
#define AGENTRT_ERR_EXEC_ARGS (-507)
#endif
#ifndef AGENTRT_ERR_EXEC_ENV
#define AGENTRT_ERR_EXEC_ENV (-508)
#endif

/* 记忆/存储错误 (-600 到 -699) */
#ifndef AGENTRT_ERR_MEM_BASE
#define AGENTRT_ERR_MEM_BASE (-600)
#endif
#ifndef AGENTRT_ERR_MEM_WRITE
#define AGENTRT_ERR_MEM_WRITE (-601)
#endif
#ifndef AGENTRT_ERR_MEM_READ
#define AGENTRT_ERR_MEM_READ (-602)
#endif
#ifndef AGENTRT_ERR_MEM_QUERY
#define AGENTRT_ERR_MEM_QUERY (-603)
#endif
#ifndef AGENTRT_ERR_MEM_EVOLVE
#define AGENTRT_ERR_MEM_EVOLVE (-604)
#endif
#ifndef AGENTRT_ERR_MEM_FULL
#define AGENTRT_ERR_MEM_FULL (-605)
#endif
#ifndef AGENTRT_ERR_MEM_CORRUPT
#define AGENTRT_ERR_MEM_CORRUPT (-606)
#endif
#ifndef AGENTRT_ERR_MEM_NOT_INIT
#define AGENTRT_ERR_MEM_NOT_INIT (-607)
#endif

/* 安全/沙箱错误 (-700 到 -799) */
#ifndef AGENTRT_ERR_SEC_BASE
#define AGENTRT_ERR_SEC_BASE (-700)
#endif
#ifndef AGENTRT_ERR_SEC_VIOLATION
#define AGENTRT_ERR_SEC_VIOLATION (-701)
#endif
#ifndef AGENTRT_ERR_SEC_SANITIZE
#define AGENTRT_ERR_SEC_SANITIZE (-702)
#endif
#ifndef AGENTRT_ERR_SEC_AUDIT
#define AGENTRT_ERR_SEC_AUDIT (-703)
#endif
#ifndef AGENTRT_ERR_SEC_PERMISSION
#define AGENTRT_ERR_SEC_PERMISSION (-704)
#endif
#ifndef AGENTRT_ERR_SEC_VALIDATION
#define AGENTRT_ERR_SEC_VALIDATION (-705)
#endif
#ifndef AGENTRT_ERR_SEC_QUOTA
#define AGENTRT_ERR_SEC_QUOTA (-706)
#endif
#ifndef AGENTRT_ERR_SEC_TEMP_DIR
#define AGENTRT_ERR_SEC_TEMP_DIR (-707)
#endif
#ifndef AGENTRT_ERR_SEC_SYMLINK
#define AGENTRT_ERR_SEC_SYMLINK (-708)
#endif
#ifndef AGENTRT_ERR_SEC_PATH_TRAV
#define AGENTRT_ERR_SEC_PATH_TRAV (-709)
#endif
#ifndef AGENTRT_ERR_ESECURITY
#define AGENTRT_ERR_ESECURITY (-710)
#endif
#ifndef AGENTRT_ERR_ESANITIZE
#define AGENTRT_ERR_ESANITIZE (-711)
#endif

/* Cupolas 安全穹顶专属错误码 (-712 到 -799)
 *
 * P0.25.4 (ACC-STD06)：任务清单原要求 -700~-705 段，但 -700~-711 已被
 * AGENTRT_ERR_SEC_* 占用（v3.4 之前已定义）。为避免数值冲突，Cupolas 专属
 * 错误码段调整为 -712~-799。Cupolas 公共 API 仍可通过 cupolas_ERR_* enum
 * （数值与 AGENTRT_ERR_* 通用码一致，如 cupolas_ERR_OUT_OF_MEMORY=-4）
 * 返回通用错误码；本段仅定义 Cupolas 特有的语义错误（如沙箱隔离、策略拒绝、
 * 审计失败等），供 cupolas 模块内部和调用方区分错误来源。
 *
 * 段分配：
 *   -712  AGENTRT_ERR_CUPOLAS_BASE       段基址
 *   -713  AGENTRT_ERR_CUPOLAS_DENIED     权限/策略拒绝（Cupolas 决策）
 *   -714  AGENTRT_ERR_CUPOLAS_QUARANTINE 隔离/隔离区
 *   -715  AGENTRT_ERR_CUPOLAS_POLICY     策略评估失败
 *   -716  AGENTRT_ERR_CUPOLAS_SANDBOX    沙箱执行失败/逃逸检测
 *   -717  AGENTRT_ERR_CUPOLAS_AUDIT      审计日志写入失败
 *   -718  AGENTRT_ERR_CUPOLAS_TAMPERED   篡改检测
 *   -719  AGENTRT_ERR_CUPOLAS_SIGNATURE  签名验证失败
 *   -720  AGENTRT_ERR_CUPOLAS_VAULT      Vault 凭据访问失败
 *   -721  AGENTRT_ERR_CUPOLAS_ENTITLEMENT 权限声明无效
 *   -722  AGENTRT_ERR_CUPOLAS_RUNTIME    运行时保护违规
 *   -723  AGENTRT_ERR_CUPOLAS_NETWORK    网络安全策略拒绝
 *   -724~-799 预留扩展
 */
#ifndef AGENTRT_ERR_CUPOLAS_BASE
#define AGENTRT_ERR_CUPOLAS_BASE (-712)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_DENIED
#define AGENTRT_ERR_CUPOLAS_DENIED (-713)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_QUARANTINE
#define AGENTRT_ERR_CUPOLAS_QUARANTINE (-714)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_POLICY
#define AGENTRT_ERR_CUPOLAS_POLICY (-715)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_SANDBOX
#define AGENTRT_ERR_CUPOLAS_SANDBOX (-716)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_AUDIT
#define AGENTRT_ERR_CUPOLAS_AUDIT (-717)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_TAMPERED
#define AGENTRT_ERR_CUPOLAS_TAMPERED (-718)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_SIGNATURE
#define AGENTRT_ERR_CUPOLAS_SIGNATURE (-719)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_VAULT
#define AGENTRT_ERR_CUPOLAS_VAULT (-720)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_ENTITLEMENT
#define AGENTRT_ERR_CUPOLAS_ENTITLEMENT (-721)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_RUNTIME
#define AGENTRT_ERR_CUPOLAS_RUNTIME (-722)
#endif
#ifndef AGENTRT_ERR_CUPOLAS_NETWORK
#define AGENTRT_ERR_CUPOLAS_NETWORK (-723)
#endif

/* 协调/规划错误 (-800 到 -899) */
#ifndef AGENTRT_ERR_COORD_BASE
#define AGENTRT_ERR_COORD_BASE (-800)
#endif
#ifndef AGENTRT_ERR_COORD_PLAN_FAIL
#define AGENTRT_ERR_COORD_PLAN_FAIL (-801)
#endif
#ifndef AGENTRT_ERR_COORD_SYNC_FAIL
#define AGENTRT_ERR_COORD_SYNC_FAIL (-802)
#endif
#ifndef AGENTRT_ERR_COORD_DISPATCH
#define AGENTRT_ERR_COORD_DISPATCH (-803)
#endif
#ifndef AGENTRT_ERR_COORD_INTENT
#define AGENTRT_ERR_COORD_INTENT (-804)
#endif
#ifndef AGENTRT_ERR_COORD_COMPENSATE
#define AGENTRT_ERR_COORD_COMPENSATE (-805)
#endif
#ifndef AGENTRT_ERR_COORD_RETRY_EXCEED
#define AGENTRT_ERR_COORD_RETRY_EXCEED (-806)
#endif

/* 协议/校验错误 (-900 到 -909)
 *
 * P0.22.1 (ARE L2)：IPC Bus 统一消息头校验失败的专属错误码段。
 * - AGENTRT_ERR_PROTOCOL  magic/version/reserved 字段不匹配（消息必须丢弃）
 * - AGENTRT_ERR_CHECKSUM  CRC32 校验和不匹配（消息必须丢弃，不得回复 ERROR）
 * 详见 Docs/Capital_Specifications/are_standards/L2_service_protocol.md §2.3
 */
#ifndef AGENTRT_ERR_PROTOCOL
#define AGENTRT_ERR_PROTOCOL (-900)
#endif
#ifndef AGENTRT_ERR_CHECKSUM
#define AGENTRT_ERR_CHECKSUM (-901)
#endif

/* ==================== 错误上下文 ==================== */

/**
 * @brief 错误上下文最大深度
 */
#define AGENTRT_ERROR_CONTEXT_MAX_DEPTH 16

/**
 * @brief 错误严重程度
 */
typedef enum {
    AGENTRT_ERR_SEVERITY_INFO = 0,
    AGENTRT_ERR_SEVERITY_WARNING = 1,
    AGENTRT_ERR_SEVERITY_ERROR = 2,
    AGENTRT_ERR_SEVERITY_CRITICAL = 3
} agentrt_error_severity_t;

/**
 * @brief 错误上下文条目
 */
typedef struct {
    const char *file;
    int line;
    const char *function;
    const char *message;
    agentrt_error_t error_code;
    uint64_t timestamp_ns;
} agentrt_error_context_entry_t;

/**
 * @brief 错误链结构
 */
typedef struct {
    agentrt_error_t code;
    int depth;
    agentrt_error_context_entry_t contexts[AGENTRT_ERROR_CONTEXT_MAX_DEPTH];
} agentrt_error_chain_t;

/* ==================== 错误处理接口 ==================== */

/**
 * @brief 获取错误码的可读描述
 * @param code 错误码
 * @return 错误描述字符串
 */
const char *agentrt_error_str(agentrt_error_t code);

/**
 * @brief 获取错误严重程度
 * @param code 错误码
 * @return 严重程度
 */
agentrt_error_severity_t agentrt_error_get_severity(agentrt_error_t code);

/**
 * @brief 获取当前线程的错误链
 * @return 错误链指针
 */
agentrt_error_chain_t *agentrt_error_get_chain(void);

/**
 * @brief 清除当前线程的错误链
 */
void agentrt_error_clear(void);

/**
 * @brief 清理当前线程的错误状态（释放线程局部存储）
 * @note 应在线程退出前调用，以释放 thread_error_state_t 及其错误链中的 message 字符串
 */
void agentrt_error_thread_cleanup(void);

/**
 * @brief 添加错误上下文
 * @param code 错误码
 * @param file 源文件名
 * @param line 行号
 * @param func 函数名
 * @param fmt 格式化消息
 * @param ... 可变参数
 */
void agentrt_error_push_ex(agentrt_error_t code, const char *file, int line, const char *func,
                           const char *fmt, ...);

/**
 * @brief 打印错误链（用于调试）
 * @param chain 错误链
 */
void agentrt_error_print_chain(const agentrt_error_chain_t *chain);

/**
 * @brief 将错误链转换为 JSON 字符串
 * @param chain 错误链
 * @return JSON 字符串（需调用者释放）
 */
char *agentrt_error_chain_to_json(const agentrt_error_chain_t *chain);

/* ==================== 便捷宏 ==================== */

/**
 * @brief 设置错误并返回
 */
#define AGENTRT_ERROR(code, msg)                                                  \
    do {                                                                          \
        agentrt_error_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        return (code);                                                            \
    } while (0)

/**
 * @brief 设置格式化错误并返回
 */
#define AGENTRT_ERROR_FMT(code, fmt, ...)                                                \
    do {                                                                                 \
        agentrt_error_push_ex((code), __FILE__, __LINE__, __func__, (fmt), __VA_ARGS__); \
        return (code);                                                                   \
    } while (0)

/**
 * @brief 设置错误并返回 NULL（用于返回指针的函数）
 *
 * 与 AGENTRT_ERROR 的区别：返回 NULL 而非错误码，适用于函数返回类型为指针的场景。
 * 错误码通过 error stack 传递，调用者可通过 agentrt_error_last() 获取。
 */
#define AGENTRT_ERROR_NULL(code, msg)                                             \
    do {                                                                          \
        agentrt_error_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        return NULL;                                                              \
    } while (0)

/**
 * @brief 条件检查，失败时返回错误
 */
#define AGENTRT_CHECK(cond, code, msg)    \
    do {                                  \
        if (!(cond)) {                    \
            AGENTRT_ERROR((code), (msg)); \
        }                                 \
    } while (0)

/**
 * @brief 空指针检查
 */
#define AGENTRT_CHECK_NULL(ptr, name) \
    AGENTRT_CHECK((ptr) != NULL, AGENTRT_ERR_NULL_POINTER, name " is NULL")

/**
 * @brief 内存分配检查
 */
#define AGENTRT_CHECK_ALLOC(ptr) \
    AGENTRT_CHECK((ptr) != NULL, AGENTRT_ERR_OUT_OF_MEMORY, "Memory allocation failed")

/**
 * @brief 错误传播宏
 */
#define AGENTRT_PROPAGATE(expr)                                                              \
    do {                                                                                     \
        agentrt_error_t __err = (expr);                                                      \
        if (__err != AGENTRT_OK) {                                                           \
            agentrt_error_push_ex(__err, __FILE__, __LINE__, __func__, "Propagated from %s", \
                                  #expr);                                                    \
            return __err;                                                                    \
        }                                                                                    \
    } while (0)

/**
 * @brief 错误检查宏（返回错误码而非直接返回）
 */
#define AGENTRT_TRY(expr)               \
    do {                                \
        agentrt_error_t __err = (expr); \
        if (__err != AGENTRT_OK) {      \
            return __err;               \
        }                               \
    } while (0)

/* ==================== 向后兼容接口（已废弃） ==================== */

#ifndef AGENTRT_ERROR_CONTEXT_T_DEFINED
#define AGENTRT_ERROR_CONTEXT_T_DEFINED
/**
 * @brief 错误上下文结构（完整版，含时间戳）
 * @note 与 atoms/coreloopthree/include/error_utils.h 保持一致
 */
typedef struct agentrt_error_context {
    agentrt_error_t code;
    char *message;
    char *file;
    int line;
    char *function;
    uint64_t timestamp_ns;
} agentrt_error_context_t;
#endif /* AGENTRT_ERROR_CONTEXT_T_DEFINED */

/**
 * @brief 错误处理回调函数类型
 * @deprecated 请使用新的错误链接口
 */
typedef void (*agentrt_error_handler_t)(agentrt_error_t err,
                                        const agentrt_error_context_t *context);

/**
 * @brief 设置错误处理回调（兼容旧代码）
 * @deprecated
 */
void agentrt_error_set_handler(agentrt_error_handler_t handler);

/**
 * @brief 兼容旧代码的错误处理宏
 * @deprecated 请使用 AGENTRT_ERROR
 */
#define AGENTRT_ERROR_HANDLE(code, msg)                                           \
    do {                                                                          \
        agentrt_error_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
    } while (0)

#define AGENTRT_ERROR_PUSH_EX(code, msg) AGENTRT_ERROR_HANDLE(code, msg)

/**
 * @brief 兼容旧代码的错误处理宏（带上下文）
 * @deprecated
 */
#define AGENTRT_ERROR_HANDLE_CONTEXT(code, user_data, msg)                        \
    do {                                                                          \
        agentrt_error_push_ex((code), __FILE__, __LINE__, __func__, "%s", (msg)); \
        (void)(user_data);                                                        \
    } while (0)

/* ==================== 错误统计 ==================== */

/**
 * @brief 错误统计信息
 */
typedef struct {
    uint64_t total_errors;
    uint64_t errors_by_code[32];
    uint64_t last_error_time;
    agentrt_error_t last_error;
} agentrt_error_stats_t;

/**
 * @brief 获取错误统计
 * @param stats 统计信息输出
 */
void agentrt_error_get_stats(agentrt_error_stats_t *stats);

/**
 * @brief 重置错误统计
 */
void agentrt_error_reset_stats(void);

/* ==================== 多语言支持 ==================== */

/**
 * @brief 支持的语言
 */
typedef enum {
    AGENTRT_LANG_EN_US = 0, /**< 英语（美国） */
    AGENTRT_LANG_ZH_CN = 1, /**< 简体中文 */
    AGENTRT_LANG_ZH_TW = 2, /**< 繁体中文 */
    AGENTRT_LANG_JA_JP = 3, /**< 日语 */
    AGENTRT_LANG_KO_KR = 4, /**< 韩语 */
    AGENTRT_LANG_DE_DE = 5, /**< 德语 */
    AGENTRT_LANG_FR_FR = 6, /**< 法语 */
    AGENTRT_LANG_ES_ES = 7  /**< 西班牙语 */
} agentrt_language_t;

/**
 * @brief 多语言错误描述结构
 */
typedef struct {
    agentrt_error_t error_code;  /**< 错误码 */
    const char *descriptions[8]; /**< 各语言描述（按agentrt_language_t顺序） */
} agentrt_error_i18n_entry_t;

/**
 * @brief 设置当前语言环境
 *
 * @param[in] lang 语言
 * @return 成功返回AGENTRT_OK，失败返回错误码
 */
agentrt_error_t agentrt_error_set_language(agentrt_language_t lang);

/**
 * @brief 获取当前语言环境
 *
 * @return 当前语言
 */
agentrt_language_t agentrt_error_get_language(void);

/**
 * @brief 获取错误码的本地化描述
 *
 * @param[in] code 错误码
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return 本地化错误描述字符串
 */
const char *agentrt_error_str_i18n(agentrt_error_t code, agentrt_language_t lang);

/**
 * @brief 注册自定义错误码的本地化描述
 *
 * @param[in] entries 错误描述条目数组
 * @param[in] count 条目数量
 * @return 成功返回AGENTRT_OK，失败返回错误码
 */
agentrt_error_t agentrt_error_register_i18n(const agentrt_error_i18n_entry_t *entries,
                                            size_t count);

/**
 * @brief 获取错误链的本地化JSON表示
 *
 * @param[in] chain 错误链
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return JSON字符串（需调用者释放）
 */
char *agentrt_error_chain_to_json_i18n(const agentrt_error_chain_t *chain, agentrt_language_t lang);

/* ==================== 错误链增强功能 ==================== */

/**
 * @brief 错误链迭代器
 */
typedef struct {
    const agentrt_error_chain_t *chain; /**< 错误链 */
    size_t current_index;               /**< 当前索引 */
} agentrt_error_chain_iterator_t;

/**
 * @brief 初始化错误链迭代器
 *
 * @param[in] chain 错误链
 * @param[out] iter 迭代器
 */
void agentrt_error_chain_iter_init(const agentrt_error_chain_t *chain,
                                   agentrt_error_chain_iterator_t *iter);

/**
 * @brief 获取下一个错误上下文条目
 *
 * @param[inout] iter 迭代器
 * @return 下一个条目指针，如果没有更多条目返回NULL
 */
const agentrt_error_context_entry_t *
agentrt_error_chain_iter_next(agentrt_error_chain_iterator_t *iter);

/**
 * @brief 重置错误链迭代器
 *
 * @param[inout] iter 迭代器
 */
void agentrt_error_chain_iter_reset(agentrt_error_chain_iterator_t *iter);

/**
 * @brief 获取错误链深度
 *
 * @param[in] chain 错误链
 * @return 链深度
 */
int agentrt_error_chain_get_depth(const agentrt_error_chain_t *chain);

/**
 * @brief 获取错误链中最早的错误码
 *
 * @param[in] chain 错误链
 * @return 最早的错误码
 */
agentrt_error_t agentrt_error_chain_get_root_error(const agentrt_error_chain_t *chain);

/**
 * @brief 获取错误链中最新的错误码
 *
 * @param[in] chain 错误链
 * @return 最新的错误码
 */
agentrt_error_t agentrt_error_chain_get_latest_error(const agentrt_error_chain_t *chain);

/**
 * @brief 将错误链格式化为可读字符串
 *
 * @param[in] chain 错误链
 * @param[in] lang 语言（如果为-1，使用当前语言环境）
 * @return 格式化字符串（需调用者释放）
 */
char *agentrt_error_chain_format(const agentrt_error_chain_t *chain, agentrt_language_t lang);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_UTILS_ERROR_H */
