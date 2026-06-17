#ifndef AGENTOS_BANNED_FUNCTIONS_H
#define AGENTOS_BANNED_FUNCTIONS_H

#ifdef AGENTOS_COMPLIANCE_STRICT

#ifndef AGENTOS_COMPLIANCE_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma GCC poison malloc
#pragma GCC poison free
#pragma GCC poison calloc
#pragma GCC poison realloc
#pragma GCC poison strdup
#pragma GCC poison strndup
#ifndef AGENTOS_HAS_CURL
#pragma GCC poison printf
#pragma GCC poison fprintf
#endif
#pragma GCC poison sprintf
#pragma GCC poison vsprintf
#pragma GCC poison gets
#pragma GCC poison scanf
#pragma GCC poison strcpy
#pragma GCC poison strcat
#pragma GCC poison tmpnam
#pragma GCC poison mktemp
#pragma GCC poison strtok
#pragma GCC poison localtime
#pragma GCC poison gmtime
#pragma GCC poison asprintf
#pragma GCC poison vasprintf

/* INF-06: 扩展 strict 模式至30条 — 新增危险函数毒化 */
#pragma GCC poison fscanf          /* BAN-151: 无界文件输入 */
#pragma GCC poison sscanf           /* BAN-152: 无界字符串解析 */
#pragma GCC poison strncpy         /* BAN-155: 不保证null终止 */
#pragma GCC poison memcpy           /* BAN-154: 需AGENTOS_MEMCPY_SAFE替代 */
#pragma GCC poison memmove          /* BAN-154: 需安全包装替代 */
#pragma GCC poison memset           /* BAN-154: 需验证大小参数 */
#endif

#else

#ifndef AGENTOS_COMPLIANCE_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

__attribute__((deprecated("Use AGENTOS_MALLOC instead")))
static inline void *__banned_malloc(size_t s) { return (malloc)(s); }

__attribute__((deprecated("Use AGENTOS_FREE instead")))
static inline void __banned_free(void *p) { (free)(p); }

__attribute__((deprecated("Use AGENTOS_CALLOC instead")))
static inline void *__banned_calloc(size_t n, size_t s) { return (calloc)(n, s); }

__attribute__((deprecated("Use AGENTOS_REALLOC instead")))
static inline void *__banned_realloc(void *p, size_t s) { return (realloc)(p, s); }

__attribute__((deprecated("Use AGENTOS_STRDUP instead")))
static inline char *__banned_strdup(const char *s) { return (strdup)(s); }

__attribute__((deprecated("Use AGENTOS_STRNDUP instead")))
static inline char *__banned_strndup(const char *s, size_t n) { return (strndup)(s, n); }

__attribute__((deprecated("Use AGENTOS_STRCPY instead")))
static inline char *__banned_strcpy(char *d, const char *s) { return (strcpy)(d, s); }

__attribute__((deprecated("Use AGENTOS_STRCAT instead")))
static inline char *__banned_strcat(char *d, const char *s) { return (strcat)(d, s); }

__attribute__((deprecated("Use strtok_r instead")))
static inline char *__banned_strtok(char *s, const char *d) { return (strtok)(s, d); }

__attribute__((deprecated("Use localtime_r instead")))
static inline struct tm *__banned_localtime(const time_t *t) { return (localtime)(t); }

__attribute__((deprecated("Use gmtime_r instead")))
static inline struct tm *__banned_gmtime(const time_t *t) { return (gmtime)(t); }

#define malloc(s)       __banned_malloc(s)
#define free(p)         __banned_free(p)
#define calloc(n, s)    __banned_calloc(n, s)
#define realloc(p, s)   __banned_realloc(p, s)
#define strdup(s)       __banned_strdup(s)
#define strndup(s, n)   __banned_strndup(s, n)
#define strcpy(d, s)    __banned_strcpy(d, s)
#define strcat(d, s)    __banned_strcat(d, s)
#define strtok(s, d)    __banned_strtok(s, d)
#define localtime(t)    __banned_localtime(t)
#define gmtime(t) __banned_gmtime(t)
#endif

/* BAN-151~BAN-162: 危险I/O函数 — gets/scanf系列在非strict模式下也标记为deprecated */
#include <stdarg.h>
#include <stdio.h>

__attribute__((deprecated("Use AGENTOS_PRINTF instead — printf() has no output limit")))
static inline int __agentos_banned_printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vprintf(fmt, ap); va_end(ap); return r; }

__attribute__((deprecated("Use fgets instead — gets() has no buffer limit and causes buffer overflow")))
static inline char *__banned_gets(char *s) { (void)s; return NULL; }

__attribute__((deprecated("Use fgets+sscanf instead — scanf(\"%s\") has no buffer limit")))
static inline int __banned_scanf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vscanf(fmt, ap); va_end(ap); return r; }

__attribute__((deprecated("Use fgets+sscanf instead — fscanf(\"%s\") has no buffer limit")))
static inline int __banned_fscanf(FILE *fp, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfscanf(fp, fmt, ap); va_end(ap); return r; }

__attribute__((deprecated("Validate buffer bounds before sscanf — prefer snprintf for output")))
static inline int __banned_sscanf(const char *s, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsscanf(s, fmt, ap); va_end(ap); return r; }

#define printf(fmt, ...) __agentos_banned_printf(fmt, ##__VA_ARGS__)
#define gets(s)         __banned_gets(s)
#define scanf(fmt, ...) __banned_scanf(fmt, ##__VA_ARGS__)
#define fscanf(fp, fmt, ...) __banned_fscanf(fp, fmt, ##__VA_ARGS__)
#define sscanf(s, fmt, ...) __banned_sscanf(s, fmt, ##__VA_ARGS__)

#endif

#endif