/**
 * @file workbench.c
 * @brief 虚拟工位实现 - 跨平台进程管理
 * @author Spharx
 * @date 2024
 */

#include "workbench.h"
#include "workbench_process.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_TIMEOUT_MS 30000
#define DEFAULT_MAX_OUTPUT_SIZE (1024 * 1024)
#define OUTPUT_CHUNK_SIZE 4096

struct workbench {
    workbench_config_t config;
    workbench_state_t state;
    domes_process_t process;
    domes_pipe_t stdin_pipe;
    domes_pipe_t stdout_pipe;
    domes_pipe_t stderr_pipe;
    char* stdout_buf;
    size_t stdout_capacity;
    size_t stdout_size;
    char* stderr_buf;
    size_t stderr_capacity;
    size_t stderr_size;
    uint64_t start_time_ms;
    domes_mutex_t lock;
};

void workbench_default_config(workbench_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(workbench_config_t));
    config->timeout_ms = DEFAULT_TIMEOUT_MS;
    config->max_output_size = DEFAULT_MAX_OUTPUT_SIZE;
    config->redirect_stdin = true;
    config->redirect_stdout = true;
    config->redirect_stderr = true;
}

workbench_t* workbench_create(const workbench_config_t* config) {
    workbench_t* wb = (workbench_t*)domes_mem_alloc(sizeof(workbench_t));
    if (!wb) return NULL;
    
    memset(wb, 0, sizeof(workbench_t));
    
    if (config) {
        memcpy(&wb->config, config, sizeof(workbench_config_t));
    } else {
        workbench_default_config(&wb->config);
    }
    
    if (domes_mutex_init(&wb->lock) != DOMES_OK) {
        domes_mem_free(wb);
        return NULL;
    }
    
    wb->state = WORKBENCH_STATE_IDLE;
    
    if (wb->config.max_output_size > 0) {
        wb->stdout_capacity = wb->config.max_output_size;
        wb->stdout_buf = (char*)domes_mem_alloc(wb->stdout_capacity);
        
        wb->stderr_capacity = wb->config.max_output_size;
        wb->stderr_buf = (char*)domes_mem_alloc(wb->stderr_capacity);
    }
    
    return wb;
}

void workbench_destroy(workbench_t* wb) {
    if (!wb) return;
    
    domes_mutex_lock(&wb->lock);
    
    if (wb->state == WORKBENCH_STATE_RUNNING) {
        workbench_terminate(wb);
    }
    
    domes_mem_free(wb->stdout_buf);
    domes_mem_free(wb->stderr_buf);
    
    domes_mutex_unlock(&wb->lock);
    domes_mutex_destroy(&wb->lock);
    domes_mem_free(wb);
}

static int create_pipes(workbench_t* wb) {
    if (wb->config.redirect_stdin) {
        if (domes_pipe_create(&wb->stdin_pipe) != DOMES_OK) {
            return DOMES_ERROR_IO;
        }
    }
    
    if (wb->config.redirect_stdout) {
        if (domes_pipe_create(&wb->stdout_pipe) != DOMES_OK) {
            if (wb->config.redirect_stdin) {
                domes_pipe_close(&wb->stdin_pipe);
            }
            return DOMES_ERROR_IO;
        }
    }
    
    if (wb->config.redirect_stderr) {
        if (domes_pipe_create(&wb->stderr_pipe) != DOMES_OK) {
            if (wb->config.redirect_stdin) {
                domes_pipe_close(&wb->stdin_pipe);
            }
            if (wb->config.redirect_stdout) {
                domes_pipe_close(&wb->stdout_pipe);
            }
            return DOMES_ERROR_IO;
        }
    }
    
    return DOMES_OK;
}

static void close_pipes(workbench_t* wb) {
    if (wb->config.redirect_stdin) {
        domes_pipe_close(&wb->stdin_pipe);
    }
    if (wb->config.redirect_stdout) {
        domes_pipe_close(&wb->stdout_pipe);
    }
    if (wb->config.redirect_stderr) {
        domes_pipe_close(&wb->stderr_pipe);
    }
}

static int read_output(workbench_t* wb) {
    char buf[OUTPUT_CHUNK_SIZE];
    size_t bytes_read;
    
    if (wb->config.redirect_stdout && wb->stdout_buf) {
        while (true) {
            int ret = domes_pipe_read(&wb->stdout_pipe, buf, sizeof(buf), &bytes_read);
            if (ret != DOMES_OK || bytes_read == 0) break;
            
            if (wb->stdout_size + bytes_read < wb->stdout_capacity) {
                memcpy(wb->stdout_buf + wb->stdout_size, buf, bytes_read);
                wb->stdout_size += bytes_read;
            }
        }
    }
    
    if (wb->config.redirect_stderr && wb->stderr_buf) {
        while (true) {
            int ret = domes_pipe_read(&wb->stderr_pipe, buf, sizeof(buf), &bytes_read);
            if (ret != DOMES_OK || bytes_read == 0) break;
            
            if (wb->stderr_size + bytes_read < wb->stderr_capacity) {
                memcpy(wb->stderr_buf + wb->stderr_size, buf, bytes_read);
                wb->stderr_size += bytes_read;
            }
        }
    }
    
    return DOMES_OK;
}

