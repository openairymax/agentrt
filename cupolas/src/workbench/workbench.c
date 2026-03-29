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
    workbench_config_t manager;
    workbench_state_t state;
    cupolas_process_t process;
    cupolas_pipe_t stdin_pipe;
    cupolas_pipe_t stdout_pipe;
    cupolas_pipe_t stderr_pipe;
    char* stdout_buf;
    size_t stdout_capacity;
    size_t stdout_size;
    char* stderr_buf;
    size_t stderr_capacity;
    size_t stderr_size;
    uint64_t start_time_ms;
    cupolas_mutex_t lock;
};

void workbench_default_config(workbench_config_t* manager) {
    if (!manager) return;
    
    memset(manager, 0, sizeof(workbench_config_t));
    manager->timeout_ms = DEFAULT_TIMEOUT_MS;
    manager->max_output_size = DEFAULT_MAX_OUTPUT_SIZE;
    manager->redirect_stdin = true;
    manager->redirect_stdout = true;
    manager->redirect_stderr = true;
}

workbench_t* workbench_create(const workbench_config_t* manager) {
    workbench_t* wb = (workbench_t*)cupolas_mem_alloc(sizeof(workbench_t));
    if (!wb) return NULL;
    
    memset(wb, 0, sizeof(workbench_t));
    
    if (manager) {
        memcpy(&wb->manager, manager, sizeof(workbench_config_t));
    } else {
        workbench_default_config(&wb->manager);
    }
    
    if (cupolas_mutex_init(&wb->lock) != cupolas_OK) {
        cupolas_mem_free(wb);
        return NULL;
    }
    
    wb->state = WORKBENCH_STATE_IDLE;
    
    if (wb->manager.max_output_size > 0) {
        wb->stdout_capacity = wb->manager.max_output_size;
        wb->stdout_buf = (char*)cupolas_mem_alloc(wb->stdout_capacity);
        
        wb->stderr_capacity = wb->manager.max_output_size;
        wb->stderr_buf = (char*)cupolas_mem_alloc(wb->stderr_capacity);
    }
    
    return wb;
}

void workbench_destroy(workbench_t* wb) {
    if (!wb) return;
    
    cupolas_mutex_lock(&wb->lock);
    
    if (wb->state == WORKBENCH_STATE_RUNNING) {
        workbench_terminate(wb);
    }
    
    cupolas_mem_free(wb->stdout_buf);
    cupolas_mem_free(wb->stderr_buf);
    
    cupolas_mutex_unlock(&wb->lock);
    cupolas_mutex_destroy(&wb->lock);
    cupolas_mem_free(wb);
}

static int create_pipes(workbench_t* wb) {
    if (wb->manager.redirect_stdin) {
        if (cupolas_pipe_create(&wb->stdin_pipe) != cupolas_OK) {
            return cupolas_ERROR_IO;
        }
    }
    
    if (wb->manager.redirect_stdout) {
        if (cupolas_pipe_create(&wb->stdout_pipe) != cupolas_OK) {
            if (wb->manager.redirect_stdin) {
                cupolas_pipe_close(&wb->stdin_pipe);
            }
            return cupolas_ERROR_IO;
        }
    }
    
    if (wb->manager.redirect_stderr) {
        if (cupolas_pipe_create(&wb->stderr_pipe) != cupolas_OK) {
            if (wb->manager.redirect_stdin) {
                cupolas_pipe_close(&wb->stdin_pipe);
            }
            if (wb->manager.redirect_stdout) {
                cupolas_pipe_close(&wb->stdout_pipe);
            }
            return cupolas_ERROR_IO;
        }
    }
    
    return cupolas_OK;
}

static void close_pipes(workbench_t* wb) {
    if (wb->manager.redirect_stdin) {
        cupolas_pipe_close(&wb->stdin_pipe);
    }
    if (wb->manager.redirect_stdout) {
        cupolas_pipe_close(&wb->stdout_pipe);
    }
    if (wb->manager.redirect_stderr) {
        cupolas_pipe_close(&wb->stderr_pipe);
    }
}

static int read_output(workbench_t* wb) {
    char buf[OUTPUT_CHUNK_SIZE];
    size_t bytes_read;
    
    if (wb->manager.redirect_stdout && wb->stdout_buf) {
        while (true) {
            int ret = cupolas_pipe_read(&wb->stdout_pipe, buf, sizeof(buf), &bytes_read);
            if (ret != cupolas_OK || bytes_read == 0) break;
            
            if (wb->stdout_size + bytes_read < wb->stdout_capacity) {
                memcpy(wb->stdout_buf + wb->stdout_size, buf, bytes_read);
                wb->stdout_size += bytes_read;
            }
        }
    }
    
    if (wb->manager.redirect_stderr && wb->stderr_buf) {
        while (true) {
            int ret = cupolas_pipe_read(&wb->stderr_pipe, buf, sizeof(buf), &bytes_read);
            if (ret != cupolas_OK || bytes_read == 0) break;
            
            if (wb->stderr_size + bytes_read < wb->stderr_capacity) {
                memcpy(wb->stderr_buf + wb->stderr_size, buf, bytes_read);
                wb->stderr_size += bytes_read;
            }
        }
    }
    
    return cupolas_OK;
}

