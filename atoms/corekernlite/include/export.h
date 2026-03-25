/**
 * @file export.h
 * @brief AgentOS Lite 内核符号导出定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 提供跨平台的符号导出/导入宏定义，支持动态库和静态库构建。
 */

#ifndef AGENTOS_LITE_EXPORT_H
#define AGENTOS_LITE_EXPORT_H

#if defined(_WIN32) || defined(_WIN64)
    #ifdef AGENTOS_LITE_BUILD_DLL
        #define AGENTOS_LITE_API __declspec(dllexport)
    #else
        #define AGENTOS_LITE_API __declspec(dllimport)
    #endif
#else
    #if __GNUC__ >= 4
        #define AGENTOS_LITE_API __attribute__((visibility("default")))
    #else
        #define AGENTOS_LITE_API
    #endif
#endif

#endif /* AGENTOS_LITE_EXPORT_H */