int workbench_execute(workbench_t* wb, const char* command, char* const argv[],
                      workbench_result_t* result) {
    if (!wb || !command) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&wb->lock);
    
    if (wb->state == WORKBENCH_STATE_RUNNING) {
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_BUSY;
    }
    
    wb->stdout_size = 0;
    wb->stderr_size = 0;
    if (wb->stdout_buf) wb->stdout_buf[0] = '\0';
    if (wb->stderr_buf) wb->stderr_buf[0] = '\0';
    
    if (create_pipes(wb) != DOMES_OK) {
        wb->state = WORKBENCH_STATE_ERROR;
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_IO;
    }
    
    domes_process_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.working_dir = wb->config.working_dir;
    attr.env = wb->config.env_vars;
    attr.redirect_stdin = wb->config.redirect_stdin;
    attr.redirect_stdout = wb->config.redirect_stdout;
    attr.redirect_stderr = wb->config.redirect_stderr;
    
    if (wb->config.redirect_stdin) {
        attr.stdin_pipe = wb->stdin_pipe;
    }
    if (wb->config.redirect_stdout) {
        attr.stdout_pipe = wb->stdout_pipe;
    }
    if (wb->config.redirect_stderr) {
        attr.stderr_pipe = wb->stderr_pipe;
    }
    
    wb->start_time_ms = domes_time_ms();
    
    int ret = domes_process_spawn(&wb->process, command, argv, &attr);
    if (ret != DOMES_OK) {
        close_pipes(wb);
        wb->state = WORKBENCH_STATE_ERROR;
        domes_mutex_unlock(&wb->lock);
        return ret;
    }
    
    wb->state = WORKBENCH_STATE_RUNNING;
    domes_mutex_unlock(&wb->lock);
    
    uint32_t timeout_ms = wb->config.timeout_ms > 0 ? wb->config.timeout_ms : 0;
    
    domes_exit_status_t status;
    ret = domes_process_wait(wb->process, &status, timeout_ms);
    
    domes_mutex_lock(&wb->lock);
    
    if (ret == DOMES_ERROR_TIMEOUT) {
        domes_process_terminate(wb->process, 9);
        domes_process_wait(wb->process, &status, 1000);
        
        read_output(wb);
        close_pipes(wb);
        domes_process_close(wb->process);
        
        if (result) {
            memset(result, 0, sizeof(workbench_result_t));
            result->exit_code = -1;
            result->timed_out = true;
            result->stdout_data = wb->stdout_buf ? domes_strdup(wb->stdout_buf) : NULL;
            result->stdout_size = wb->stdout_size;
            result->stderr_data = wb->stderr_buf ? domes_strdup(wb->stderr_buf) : NULL;
            result->stderr_size = wb->stderr_size;
            result->start_time_ms = wb->start_time_ms;
            result->end_time_ms = domes_time_ms();
        }
        
        wb->state = WORKBENCH_STATE_STOPPED;
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_TIMEOUT;
    }
    
    read_output(wb);
    close_pipes(wb);
    domes_process_close(wb->process);
    
    if (result) {
        memset(result, 0, sizeof(workbench_result_t));
        result->exit_code = status.code;
        result->signaled = status.signaled;
        result->signal = status.signal;
        result->stdout_data = wb->stdout_buf ? domes_strdup(wb->stdout_buf) : NULL;
        result->stdout_size = wb->stdout_size;
        result->stderr_data = wb->stderr_buf ? domes_strdup(wb->stderr_buf) : NULL;
        result->stderr_size = wb->stderr_size;
        result->start_time_ms = wb->start_time_ms;
        result->end_time_ms = domes_time_ms();
    }
    
    wb->state = WORKBENCH_STATE_IDLE;
    domes_mutex_unlock(&wb->lock);
    
    return DOMES_OK;
}

int workbench_execute_async(workbench_t* wb, const char* command, char* const argv[]) {
    if (!wb || !command) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&wb->lock);
    
    if (wb->state == WORKBENCH_STATE_RUNNING) {
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_BUSY;
    }
    
    wb->stdout_size = 0;
    wb->stderr_size = 0;
    
    if (create_pipes(wb) != DOMES_OK) {
        wb->state = WORKBENCH_STATE_ERROR;
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_IO;
    }
    
    domes_process_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.working_dir = wb->config.working_dir;
    attr.env = wb->config.env_vars;
    attr.redirect_stdin = wb->config.redirect_stdin;
    attr.redirect_stdout = wb->config.redirect_stdout;
    attr.redirect_stderr = wb->config.redirect_stderr;
    
    wb->start_time_ms = domes_time_ms();
    
    int ret = domes_process_spawn(&wb->process, command, argv, &attr);
    if (ret != DOMES_OK) {
        close_pipes(wb);
        wb->state = WORKBENCH_STATE_ERROR;
        domes_mutex_unlock(&wb->lock);
        return ret;
    }
    
    wb->state = WORKBENCH_STATE_RUNNING;
    domes_mutex_unlock(&wb->lock);
    
    return DOMES_OK;
}

