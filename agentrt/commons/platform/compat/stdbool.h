/*
 * C99 stdbool.h compatibility header
 *
 * Provides <stdbool.h> for compilers that lack it.
 *
 * 原位置: agentos/include/agentrt/compat/stdbool.h
 * 迁移至: agentrt/commons/platform/compat/ (2026-04-19 include/整合重构)
 */

#ifndef AGENTRT_COMPAT_STDBOOL_H
#define AGENTRT_COMPAT_STDBOOL_H

#ifndef __cplusplus

/* C99 compatible bool type */
#ifndef __STDC_VERSION__
typedef unsigned char _Bool;
#endif

#ifndef bool
#define bool _Bool
#endif

#ifndef true
#define true 1
#endif

#ifndef false
#define false 0
#endif

#else /* __cplusplus */

/* In C++, bool is a built-in type */
#ifndef bool
#define bool bool
#endif

#endif /* __cplusplus */

#endif /* AGENTRT_COMPAT_STDBOOL_H */
