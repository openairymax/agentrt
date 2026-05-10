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

#define strcasecmp      _stricmp
#define strncasecmp     _strnicmp
#define strdup          _strdup

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

#endif /* _MSC_VER */
#endif /* AGENTOS_WINDOWS_PREINCLUDE_H */