int workbench_wait(workbench_t* wb, workbench_result_t* result, uint32_t timeout_ms) {
    if (!wb) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&wb->lock);
    
    if (wb->state != WORKBENCH_STATE_RUNNING) {
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_INVALID_ARG;
    }
    
    domes_mutex_unlock(&wb->lock);
    
    domes_exit_status_t status;
    int ret = domes_process_wait(wb->process, &status, timeout_ms);
    
    domes_mutex_lock(&wb->lock);
    
    if (ret == DOMES_ERROR_TIMEOUT) {
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_TIMEOUT;
    }
    
    read_output(wb);
    close_pipes(wb);
    domes_process_close(wb->process);
    
    if (result) {
        memset(result, 0, sizeof(workbench_result_t));
        result->exit_code = status.code;
        result->signaled = status.signaled;
        result->signal = status.signal;
        result->stdout_data = wb->stdout_buf ? domes_strdup(wb->stdout_buf) : NULL;
        result->stdout_size = wb->stdout_size;
        result->stderr_data = wb->stderr_buf ? domes_strdup(wb->stderr_buf) : NULL;
        result->stderr_size = wb->stderr_size;
        result->start_time_ms = wb->start_time_ms;
        result->end_time_ms = domes_time_ms();
    }
    
    wb->state = WORKBENCH_STATE_IDLE;
    domes_mutex_unlock(&wb->lock);
    
    return DOMES_OK;
}

int workbench_terminate(workbench_t* wb) {
    if (!wb) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&wb->lock);
    
    if (wb->state != WORKBENCH_STATE_RUNNING) {
        domes_mutex_unlock(&wb->lock);
        return DOMES_OK;
    }
    
    int ret = domes_process_terminate(wb->process, 9);
    
    domes_exit_status_t status;
    domes_process_wait(wb->process, &status, 1000);
    
    read_output(wb);
    close_pipes(wb);
    domes_process_close(wb->process);
    
    wb->state = WORKBENCH_STATE_STOPPED;
    domes_mutex_unlock(&wb->lock);
    
    return ret;
}

workbench_state_t workbench_get_state(workbench_t* wb) {
    if (!wb) return WORKBENCH_STATE_ERROR;
    
    domes_mutex_lock(&wb->lock);
    workbench_state_t state = wb->state;
    domes_mutex_unlock(&wb->lock);
    
    return state;
}

int64_t workbench_get_pid(workbench_t* wb) {
    if (!wb) return -1;
    
    domes_mutex_lock(&wb->lock);
    
    if (wb->state != WORKBENCH_STATE_RUNNING) {
        domes_mutex_unlock(&wb->lock);
        return -1;
    }
    
    domes_pid_t pid = domes_process_getpid(wb->process);
    domes_mutex_unlock(&wb->lock);
    
    return (int64_t)pid;
}

int workbench_write_stdin(workbench_t* wb, const void* data, size_t size, size_t* written) {
    if (!wb || !data || !written) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&wb->lock);
    
    if (wb->state != WORKBENCH_STATE_RUNNING || !wb->config.redirect_stdin) {
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_INVALID_ARG;
    }
    
    int ret = domes_pipe_write(&wb->stdin_pipe, data, size, written);
    domes_mutex_unlock(&wb->lock);
    
    return ret;
}

int workbench_read_stdout(workbench_t* wb, void* buf, size_t size, size_t* read_size) {
    if (!wb || !buf || !read_size) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&wb->lock);
    
    if (!wb->config.redirect_stdout) {
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_INVALID_ARG;
    }
    
    int ret = domes_pipe_read(&wb->stdout_pipe, buf, size, read_size);
    domes_mutex_unlock(&wb->lock);
    
    return ret;
}

int workbench_read_stderr(workbench_t* wb, void* buf, size_t size, size_t* read_size) {
    if (!wb || !buf || !read_size) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&wb->lock);
    
    if (!wb->config.redirect_stderr) {
        domes_mutex_unlock(&wb->lock);
        return DOMES_ERROR_INVALID_ARG;
    }
    
    int ret = domes_pipe_read(&wb->stderr_pipe, buf, size, read_size);
    domes_mutex_unlock(&wb->lock);
    
    return ret;
}

void workbench_result_free(workbench_result_t* result) {
    if (!result) return;
    
    domes_mem_free(result->stdout_data);
    domes_mem_free(result->stderr_data);
    memset(result, 0, sizeof(workbench_result_t));
}