int workbench_execute(workbench_t* wb, const char* command, char* const argv[],
                      workbench_result_t* result) {
    if (!wb || !command) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&wb->lock);
    
    if (wb->state == WORKBENCH_STATE_RUNNING) {
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_BUSY;
    }
    
    wb->stdout_size = 0;
    wb->stderr_size = 0;
    if (wb->stdout_buf) wb->stdout_buf[0] = '\0';
    if (wb->stderr_buf) wb->stderr_buf[0] = '\0';
    
    if (create_pipes(wb) != cupolas_OK) {
        wb->state = WORKBENCH_STATE_ERROR;
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_IO;
    }
    
    cupolas_process_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.working_dir = wb->manager.working_dir;
    attr.env = wb->manager.env_vars;
    attr.redirect_stdin = wb->manager.redirect_stdin;
    attr.redirect_stdout = wb->manager.redirect_stdout;
    attr.redirect_stderr = wb->manager.redirect_stderr;
    
    if (wb->manager.redirect_stdin) {
        attr.stdin_pipe = wb->stdin_pipe;
    }
    if (wb->manager.redirect_stdout) {
        attr.stdout_pipe = wb->stdout_pipe;
    }
    if (wb->manager.redirect_stderr) {
        attr.stderr_pipe = wb->stderr_pipe;
    }
    
    wb->start_time_ms = cupolas_time_ms();
    
    int ret = cupolas_process_spawn(&wb->process, command, argv, &attr);
    if (ret != cupolas_OK) {
        close_pipes(wb);
        wb->state = WORKBENCH_STATE_ERROR;
        cupolas_mutex_unlock(&wb->lock);
        return ret;
    }
    
    wb->state = WORKBENCH_STATE_RUNNING;
    cupolas_mutex_unlock(&wb->lock);
    
    uint32_t timeout_ms = wb->manager.timeout_ms > 0 ? wb->manager.timeout_ms : 0;
    
    cupolas_exit_status_t status;
    ret = cupolas_process_wait(wb->process, &status, timeout_ms);
    
    cupolas_mutex_lock(&wb->lock);
    
    if (ret == cupolas_ERROR_TIMEOUT) {
        cupolas_process_terminate(wb->process, 9);
        cupolas_process_wait(wb->process, &status, 1000);
        
        read_output(wb);
        close_pipes(wb);
        cupolas_process_close(wb->process);
        
        if (result) {
            memset(result, 0, sizeof(workbench_result_t));
            result->exit_code = -1;
            result->timed_out = true;
            result->stdout_data = wb->stdout_buf ? cupolas_strdup(wb->stdout_buf) : NULL;
            result->stdout_size = wb->stdout_size;
            result->stderr_data = wb->stderr_buf ? cupolas_strdup(wb->stderr_buf) : NULL;
            result->stderr_size = wb->stderr_size;
            result->start_time_ms = wb->start_time_ms;
            result->end_time_ms = cupolas_time_ms();
        }
        
        wb->state = WORKBENCH_STATE_STOPPED;
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_TIMEOUT;
    }
    
    read_output(wb);
    close_pipes(wb);
    cupolas_process_close(wb->process);
    
    if (result) {
        memset(result, 0, sizeof(workbench_result_t));
        result->exit_code = status.code;
        result->signaled = status.signaled;
        result->signal = status.signal;
        result->stdout_data = wb->stdout_buf ? cupolas_strdup(wb->stdout_buf) : NULL;
        result->stdout_size = wb->stdout_size;
        result->stderr_data = wb->stderr_buf ? cupolas_strdup(wb->stderr_buf) : NULL;
        result->stderr_size = wb->stderr_size;
        result->start_time_ms = wb->start_time_ms;
        result->end_time_ms = cupolas_time_ms();
    }
    
    wb->state = WORKBENCH_STATE_IDLE;
    cupolas_mutex_unlock(&wb->lock);
    
    return cupolas_OK;
}

int workbench_execute_async(workbench_t* wb, const char* command, char* const argv[]) {
    if (!wb || !command) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&wb->lock);
    
    if (wb->state == WORKBENCH_STATE_RUNNING) {
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_BUSY;
    }
    
    wb->stdout_size = 0;
    wb->stderr_size = 0;
    
    if (create_pipes(wb) != cupolas_OK) {
        wb->state = WORKBENCH_STATE_ERROR;
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_IO;
    }
    
    cupolas_process_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.working_dir = wb->manager.working_dir;
    attr.env = wb->manager.env_vars;
    attr.redirect_stdin = wb->manager.redirect_stdin;
    attr.redirect_stdout = wb->manager.redirect_stdout;
    attr.redirect_stderr = wb->manager.redirect_stderr;
    
    wb->start_time_ms = cupolas_time_ms();
    
    int ret = cupolas_process_spawn(&wb->process, command, argv, &attr);
    if (ret != cupolas_OK) {
        close_pipes(wb);
        wb->state = WORKBENCH_STATE_ERROR;
        cupolas_mutex_unlock(&wb->lock);
        return ret;
    }
    
    wb->state = WORKBENCH_STATE_RUNNING;
    cupolas_mutex_unlock(&wb->lock);
    
    return cupolas_OK;
}

