#ifndef STRING_COMPAT_H
#define STRING_COMPAT_H

/* 字符串兼容性头文件 */

#ifdef _WIN32
#include <string.h>
#include <stdint.h>

/* 定义 ssize_t 类型 */
typedef intptr_t ssize_t;

#else
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#endif

#endif /* STRING_COMPAT_H */