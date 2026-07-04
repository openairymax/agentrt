// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file platform.h
 * @brief 平台抽象兼容层（daemon 专用）
 *
 * 本文件是 agentos/commons/platform 的兼容层，提供向后兼容的 API。
 * 新代码应直接使用 #include <platform.h>
 *
 * @see agentos/commons/platform/include/platform.h
 */

#ifndef AGENTOS_DAEMON_COMMON_PLATFORM_H
#define AGENTOS_DAEMON_COMMON_PLATFORM_H

#include "compat.h"

/* ==================== 基本类型 ==================== */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 包含 commons 的统一平台抽象层 ==================== */
/* 使用相对路径避免递归包含自身 */
#include "../../../commons/platform/include/platform.h"

#include <atomic_compat.h>
#include <compat.h>

/* ==================== daemon 额外需要的系统头文件 ==================== */
#if AGENTOS_PLATFORM_POSIX
#include <dlfcn.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <sys/utsname.h>
#endif

/* ==================== 兼容性别名 ==================== */

typedef agentos_mutex_t agentos_platform_mutex_t;
#define agentos_platform_mutex_init agentos_mutex_init
#define agentos_platform_mutex_lock agentos_mutex_lock
#define agentos_platform_mutex_unlock agentos_mutex_unlock
#define agentos_platform_mutex_destroy agentos_mutex_destroy
#define agentos_platform_get_time_ms agentos_time_ms

typedef agentos_cond_t agentos_platform_cond_val_t;
#define agentos_platform_cond_init agentos_cond_init
#define agentos_platform_cond_destroy agentos_cond_destroy
#define agentos_platform_cond_wait agentos_cond_wait
#define agentos_platform_cond_timedwait agentos_cond_timedwait
#define agentos_platform_cond_signal agentos_cond_signal
#define agentos_platform_cond_broadcast agentos_cond_broadcast

typedef agentos_thread_t agentos_platform_thread_t;
#define agentos_platform_thread_create agentos_platform_thread_create
#define agentos_platform_thread_join agentos_platform_thread_join

#ifndef AGENTOS_THREAD_LOCAL
#define AGENTOS_THREAD_LOCAL _Thread_local
#endif

#ifndef AGENTOS_INLINE
#define AGENTOS_INLINE inline
#endif

#ifndef AGENTOS_UNUSED
#define AGENTOS_UNUSED(x) (void)(x)
#endif

/* ==================== 类型兼容 ==================== */

typedef agentos_mutex_t *agentos_mutex_handle_t;
typedef agentos_cond_t *agentos_cond_handle_t;
typedef agentos_cond_t *agentos_platform_cond_t;
typedef agentos_thread_t *agentos_thread_handle_t;
typedef agentos_socket_t *agentos_socket_handle_t;

/* ==================== 额外类型定义 ==================== */

#ifndef AGENTOS_TIMESTAMP_T_DEFINED
#define AGENTOS_TIMESTAMP_T_DEFINED
typedef uint64_t agentos_timestamp_t;
#endif

typedef struct {
    uint64_t seconds;
    uint32_t nanoseconds;
} agentos_timestamp_struct_t;

typedef struct {
    int exit_code;
    int signal;
} agentos_process_status_t;

typedef enum { AGENTOS_AF_INET, AGENTOS_AF_INET6, AGENTOS_AF_UNIX } agentos_address_family_t;

typedef enum {
    AGENTOS_SOCK_STREAM,
    AGENTOS_SOCK_DGRAM,
    AGENTOS_SOCK_SEQPACKET
} agentos_socket_type_t;

typedef struct {
    agentos_address_family_t family;
    uint16_t port;
    union {
        uint8_t ipv4[4];
        uint8_t ipv6[16];
        char path[108];
    } addr;
} agentos_sockaddr_t;

/* ==================== 兼容性函数声明（实现在 platform_compat.c） ==================== */

int agentos_time_now(agentos_timestamp_t *ts);
int agentos_time_monotonic(agentos_timestamp_t *ts);
uint64_t agentos_time_to_ms(const agentos_timestamp_t *ts);
void agentos_time_from_ms(uint64_t ms, agentos_timestamp_t *ts);
void agentos_sleep_ms(uint32_t ms);
uint32_t agentos_process_self(void);
uint64_t agentos_thread_self(void);
int agentos_thread_setname(const char *name);
int agentos_thread_getname(char *name, size_t size);
int agentos_mkdir(const char *path, int recursive);

typedef void *agentos_dl_t;
agentos_dl_t agentos_dl_open(const char *path);
int agentos_dl_close(agentos_dl_t dl);
void *agentos_dl_sym(agentos_dl_t dl, const char *name);
const char *agentos_dl_error(void);

#ifndef AGENTOS_SYSINFO_T_DEFINED
#define AGENTOS_SYSINFO_T_DEFINED
typedef struct {
    char os_name[64];
    char os_version[64];
    char hostname[64];
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} agentos_sysinfo_t;
#endif

int agentos_get_sysinfo(agentos_sysinfo_t *info);

/* ==================== 原子操作兼容 ==================== */
/* atomic_load_32/atomic_store_32/atomic_fetch_add_32/atomic_fetch_sub_32
   are provided by atomic_compat.h — do NOT redefine here */

#ifndef ATOMIC_COMPAT_HAS_32
/* Fallback only if atomic_compat.h doesn't provide them */
#endif

/* ==================== 服务器端 Socket 兼容 ==================== */

int agentos_socket_init(void);
void agentos_socket_cleanup(void);
agentos_socket_t agentos_socket_create_tcp_server(const char *host, uint16_t port);

#if AGENTOS_PLATFORM_POSIX
agentos_socket_t agentos_socket_create_unix_server(const char *path);
#endif

agentos_socket_t agentos_socket_accept(agentos_socket_t server_fd, uint32_t timeout_ms);
ssize_t agentos_socket_recv(agentos_socket_t sock, void *buf, size_t len);
ssize_t agentos_socket_send(agentos_socket_t sock, const void *buf, size_t len);

#endif /* AGENTOS_DAEMON_COMMON_PLATFORM_H */
