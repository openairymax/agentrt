/**
 * @file string.h
 * @brief Standard string compatibility interface for AgentOS
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 * 
 * @note 此文件名与系统 <string.h> 冲突。
 * 当使用 #include <string.h> 时，编译器可能匹配到此文件而非系统头文件。
 * 建议使用 #include <agentos/string_compat.h> 替代。
 */

#ifndef AGENTOS_STRING_STANDARD_H
#define AGENTOS_STRING_STANDARD_H

#include <agentos/compat/stdint.h>

/* 包含系统的 string.h - 由于头文件保护宏已设置，递归包含会被阻止 */
#include <string.h>

#include "../../commons/utils/string/include/string_compat.h"

#endif /* AGENTOS_STRING_STANDARD_H */
