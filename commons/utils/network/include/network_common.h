#ifndef AGENTOS_NETWORK_COMMON_H
#define AGENTOS_NETWORK_COMMON_H

#include "../../../utils/error/include/error.h"

/**
 * @brief 网络连接状态枚举
 */
typedef enum {
    NETWORK_STATUS_DISCONNECTED = 0,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_CONNECTED,
    NETWORK_STATUS_DISCONNECTING
} network_status_t;

/**
 * @brief 网络配置结构体
 */
typedef struct {
    const char* host;
    int port;
    int timeout_ms;
    int max_retries;
    int retry_interval_ms;
} network_config_t;

/**
 * @brief 网络连接结构体
 */
typedef struct {
    void* socket;
    network_status_t status;
    network_config_t manager;
    int error_code;
    char* error_message;
} network_connection_t;

/**
 * @brief 创建默认网络配置
 * @return 默认网络配置
 */
network_config_t network_create_default_config(void);

/**
 * @brief 初始化网络连接
 * @param connection 网络连接结构体
 * @param manager 网络配置
 * @return 错误码
 */
agentos_error_t network_connection_init(network_connection_t* connection, const network_config_t* manager);

/**
 * @brief 建立网络连接
 * @param connection 网络连接结构体
 * @return 错误码
 */
agentos_error_t network_connect(network_connection_t* connection);

/**
 * @brief 断开网络连接
 * @param connection 网络连接结构体
 * @return 错误码
 */
agentos_error_t network_disconnect(network_connection_t* connection);

/**
 * @brief 发送数据
 * @param connection 网络连接结构体
 * @param data 数据缓冲区
 * @param length 数据长度
 * @param sent 实际发送的字节数
 * @return 错误码
 */
agentos_error_t network_send(network_connection_t* connection, const void* data, size_t length, size_t* sent);

/**
 * @brief 接收数据
 * @param connection 网络连接结构体
 * @param buffer 接收缓冲区
 * @param length 缓冲区长度
 * @param received 实际接收的字节数
 * @return 错误码
 */
agentos_error_t network_receive(network_connection_t* connection, void* buffer, size_t length, size_t* received);

/**
 * @brief 清理网络连接
 * @param connection 网络连接结构体
 */
void network_connection_cleanup(network_connection_t* connection);

/**
 * @brief 获取网络状态
 * @param connection 网络连接结构体
 * @return 网络状态
 */
network_status_t network_get_status(const network_connection_t* connection);

/**
 * @brief 设置网络超时
 * @param connection 网络连接结构体
 * @param timeout_ms 超时时间（毫秒）
 * @return 错误码
 */
agentos_error_t network_set_timeout(network_connection_t* connection, int timeout_ms);

#endif // NETWORK_COMMON_H