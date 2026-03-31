/**
 * @file workbench.h
 * @brief ���⹤λ�����ӿ� - �����ִ�л���
 * @author Spharx
 * @date 2024
 * 
 * ���ԭ��
 * - ����ִ�У�ÿ�� Agent �����ڶ�����λ
 * - ��Դ���ƣ�CPU���ڴ桢ʱ������
 * - ��ȫ�߽磺�ļ�ϵͳ���������
 * - �ɹ۲��ԣ��������״̬���
 * 
 * ��Դ����˵����
 * - Linux: ʹ�� cgroups v2
 * - Windows: ʹ�� Job Objects
 * - macOS: ʹ�������� mach �˿ڵ���Դ��
 */

#ifndef CUPOLAS_WORKBENCH_H
#define CUPOLAS_WORKBENCH_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ��λ״̬ */
typedef enum workbench_state {
    WORKBENCH_STATE_IDLE = 0,
    WORKBENCH_STATE_RUNNING,
    WORKBENCH_STATE_STOPPED,
    WORKBENCH_STATE_ERROR
} workbench_state_t;

/* ��Դ�������� */
typedef struct workbench_limits {
    size_t max_memory_bytes;       /* ����ڴ����ƣ��ֽڣ���0 ��ʾ������ */
    uint32_t max_cpu_time_ms;      /* ��� CPU ʱ�䣨���룩��0 ��ʾ������ */
    size_t max_output_bytes;       /* ��������С���ֽڣ���0 ��ʾʹ��Ĭ��ֵ */
    uint32_t max_processes;        /* ����ӽ�������0 ��ʾ������ */
    uint32_t max_threads;          /* ����߳�����0 ��ʾ������ */
    size_t max_file_size_bytes;    /* ����ļ���С���ֽڣ���0 ��ʾ������ */
} workbench_limits_t;

/* ��λ���� */
typedef struct workbench_config {
    const char* working_dir;
    const char** env_vars;
    size_t env_count;
    uint32_t timeout_ms;
    size_t max_output_size;
    bool redirect_stdin;
    bool redirect_stdout;
    bool redirect_stderr;
    workbench_limits_t limits;     /* ��Դ���� */
    bool enable_limits;           /* �Ƿ�������Դ���� */
} workbench_config_t;

/* ��λִ�н�� */
typedef struct workbench_result {
    int exit_code;
    bool timed_out;
    bool signaled;
    int signal;
    char* stdout_data;
    size_t stdout_size;
    char* stderr_data;
    size_t stderr_size;
    uint64_t start_time_ms;
    uint64_t end_time_ms;
} workbench_result_t;

/* ��λ��� */
typedef struct workbench workbench_t;

/**
 * @brief ������λ
 * @param manager ���ã���ѡ��NULL ʹ��Ĭ�����ã�
 * @return ��λ�����ʧ�ܷ��� NULL
 */
workbench_t* workbench_create(const workbench_config_t* manager);

/**
 * @brief ���ٹ�λ
 * @param wb ��λ���
 */
void workbench_destroy(workbench_t* wb);

/**
 * @brief ִ�����ͬ����
 * @param wb ��λ���
 * @param command ����
 * @param argv �������飨�� NULL ��β��
 * @param result ִ�н��
 * @return 0 �ɹ�������ʧ��
 */
int workbench_execute(workbench_t* wb, const char* command, char* const argv[],
                      workbench_result_t* result);

/**
 * @brief ִ������첽��
 * @param wb ��λ���
 * @param command ����
 * @param argv �������飨�� NULL ��β��
 * @return 0 �ɹ�������ʧ��
 */
int workbench_execute_async(workbench_t* wb, const char* command, char* const argv[]);

/**
 * @brief �ȴ�ִ�����
 * @param wb ��λ���
 * @param result ִ�н��
 * @param timeout_ms ��ʱʱ�䣨���룩��0 ��ʾ���޵ȴ�
 * @return 0 �ɹ���cupolas_ERROR_TIMEOUT ��ʱ������ʧ��
 */
int workbench_wait(workbench_t* wb, workbench_result_t* result, uint32_t timeout_ms);

/**
 * @brief ��ִֹ��
 * @param wb ��λ���
 * @return 0 �ɹ�������ʧ��
 */
int workbench_terminate(workbench_t* wb);

/**
 * @brief ��ȡ��λ״̬
 * @param wb ��λ���
 * @return ��λ״̬
 */
workbench_state_t workbench_get_state(workbench_t* wb);

/**
 * @brief ��ȡ���� ID
 * @param wb ��λ���
 * @return ���� ID��ʧ�ܷ��� -1
 */
int64_t workbench_get_pid(workbench_t* wb);

/**
 * @brief д���׼����
 * @param wb ��λ���
 * @param data ����
 * @param size ���ݴ�С
 * @param written ʵ��д���С
 * @return 0 �ɹ�������ʧ��
 */
int workbench_write_stdin(workbench_t* wb, const void* data, size_t size, size_t* written);

/**
 * @brief ��ȡ��׼���
 * @param wb ��λ���
 * @param buf ������
 * @param size ��������С
 * @param read_size ʵ�ʶ�ȡ��С
 * @return 0 �ɹ�������ʧ��
 */
int workbench_read_stdout(workbench_t* wb, void* buf, size_t size, size_t* read_size);

/**
 * @brief ��ȡ��׼����
 * @param wb ��λ���
 * @param buf ������
 * @param size ��������С
 * @param read_size ʵ�ʶ�ȡ��С
 * @return 0 �ɹ�������ʧ��
 */
int workbench_read_stderr(workbench_t* wb, void* buf, size_t size, size_t* read_size);

/**
 * @brief �ͷ�ִ�н��
 * @param result ִ�н��
 */
void workbench_result_free(workbench_result_t* result);

/**
 * @brief ��ȡĬ������
 * @param manager �������
 */
void workbench_default_config(workbench_config_t* manager);

/**
 * @brief ������Դ����
 * @param wb ��λ���
 * @param limits ��Դ���ƣ�NULL ��ʾ�������ƣ�
 * @return 0 �ɹ�������ʧ��
 */
int workbench_set_limits(workbench_t* wb, const workbench_limits_t* limits);

/**
 * @brief ��ȡ��Դ����
 * @param wb ��λ���
 * @param limits ��Դ�������
 * @return 0 �ɹ�������ʧ��
 */
int workbench_get_limits(workbench_t* wb, workbench_limits_t* limits);

/**
 * @brief ��ȡ��ǰ��Դʹ�����
 * @param wb ��λ���
 * @param memory_usage �ڴ�ʹ�ã��ֽڣ����
 * @param cpu_usage CPU ʱ�䣨���룩���
 * @return 0 �ɹ�������ʧ��
 */
int workbench_get_usage(workbench_t* wb, size_t* memory_usage, uint64_t* cpu_usage);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_WORKBENCH_H */
