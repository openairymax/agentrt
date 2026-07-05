// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file platform.h
 * @brief 平台抽象兼容层（daemon 专用）
 *
 * 本文件是 agentrt/commons/platform 的兼容层，提供向后兼容的 API。
 * 新代码应直接使用 #include <platform.h>
 *
 * @see agentrt/commons/platform/include/platform.h
 */

#ifndef AGENTRT_DAEMON_COMMON_PLATFORM_H
#define AGENTRT_DAEMON_COMMON_PLATFORM_H

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
#if AGENTRT_PLATFORM_POSIX
#include <dlfcn.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <sys/utsname.h>
#endif

/* ==================== 兼容性别名 ==================== */

typedef agentrt_mutex_t agentrt_platform_mutex_t;
#define agentrt_platform_mutex_init agentrt_mutex_init
#define agentrt_platform_mutex_lock agentrt_mutex_lock
#define agentrt_platform_mutex_unlock agentrt_mutex_unlock
#define agentrt_platform_mutex_destroy agentrt_mutex_destroy
#define agentrt_platform_get_time_ms agentrt_time_ms

typedef agentrt_cond_t agentrt_platform_cond_val_t;
#define agentrt_platform_cond_init agentrt_cond_init
#define agentrt_platform_cond_destroy agentrt_cond_destroy
#define agentrt_platform_cond_wait agentrt_cond_wait
#define agentrt_platform_cond_timedwait agentrt_cond_timedwait
#define agentrt_platform_cond_signal agentrt_cond_signal
#define agentrt_platform_cond_broadcast agentrt_cond_broadcast

typedef agentrt_thread_t agentrt_platform_thread_t;
#define agentrt_platform_thread_create agentrt_platform_thread_create
#define agentrt_platform_thread_join agentrt_platform_thread_join

#ifndef AGENTRT_THREAD_LOCAL
#define AGENTRT_THREAD_LOCAL _Thread_local
#endif

#ifndef AGENTRT_INLINE
#define AGENTRT_INLINE inline
#endif

#ifndef AGENTRT_UNUSED
#define AGENTRT_UNUSED(x) (void)(x)
#endif

/* ==================== 类型兼容 ==================== */

typedef agentrt_mutex_t *agentrt_mutex_handle_t;
typedef agentrt_cond_t *agentrt_cond_handle_t;
typedef agentrt_cond_t *agentrt_platform_cond_t;
typedef agentrt_thread_t *agentrt_thread_handle_t;
typedef agentrt_socket_t *agentrt_socket_handle_t;

/* ==================== 额外类型定义 ==================== */

#ifndef AGENTRT_TIMESTAMP_T_DEFINED
#define AGENTRT_TIMESTAMP_T_DEFINED
typedef uint64_t agentrt_timestamp_t;
#endif

typedef struct {
    uint64_t seconds;
    uint32_t nanoseconds;
} agentrt_timestamp_struct_t;

typedef struct {
    int exit_code;
    int signal;
} agentrt_process_status_t;

typedef enum { AGENTRT_AF_INET, AGENTRT_AF_INET6, AGENTRT_AF_UNIX } agentrt_address_family_t;

typedef enum {
    AGENTRT_SOCK_STREAM,
    AGENTRT_SOCK_DGRAM,
    AGENTRT_SOCK_SEQPACKET
} agentrt_socket_type_t;

typedef struct {
    agentrt_address_family_t family;
    uint16_t port;
    union {
        uint8_t ipv4[4];
        uint8_t ipv6[16];
        char path[108];
    } addr;
} agentrt_sockaddr_t;

/* ==================== 兼容性函数声明（实现在 platform_compat.c） ==================== */

int agentrt_time_now(agentrt_timestamp_t *ts);
int agentrt_time_monotonic(agentrt_timestamp_t *ts);
uint64_t agentrt_time_to_ms(const agentrt_timestamp_t *ts);
void agentrt_time_from_ms(uint64_t ms, agentrt_timestamp_t *ts);
void agentrt_sleep_ms(uint32_t ms);
uint32_t agentrt_process_self(void);
uint64_t agentrt_thread_self(void);
int agentrt_thread_setname(const char *name);
int agentrt_thread_getname(char *name, size_t size);
int agentrt_mkdir(const char *path, int recursive);

typedef void *agentrt_dl_t;
agentrt_dl_t agentrt_dl_open(const char *path);
int agentrt_dl_close(agentrt_dl_t dl);
void *agentrt_dl_sym(agentrt_dl_t dl, const char *name);
const char *agentrt_dl_error(void);

#ifndef AGENTRT_SYSINFO_T_DEFINED
#define AGENTRT_SYSINFO_T_DEFINED
typedef struct {
    char os_name[64];
    char os_version[64];
    char hostname[64];
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} agentrt_sysinfo_t;
#endif

int agentrt_get_sysinfo(agentrt_sysinfo_t *info);

/* ==================== 原子操作兼容 ==================== */
/* atomic_load_32/atomic_store_32/atomic_fetch_add_32/atomic_fetch_sub_32
   are provided by atomic_compat.h — do NOT redefine here */

#ifndef ATOMIC_COMPAT_HAS_32
/* Fallback only if atomic_compat.h doesn't provide them */
#endif

/* ==================== 服务器端 Socket 兼容 ==================== */

int agentrt_socket_init(void);
void agentrt_socket_cleanup(void);
agentrt_socket_t agentrt_socket_create_tcp_server(const char *host, uint16_t port);

#if AGENTRT_PLATFORM_POSIX
agentrt_socket_t agentrt_socket_create_unix_server(const char *path);
#endif

agentrt_socket_t agentrt_socket_accept(agentrt_socket_t server_fd, uint32_t timeout_ms);
ssize_t agentrt_socket_recv(agentrt_socket_t sock, void *buf, size_t len);
ssize_t agentrt_socket_send(agentrt_socket_t sock, const void *buf, size_t len);

#endif /* AGENTRT_DAEMON_COMMON_PLATFORM_H */
