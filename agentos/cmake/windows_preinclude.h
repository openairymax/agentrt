#ifndef AGENTOS_WINDOWS_PREINCLUDE_H
#define AGENTOS_WINDOWS_PREINCLUDE_H

#ifdef _MSC_VER

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

#ifndef AGENTOS_UNUSED
#define AGENTOS_UNUSED  __pragma(warning(suppress:4100))
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

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL    0
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR       SD_BOTH
#endif

#ifndef CORELOOPTHREE_HAS_CJSON
struct cJSON { int type; char *valuestring; double valuedouble; int valueint; char *string; struct cJSON *next; struct cJSON *prev; struct cJSON *child; };
typedef struct cJSON cJSON;
static inline cJSON* cJSON_CreateObject(void) { return (cJSON*)calloc(1, sizeof(cJSON)); }
static inline cJSON* cJSON_CreateArray(void) { return (cJSON*)calloc(1, sizeof(cJSON)); }
static inline void cJSON_Delete(cJSON* c) { free(c); }
static inline char* cJSON_PrintUnformatted(cJSON* c) { (void)c; return _strdup("{}"); }
static inline void cJSON_AddNumberToObject(cJSON* o, const char* n, double d) { (void)o;(void)n;(void)d; }
static inline void cJSON_AddStringToObject(cJSON* o, const char* n, const char* s) { (void)o;(void)n;(void)s; }
static inline void cJSON_AddBoolToObject(cJSON* o, const char* n, int b) { (void)o;(void)n;(void)b; }
static inline void cJSON_AddItemToArray(cJSON* a, cJSON* i) { (void)a;(void)i; }
static inline void cJSON_AddItemToObject(cJSON* o, const char* n, cJSON* i) { (void)o;(void)n;(void)i; }
#endif

#endif /* _MSC_VER */
#endif /* AGENTOS_WINDOWS_PREINCLUDE_H */
