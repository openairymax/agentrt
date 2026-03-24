/**
 * @file stdio_gateway.c
 * @brief Stdio网关实现 - 本地进程通信协议
 * 
 * 实现标准输入输出通信协议，用于本地进程间通信和调试。
 * 支持命令行界面和脚本集成，提供与AgentOS内核的系统调用接口。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "stdio_gateway.h"
#include "../server.h"
#include "../session.h"
#include "../health.h"
#include "../telemetry.h"
#include "../auth.h"
#include "../ratelimit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <sys/select.h>

/* ========== Stdio网关内部结构 ========== */

/**
 * @brief Stdio连接状态
 */
typedef enum {
    STDIO_STATE_IDLE = 0,        /**< 空闲状态 */
    STDIO_STATE_ACTIVE,          /**< 活跃状态 */
    STDIO_STATE_PROCESSING,      /**< 处理中状态 */
    STDIO_STATE_SHUTTING_DOWN    /**< 关闭中状态 */
} stdio_connection_state_t;

/**
 * @brief Stdio连接上下文
 */
typedef struct stdio_connection_context {
    dynamic_server_t* server;        /**< 服务器引用 */
    stdio_connection_state_t state;   /**< 连接状态 */
    char* session_id;                /**< 会话ID */
    uint64_t connect_time_ns;        /**< 连接时间 */
    uint64_t last_activity_ns;       /**< 最后活动时间 */
    
    /* 统计信息 */
    size_t commands_processed;        /**< 处理命令数 */
    size_t bytes_received;           /**< 接收字节数 */
    size_t bytes_sent;               /**< 发送字节数 */
    
    /* 输入缓冲区 */
    char input_buffer[8192];         /**< 输入缓冲区 */
    size_t input_buffer_pos;         /**< 输入缓冲区位置 */
    pthread_mutex_t input_lock;      /**< 输入锁 */
    pthread_cond_t input_cond;      /**< 输入条件变量 */
} stdio_connection_context_t;

/**
 * @brief Stdio网关内部结构
 */
typedef struct stdio_gateway {
    dynamic_server_t* server;        /**< 服务器引用 */
    
    /* 统计信息 */
    atomic_uint_fast64_t commands_total;     /**< 总命令数 */
    atomic_uint_fast64_t commands_failed;    /**< 失败命令数 */
    atomic_uint_fast64_t bytes_received;     /**< 接收字节数 */
    atomic_uint_fast64_t bytes_sent;         /**< 发送字节数 */
    
    /* 连接管理 */
    pthread_mutex_t connection_lock;         /**< 连接锁 */
    stdio_connection_context_t* connection;   /**< 连接上下文 */
    
    /* 线程管理 */
    pthread_t input_thread;                  /**< 输入处理线程 */
    pthread_t output_thread;                 /**< 输出处理线程 */
    atomic_bool running;                      /**< 运行标志 */
    
    /* 限流器 */
    ratelimit_t* ratelimiter;                /**< 命令限流器 */
} stdio_gateway_t;

/* ========== 命令协议定义 ========== */

/**
 * @brief Stdio命令类型
 */
typedef enum {
    STDIO_CMD_HELP = 1,            /**< 帮助命令 */
    STDIO_CMD_SESSION_CREATE,      /**< 创建会话 */
    STDIO_CMD_SESSION_LIST,        /**< 列出会话 */
    STDIO_CMD_SESSION_CLOSE,       /**< 关闭会话 */
    STDIO_CMD_RPC,                 /**< RPC调用 */
    STDIO_CMD_STATS,               /**< 统计信息 */
    STDIO_CMD_HEALTH,              /**< 健康检查 */
    STDIO_CMD_METRICS,            /**< 指标查看 */
    STDIO_CMD_EXIT,                /* 退出命令 */
    STDIO_CMD_UNKNOWN              /**< 未知命令 */
} stdio_command_type_t;

/**
 * @brief Stdio命令结构
 */
typedef struct stdio_command {
    stdio_command_type_t type;     /**< 命令类型 */
    char* args;                    /**< 命令参数 */
    char* session_id;              /**< 会话ID */
    cJSON* payload;                /**< 命令载荷 */
    uint64_t timestamp_ns;         /**< 时间戳 */
} stdio_command_t;

