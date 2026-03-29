/**
 * @file agentos.h
 * @brief AgentOS gateway ģ�����??
 *
 * ���ļ���Ϊ���ݲ㣬�� gateway ģ��Ľӿ�ӳ�䵽 commons ģ��??
 * gateway ģ��ԭ������ʵ��??agentos_* �ӿڣ�����ͳһʹ�� commons ģ��??
 *
 * ӳ���ϵ??
 * - platform_* ����/���� -> agentos_* ����/����
 * - AGENTOS_LOG_* ??-> ʹ�� commons/utils/observability/include/logger.h
 * - agentos_error_t -> ʹ�� commons/utils/error/include/error.h
 * - agentos_time_* -> ʹ�� commons/platform/platform.h ??agentos_time_ns/ms
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_gateway_AGENTOS_H
#define AGENTOS_gateway_AGENTOS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== ���ŵ���??==================== */

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

#ifndef AGENTOS_API
    #define AGENTOS_API
#endif

/* ==================== �����붨�壨���� commons/error.h??==================== */

/*
 * ������ֶι滮��
 *   -1 ??-99:      ͨ�û�������
 *   -100 ??-199:   ϵͳ��ƽ̨��??
 *   -200 ??-299:   �ں˲��??
 *   -300 ??-399:   ������??
 *   -400 ??-499:   LLM/AI�������
 *   -500 ??-599:   ִ��/���ߴ���
 *   -600 ??-699:   ����/�洢����
 *   -700 ??-799:   ��ȫ/ɳ�����
 *   -800 ??-899:   Э��/�滮����
 */

typedef int32_t agentos_error_t;

#define AGENTOS_OK                      0
#define AGENTOS_SUCCESS                0
#define AGENTOS_ERR_UNKNOWN           (-1)
#define AGENTOS_ERR_INVALID_PARAM      (-2)
#define AGENTOS_ERR_NULL_POINTER       (-3)
#define AGENTOS_ERR_OUT_OF_MEMORY      (-4)
#define AGENTOS_ERR_BUFFER_TOO_SMALL  (-5)
#define AGENTOS_ERR_NOT_FOUND          (-6)
#define AGENTOS_ERR_ALREADY_EXISTS     (-7)
#define AGENTOS_ERR_TIMEOUT            (-8)
#define AGENTOS_ERR_NOT_SUPPORTED      (-9)
#define AGENTOS_ERR_PERMISSION_DENIED  (-10)
#define AGENTOS_ERR_IO                 (-11)
#define AGENTOS_ERR_BUSY               (-12)
#define AGENTOS_ERR_STATE_ERROR        (-13)
#define AGENTOS_ERR_OVERFLOW           (-14)
#define AGENTOS_ERR_CANCELED           (-15)
#define AGENTOS_ERR_NOT_INITIALIZED    (-16)
#define AGENTOS_ERR_ALREADY_INITIALIZED (-17)
#define AGENTOS_ERR_CONNECTION_FAILED  (-18)
#define AGENTOS_ERR_CONNECTION_CLOSED  (-19)
#define AGENTOS_ERR_PROTOCOL_ERROR     (-20)
#define AGENTOS_ERR_PARSE_ERROR        (-21)

/* �����Ա�����������??*/
#define AGENTOS_EINVAL                 AGENTOS_ERR_INVALID_PARAM
#define AGENTOS_ENOMEM                 AGENTOS_ERR_OUT_OF_MEMORY
#define AGENTOS_EBUSY                  AGENTOS_ERR_BUSY
#define AGENTOS_ENOENT                 AGENTOS_ERR_NOT_FOUND
#define AGENTOS_EPERM                  AGENTOS_ERR_PERMISSION_DENIED
#define AGENTOS_ETIMEDOUT              AGENTOS_ERR_TIMEOUT
#define AGENTOS_EEXIST                 AGENTOS_ERR_ALREADY_EXISTS
#define AGENTOS_ECANCELED              AGENTOS_ERR_CANCELED
#define AGENTOS_ENOTSUP                AGENTOS_ERR_NOT_SUPPORTED
#define AGENTOS_EIO                    AGENTOS_ERR_IO
#define AGENTOS_ERROR                  (-100)

/* ==================== ��������ӳ�䣨platform -> agentos??==================== */

/*
 * gateway ģ��ʹ�� platform_* ǰ׺��commons ģ��ʹ�� agentos_* ǰ׺
 * ͨ����ӳ��ʵ�ּ�??
 */

#define platform_thread_t              agentos_thread_t
#define platform_thread_id_t           agentos_thread_id_t
#define platform_mutex_t               agentos_mutex_t
#define platform_cond_t                agentos_cond_t
#define platform_socket_t              agentos_socket_t

