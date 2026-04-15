/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * workbench_process_core.c - Cross-platform Process Management Implementation
 */

/**
 * @file workbench_process_core.c
 * @brief Cross-platform Process Management Implementation
 * @author Spharx AgentOS Team
 * @date 2024
 */

#include "workbench_process.h"
#include "utils/cupolas_utils.h"
#include <stdlib.h>
#include <string.h>

#if cupolas_PLATFORM_WINDOWS
#include <io.h>
#include <fcntl.h>
#include <process.h>

int cupolas_process_spawn(cupolas_process_t* proc, 
                        const char* path, 
                        char* const argv[],
                        const cupolas_process_attr_t* attr) {
    if (!proc || !path) return cupolas_ERROR_INVALID_ARG;
    
    HANDLE hStdinRead = NULL, hStdinWrite = NULL;
    HANDLE hStdoutRead = NULL, hStdoutWrite = NULL;
    HANDLE hStderrRead = NULL, hStderrWrite = NULL;
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    
    if (attr && attr->redirect_stdin) {
        if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
            return cupolas_ERROR_IO;
        }
        SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    }
    
    if (attr && attr->redirect_stdout) {
        if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
            if (hStdinRead) CloseHandle(hStdinRead);
            if (hStdinWrite) CloseHandle(hStdinWrite);
            return cupolas_ERROR_IO;
        }
        SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    }
    
    if (attr && attr->redirect_stderr) {
        if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
            if (hStdinRead) CloseHandle(hStdinRead);
            if (hStdinWrite) CloseHandle(hStdinWrite);
            if (hStdoutRead) CloseHandle(hStdoutRead);
            if (hStdoutWrite) CloseHandle(hStdoutWrite);
            return cupolas_ERROR_IO;
        }
        SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
    }
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    
    si.hStdInput = hStdinRead ? hStdinRead : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hStdoutWrite ? hStdoutWrite : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = hStderrWrite ? hStderrWrite : GetStdHandle(STD_ERROR_HANDLE);
    
    char cmdline[32768];
    size_t len = 0;
    cmdline[0] = '\0';
    
    const char* p = path;
    while (*p && len < sizeof(cmdline) - 2) {
        if (*p == ' ' || *p == '\t') {
            cmdline[len++] = '"';
            while (*p && len < sizeof(cmdline) - 1) {
                cmdline[len++] = *p++;
            }
            cmdline[len++] = '"';
        } else {
            cmdline[len++] = *p++;
        }
    }
    
    if (argv) {
        for (int i = 0; argv[i] && len < sizeof(cmdline) - 2; i++) {
            cmdline[len++] = ' ';
            const char* arg = argv[i];
            int need_quote = (strchr(arg, ' ') || strchr(arg, '\t'));
            if (need_quote) cmdline[len++] = '"';
            while (*arg && len < sizeof(cmdline) - 1) {
                cmdline[len++] = *arg++;
            }
            if (need_quote) cmdline[len++] = '"';
        }
    }
    cmdline[len] = '\0';
    
    DWORD creationFlags = CREATE_NO_WINDOW;
    BOOL success = CreateProcessA(
        NULL,
        cmdline,
        NULL,
        NULL,
        TRUE,
        creationFlags,
        NULL,
        attr && attr->working_dir ? attr->working_dir : NULL,
        &si,
        &pi
    );
    
    if (hStdinRead) CloseHandle(hStdinRead);
    if (hStdoutWrite) CloseHandle(hStdoutWrite);
    if (hStderrWrite) CloseHandle(hStderrWrite);
    
    if (!success) {
        if (hStdinWrite) CloseHandle(hStdinWrite);
        if (hStdoutRead) CloseHandle(hStdoutRead);
        if (hStderrRead) CloseHandle(hStderrRead);
        return cupolas_ERROR_IO;
    }
    
    CloseHandle(pi.hThread);
    
    if (attr && attr->redirect_stdin) {
        attr->stdin_pipe[0] = NULL;
        attr->stdin_pipe[1] = hStdinWrite;
    }
    if (attr && attr->redirect_stdout) {
        attr->stdout_pipe[0] = hStdoutRead;
        attr->stdout_pipe[1] = NULL;
    }
    if (attr && attr->redirect_stderr) {
        attr->stderr_pipe[0] = hStderrRead;
        attr->stderr_pipe[1] = NULL;
    }
    
    *proc = pi.hProcess;
    return cupolas_OK;
}

int cupolas_process_wait(cupolas_process_t proc, cupolas_exit_status_t* status, uint32_t timeout_ms) {
    if (!proc || !status) return cupolas_ERROR_INVALID_ARG;
    
    DWORD wait_time = timeout_ms > 0 ? timeout_ms : INFINITE;
    DWORD result = WaitForSingleObject(proc, wait_time);
    
    if (result == WAIT_TIMEOUT) {
        return cupolas_ERROR_TIMEOUT;
    }
    
    if (result != WAIT_OBJECT_0) {
        return cupolas_ERROR_UNKNOWN;
    }
    
    DWORD exit_code;
    if (!GetExitCodeProcess(proc, &exit_code)) {
        return cupolas_ERROR_UNKNOWN;
    }
    
    status->code = (int)exit_code;
    status->signaled = false;
    status->signal = 0;
    
    return cupolas_OK;
}

int cupolas_process_terminate(cupolas_process_t proc, int signal) {
    (void)signal;
    if (!proc) return cupolas_ERROR_INVALID_ARG;
    
    return TerminateProcess(proc, 1) ? cupolas_OK : cupolas_ERROR_UNKNOWN;
}