int workbench_wait(workbench_t* wb, workbench_result_t* result, uint32_t timeout_ms) {
    if (!wb) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&wb->lock);
    
    if (wb->state != WORKBENCH_STATE_RUNNING) {
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_INVALID_ARG;
    }
    
    cupolas_mutex_unlock(&wb->lock);
    
    cupolas_exit_status_t status;
    int ret = cupolas_process_wait(wb->process, &status, timeout_ms);
    
    cupolas_mutex_lock(&wb->lock);
    
    if (ret == cupolas_ERROR_TIMEOUT) {
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_TIMEOUT;
    }
    
    read_output(wb);
    close_pipes(wb);
    cupolas_process_close(wb->process);
    
    if (result) {
        memset(result, 0, sizeof(workbench_result_t));
        result->exit_code = status.code;
        result->signaled = status.signaled;
        result->signal = status.signal;
        result->stdout_data = wb->stdout_buf ? cupolas_strdup(wb->stdout_buf) : NULL;
        result->stdout_size = wb->stdout_size;
        result->stderr_data = wb->stderr_buf ? cupolas_strdup(wb->stderr_buf) : NULL;
        result->stderr_size = wb->stderr_size;
        result->start_time_ms = wb->start_time_ms;
        result->end_time_ms = cupolas_time_ms();
    }
    
    wb->state = WORKBENCH_STATE_IDLE;
    cupolas_mutex_unlock(&wb->lock);
    
    return cupolas_OK;
}

int workbench_terminate(workbench_t* wb) {
    if (!wb) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&wb->lock);
    
    if (wb->state != WORKBENCH_STATE_RUNNING) {
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_OK;
    }
    
    int ret = cupolas_process_terminate(wb->process, 9);
    
    cupolas_exit_status_t status;
    cupolas_process_wait(wb->process, &status, 1000);
    
    read_output(wb);
    close_pipes(wb);
    cupolas_process_close(wb->process);
    
    wb->state = WORKBENCH_STATE_STOPPED;
    cupolas_mutex_unlock(&wb->lock);
    
    return ret;
}

workbench_state_t workbench_get_state(workbench_t* wb) {
    if (!wb) return WORKBENCH_STATE_ERROR;
    
    cupolas_mutex_lock(&wb->lock);
    workbench_state_t state = wb->state;
    cupolas_mutex_unlock(&wb->lock);
    
    return state;
}

int64_t workbench_get_pid(workbench_t* wb) {
    if (!wb) return -1;
    
    cupolas_mutex_lock(&wb->lock);
    
    if (wb->state != WORKBENCH_STATE_RUNNING) {
        cupolas_mutex_unlock(&wb->lock);
        return -1;
    }
    
    cupolas_pid_t pid = cupolas_process_getpid(wb->process);
    cupolas_mutex_unlock(&wb->lock);
    
    return (int64_t)pid;
}

int workbench_write_stdin(workbench_t* wb, const void* data, size_t size, size_t* written) {
    if (!wb || !data || !written) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&wb->lock);
    
    if (wb->state != WORKBENCH_STATE_RUNNING || !wb->manager.redirect_stdin) {
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_INVALID_ARG;
    }
    
    int ret = cupolas_pipe_write(&wb->stdin_pipe, data, size, written);
    cupolas_mutex_unlock(&wb->lock);
    
    return ret;
}

int workbench_read_stdout(workbench_t* wb, void* buf, size_t size, size_t* read_size) {
    if (!wb || !buf || !read_size) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&wb->lock);
    
    if (!wb->manager.redirect_stdout) {
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_INVALID_ARG;
    }
    
    int ret = cupolas_pipe_read(&wb->stdout_pipe, buf, size, read_size);
    cupolas_mutex_unlock(&wb->lock);
    
    return ret;
}

int workbench_read_stderr(workbench_t* wb, void* buf, size_t size, size_t* read_size) {
    if (!wb || !buf || !read_size) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&wb->lock);
    
    if (!wb->manager.redirect_stderr) {
        cupolas_mutex_unlock(&wb->lock);
        return cupolas_ERROR_INVALID_ARG;
    }
    
    int ret = cupolas_pipe_read(&wb->stderr_pipe, buf, size, read_size);
    cupolas_mutex_unlock(&wb->lock);
    
    return ret;
}

void workbench_result_free(workbench_result_t* result) {
    if (!result) return;
    
    cupolas_mem_free(result->stdout_data);
    cupolas_mem_free(result->stderr_data);
    memset(result, 0, sizeof(workbench_result_t));
}
