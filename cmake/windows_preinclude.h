#ifndef AIRY_RT_WINDOWS_PREINCLUDE_H
#define AIRY_RT_WINDOWS_PREINCLUDE_H

#ifdef _MSC_VER

/* /std:c11 → __STDC__=1 → UCRT corecrt.h 默认 _CRT_INTERNAL_NONSTDC_NAMES=0，
 * 关闭 _access/_open/_write/_close/_mktemp_s 与 S_ISDIR 等 POSIX 系声明
 * （#116 windows-build C4013 实证）。corecrt.h 的取值在首个 UCRT 头展开
 * 时锁定——本文件的 winsock2.h 即触发点——因此必须在此（任何 #include
 * 之前）定义为 1。 */
#ifndef _CRT_DECLARE_NONSTDC_NAMES
#define _CRT_DECLARE_NONSTDC_NAMES 1
#endif
/* rand_s 声明要求 _CRT_RAND_S 在 stdlib.h 首次展开前定义（本文件下方
 * 即 #include <stdlib.h>，include guard 使后续源文件的重复包含失效）。
 * #116 实证：atoms/memory/builtin_storage.c C4013 'rand_s'。 */
#ifndef _CRT_RAND_S
#define _CRT_RAND_S 1
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <string.h>

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

/* pid_t：POSIX 进程 ID 类型。UCRT 无此类型（#124 实证 agent_d
 * service_spawn.c 在 Windows 分支使用 pid_t → C2081/C2059）。Windows
 * 侧子进程句柄为 HANDLE，pid_t 在此仅作"-1 = 无子进程"哨兵（POSIX
 * 成员域在 service.h 已按 AIRY_PLATFORM_POSIX 守卫），int 语义足够。 */
#ifndef AIRY_MSVC_PID_T_DEFINED
#define AIRY_MSVC_PID_T_DEFINED
typedef int pid_t;
#endif

#ifndef AIRY_UNUSED
#define AIRY_UNUSED  __pragma(warning(suppress:4100))
#endif

#define __attribute__(x)

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

#define strcasecmp      _stricmp
#define strncasecmp     _strnicmp
#define strdup          _strdup

#ifndef _STRINGS_H
#define _STRINGS_H
#endif

/* POSIX S_IS* 宏族：MSVC sys/stat.h 只提供 _S_IFMT/_S_IFDIR 等常量，
 * 不定义 S_ISDIR/S_ISREG 函数宏（#112 实证：gateway_hall_store.obj /
 * tool_d plugin_discovery.obj 把 S_ISDIR(mode) 当隐式函数调用 → LNK2001）。
 * 宏体在调用点展开，此时源文件已包含 <sys/stat.h>（_S_IFMT 已定义）。
 * #ifndef 守卫：兼容未来任何提供 S_IS* 的头文件。 */
#ifndef S_ISDIR
#define S_ISDIR(m)  (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m)  (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISCHR
#define S_ISCHR(m)  (((m) & _S_IFMT) == _S_IFCHR)
#endif

#define __ATOMIC_RELAXED    0
#define __ATOMIC_CONSUME    1
#define __ATOMIC_ACQUIRE    2
#define __ATOMIC_RELEASE    3
#define __ATOMIC_ACQ_REL    4
#define __ATOMIC_SEQ_CST    5

#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE            1
#endif
#ifndef _SC_NPROCESSORS_ONLN
#define _SC_NPROCESSORS_ONLN    2
#endif
#ifndef _SC_OPEN_MAX
#define _SC_OPEN_MAX            3
#endif
#ifndef _SC_CLK_TCK
#define _SC_CLK_TCK             4
#endif

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC         1
#endif
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME          0
#endif

#define strtok_r        strtok_s

/* memmem：GNU 扩展（glibc），UCRT 无此函数。cli_gw.c 用于 HTTP 响应头
 * 解析（#118 实证 LNK2001 memmem）。实现子串搜索（O(n*m)，仅作头解析
 * 足够）；needle_len==0 返回 haystack 起点（与 glibc 语义一致）。 */
static inline void *airy_msvc_memmem(const void *haystack, size_t haystack_len,
                                     const void *needle, size_t needle_len)
{
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    if (!needle_len)
        return (void *)h;
    if (!haystack || !needle || haystack_len < needle_len)
        return NULL;
    for (size_t i = 0; i + needle_len <= haystack_len; i++) {
        if (h[i] == n[0] && memcmp(h + i, n, needle_len) == 0)
            return (void *)(h + i);
    }
    return NULL;
}
#define memmem(haystack, haystack_len, needle, needle_len) \
    airy_msvc_memmem((haystack), (haystack_len), (needle), (needle_len))

/* GCC __builtin_* 函数映射到标准 C 函数（MSVC 不支持 __builtin_ 前缀）*/
#define __builtin_memcpy    memcpy
#define __builtin_memset    memset
#define __builtin_memmove   memmove
#define __builtin_fprintf   fprintf
#define __builtin_strcmp    strcmp
#define __builtin_strncpy   strncpy
#define __builtin_strncmp   strncmp
#define __builtin_strlen    strlen
#define __builtin_expect(x, y)  (x)
#define __builtin_prefetch(x, ...)  ((void)(x))
#define __builtin_offsetof(type, member)  offsetof(type, member)
#define __builtin_snprintf  snprintf

/* MSVC 等效的位操作内建函数 */
static inline int airy_msvc_ctz(unsigned int x) {
    unsigned long r;
    _BitScanForward(&r, x);
    return (int)r;
}
static inline int airy_msvc_clz(unsigned int x) {
    unsigned long r;
    _BitScanReverse(&r, x);
    return 31 - (int)r;
}
#define __builtin_ctz(x)        airy_msvc_ctz((unsigned int)(x))
#define __builtin_clz(x)        airy_msvc_clz((unsigned int)(x))
#define __builtin_popcount(x)   __popcnt((unsigned int)(x))
#define __builtin_unreachable() __assume(0)

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL    0
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR       SD_BOTH
#endif

#ifndef AIRY_HAS_CJSON
struct cJSON { int type; char *valuestring; double valuedouble; int valueint; char *string; struct cJSON *next; struct cJSON *prev; struct cJSON *child; };
typedef struct cJSON cJSON;
static inline cJSON* cJSON_CreateObject(void) { return (cJSON*)calloc(1, sizeof(cJSON)); }
static inline cJSON* cJSON_CreateArray(void) { return (cJSON*)calloc(1, sizeof(cJSON)); }
static inline void cJSON_Delete(cJSON* c) { free(c); }
static inline char* cJSON_PrintUnformatted(cJSON* c) { (void)c; errno = ENOSYS; return NULL; }
static inline void cJSON_AddNumberToObject(cJSON* o, const char* n, double d) { (void)o;(void)n;(void)d; errno = ENOSYS; }
static inline void cJSON_AddStringToObject(cJSON* o, const char* n, const char* s) { (void)o;(void)n;(void)s; errno = ENOSYS; }
static inline void cJSON_AddBoolToObject(cJSON* o, const char* n, int b) { (void)o;(void)n;(void)b; errno = ENOSYS; }
static inline void cJSON_AddItemToArray(cJSON* a, cJSON* i) { (void)a;(void)i; errno = ENOSYS; }
static inline void cJSON_AddItemToObject(cJSON* o, const char* n, cJSON* i) { (void)o;(void)n;(void)i; errno = ENOSYS; }
#endif

#endif /* _MSC_VER */
#endif /* AIRY_RT_WINDOWS_PREINCLUDE_H */