/* ========== 命令处理 ========== */

/**
 * @brief 解析命令字符串
 * @param input 输入字符串
 * @return 命令对象，需调用者free
 */
static stdio_command_t* stdio_command_parse(const char* input) {
    stdio_command_t* cmd = calloc(1, sizeof(stdio_command_t));
    if (!cmd) return NULL;
    
    cmd->timestamp_ns = time_ns();
    
    if (!input || strlen(input) == 0) {
        cmd->type = STDIO_CMD_UNKNOWN;
        return cmd;
    }
    
    /* 去除前后空白 */
    char* trimmed = strdup(input);
    while (trimmed[0] == ' ' || trimmed[0] == '\t') trimmed++;
    char* end = trimmed + strlen(trimmed) - 1;
    while (end >= trimmed && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    
    if (strlen(trimmed) == 0) {
        free(trimmed);
        cmd->type = STDIO_CMD_UNKNOWN;
        return cmd;
    }
    
    /* 解析命令 */
    char* space = strchr(trimmed, ' ');
    if (space) {
        *space = '\0';
        cmd->args = strdup(space + 1);
    }
    
    /* 识别命令类型 */
    if (strcmp(trimmed, "help") == 0 || strcmp(trimmed, "?") == 0) {
        cmd->type = STDIO_CMD_HELP;
    }
    else if (strcmp(trimmed, "session") == 0 && cmd->args) {
        char* subcmd = strtok(cmd->args, " ");
        if (subcmd) {
            if (strcmp(subcmd, "create") == 0) {
                cmd->type = STDIO_CMD_SESSION_CREATE;
                char* metadata = strtok(NULL, "");
                if (metadata) cmd->payload = cJSON_Parse(metadata);
            }
            else if (strcmp(subcmd, "list") == 0) {
                cmd->type = STDIO_CMD_SESSION_LIST;
            }
            else if (strcmp(subcmd, "close") == 0) {
                cmd->type = STDIO_CMD_SESSION_CLOSE;
                char* session_id = strtok(NULL, " ");
                if (session_id) cmd->session_id = strdup(session_id);
            }
        }
    }
    else if (strcmp(trimmed, "rpc") == 0 && cmd->args) {
        cmd->type = STDIO_CMD_RPC;
        /* 解析JSON-RPC请求 */
        cmd->payload = cJSON_Parse(cmd->args);
    }
    else if (strcmp(trimmed, "stats") == 0) {
        cmd->type = STDIO_CMD_STATS;
    }
    else if (strcmp(trimmed, "health") == 0) {
        cmd->type = STDIO_CMD_HEALTH;
    }
    else if (strcmp(trimmed, "metrics") == 0) {
        cmd->type = STDIO_CMD_METRICS;
    }
    else if (strcmp(trimmed, "exit") == 0 || strcmp(trimmed, "quit") == 0) {
        cmd->type = STDIO_CMD_EXIT;
    }
    else {
        cmd->type = STDIO_CMD_UNKNOWN;
    }
    
    free(trimmed);
    return cmd;
}

/**
 * @brief 销毁命令对象
 * @param cmd 命令对象
 */
static void stdio_command_destroy(stdio_command_t* cmd) {
    if (!cmd) return;
    
    if (cmd->args) free(cmd->args);
    if (cmd->session_id) free(cmd->session_id);
    if (cmd->payload) cJSON_Delete(cmd->payload);
    free(cmd);
}

/**
 * @brief 执行命令
 * @param context 连接上下文
 * @param cmd 命令对象
 * @return 响应字符串，需调用者free
 */
static char* stdio_command_execute(stdio_connection_context_t* context, stdio_command_t* cmd) {
    switch (cmd->type) {
        case STDIO_CMD_HELP:
            return strdup("Available commands:\n"
                          "  help/?                 - Show this help\n"
                          "  session create [json]  - Create new session\n"
                          "  session list          - List all sessions\n"
                          "  session close <id>    - Close session\n"
                          "  rpc <json-rpc>        - Execute RPC call\n"
                          "  stats                - Show statistics\n"
                          "  health               - Show health status\n"
                          "  metrics              - Show metrics\n"
                          "  exit/quit            - Exit program\n");
            
        case STDIO_CMD_SESSION_CREATE: {
            char* session_id = NULL;
            agentos_error_t err = session_manager_create_session(
                context->server->session_mgr,
                cmd->payload ? cJSON_Print(cmd->payload) : NULL,
                &session_id);
            
            if (err == AGENTOS_SUCCESS) {
                char* response = malloc(100);
                snprintf(response, 100, "Session created: %s\n", session_id);
                free(session_id);
                return response;
            } else {
                char* response = malloc(50);
                snprintf(response, 50, "Failed to create session: %d\n", err);
                return response;
            }
        }
            
        case STDIO_CMD_SESSION_LIST: {
            char** sessions = NULL;
            size_t count = 0;
            agentos_error_t err = session_manager_list_sessions(
                context->server->session_mgr, &sessions, &count);
            
            if (err == AGENTOS_SUCCESS && count > 0) {
                char* response = malloc(count * 50 + 100);
                strcpy(response, "Active sessions:\n");
                for (size_t i = 0; i < count; i++) {
                    strcat(response, "  ");
                    strcat(response, sessions[i]);
                    strcat(response, "\n");
                    free(sessions[i]);
                }
                free(sessions);
                return response;
            } else {
                return strdup("No active sessions\n");
            }
        }
            
        case STDIO_CMD_SESSION_CLOSE: {
            if (!cmd->session_id) {
                return strdup("Usage: session close <session-id>\n");
            }
            
            agentos_error_t err = session_manager_close_session(
                context->server->session_mgr, cmd->session_id);
            
            if (err == AGENTOS_SUCCESS) {
                char* response = malloc(50);
                snprintf(response, 50, "Session closed: %s\n", cmd->session_id);
                free(cmd->session_id);
                return response;
            } else {
                char* response = malloc(50);
                snprintf(response, 50, "Failed to close session: %d\n", err);
                free(cmd->session_id);
                return response;
            }
        }
            
        case STDIO_CMD_RPC: {
            if (!cmd->payload) {
                return strdup("Invalid RPC request. Please provide valid JSON-RPC.\n");
            }
            
            /* 执行RPC调用（与HTTP网关相同的逻辑） */
            char* response = handle_system_call(context, "agentos_sys_task_submit", cmd->payload);
            return response;
        }
            
        case STDIO_CMD_STATS: {
            cJSON* stats = cJSON_CreateObject();
            cJSON_AddNumberToObject(stats, "commands_total", atomic_load(&context->server->requests_total));
            cJSON_AddNumberToObject(stats, "commands_failed", atomic_load(&context->server->requests_failed));
            cJSON_AddNumberToObject(stats, "bytes_received", atomic_load(&context->server->bytes_received));
            cJSON_AddNumberToObject(stats, "bytes_sent", atomic_load(&context->server->bytes_sent));
            cJSON_AddNumberToObject(stats, "active_sessions", session_manager_count(context->server->session_mgr));
            
            char* json_str = cJSON_Print(stats);
            cJSON_Delete(stats);
            
            return json_str;
        }
            
        case STDIO_CMD_HEALTH: {
            char* health_json = NULL;
            health_checker_get_report(context->server->health, &health_json);
            return health_json;
        }
            
        case STDIO_CMD_METRICS: {
            char* metrics_json = NULL;
            telemetry_export_metrics(context->server->telemetry, &metrics_json);
            return metrics_json;
        }
            
        case STDIO_CMD_EXIT:
            return strdup("Exiting...\n");
            
        default:
            return strdup("Unknown command. Type 'help' for available commands.\n");
    }
}

/* ========== 输入处理线程 ========== */

/**
 * @brief 输入处理线程函数
 * @param arg 网关实例
 * @return NULL
 */
static void* stdio_input_thread(void* arg) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)arg;
    stdio_connection_context_t* context = gateway->connection;
    
    char buffer[1024];
    
    while (atomic_load(&gateway->running)) {
        /* 使用select检测输入 */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ret = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);
        if (ret > 0 && FD_ISSET(STDIN_FILENO, &read_fds)) {
            /* 读取输入 */
            if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                pthread_mutex_lock(&context->input_lock);
                
                /* 将输入添加到缓冲区 */
                size_t input_len = strlen(buffer);
                if (context->input_buffer_pos + input_len < sizeof(context->input_buffer)) {
                    memcpy(context->input_buffer + context->input_buffer_pos, buffer, input_len);
                    context->input_buffer_pos += input_len;
                    
                    /* 检查是否有完整的命令（以换行符结尾） */
                    char* newline = memchr(context->input_buffer, '\n', context->input_buffer_pos);
                    if (newline) {
                        *newline = '\0';
                        char* command_line = strdup(context->input_buffer);
                        context->input_buffer_pos -= (newline + 1 - context->input_buffer);
                        memmove(context->input_buffer, newline + 1, context->input_buffer_pos);
                        
                        /* 处理命令 */
                        stdio_command_t* cmd = stdio_command_parse(command_line);
                        if (cmd) {
                            char* response = stdio_command_execute(context, cmd);
                            stdio_command_destroy(cmd);
                            
                            /* 输出响应 */
                            printf("%s", response);
                            free(response);
                            
                            atomic_fetch_add(&gateway->commands_total, 1);
                            context->commands_processed++;
                            context->bytes_sent += strlen(response);
                        }
                        free(command_line);
                    }
                }
                
                pthread_mutex_unlock(&context->input_lock);
            }
        }
    }
    
    return NULL;
}

