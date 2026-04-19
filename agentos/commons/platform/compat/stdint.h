/*
 * C99 stdint.h compatibility header
 *
 * Provides <stdint.h> for compilers that lack it.
 * Falls back to built-in types when necessary.
 *
 * 原位置: agentos/include/agentos/compat/stdint.h
 * 迁移至: agentos/commons/platform/compat/ (2026-04-19 include/整合重构)
 */

#ifndef AGENTOS_COMPAT_STDINT_H
#define AGENTOS_COMPAT_STDINT_H

#include <stdint.h>

/* Ensure standard types are available */
#ifndef int8_t
typedef signed char int8_t;
#endif

#ifndef uint8_t
typedef unsigned char uint8_t;
#endif

#ifndef int16_t
typedef short int16_t;
#endif

#ifndef uint16_t
typedef unsigned short uint16_t;
#endif

#ifndef int32_t
typedef int int32_t;
#endif

#ifndef uint32_t
typedef unsigned int uint32_t;
#endif

#ifndef int64_t
#ifdef _WIN64
typedef __int64 int64_t;
#else
typedef long long int64_t;
#endif
#endif

#ifndef uint64_t
#ifdef _WIN64
typedef unsigned __int64 uint64_t;
#else
typedef unsigned long long uint64_t;
#endif
#endif

#ifndef intptr_t
typedef ptrdiff_t intptr_t;
#endif

#ifndef uintptr_t
typedef size_t uintptr_t;
#endif

#endif /* AGENTOS_COMPAT_STDINT_H */
