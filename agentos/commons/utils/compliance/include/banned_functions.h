#ifndef AGENTOS_BANNED_FUNCTIONS_H
#define AGENTOS_BANNED_FUNCTIONS_H

#ifdef AGENTOS_COMPLIANCE_STRICT

#ifndef AGENTOS_COMPLIANCE_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma GCC poison malloc
#pragma GCC poison free
#pragma GCC poison calloc
#pragma GCC poison realloc
#pragma GCC poison strdup
#pragma GCC poison fprintf
#pragma GCC poison sprintf
#pragma GCC poison vsprintf
#pragma GCC poison gets
#pragma GCC poison scanf
#pragma GCC poison strcpy
#pragma GCC poison strcat
#pragma GCC poison tmpnam
#pragma GCC poison mktemp
#endif

#else

#ifndef AGENTOS_COMPLIANCE_IMPL
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

__attribute__((deprecated("Use AGENTOS_STRCPY instead")))
static inline char *__banned_strcpy(char *d, const char *s) { return (strcpy)(d, s); }

__attribute__((deprecated("Use AGENTOS_STRCAT instead")))
static inline char *__banned_strcat(char *d, const char *s) { return (strcat)(d, s); }

#define malloc(s)       __banned_malloc(s)
#define free(p)         __banned_free(p)
#define calloc(n, s)    __banned_calloc(n, s)
#define realloc(p, s)   __banned_realloc(p, s)
#define strdup(s)       __banned_strdup(s)
#define strcpy(d, s)    __banned_strcpy(d, s)
#define strcat(d, s)    __banned_strcat(d, s)

#endif

#endif

#endif