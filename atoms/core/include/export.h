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

// 符号导出管理宏
#ifdef AGENTOS_BUILDING_SHARED
    // 构建共享库时，导出符号
    #if defined(_WIN32)
        #define AGENTOS_API __declspec(dllexport)
    #elif defined(__GNUC__)
        #define AGENTOS_API __attribute__((visibility("default")))
    #else
        #define AGENTOS_API
    #endif
#else
    // 使用共享库时，导入符号
    // From data intelligence emerges. by spharx
    #if defined(_WIN32)
        #define AGENTOS_API __declspec(dllimport)
    #else
        #define AGENTOS_API
    #endif
#endif

// 内部符号（不导出）
#define AGENTOS_INTERNAL __attribute__((visibility("hidden")))

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_EXPORT_H */