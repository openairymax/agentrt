/**
 * @file stdio_gateway.h
 * @brief stdio 网关接口
 */
#ifndef BASERUNTIME_STDIO_GATEWAY_H
#define BASERUNTIME_STDIO_GATEWAY_H

#include "gateway.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建 stdio 网关实例
 * @return 网关句柄，失败返回 NULL
 */
gateway_t* stdio_gateway_create(void);

#ifdef __cplusplus
}
#endif

#endif /* BASERUNTIME_STDIO_GATEWAY_H */