/* ========== 输出处理线程 ========== */

/**
 * @brief 输出处理线程函数
 * @param arg 网关实例
 * @return NULL
 */
static void* stdio_output_thread(void* arg) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)arg;
    stdio_connection_context_t* context = gateway->connection;
    
    while (atomic_load(&gateway->running)) {
        /* 定期输出状态信息 */
        sleep(30);
        
        if (atomic_load(&gateway->running)) {
            printf("\n[AgentOS Stdio Gateway] Status: Active Sessions: %zu, Commands: %zu\n",
                   session_manager_count(context->server->session_mgr),
                   context->commands_processed);
        }
    }
    
    return NULL;
}

/* ========== 网关操作表 ========== */

/**
 * @brief 启动Stdio网关
 * @param gateway 网关实例
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t stdio_gateway_start(void* gateway_impl) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    
    /* 创建连接上下文 */
    gateway->connection = calloc(1, sizeof(stdio_connection_context_t));
    if (!gateway->connection) {
        return AGENTOS_ENOMEM;
    }
    
    gateway->connection->server = gateway->server;
    gateway->connection->state = STDIO_STATE_ACTIVE;
    gateway->connection->connect_time_ns = time_ns();
    gateway->connection->last_activity_ns = time_ns();
    gateway->connection->input_buffer_pos = 0;
    
    pthread_mutex_init(&gateway->connection->input_lock, NULL);
    pthread_cond_init(&gateway->connection->input_cond, NULL);
    
    /* 启动运行标志 */
    atomic_init(&gateway->running, 1);
    
    /* 启动输入处理线程 */
    if (pthread_create(&gateway->input_thread, NULL, stdio_input_thread, gateway) != 0) {
        pthread_mutex_destroy(&gateway->connection->input_lock);
        pthread_cond_destroy(&gateway->connection->input_cond);
        free(gateway->connection);
        return AGENTOS_EBUSY;
    }
    
    /* 启动输出处理线程 */
    if (pthread_create(&gateway->output_thread, NULL, stdio_output_thread, gateway) != 0) {
        atomic_store(&gateway->running, 0);
        pthread_join(gateway->input_thread, NULL);
        pthread_mutex_destroy(&gateway->connection->input_lock);
        pthread_cond_destroy(&gateway->connection->input_cond);
        free(gateway->connection);
        return AGENTOS_EBUSY;
    }
    
    AGENTOS_LOG_INFO("Stdio gateway started");
    printf("AgentOS Stdio Gateway started. Type 'help' for available commands.\n");
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 停止Stdio网关
 * @param gateway 网关实例
 */
