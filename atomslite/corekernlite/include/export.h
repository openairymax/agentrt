/**
 * @file export.h
 * @brief 符号导出定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LITE_EXPORT_H
#define AGENTOS_LITE_EXPORT_H

#ifdef _WIN32
    #ifdef AGENTOS_LITE_BUILD_DLL
        #define AGENTOS_LITE_API __declspec(dllexport)
    #else
        #define AGENTOS_LITE_API __declspec(dllimport)
    #endif
#else
    #define AGENTOS_LITE_API __attribute__((visibility("default")))
#endif

#endif /* AGENTOS_LITE_EXPORT_H */