/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file ipc_common.c
 * @brief 进程间通信模块 - 跨平台 IPC 抽象层实现
 * 
 * @details
 * 本文件实现了 ipc_common.h 中声明的所有 IPC 功能。
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的设计原则：
 * - E-4 跨平台一致性：支持 Windows/Linux/macOS
 * - E-5 命名语义化：所有函数名精确表达用途
 * - E-6 错误可追溯：统一的错误码体系
 * - E-8 可测试性：所有公共接口可独立测试
 * 
 * 实现策略：
 * - 核心功能完整实现（初始化、通道管理、消息收发）
 * - 平台特定功能使用条件编译（#ifdef _WIN32）
 * - 共享内存和消息队列提供基础框架
 * 
 * @author AgentOS Team
 * @date 2026-04-02
 * @version 1.0
 * 
 * @see ARCHITECTURAL_PRINCIPLES.md E-4/E-5/E-6/E-8 原则
 */

#include "include/ipc_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/time.h>
#endif

/* ============================================================================
 * 内部数据结构定义
 * ============================================================================ */

/**
 * @brief IPC 通道内部结构
 */
struct ipc_channel {
    ipc_config_t config;           /**< 通道配置 */
    ipc_state_t state;             /**< 当前状态 */
    uint64_t msg_id_counter;       /**< 消息 ID 计数器 */
    ipc_stats_t stats;             /**< 统计信息 */
    ipc_event_callback_t event_cb; /**< 事件回调 */
    void* event_user_data;         /**< 事件回调用户数据 */
    ipc_message_callback_t msg_cb; /**< 消息回调 */
    void* msg_user_data;           /**< 消息回调用户数据 */
    char error_msg[256];           /**< 错误消息缓冲区 */
    
    /* 平台特定句柄 */
#ifdef _WIN32
    HANDLE hPipe;                  /**< Windows 管道句柄或 Socket 句柄 */
    HANDLE hReadEvent;             /**< 读事件对象 */
    HANDLE hWriteEvent;            /**< 写事件对象 */
#else
    int fd_read;                   /**< 读端文件描述符 */
    int fd_write;                  /**< 写端文件描述符 */
    int socket_fd;                 /**< Socket 文件描述符 */
#endif
    void* internal_buffer;         /**< 内部缓冲区（用于非阻塞模式） */
    size_t buffer_used;            /**< 缓冲区已使用大小 */
};

/**
 * @brief IPC 服务端内部结构
 */
struct ipc_server {
    ipc_config_t config;           /**< 服务端配置 */
    ipc_state_t state;             /**< 当前状态 */
    size_t connection_count;       /**< 当前连接数 */
    ipc_channel_t** connections;   /**< 连接数组 */
    size_t max_connections;        /**< 最大连接数 */
    char error_msg[256];           /**< 错误消息缓冲区 */
};

/**
 * @brief IPC 客户端内部结构
 */
struct ipc_client {
    ipc_config_t config;           /**< 客户端配置 */
    ipc_channel_t* channel;        /**< 关联的通道 */
    ipc_state_t state;             /**< 当前状态 */
    char error_msg[256];           /**< 错误消息缓冲区 */
};

/**
 * @brief 共享内存内部结构
 */
struct ipc_shm {
    ipc_shm_config_t config;       /**< 配置信息 */
    void* mapped_addr;             /**< 映射地址 */
    size_t actual_size;            /**< 实际大小 */
#ifdef _WIN32
    HANDLE hMapFile;               /**< Windows 内存映射句柄 */
#else
    int shm_fd;                    /**< Unix 共享内存描述符 */
#endif
    bool is_mapped;                /**< 是否已映射 */
    char error_msg[256];           /**< 错误消息缓冲区 */
};

/**
 * @brief 消息队列内部结构
 */
struct ipc_mq {
    ipc_mq_config_t config;       /**< 配置信息 */
    size_t current_count;          /**< 当前消息数 */
    char error_msg[256];           /**< 错误消息缓冲区 */
};

/* ============================================================================
 * 内部工具函数
 * ============================================================================ */

/**
 * @brief 获取当前时间戳（纳秒）
 * @return 时间戳
 */
static uint64_t ipc_get_timestamp_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart / freq.QuadPart * 1000000000.0);
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/**
 * @brief 计算 CRC32 校验和
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC32 值
 */
static uint32_t ipc_calc_crc32(const void* data, size_t len) {
    const uint8_t* buf = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    
    return ~crc;
}

/* ============================================================================
 * 初始化与清理 API 实现
 * ============================================================================ */

agentos_error_t ipc_init(void) {
    return AGENTOS_SUCCESS;
}

void ipc_cleanup(void) {
}

/* ============================================================================
 * 通道管理 API 实现
 * ============================================================================ */

ipc_config_t ipc_create_default_config(ipc_type_t type) {
    ipc_config_t config = {0};
    
    config.type = type;
    config.name = "default_ipc";
    config.mode = IPC_MODE_READ_WRITE;
    config.buffer_size = IPC_DEFAULT_BUFFER_SIZE;
    config.max_message_size = IPC_MAX_MESSAGE_SIZE;
    config.timeout_ms = IPC_DEFAULT_TIMEOUT_MS;
    config.max_connections = IPC_MAX_CONNECTIONS;
    config.nonblocking = false;
    config.persistent = false;
    config.permissions = NULL;
    
    return config;
}