static void stdio_gateway_stop(void* gateway_impl) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    
    if (gateway->connection) {
        /* 设置停止标志 */
        atomic_store(&gateway->running, 0);
        
        /* 等待线程结束 */
        pthread_join(gateway->input_thread, NULL);
        pthread_join(gateway->output_thread, NULL);
        
        /* 清理连接上下文 */
        pthread_mutex_destroy(&gateway->connection->input_lock);
        pthread_cond_destroy(&gateway->connection->input_cond);
        free(gateway->connection);
        gateway->connection = NULL;
    }
    
    AGENTOS_LOG_INFO("Stdio gateway stopped");
}

/**
 * @brief 销毁Stdio网关
 * @param gateway 网关实例
 */
static void stdio_gateway_destroy(void* gateway_impl) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    
    stdio_gateway_stop(gateway);
    
    if (gateway->ratelimiter) {
        ratelimit_destroy(gateway->ratelimiter);
    }
    
    free(gateway);
}

/**
 * @brief 获取Stdio网关名称
 * @param gateway 网关实例
 * @return 名称字符串
 */
static const char* stdio_gateway_get_name(void* gateway_impl) {
    return "Stdio Gateway";
}

/**
 * @brief 获取Stdio网关统计信息
 * @param gateway 网关实例
 * @param out_json 输出JSON字符串（需调用者free）
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t stdio_gateway_get_stats(void* gateway_impl, char** out_json) {
    stdio_gateway_t* gateway = (stdio_gateway_t*)gateway_impl;
    
    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "commands_total", atomic_load(&gateway->commands_total));
    cJSON_AddNumberToObject(stats, "commands_failed", atomic_load(&gateway->commands_failed));
    cJSON_AddNumberToObject(stats, "bytes_received", atomic_load(&gateway->bytes_received));
    cJSON_AddNumberToObject(stats, "bytes_sent", atomic_load(&gateway->bytes_sent));
    
    if (gateway->connection) {
        cJSON_AddNumberToObject(stats, "connection_commands", gateway->connection->commands_processed);
        cJSON_AddNumberToObject(stats, "connection_bytes_received", gateway->connection->bytes_received);
        cJSON_AddNumberToObject(stats, "connection_bytes_sent", gateway->connection->bytes_sent);
    }
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

/* ========== 网关操作表 ========== */

