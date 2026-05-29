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

#endif

#endif