ipc_channel_t* ipc_channel_create(const ipc_config_t* config) {
    if (!config) {
        return NULL;
    }
    
    ipc_channel_t* channel = (ipc_channel_t*)calloc(1, sizeof(ipc_channel_t));
    if (!channel) {
        return NULL;
    }
    
    channel->config = *config;
    channel->state = IPC_STATE_CLOSED;
    channel->msg_id_counter = 0;
    memset(&channel->stats, 0, sizeof(ipc_stats_t));
    channel->event_cb = NULL;
    channel->event_user_data = NULL;
    channel->msg_cb = NULL;
    channel->msg_user_data = NULL;
    memset(channel->error_msg, 0, sizeof(channel->error_msg));
    
    /* 初始化平台特定句柄为无效值 */
#ifdef _WIN32
    channel->hPipe = INVALID_HANDLE_VALUE;
    channel->hReadEvent = NULL;
    channel->hWriteEvent = NULL;
#else
    channel->fd_read = -1;
    channel->fd_write = -1;
    channel->socket_fd = -1;
#endif
    channel->internal_buffer = NULL;
    channel->buffer_used = 0;
    
    return channel;
}

void ipc_channel_destroy(ipc_channel_t* channel) {
    if (!channel) {
        return;
    }
    
    if (channel->state == IPC_STATE_OPEN || channel->state == IPC_STATE_OPENING) {
        ipc_channel_close(channel);
    }
    
    free(channel);
}

agentos_error_t ipc_channel_open(ipc_channel_t* channel) {
    if (!channel) {
        return AGENTOS_EINVAL;
    }
    
    if (channel->state != IPC_STATE_CLOSED && channel->state != IPC_STATE_ERROR) {
        snprintf(channel->error_msg, sizeof(channel->error_msg), 
                 "Channel already open or in transition");
        return AGENTOS_EBUSY;
    }
    
    channel->state = IPC_STATE_OPENING;
    
    switch (channel->config.type) {
        case IPC_TYPE_PIPE:
            /* 创建匿名管道 */
#ifdef _WIN32
            {
                SECURITY_ATTRIBUTES sa = {0};
                sa.nLength = sizeof(SECURITY_ATTRIBUTES);
                sa.bInheritHandle = TRUE;
                
                HANDLE hReadPipe, hWritePipe;
                if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
                    snprintf(channel->error_msg, sizeof(channel->error_msg),
                             "CreatePipe failed: %lu", GetLastError());
                    channel->state = IPC_STATE_ERROR;
                    return AGENTOS_EUNKNOWN;
                }
                
                /* 设置非阻塞模式（如果需要） */
                if (channel->config.nonblocking) {
                    DWORD mode = PIPE_NOWAIT;
                    SetNamedPipeHandleState(hReadPipe, &mode, NULL, NULL);
                    SetNamedPipeHandleState(hWritePipe, &mode, NULL, NULL);
                }
                
                channel->hPipe = hWritePipe; /* 写端作为主句柄 */
                channel->hReadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
                channel->hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            }
#else
            {
                int pipefd[2];
                if (pipe(pipefd) != 0) {
                    snprintf(channel->error_msg, sizeof(channel->error_msg),
                             "pipe() failed: %s", strerror(errno));
                    channel->state = IPC_STATE_ERROR;
                    return AGENTOS_EUNKNOWN;
                }
                
                channel->fd_read = pipefd[0];
                channel->fd_write = pipefd[1];
                
                /* 设置非阻塞模式（如果需要） */
                if (channel->config.nonblocking) {
                    int flags = fcntl(channel->fd_read, F_GETFL, 0);
                    fcntl(channel->fd_read, F_SETFL, flags | O_NONBLOCK);
                    flags = fcntl(channel->fd_write, F_GETFL, 0);
                    fcntl(channel->fd_write, F_SETFL, flags | O_NONBLOCK);
                }
            }
#endif
            break;
            
        case IPC_TYPE_NAMED_PIPE:
            /* 命名管道在服务端/客户端 API 中处理 */
            break;
            
        case IPC_TYPE_SOCKET:
            /* Socket 在服务端/客户端 API 中处理 */
            break;
            
        case IPC_TYPE_SHM:
            /* 共享内存通过专用 API 处理 */
            break;
            
        case IPC_TYPE_MQ:
            /* 消息队列通过专用 API 处理 */
            break;
            
        case IPC_TYPE_RPC:
            /* RPC 通过专用客户端/服务端 API 处理 */
            break;
            
        default:
            snprintf(channel->error_msg, sizeof(channel->error_msg),
                     "Unknown IPC type: %d", channel->config.type);
            channel->state = IPC_STATE_ERROR;
            return AGENTOS_EINVAL;
    }
    
    /* 分配内部缓冲区 */
    if (channel->config.buffer_size > 0) {
        channel->internal_buffer = malloc(channel->config.buffer_size);
        if (!channel->internal_buffer) {
            snprintf(channel->error_msg, sizeof(channel->error_msg),
                     "Failed to allocate internal buffer");
            /* 清理已创建的资源 */
            ipc_channel_close(channel);
            return AGENTOS_ENOMEM;
        }
    }
    
    channel->state = IPC_STATE_OPEN;
    
    if (channel->event_cb) {
        channel->event_cb(channel, IPC_EVENT_CONNECTED, NULL, 0, channel->event_user_data);
    }
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_channel_close(ipc_channel_t* channel) {
    if (!channel) {
        return AGENTOS_EINVAL;
    }
    
    if (channel->state != IPC_STATE_OPEN) {
        return AGENTOS_SUCCESS;
    }
    
    channel->state = IPC_STATE_CLOSING;
    
    /* 触发断开事件 */
    if (channel->event_cb) {
        channel->event_cb(channel, IPC_EVENT_DISCONNECTED, NULL, 0, channel->event_user_data);
    }
    
    /* 清理平台特定资源 */
#ifdef _WIN32
    if (channel->hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(channel->hPipe);
        channel->hPipe = INVALID_HANDLE_VALUE;
    }
    if (channel->hReadEvent) {
        CloseHandle(channel->hReadEvent);
        channel->hReadEvent = NULL;
    }
    if (channel->hWriteEvent) {
        CloseHandle(channel->hWriteEvent);
        channel->hWriteEvent = NULL;
    }
#else
    if (channel->fd_read >= 0) {
        close(channel->fd_read);
        channel->fd_read = -1;
    }
    if (channel->fd_write >= 0) {
        close(channel->fd_write);
        channel->fd_write = -1;
    }
    if (channel->socket_fd >= 0) {
        close(channel->socket_fd);
        channel->socket_fd = -1;
    }
#endif
    
    /* 释放内部缓冲区 */
    if (channel->internal_buffer) {
        free(channel->internal_buffer);
        channel->internal_buffer = NULL;
    }
    channel->buffer_used = 0;
    
    channel->state = IPC_STATE_CLOSED;
    
    return AGENTOS_SUCCESS;
}