int cupolas_process_close(cupolas_process_t proc) {
    if (!proc) return cupolas_ERROR_INVALID_ARG;
    
    CloseHandle(proc);
    return cupolas_OK;
}

cupolas_pid_t cupolas_process_getpid(cupolas_process_t proc) {
    if (!proc) return 0;
    return GetProcessId(proc);
}

#else

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

/**
 * @brief 创建标准输入输出重定向管道 (POSIX)
 */
static int create_redirect_pipes_posix(
    int* stdin_pipe, int* stdout_pipe, int* stderr_pipe,
    const cupolas_process_attr_t* attr) {
    
    if (attr && attr->redirect_stdin) {
        if (pipe(stdin_pipe) != 0) {
            return cupolas_ERROR_IO;
        }
    }
    
    if (attr && attr->redirect_stdout) {
        if (pipe(stdout_pipe) != 0) {
            if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
            if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
            return cupolas_ERROR_IO;
        }
    }
    
    if (attr && attr->redirect_stderr) {
        if (pipe(stderr_pipe) != 0) {
            if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
            if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
            if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
            if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
            return cupolas_ERROR_IO;
        }
    }
    
    return cupolas_OK;
}

/**
 * @brief 清理管道文件描述符
 */
static void cleanup_pipes_posix(int* stdin_pipe, int* stdout_pipe, int* stderr_pipe,
                               int close_stdin_read, int close_stdout_write, int close_stderr_write) {
    if (close_stdin_read && stdin_pipe[0] >= 0) close(stdin_pipe[0]);
    if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
    if (close_stdout_write && stdout_pipe[1] >= 0) close(stdout_pipe[1]);
    if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
    if (close_stderr_write && stderr_pipe[1] >= 0) close(stderr_pipe[1]);
    if (stderr_pipe[0] >= 0) close(stderr_pipe[0]);
}

/**
 * @brief 子进程设置函数
 */
static void setup_child_process(int* stdin_pipe, int* stdout_pipe, int* stderr_pipe,
                               const cupolas_process_attr_t* attr,
                               const char* path, char* const argv[]) {
    if (attr && attr->working_dir) {
        if (chdir(attr->working_dir) != 0) {
            _exit(127);
        }
    }

    if (stdin_pipe[0] >= 0) {
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
    }

    if (stdout_pipe[1] >= 0) {
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
    }

    if (stderr_pipe[1] >= 0) {
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
    }

    for (int fd = 3; fd < 1024; fd++) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
    }

    execvp(path ? path : NULL, argv ? argv : NULL);
    _exit(127);
}

int cupolas_process_spawn(cupolas_process_t* proc, 
                        const char* path, 
                        char* const argv[],
                        const cupolas_process_attr_t* attr) {
    if (!proc || !path) return cupolas_ERROR_INVALID_ARG;
    
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    
    int err = create_redirect_pipes_posix(stdin_pipe, stdout_pipe, stderr_pipe, attr);
    if (err != cupolas_OK) {
        return err;
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        cleanup_pipes_posix(stdin_pipe, stdout_pipe, stderr_pipe, 1, 1, 1);
        return cupolas_ERROR_UNKNOWN;
    }
    
    if (pid == 0) {
        setup_child_process(stdin_pipe, stdout_pipe, stderr_pipe, attr, path, argv);
    }
    
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    
    if (attr) {
        if (attr->redirect_stdin && stdin_pipe[1] >= 0) {
            close(stdin_pipe[1]);
        }
        if (attr->redirect_stdout && stdout_pipe[0] >= 0) {
            close(stdout_pipe[0]);
        }
        if (attr->redirect_stderr && stderr_pipe[0] >= 0) {
            close(stderr_pipe[0]);
        }
    }
    
    *proc = pid;
    return cupolas_OK;
}

int cupolas_process_wait(cupolas_process_t proc, cupolas_exit_status_t* status, uint32_t timeout_ms) {
    if (!status) return cupolas_ERROR_INVALID_ARG;
    
    int options = 0;
    
    if (timeout_ms > 0) {
        int elapsed = 0;
        while (elapsed < (int)timeout_ms) {
            int result = waitpid(proc, &status->code, WNOHANG);
            if (result > 0) {
                if (WIFEXITED(status->code)) {
                    status->code = WEXITSTATUS(status->code);
                    status->signaled = false;
                    status->signal = 0;
                } else if (WIFSIGNALED(status->code)) {
                    status->signal = WTERMSIG(status->code);
                    status->signaled = true;
                    status->code = -1;
                }
                return cupolas_OK;
            } else if (result < 0) {
                return cupolas_ERROR_UNKNOWN;
            }
            
            usleep(100000);
            elapsed += 100;
        }
        return cupolas_ERROR_TIMEOUT;
    }
    
    int result = waitpid(proc, &status->code, options);
    if (result < 0) {
        return cupolas_ERROR_UNKNOWN;
    }
    
    if (WIFEXITED(status->code)) {
        status->code = WEXITSTATUS(status->code);
        status->signaled = false;
        status->signal = 0;
    } else if (WIFSIGNALED(status->code)) {
        status->signal = WTERMSIG(status->code);
        status->signaled = true;
        status->code = -1;
    }
    
    return cupolas_OK;
}

int cupolas_process_terminate(cupolas_process_t proc, int signal) {
    if (proc <= 0) return cupolas_ERROR_INVALID_ARG;
    
    return kill(proc, signal > 0 ? signal : SIGTERM) == 0 ? cupolas_OK : cupolas_ERROR_UNKNOWN;
}

int cupolas_process_close(cupolas_process_t proc) {
    (void)proc;
    return cupolas_OK;
}

cupolas_pid_t cupolas_process_getpid(cupolas_process_t proc) {
    return proc;
}

#endif