#define PLATFORM_THREAD_INVALID        AGENTOS_INVALID_THREAD
#define PLATFORM_MUTEX_INVALID         AGENTOS_INVALID_MUTEX
#define PLATFORM_SOCKET_INVALID        AGENTOS_INVALID_SOCKET

#define platform_mutex_init            agentos_mutex_init
#define platform_mutex_destroy         agentos_mutex_destroy
#define platform_mutex_lock            agentos_mutex_lock
#define platform_mutex_trylock         agentos_mutex_trylock
#define platform_mutex_unlock          agentos_mutex_unlock

#define platform_cond_init             agentos_cond_init
#define platform_cond_destroy          agentos_cond_destroy
#define platform_cond_wait             agentos_cond_wait
#define platform_cond_timedwait        agentos_cond_timedwait
#define platform_cond_signal           agentos_cond_signal
#define platform_cond_broadcast        agentos_cond_broadcast

#define platform_thread_create         agentos_thread_create
#define platform_thread_join           agentos_thread_join
#define platform_thread_self           agentos_thread_id

#define platform_socket_tcp             agentos_socket_tcp
#define platform_socket_unix            agentos_socket_unix
#define platform_socket_close          agentos_socket_close
#define platform_socket_set_nonblock   agentos_socket_set_nonblock
#define platform_socket_set_reuseaddr   agentos_socket_set_reuseaddr

#define platform_network_init          agentos_network_init
#define platform_network_cleanup       agentos_network_cleanup
#define platform_ignore_sigpipe        agentos_ignore_sigpipe

/* ==================== ʱ�亯��ӳ�� ==================== */

/**
 * @brief ��ȡ��ǰʱ�䣨���룩- ����ʱ��
 * @return ��ǰʱ������??
 */
AGENTOS_API uint64_t agentos_time_ns(void);

/**
 * @brief ��ȡ��ǰʱ�䣨���룩- ����ʱ��
 * @return ��ǰʱ�����??
 */
AGENTOS_API uint64_t agentos_time_ms(void);

#define agentos_time_monotonic_ns()    agentos_time_ns()
#define agentos_time_monotonic_ms()    agentos_time_ms()

/**
 * @brief ��ȡ��ǰʱ�䣨���룩- ǽ��ʱ��
 * @return ��ǰʱ������??
 */
AGENTOS_API uint64_t agentos_time_current_ns(void);

/**
 * @brief ���뼶˯??
 * @param ns ˯��ʱ�䣨���룩
 * @return AGENTOS_SUCCESS �ɹ�
 */
AGENTOS_API agentos_error_t agentos_time_nanosleep(uint64_t ns);

/* ==================== �̺߳������ͼ��� ==================== */

/*
 * gateway ģ��ʹ�ò�ͬ���̺߳������Ͷ�??
 * ͨ��ת��������
 */

#ifdef _WIN32
    #define platform_thread_func_t     agentos_thread_func_t
#else
    #define platform_thread_func_t      agentos_thread_func_t
#endif

typedef void* (*agentos_thread_func_t)(void* arg);

/* ==================== ��־�ӿڣ���??commons/logger.h??==================== */

/*
 * ��־������
 */
#define AGENTOS_LOG_LEVEL_ERROR 1
#define AGENTOS_LOG_LEVEL_WARN  2
#define AGENTOS_LOG_LEVEL_INFO  3
#define AGENTOS_LOG_LEVEL_DEBUG 4

#ifndef AGENTOS_LOG_LEVEL
#define AGENTOS_LOG_LEVEL AGENTOS_LOG_LEVEL_INFO
#endif

/*
 * ��־����������ʵ���� commons ģ���ṩ??
 */
AGENTOS_API const char* agentos_log_set_trace_id(const char* trace_id);
AGENTOS_API const char* agentos_log_get_trace_id(void);
AGENTOS_API void agentos_log_write(int level, const char* file, int line, const char* fmt, ...);

#define AGENTOS_LOG_ERROR(fmt, ...) agentos_log_write(AGENTOS_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_WARN(fmt, ...)  agentos_log_write(AGENTOS_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_INFO(fmt, ...)  agentos_log_write(AGENTOS_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define AGENTOS_LOG_DEBUG(fmt, ...) agentos_log_write(AGENTOS_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* ==================== ������ӿڣ���??commons/error.h??==================== */

/*
 * �����ַ�����ȡ��ʵ��??commons ģ���ṩ??
 */
AGENTOS_API const char* agentos_strerror(agentos_error_t err);

/* ==================== ��־��ʼ���ӿڣ����ݲ㣩 ==================== */

AGENTOS_API int agentos_logger_init(const char* config_path);
AGENTOS_API void agentos_logger_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_gateway_AGENTOS_H */