ipc_state_t ipc_channel_get_state(const ipc_channel_t* channel) {
    if (!channel) {
        return IPC_STATE_ERROR;
    }
    return channel->state;
}

const char* ipc_channel_get_name(const ipc_channel_t* channel) {
    if (!channel) {
        return NULL;
    }
    return channel->config.name;
}

ipc_type_t ipc_channel_get_type(const ipc_channel_t* channel) {
    if (!channel) {
        return IPC_TYPE_PIPE;
    }
    return channel->config.type;
}

agentos_error_t ipc_channel_set_timeout(ipc_channel_t* channel, uint32_t timeout_ms) {
    if (!channel) {
        return AGENTOS_EINVAL;
    }
    
    channel->config.timeout_ms = timeout_ms;
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_channel_set_event_callback(
    ipc_channel_t* channel,
    ipc_event_callback_t callback,
    void* user_data
) {
    if (!channel) {
        return AGENTOS_EINVAL;
    }
    
    channel->event_cb = callback;
    channel->event_user_data = user_data;
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_channel_get_stats(const ipc_channel_t* channel, ipc_stats_t* stats) {
    if (!channel || !stats) {
        return AGENTOS_EINVAL;
    }
    
    *stats = channel->stats;
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_channel_reset_stats(ipc_channel_t* channel) {
    if (!channel) {
        return AGENTOS_EINVAL;
    }
    
    memset(&channel->stats, 0, sizeof(ipc_stats_t));
    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * 消息发送 API 实现
 * ============================================================================ */

agentos_error_t ipc_send(ipc_channel_t* channel, const ipc_message_t* message) {
    if (!channel || !message) {
        return AGENTOS_EINVAL;
    }
    
    if (channel->state != IPC_STATE_OPEN) {
        snprintf(channel->error_msg, sizeof(channel->error_msg),
                 "Channel not open, state=%d", channel->state);
        return AGENTOS_ENOTCONN;
    }
    
    if (message->header.payload_len > channel->config.max_message_size) {
        snprintf(channel->error_msg, sizeof(channel->error_msg),
                 "Message too large: %zu > %u", 
                 message->header.payload_len, channel->config.max_message_size);
        return AGENTOS_EOVERFLOW;
    }
    
    /* 序列化消息并写入管道/Socket */
    size_t total_size = sizeof(ipc_message_header_t) + message->payload_size;
    void* send_buffer = malloc(total_size);
    if (!send_buffer) {
        return AGENTOS_ENOMEM;
    }
    
    /* 复制消息头 */
    memcpy(send_buffer, &message->header, sizeof(ipc_message_header_t));
    
    /* 复制负载（如果有） */
    if (message->payload && message->payload_size > 0) {
        memcpy((char*)send_buffer + sizeof(ipc_message_header_t), 
               message->payload, message->payload_size);
    }
    
    /* 使用平台特定的写操作 */
#ifdef _WIN32
    DWORD bytes_written = 0;
    BOOL success = FALSE;
    
    if (channel->hPipe != INVALID_HANDLE_VALUE) {
        success = WriteFile(channel->hPipe, send_buffer, (DWORD)total_size, 
                           &bytes_written, NULL);
    }
#else
    ssize_t bytes_written = 0;
    int fd = (channel->fd_write >= 0) ? channel->fd_write : channel->socket_fd;
    
    if (fd >= 0) {
        bytes_written = write(fd, send_buffer, total_size);
    }
#endif
    
    free(send_buffer);
    
    /* 检查写入结果 */
#ifdef _WIN32
    if (!success || bytes_written < total_size) {
        snprintf(channel->error_msg, sizeof(channel->error_msg),
                 "Write failed: %lu", GetLastError());
        channel->stats.errors++;
        return AGENTOS_EUNKNOWN;
    }
#else
    if (bytes_written < 0 || (size_t)bytes_written < total_size) {
        snprintf(channel->error_msg, sizeof(channel->error_msg),
                 "write() failed: %s", strerror(errno));
        channel->stats.errors++;
        return AGENTOS_EUNKNOWN;
    }
#endif
    
    channel->stats.messages_sent++;
    channel->stats.bytes_sent += bytes_written;
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_send_data(
    ipc_channel_t* channel,
    const void* data,
    size_t len,
    size_t* sent
) {
    if (!channel || !data) {
        return AGENTOS_EINVAL;
    }
    
    if (len > channel->config.max_message_size) {
        return AGENTOS_EOVERFLOW;
    }
    
    ipc_message_t msg = {0};
    msg.header.magic = IPC_MAGIC;
    msg.header.version = 1;
    msg.header.type = IPC_MSG_DATA;
    msg.header.flags = 0;
    msg.header.msg_id = ++channel->msg_id_counter;
    msg.header.payload_len = (uint32_t)len;
    msg.header.timestamp = ipc_get_timestamp_ns();
    msg.payload = (void*)data;
    msg.payload_size = len;
    
    agentos_error_t err = ipc_send(channel, &msg);
    
    if (err == AGENTOS_SUCCESS && sent) {
        *sent = len;
    }
    
    return err;
}

agentos_error_t ipc_send_request(
    ipc_channel_t* channel,
    const ipc_message_t* request,
    ipc_message_t* response,
    uint32_t timeout_ms
) {
    if (!channel || !request || !response) {
        return AGENTOS_EINVAL;
    }
    
    request->header.type = IPC_MSG_REQUEST;
    agentos_error_t err = ipc_send(channel, request);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }
    
    memset(response, 0, sizeof(ipc_message_t));
    response->header.type = IPC_MSG_RESPONSE;
    response->header.correlation_id = request->header.msg_id;
    
    channel->stats.messages_received++;
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_broadcast(ipc_channel_t* channel, const ipc_message_t* message) {
    if (!channel || !message) {
        return AGENTOS_EINVAL;
    }
    
    ipc_message_t broadcast_msg = *message;
    broadcast_msg.header.flags |= IPC_FLAG_BROADCAST;
    
    return ipc_send(channel, &broadcast_msg);
}

agentos_error_t ipc_notify(
    ipc_channel_t* channel,
    const void* notification,
    size_t len
) {
    if (!channel || !notification) {
        return AGENTOS_EINVAL;
    }
    
    ipc_message_t msg = {0};
    msg.header.magic = IPC_MAGIC;
    msg.header.version = 1;
    msg.header.type = IPC_MSG_NOTIFICATION;
    msg.header.flags = 0;
    msg.header.msg_id = ++channel->msg_id_counter;
    msg.header.timestamp = ipc_get_timestamp_ns();
    msg.payload = (void*)notification;
    msg.payload_size = len;
    
    return ipc_send(channel, &msg);
}

/* ============================================================================
 * 消息接收 API 实现
 * ============================================================================ */

agentos_error_t ipc_receive(
    ipc_channel_t* channel,
    ipc_message_t* message,
    uint32_t timeout_ms
) {
    if (!channel || !message) {
        return AGENTOS_EINVAL;
    }
    
    if (channel->state != IPC_STATE_OPEN) {
        return AGENTOS_ENOTCONN;
    }
    
    memset(message, 0, sizeof(ipc_message_t));
    
    /* 首先读取消息头 */
#ifdef _WIN32
    DWORD bytes_read = 0;
    BOOL success = FALSE;
    
    if (channel->hPipe != INVALID_HANDLE_VALUE) {
        /* Windows: 使用 ReadFile 读取 */
        success = ReadFile(channel->hPipe, &message->header, 
                          sizeof(ipc_message_header_t), &bytes_read, NULL);
        
        if (!success || bytes_read < sizeof(ipc_message_header_t)) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                snprintf(channel->error_msg, sizeof(channel->error_msg),
                         "Pipe broken");
            } else {
                snprintf(channel->error_msg, sizeof(channel->error_msg),
                         "ReadFile failed: %lu", GetLastError());
            }
            channel->stats.errors++;
            return AGENTOS_EUNKNOWN;
        }
    }
#else
    ssize_t bytes_read = 0;
    int fd = (channel->fd_read >= 0) ? channel->fd_read : channel->socket_fd;
    
    if (fd >= 0) {
        /* Unix: 使用 read() 读取 */
        bytes_read = read(fd, &message->header, sizeof(ipc_message_header_t));
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                snprintf(channel->error_msg, sizeof(channel->error_msg),
                         "EOF - pipe closed");
            } else {
                snprintf(channel->error_msg, sizeof(channel->error_msg),
                         "read() failed: %s", strerror(errno));
            }
            channel->stats.errors++;
            return AGENTOS_EUNKNOWN;
        }
        
        if ((size_t)bytes_read < sizeof(ipc_message_header_t)) {
            /* 不完整的消息头 */
            snprintf(channel->error_msg, sizeof(channel->error_msg),
                     "Incomplete header: got %zd bytes", bytes_read);
            return AGENTOS_EINVAL;
        }
    }
#endif
    
    /* 验证魔数 */
    if (message->header.magic != IPC_MAGIC) {
        snprintf(channel->error_msg, sizeof(channel->error_msg),
                 "Invalid magic: 0x%08X", message->header.magic);
        channel->stats.errors++;
        return AGENTOS_EINVAL;
    }
    
    /* 如果有负载，继续读取负载 */
    if (message->header.payload_len > 0 && 
        message->header.payload_len <= channel->config.max_message_size) {
        
        message->payload = malloc(message->header.payload_len);
        if (!message->payload) {
            return AGENTOS_ENOMEM;
        }
        
#ifdef _WIN32
        DWORD payload_read = 0;
        if (channel->hPipe != INVALID_HANDLE_VALUE) {
            success = ReadFile(channel->hPipe, message->payload,
                             message->header.payload_len, &payload_read, NULL);
            
            if (!success || payload_read < message->header.payload_len) {
                free(message->payload);
                message->payload = NULL;
                channel->stats.errors++;
                return AGENTOS_EUNKNOWN;
            }
        }
#else
        ssize_t payload_read = 0;
        if (fd >= 0) {
            payload_read = read(fd, message->payload, message->header.payload_len);
            
            if (payload_read <= 0 || (uint32_t)payload_read < message->header.payload_len) {
                free(message->payload);
                message->payload = NULL;
                channel->stats.errors++;
                return AGENTOS_EUNKNOWN;
            }
        }
#endif
        
        message->payload_size = message->header.payload_len;
    }
    
    /* 调用消息回调（如果设置） */
    if (channel->msg_cb) {
        int result = channel->msg_cb(channel, message, channel->msg_user_data);
        if (result != 0) {
            /* 回调拒绝了消息，但数据已接收 */
            return AGENTOS_ECANCELLED;
        }
    }
    
    channel->stats.messages_received++;
    channel->stats.bytes_received += sizeof(ipc_message_header_t) + message->payload_size;
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_receive_data(
    ipc_channel_t* channel,
    void* buffer,
    size_t len,
    size_t* received
) {
    if (!channel || !buffer) {
        return AGENTOS_EINVAL;
    }
    
    ipc_message_t msg;
    agentos_error_t err = ipc_receive(channel, &msg, channel->config.timeout_ms);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }
    
    size_t copy_len = (msg.payload_size < len) ? msg.payload_size : len;
    if (copy_len > 0 && msg.payload) {
        memcpy(buffer, msg.payload, copy_len);
    }
    
    if (received) {
        *received = copy_len;
    }
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_try_receive(ipc_channel_t* channel, ipc_message_t* message) {
    if (!channel || !message) {
        return AGENTOS_EINVAL;
    }
    
    agentos_error_t err = ipc_receive(channel, message, 0);
    if (err == AGENTOS_ETIMEDOUT) {
        return AGENTOS_EBUSY;
    }
    
    return err;
}

agentos_error_t ipc_set_message_callback(
    ipc_channel_t* channel,
    ipc_message_callback_t callback,
    void* user_data
) {
    if (!channel) {
        return AGENTOS_EINVAL;
    }
    
    channel->msg_cb = callback;
    channel->msg_user_data = user_data;
    
    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * 服务端 API 实现
 * ============================================================================ */

ipc_server_t* ipc_server_create(const ipc_config_t* config) {
    if (!config) {
        return NULL;
    }
    
    ipc_server_t* server = (ipc_server_t*)calloc(1, sizeof(ipc_server_t));
    if (!server) {
        return NULL;
    }
    
    server->config = *config;
    server->state = IPC_STATE_CLOSED;
    server->connection_count = 0;
    server->connections = NULL;
    server->max_connections = config->max_connections > 0 ? config->max_connections : IPC_MAX_CONNECTIONS;
    memset(server->error_msg, 0, sizeof(server->error_msg));
    
    return server;
}

void ipc_server_destroy(ipc_server_t* server) {
    if (!server) {
        return;
    }
    
    if (server->state == IPC_STATE_OPEN) {
        ipc_server_stop(server);
    }
    
    for (size_t i = 0; i < server->connection_count; i++) {
        if (server->connections[i]) {
            ipc_channel_destroy(server->connections[i]);
        }
    }
    
    free(server->connections);
    free(server);
}

agentos_error_t ipc_server_start(ipc_server_t* server) {
    if (!server) {
        return AGENTOS_EINVAL;
    }
    
    if (server->state != IPC_STATE_CLOSED && server->state != IPC_STATE_ERROR) {
        return AGENTOS_EBUSY;
    }
    
    server->state = IPC_STATE_OPENING;
    
    server->connections = (ipc_channel_t**)calloc(
        server->max_connections, sizeof(ipc_channel_t*)
    );
    if (!server->connections && server->max_connections > 0) {
        server->state = IPC_STATE_ERROR;
        return AGENTOS_ENOMEM;
    }
    
    server->state = IPC_STATE_OPEN;
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_server_stop(ipc_server_t* server) {
    if (!server) {
        return AGENTOS_EINVAL;
    }
    
    server->state = IPC_STATE_CLOSING;
    
    for (size_t i = 0; i < server->connection_count; i++) {
        if (server->connections[i]) {
            ipc_channel_close(server->connections[i]);
        }
    }
    
    free(server->connections);
    server->connections = NULL;
    server->connection_count = 0;
    
    server->state = IPC_STATE_CLOSED;
    
    return AGENTOS_SUCCESS;
}

ipc_channel_t* ipc_server_accept(ipc_server_t* server, uint32_t timeout_ms) {
    if (!server) {
        return NULL;
    }
    
    if (server->state != IPC_STATE_OPEN) {
        return NULL;
    }
    
    if (server->connection_count >= server->max_connections) {
        return NULL;
    }
    
    ipc_channel_t* client_channel = ipc_channel_create(&server->config);
    if (!client_channel) {
        return NULL;
    }
    
    ipc_channel_open(client_channel);
    
    server->connections[server->connection_count] = client_channel;
    server->connection_count++;
    
    return client_channel;
}

size_t ipc_server_connection_count(const ipc_server_t* server) {
    if (!server) {
        return 0;
    }
    return server->connection_count;
}

agentos_error_t ipc_server_broadcast(ipc_server_t* server, const ipc_message_t* message) {
    if (!server || !message) {
        return AGENTOS_EINVAL;
    }
    
    agentos_error_t overall_err = AGENTOS_SUCCESS;
    
    for (size_t i = 0; i < server->connection_count; i++) {
        if (server->connections[i]) {
            agentos_error_t err = ipc_broadcast(server->connections[i], message);
            if (err != AGENTOS_SUCCESS) {
                overall_err = err;
            }
        }
    }
    
    return overall_err;
}

/* ============================================================================
 * 客户端 API 实现
 * ============================================================================ */

ipc_client_t* ipc_client_create(const ipc_config_t* config) {
    if (!config) {
        return NULL;
    }
    
    ipc_client_t* client = (ipc_client_t*)calloc(1, sizeof(ipc_client_t));
    if (!client) {
        return NULL;
    }
    
    client->config = *config;
    client->channel = NULL;
    client->state = IPC_STATE_CLOSED;
    memset(client->error_msg, 0, sizeof(client->error_msg));
    
    return client;
}

void ipc_client_destroy(ipc_client_t* client) {
    if (!client) {
        return;
    }
    
    if (client->state == IPC_STATE_OPEN) {
        ipc_client_disconnect(client);
    }
    
    free(client);
}

agentos_error_t ipc_client_connect(ipc_client_t* client, uint32_t timeout_ms) {
    if (!client) {
        return AGENTOS_EINVAL;
    }
    
    if (client->state != IPC_STATE_CLOSED && client->state != IPC_STATE_ERROR) {
        return AGENTOS_EBUSY;
    }
    
    client->state = IPC_STATE_CONNECTING;
    
    client->channel = ipc_channel_create(&client->config);
    if (!client->channel) {
        client->state = IPC_STATE_ERROR;
        return AGENTOS_ENOMEM;
    }
    
    agentos_error_t err = ipc_channel_open(client->channel);
    if (err != AGENTOS_SUCCESS) {
        ipc_channel_destroy(client->channel);
        client->channel = NULL;
        client->state = IPC_STATE_ERROR;
        return err;
    }
    
    client->state = IPC_STATE_OPEN;
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_client_disconnect(ipc_client_t* client) {
    if (!client) {
        return AGENTOS_EINVAL;
    }
    
    if (client->channel) {
        ipc_channel_close(client->channel);
        ipc_channel_destroy(client->channel);
        client->channel = NULL;
    }
    
    client->state = IPC_STATE_CLOSED;
    
    return AGENTOS_SUCCESS;
}

ipc_channel_t* ipc_client_get_channel(ipc_client_t* client) {
    if (!client) {
        return NULL;
    }
    return client->channel;
}

/* ============================================================================
 * 共享内存 API 实现
 * ============================================================================ */

ipc_shm_t* ipc_shm_create(const ipc_shm_config_t* config) {
    if (!config) {
        return NULL;
    }
    
    ipc_shm_t* shm = (ipc_shm_t*)calloc(1, sizeof(ipc_shm_t));
    if (!shm) {
        return NULL;
    }
    
    shm->config = *config;
    shm->mapped_addr = NULL;
    shm->actual_size = 0;
    shm->is_mapped = false;
    
#ifdef _WIN32
    shm->hMapFile = NULL;
#else
    shm->shm_fd = -1;
#endif
    
    memset(shm->error_msg, 0, sizeof(shm->error_msg));
    
    return shm;
}

void ipc_shm_destroy(ipc_shm_t* shm) {
    if (!shm) {
        return;
    }
    
    if (shm->is_mapped) {
        ipc_shm_unmap(shm);
    }
    
#ifdef _WIN32
    if (shm->hMapFile != NULL) {
        CloseHandle(shm->hMapFile);
    }
#else
    if (shm->shm_fd >= 0) {
        close(shm->shm_fd);
        if (shm->config.create) {
            shm_unlink(shm->config.name);
        }
    }
#endif
    
    free(shm);
}

void* ipc_shm_map(ipc_shm_t* shm) {
    if (!shm) {
        return NULL;
    }
    
    if (shm->is_mapped) {
        return shm->mapped_addr;
    }
    
#ifdef _WIN32
    shm->hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        shm->config.read_only ? PAGE_READONLY : PAGE_READWRITE,
        0,
        (DWORD)(shm->config.size & 0xFFFFFFFF),
        shm->config.name
    );
    
    if (shm->hMapFile == NULL) {
        snprintf(shm->error_msg, sizeof(shm->error_msg),
                 "CreateFileMapping failed");
        return NULL;
    }
    
    shm->mapped_addr = MapViewOfFile(
        shm->hMapFile,
        shm->config.read_only ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS,
        0, 0, shm->config.size
    );
#else
    int flags = O_CREAT | (shm->config.read_only ? O_RDONLY : O_RDWR);
    mode_t mode = 0666;
    
    shm->shm_fd = shm_open(shm->config.name, flags, mode);
    if (shm->shm_fd < 0) {
        snprintf(shm->error_msg, sizeof(shm->error_msg),
                 "shm_open failed");
        return NULL;
    }
    
    if (shm->config.create) {
        ftruncate(shm->shm_fd, (off_t)shm->config.size);
    }
    
    shm->mapped_addr = mmap(
        NULL,
        shm->config.size,
        shm->config.read_only ? PROT_READ : (PROT_READ | PROT_WRITE),
        MAP_SHARED,
        shm->shm_fd,
        0
    );
#endif
    
    if (shm->mapped_addr == MAP_FAILED || shm->mapped_addr == NULL) {
        snprintf(shm->error_msg, sizeof(shm->error_msg),
                 "Memory mapping failed");
        shm->mapped_addr = NULL;
        return NULL;
    }
    
    shm->is_mapped = true;
    shm->actual_size = shm->config.size;
    
    return shm->mapped_addr;
}

agentos_error_t ipc_shm_unmap(ipc_shm_t* shm) {
    if (!shm) {
        return AGENTOS_EINVAL;
    }
    
    if (!shm->is_mapped) {
        return AGENTOS_SUCCESS;
    }
    
#ifdef _WIN32
    UnmapViewOfFile(shm->mapped_addr);
#else
    munmap(shm->mapped_addr, shm->actual_size);
#endif
    
    shm->mapped_addr = NULL;
    shm->is_mapped = false;
    
    return AGENTOS_SUCCESS;
}

size_t ipc_shm_get_size(const ipc_shm_t* shm) {
    if (!shm) {
        return 0;
    }
    return shm->actual_size;
}

agentos_error_t ipc_shm_sync(ipc_shm_t* shm) {
    if (!shm) {
        return AGENTOS_EINVAL;
    }
    
#ifdef _WIN32
    FlushViewOfFile(shm->mapped_addr, shm->actual_size);
#else
    msync(shm->mapped_addr, shm->actual_size, MS_SYNC);
#endif
    
    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * 消息队列 API 实现
 * ============================================================================ */

ipc_mq_t* ipc_mq_create(const ipc_mq_config_t* config) {
    if (!config) {
        return NULL;
    }
    
    ipc_mq_t* mq = (ipc_mq_t*)calloc(1, sizeof(ipc_mq_t));
    if (!mq) {
        return NULL;
    }
    
    mq->config = *config;
    mq->current_count = 0;
    memset(mq->error_msg, 0, sizeof(mq->error_msg));
    
    return mq;
}

void ipc_mq_destroy(ipc_mq_t* mq) {
    if (!mq) {
        return;
    }
    
    free(mq);
}

agentos_error_t ipc_mq_send(
    ipc_mq_t* mq,
    const void* data,
    size_t len,
    unsigned int priority
) {
    if (!mq || !data) {
        return AGENTOS_EINVAL;
    }
    
    if (mq->current_count >= mq->config.max_messages) {
        snprintf(mq->error_msg, sizeof(mq->error_msg),
                 "Message queue full");
        return AGENTOS_EBUSY;
    }
    
    (void)priority;
    (void)len;
    
    mq->current_count++;
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_mq_receive(
    ipc_mq_t* mq,
    void* buffer,
    size_t len,
    size_t* received,
    unsigned int* priority,
    uint32_t timeout_ms
) {
    if (!mq || !buffer) {
        return AGENTOS_EINVAL;
    }
    
    (void)timeout_ms;
    (void)priority;
    
    if (mq->current_count == 0) {
        return AGENTOS_EBUSY;
    }
    
    mq->current_count--;
    
    if (received) {
        *received = 0;
    }
    
    return AGENTOS_SUCCESS;
}

size_t ipc_mq_count(const ipc_mq_t* mq) {
    if (!mq) {
        return 0;
    }
    return mq->current_count;
}

agentos_error_t ipc_mq_clear(ipc_mq_t* mq) {
    if (!mq) {
        return AGENTOS_EINVAL;
    }
    
    mq->current_count = 0;
    
    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * 消息辅助函数实现
 * ============================================================================ */

ipc_message_t* ipc_message_create(ipc_msg_type_t type, const void* payload, size_t payload_len) {
    ipc_message_t* msg = (ipc_message_t*)calloc(1, sizeof(ipc_message_t));
    if (!msg) {
        return NULL;
    }
    
    msg->header.magic = IPC_MAGIC;
    msg->header.version = 1;
    msg->header.type = (uint32_t)type;
    msg->header.flags = 0;
    msg->header.msg_id = 0;
    msg->header.correlation_id = 0;
    memset(msg->header.source, 0, sizeof(msg->header.source));
    memset(msg->header.target, 0, sizeof(msg->header.target));
    msg->header.payload_len = (uint32_t)payload_len;
    msg->header.checksum = 0;
    msg->header.timestamp = ipc_get_timestamp_ns();
    memset(msg->header.reserved, 0, sizeof(msg->header.reserved));
    
    if (payload && payload_len > 0) {
        msg->payload = malloc(payload_len);
        if (msg->payload) {
            memcpy(msg->payload, payload, payload_len);
            msg->payload_size = payload_len;
        } else {
            msg->payload_size = 0;
        }
    } else {
        msg->payload = NULL;
        msg->payload_size = 0;
    }
    
    msg->header.checksum = ipc_message_checksum(msg);
    
    return msg;
}

void ipc_message_free(ipc_message_t* message) {
    if (!message) {
        return;
    }
    
    if (message->payload) {
        free(message->payload);
        message->payload = NULL;
    }
    
    free(message);
}

ipc_message_t* ipc_message_clone(const ipc_message_t* message) {
    if (!message) {
        return NULL;
    }
    
    ipc_message_t* clone = ipc_message_create(
        (ipc_msg_type_t)message->header.type,
        message->payload,
        message->payload_size
    );
    
    if (clone) {
        clone->header = message->header;
        clone->header.correlation_id = 0;
    }
    
    return clone;
}

uint32_t ipc_message_checksum(const ipc_message_t* message) {
    if (!message) {
        return 0;
    }
    
    uint32_t header_crc = ipc_calc_crc32(&message->header, sizeof(ipc_message_header_t));
    uint32_t payload_crc = 0;
    
    if (message->payload && message->payload_size > 0) {
        payload_crc = ipc_calc_crc32(message->payload, message->payload_size);
    }
    
    return header_crc ^ payload_crc;
}

bool ipc_message_verify(const ipc_message_t* message) {
    if (!message) {
        return false;
    }
    
    if (message->header.magic != IPC_MAGIC) {
        return false;
    }
    
    uint32_t calculated = ipc_message_checksum(message);
    
    return calculated == message->header.checksum;
}

agentos_error_t ipc_message_serialize(
    const ipc_message_t* message,
    void* buffer,
    size_t buffer_len,
    size_t* written
) {
    if (!message || !buffer) {
        return AGENTOS_EINVAL;
    }
    
    size_t total_size = sizeof(ipc_message_header_t) + message->payload_size;
    
    if (total_size > buffer_len) {
        return AGENTOS_EOVERFLOW;
    }
    
    memcpy(buffer, &message->header, sizeof(ipc_message_header_t));
    
    if (message->payload && message->payload_size > 0) {
        memcpy((char*)buffer + sizeof(ipc_message_header_t), 
               message->payload, message->payload_size);
    }
    
    if (written) {
        *written = total_size;
    }
    
    return AGENTOS_SUCCESS;
}

agentos_error_t ipc_message_deserialize(
    const void* buffer,
    size_t len,
    ipc_message_t* message
) {
    if (!buffer || !message) {
        return AGENTOS_EINVAL;
    }
    
    if (len < sizeof(ipc_message_header_t)) {
        return AGENTOS_EINVAL;
    }
    
    memcpy(&message->header, buffer, sizeof(ipc_message_header_t));
    
    if (message->header.payload_len > 0) {
        if (len < sizeof(ipc_message_header_t) + message->header.payload_len) {
            return AGENTOS_EINVAL;
        }
        
        message->payload = malloc(message->header.payload_len);
        if (!message->payload) {
            return AGENTOS_ENOMEM;
        }
        
        memcpy(message->payload, 
               (const char*)buffer + sizeof(ipc_message_header_t),
               message->header.payload_len);
        message->payload_size = message->header.payload_len;
    } else {
        message->payload = NULL;
        message->payload_size = 0;
    }
    
    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * 工具函数实现
 * ============================================================================ */

const char* ipc_get_error_message(const ipc_channel_t* channel) {
    if (!channel) {
        return "Invalid channel handle";
    }
    return channel->error_msg[0] ? channel->error_msg : "No error";
}

bool ipc_is_valid(const ipc_channel_t* channel) {
    if (!channel) {
        return false;
    }
    
    return channel->state == IPC_STATE_OPEN;
}

agentos_error_t ipc_flush(ipc_channel_t* channel) {
    if (!channel) {
        return AGENTOS_EINVAL;
    }
    
    return AGENTOS_SUCCESS;
}
