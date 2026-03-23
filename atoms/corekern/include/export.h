/**
 * @file export.h
 * @brief AgentOS 符号导出管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_EXPORT_H
#define AGENTOS_EXPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef AGENTOS_BUILDING_SHARED
    #if defined(_WIN32) || defined(_WIN64)
        #define AGENTOS_API __declspec(dllexport)
    #elif defined(__GNUC__) || defined(__clang__)
        #define AGENTOS_API __attribute__((visibility("default")))
    #else
        #define AGENTOS_API
    #endif
#else
    #if defined(_WIN32) || defined(_WIN64)
        #define AGENTOS_API __declspec(dllimport)
    #else
        #define AGENTOS_API
    #endif
#endif

#if defined(_WIN32) || defined(_WIN64)
    #define AGENTOS_INTERNAL
#else
    #define AGENTOS_INTERNAL __attribute__((visibility("hidden")))
#endif

#ifndef AGENTOS_API
    #define AGENTOS_API
#endif

#ifndef AGENTOS_INTERNAL
    #define AGENTOS_INTERNAL
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_EXPORT_H */
