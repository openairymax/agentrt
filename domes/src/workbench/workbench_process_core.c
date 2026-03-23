/**
 * @file workbench_process_core.c
 * @brief 跨平台进程管理实现
 * @author Spharx
 * @date 2024
 */

#include "workbench_process.h"
#include <stdlib.h>
#include <string.h>

#if DOMES_PLATFORM_WINDOWS
#include <io.h>
#include <fcntl.h>
#include <process.h>

int domes_process_spawn(domes_process_t* proc, 
                        const char* path, 
                        char* const argv[],
                        const domes_process_attr_t* attr) {
    if (!proc || !path) return DOMES_ERROR_INVALID_ARG;
    
    HANDLE hStdinRead = NULL, hStdinWrite = NULL;
    HANDLE hStdoutRead = NULL, hStdoutWrite = NULL;
    HANDLE hStderrRead = NULL, hStderrWrite = NULL;
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    
    if (attr && attr->redirect_stdin) {
        if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
            return DOMES_ERROR_IO;
        }
        SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    }
    
    if (attr && attr->redirect_stdout) {
        if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
            if (hStdinRead) CloseHandle(hStdinRead);
            if (hStdinWrite) CloseHandle(hStdinWrite);
            return DOMES_ERROR_IO;
        }
        SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    }
    
    if (attr && attr->redirect_stderr) {
        if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
            if (hStdinRead) CloseHandle(hStdinRead);
            if (hStdinWrite) CloseHandle(hStdinWrite);
            if (hStdoutRead) CloseHandle(hStdoutRead);
            if (hStdoutWrite) CloseHandle(hStdoutWrite);
            return DOMES_ERROR_IO;
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
        return DOMES_ERROR_IO;
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
    return DOMES_OK;
}

int domes_process_wait(domes_process_t proc, domes_exit_status_t* status, uint32_t timeout_ms) {
    if (!proc || !status) return DOMES_ERROR_INVALID_ARG;
    
    DWORD wait_time = timeout_ms > 0 ? timeout_ms : INFINITE;
    DWORD result = WaitForSingleObject(proc, wait_time);
    
    if (result == WAIT_TIMEOUT) {
        return DOMES_ERROR_TIMEOUT;
    }
    
    if (result != WAIT_OBJECT_0) {
        return DOMES_ERROR_UNKNOWN;
    }
    
    DWORD exit_code;
    if (!GetExitCodeProcess(proc, &exit_code)) {
        return DOMES_ERROR_UNKNOWN;
    }
    
    status->code = (int)exit_code;
    status->signaled = false;
    status->signal = 0;
    
    return DOMES_OK;
}

int domes_process_terminate(domes_process_t proc, int signal) {
    (void)signal;
    if (!proc) return DOMES_ERROR_INVALID_ARG;
    
    return TerminateProcess(proc, 1) ? DOMES_OK : DOMES_ERROR_UNKNOWN;
}

int domes_process_close(domes_process_t proc) {
    if (!proc) return DOMES_ERROR_INVALID_ARG;
    
    CloseHandle(proc);
    return DOMES_OK;
}

domes_pid_t domes_process_getpid(domes_process_t proc) {
    if (!proc) return 0;
    return GetProcessId(proc);
}

#else

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

int domes_process_spawn(domes_process_t* proc, 
                        const char* path, 
                        char* const argv[],
                        const domes_process_attr_t* attr) {
    if (!proc || !path) return DOMES_ERROR_INVALID_ARG;
    
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    int stderr_pipe[2] = {-1, -1};
    
    if (attr && attr->redirect_stdin) {
        if (pipe(stdin_pipe) != 0) {
            return DOMES_ERROR_IO;
        }
    }
    
    if (attr && attr->redirect_stdout) {
        if (pipe(stdout_pipe) != 0) {
            if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
            if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
            return DOMES_ERROR_IO;
        }
    }
    
    if (attr && attr->redirect_stderr) {
        if (pipe(stderr_pipe) != 0) {
            if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
            if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
            if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
            if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
            return DOMES_ERROR_IO;
        }
    }
    
    pid_t pid = fork();
    
    if (pid < 0) {
        if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
        if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
        if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
        if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
        if (stderr_pipe[0] >= 0) close(stderr_pipe[0]);
        if (stderr_pipe[1] >= 0) close(stderr_pipe[1]);
        return DOMES_ERROR_UNKNOWN;
    }
    
    if (pid == 0) {
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
        
        execvp(path, argv);
        _exit(127);
    }
    
    if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
    if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);
    if (stderr_pipe[1] >= 0) close(stderr_pipe[1]);
    
    if (attr && attr->redirect_stdin) {
        attr->stdin_pipe[0] = stdin_pipe[1];
        attr->stdin_pipe[1] = -1;
    }
    if (attr && attr->redirect_stdout) {
        attr->stdout_pipe[0] = stdout_pipe[0];
        attr->stdout_pipe[1] = -1;
    }
    if (attr && attr->redirect_stderr) {
        attr->stderr_pipe[0] = stderr_pipe[0];
        attr->stderr_pipe[1] = -1;
    }
    
    *proc = pid;
    return DOMES_OK;
}

int domes_process_wait(domes_process_t proc, domes_exit_status_t* status, uint32_t timeout_ms) {
    if (!status) return DOMES_ERROR_INVALID_ARG;
    
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
                return DOMES_OK;
            } else if (result < 0) {
                return DOMES_ERROR_UNKNOWN;
            }
            
            usleep(100000);
            elapsed += 100;
        }
        return DOMES_ERROR_TIMEOUT;
    }
    
    int result = waitpid(proc, &status->code, options);
    if (result < 0) {
        return DOMES_ERROR_UNKNOWN;
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
    
    return DOMES_OK;
}

int domes_process_terminate(domes_process_t proc, int signal) {
    if (proc <= 0) return DOMES_ERROR_INVALID_ARG;
    
    return kill(proc, signal > 0 ? signal : SIGTERM) == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
}

int domes_process_close(domes_process_t proc) {
    (void)proc;
    return DOMES_OK;
}

domes_pid_t domes_process_getpid(domes_process_t proc) {
    return proc;
}

#endif