static const gateway_ops_t stdio_gateway_ops = {
    .start = stdio_gateway_start,
    .stop = stdio_gateway_stop,
    .destroy = stdio_gateway_destroy,
    .get_name = stdio_gateway_get_name,
    .get_stats = stdio_gateway_get_stats
};

/* ========== 公共接口 ========== */

gateway_t* stdio_gateway_create(dynamic_server_t* server) {
    if (!server) {
        return NULL;
    }
    
    stdio_gateway_t* gateway = calloc(1, sizeof(stdio_gateway_t));
    if (!gateway) {
        return NULL;
    }
    
    gateway->server = server;
    
    /* 初始化统计信息 */
    atomic_init(&gateway->commands_total, 0);
    atomic_init(&gateway->commands_failed, 0);
    atomic_init(&gateway->bytes_received, 0);
    atomic_init(&gateway->bytes_sent, 0);
    
    /* 初始化连接管理 */
    pthread_mutex_init(&gateway->connection_lock, NULL);
    
    /* 创建限流器 */
    gateway->ratelimiter = ratelimit_create(10, 60); /* 10命令/分钟 */
    if (!gateway->ratelimiter) {
        pthread_mutex_destroy(&gateway->connection_lock);
        free(gateway);
        return NULL;
    }
    
    /* 创建网关实例 */
    gateway_t* gw = malloc(sizeof(gateway_t));
    if (!gw) {
        pthread_mutex_destroy(&gateway->connection_lock);
        ratelimit_destroy(gateway->ratelimiter);
        free(gateway);
        return NULL;
    }
    
    gw->ops = &stdio_gateway_ops;
    gw->server = server;
    gw->impl = gateway;
    
    return gw;
}