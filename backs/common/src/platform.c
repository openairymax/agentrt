/**
 * @file platform.c
 * @brief 跨平台兼容层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(AGENTOS_PLATFORM_WINDOWS)
    #include <io.h>
    #include <direct.h>
    #include <process.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <errno.h>
#endif

/* ==================== 线程实现 ==================== */

#if defined(AGENTOS_PLATFORM_WINDOWS)

int agentos_thread_create(agentos_thread_t* thread, void* (*func)(void*), void* arg) {
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    if (h == NULL) {
        return (int)GetLastError();
    }
    *thread = h;
    return 0;
}

int agentos_thread_join(agentos_thread_t thread, void** retval) {
    (void)retval;
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return -1;
    }
    CloseHandle(thread);
    return 0;
}

int agentos_thread_detach(agentos_thread_t thread) {
    CloseHandle(thread);
    return 0;
}

agentos_thread_t agentos_thread_self(void) {
    return GetCurrentThread();
}

#else

int agentos_thread_create(agentos_thread_t* thread, void* (*func)(void*), void* arg) {
    return pthread_create(thread, NULL, func, arg);
}

int agentos_thread_join(agentos_thread_t thread, void** retval) {
    return pthread_join(thread, retval);
}

int agentos_thread_detach(agentos_thread_t thread) {
    return pthread_detach(thread);
}

agentos_thread_t agentos_thread_self(void) {
    return pthread_self();
}

#endif

/* ==================== 互斥锁实现 ==================== */

#if defined(AGENTOS_PLATFORM_WINDOWS)

int agentos_mutex_init(agentos_mutex_t* mutex) {
    InitializeCriticalSection(mutex);
    return 0;
}

int agentos_mutex_lock(agentos_mutex_t* mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    return TryEnterCriticalSection(mutex) ? 0 : -1;
}

int agentos_mutex_unlock(agentos_mutex_t* mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

int agentos_mutex_destroy(agentos_mutex_t* mutex) {
    DeleteCriticalSection(mutex);
    return 0;
}

#else

int agentos_mutex_init(agentos_mutex_t* mutex) {
    return pthread_mutex_init(mutex, NULL);
}

int agentos_mutex_lock(agentos_mutex_t* mutex) {
    return pthread_mutex_lock(mutex);
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    return pthread_mutex_trylock(mutex);
}

int agentos_mutex_unlock(agentos_mutex_t* mutex) {
    return pthread_mutex_unlock(mutex);
}

int agentos_mutex_destroy(agentos_mutex_t* mutex) {
    return pthread_mutex_destroy(mutex);
}

#endif

/* ==================== 条件变量实现 ==================== */

#if defined(AGENTOS_PLATFORM_WINDOWS)

int agentos_cond_init(agentos_cond_t* cond) {
    InitializeConditionVariable(cond);
    return 0;
}

int agentos_cond_wait(agentos_cond_t* cond, agentos_mutex_t* mutex) {
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
}

int agentos_cond_timedwait(agentos_cond_t* cond, agentos_mutex_t* mutex, uint32_t timeout_ms) {
    return SleepConditionVariableCS(cond, mutex, timeout_ms) ? 0 : -1;
}

int agentos_cond_signal(agentos_cond_t* cond) {
    WakeConditionVariable(cond);
    return 0;
}

int agentos_cond_broadcast(agentos_cond_t* cond) {
    WakeAllConditionVariable(cond);
    return 0;
}

int agentos_cond_destroy(agentos_cond_t* cond) {
    (void)cond;
    return 0;
}

#else

int agentos_cond_init(agentos_cond_t* cond) {
    return pthread_cond_init(cond, NULL);
}

int agentos_cond_wait(agentos_cond_t* cond, agentos_mutex_t* mutex) {
    return pthread_cond_wait(cond, mutex);
}

int agentos_cond_timedwait(agentos_cond_t* cond, agentos_mutex_t* mutex, uint32_t timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    return pthread_cond_timedwait(cond, mutex, &ts);
}

int agentos_cond_signal(agentos_cond_t* cond) {
    return pthread_cond_signal(cond);
}

int agentos_cond_broadcast(agentos_cond_t* cond) {
    return pthread_cond_broadcast(cond);
}

int agentos_cond_destroy(agentos_cond_t* cond) {
    return pthread_cond_destroy(cond);
}

#endif

/* ==================== Socket 实现 ==================== */

int agentos_socket_init(void) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

void agentos_socket_cleanup(void) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    WSACleanup();
#endif
}

agentos_socket_t agentos_socket_create(int domain, int type, int protocol) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    return socket(domain, type, protocol);
#else
    return socket(domain, type, protocol);
#endif
}

int agentos_socket_connect(agentos_socket_t sock, const char* host, uint16_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
#if defined(AGENTOS_PLATFORM_WINDOWS)
    addr.sin_addr.s_addr = inet_addr(host);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        struct hostent* he = gethostbyname(host);
        if (he == NULL) {
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
#else
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        return -1;
    }
#endif
    
    return connect(sock, (struct sockaddr*)&addr, sizeof(addr));
}

int agentos_socket_bind(agentos_socket_t sock, const char* addr, uint16_t port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    
    if (addr == NULL || strlen(addr) == 0) {
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
#if defined(AGENTOS_PLATFORM_WINDOWS)
        sa.sin_addr.s_addr = inet_addr(addr);
#else
        inet_pton(AF_INET, addr, &sa.sin_addr);
#endif
    }
    
    return bind(sock, (struct sockaddr*)&sa, sizeof(sa));
}

int agentos_socket_listen(agentos_socket_t sock, int backlog) {
    return listen(sock, backlog);
}

agentos_socket_t agentos_socket_accept(agentos_socket_t sock) {
    return accept(sock, NULL, NULL);
}

int agentos_socket_set_nonblock(agentos_socket_t sock) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

int agentos_socket_set_reuseaddr(agentos_socket_t sock) {
    int opt = 1;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
}

int agentos_socket_close(agentos_socket_t sock) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    return closesocket(sock);
#else
    return close(sock);
#endif
}

int agentos_socket_read(agentos_socket_t sock, void* buf, size_t len) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    return (int)recv(sock, (char*)buf, (int)len, 0);
#else
    return (int)read(sock, buf, len);
#endif
}

int agentos_socket_write(agentos_socket_t sock, const void* buf, size_t len) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    return (int)send(sock, (const char*)buf, (int)len, 0);
#else
    return (int)write(sock, buf, len);
#endif
}

/* ==================== 管道实现 ==================== */

int agentos_pipe_create(agentos_pipe_t* pipe) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    if (!CreatePipe(&pipe->read_fd, &pipe->write_fd, &sa, 0)) {
        return (int)GetLastError();
    }
    return 0;
#else
    int fds[2];
    int ret = pipe(fds);
    if (ret == 0) {
        pipe->read_fd = fds[0];
        pipe->write_fd = fds[1];
    }
    return ret;
#endif
}

int agentos_pipe_read(agentos_pipe_t* pipe, void* buf, size_t len) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    DWORD bytes_read;
    if (!ReadFile(pipe->read_fd, buf, (DWORD)len, &bytes_read, NULL)) {
        return -1;
    }
    return (int)bytes_read;
#else
    return (int)read(pipe->read_fd, buf, len);
#endif
}

int agentos_pipe_write(agentos_pipe_t* pipe, const void* buf, size_t len) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    DWORD bytes_written;
    if (!WriteFile(pipe->write_fd, buf, (DWORD)len, &bytes_written, NULL)) {
        return -1;
    }
    return (int)bytes_written;
#else
    return (int)write(pipe->write_fd, buf, len);
#endif
}

int agentos_pipe_close_read(agentos_pipe_t* pipe) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    return CloseHandle(pipe->read_fd) ? 0 : -1;
#else
    return close(pipe->read_fd);
#endif
}

int agentos_pipe_close_write(agentos_pipe_t* pipe) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    return CloseHandle(pipe->write_fd) ? 0 : -1;
#else
    return close(pipe->write_fd);
#endif
}

/* ==================== 进程实现 ==================== */

#if defined(AGENTOS_PLATFORM_WINDOWS)

int agentos_process_create(const char* executable, char* const argv[],
                           const char* working_dir,
                           agentos_pipe_t* stdin_pipe,
                           agentos_pipe_t* stdout_pipe,
                           agentos_pipe_t* stderr_pipe,
                           agentos_process_t* proc) {
    memset(proc, 0, sizeof(agentos_process_t));
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE hStdinRead = NULL, hStdinWrite = NULL;
    HANDLE hStdoutRead = NULL, hStdoutWrite = NULL;
    HANDLE hStderrRead = NULL, hStderrWrite = NULL;
    
    if (stdin_pipe) {
        if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
            return -1;
        }
        SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    }
    
    if (stdout_pipe) {
        if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
            if (hStdinRead) CloseHandle(hStdinRead);
            if (hStdinWrite) CloseHandle(hStdinWrite);
            return -1;
        }
        SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    }
    
    if (stderr_pipe) {
        if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0)) {
            if (hStdinRead) CloseHandle(hStdinRead);
            if (hStdinWrite) CloseHandle(hStdinWrite);
            if (hStdoutRead) CloseHandle(hStdoutRead);
            if (hStdoutWrite) CloseHandle(hStdoutWrite);
            return -1;
        }
        SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);
    }
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdinRead ? hStdinRead : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hStdoutWrite ? hStdoutWrite : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = hStderrWrite ? hStderrWrite : GetStdHandle(STD_ERROR_HANDLE);
    
    memset(&pi, 0, sizeof(pi));
    
    char cmdline[4096] = {0};
    snprintf(cmdline, sizeof(cmdline), "\"%s\"", executable);
    for (int i = 1; argv && argv[i]; i++) {
        snprintf(cmdline + strlen(cmdline), sizeof(cmdline) - strlen(cmdline),
                 " \"%s\"", argv[i]);
    }
    
    BOOL success = CreateProcessA(
        NULL,
        cmdline,
        NULL,
        NULL,
        TRUE,
        0,
        NULL,
        working_dir,
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
        return -1;
    }
    
    proc->process_handle = pi.hProcess;
    proc->thread_handle = pi.hThread;
    proc->pid = pi.dwProcessId;
    
    if (stdin_pipe) {
        stdin_pipe->write_fd = hStdinWrite;
    }
    if (stdout_pipe) {
        stdout_pipe->read_fd = hStdoutRead;
    }
    if (stderr_pipe) {
        stderr_pipe->read_fd = hStderrRead;
    }
    
    return 0;
}

int agentos_process_wait(agentos_process_t* proc, uint32_t timeout_ms, int* exit_code) {
    DWORD result = WaitForSingleObject(proc->process_handle, timeout_ms);
    if (result == WAIT_TIMEOUT) {
        return -2;
    }
    if (result != WAIT_OBJECT_0) {
        return -1;
    }
    
    DWORD code;
    if (!GetExitCodeProcess(proc->process_handle, &code)) {
        return -1;
    }
    
    if (exit_code) {
        *exit_code = (int)code;
    }
    
    return 0;
}

int agentos_process_kill(agentos_process_t* proc) {
    return TerminateProcess(proc->process_handle, 1) ? 0 : -1;
}

void agentos_process_close_pipes(agentos_process_t* proc) {
    if (proc->process_handle) {
        CloseHandle(proc->process_handle);
        proc->process_handle = NULL;
    }
    if (proc->thread_handle) {
        CloseHandle(proc->thread_handle);
        proc->thread_handle = NULL;
    }
}

#else

int agentos_process_create(const char* executable, char* const argv[],
                           const char* working_dir,
                           agentos_pipe_t* stdin_pipe,
                           agentos_pipe_t* stdout_pipe,
                           agentos_pipe_t* stderr_pipe,
                           agentos_process_t* proc) {
    memset(proc, 0, sizeof(agentos_process_t));
    
    int stdin_fds[2] = {-1, -1};
    int stdout_fds[2] = {-1, -1};
    int stderr_fds[2] = {-1, -1};
    
    if (stdin_pipe && pipe(stdin_fds) < 0) {
        return -1;
    }
    if (stdout_pipe && pipe(stdout_fds) < 0) {
        if (stdin_fds[0] >= 0) close(stdin_fds[0]);
        if (stdin_fds[1] >= 0) close(stdin_fds[1]);
        return -1;
    }
    if (stderr_pipe && pipe(stderr_fds) < 0) {
        if (stdin_fds[0] >= 0) close(stdin_fds[0]);
        if (stdin_fds[1] >= 0) close(stdin_fds[1]);
        if (stdout_fds[0] >= 0) close(stdout_fds[0]);
        if (stdout_fds[1] >= 0) close(stdout_fds[1]);
        return -1;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        if (stdin_fds[0] >= 0) close(stdin_fds[0]);
        if (stdin_fds[1] >= 0) close(stdin_fds[1]);
        if (stdout_fds[0] >= 0) close(stdout_fds[0]);
        if (stdout_fds[1] >= 0) close(stdout_fds[1]);
        if (stderr_fds[0] >= 0) close(stderr_fds[0]);
        if (stderr_fds[1] >= 0) close(stderr_fds[1]);
        return -1;
    }
    
    if (pid == 0) {
        if (stdin_pipe) {
            close(stdin_fds[1]);
            dup2(stdin_fds[0], STDIN_FILENO);
            close(stdin_fds[0]);
        }
        if (stdout_pipe) {
            close(stdout_fds[0]);
            dup2(stdout_fds[1], STDOUT_FILENO);
            close(stdout_fds[1]);
        }
        if (stderr_pipe) {
            close(stderr_fds[0]);
            dup2(stderr_fds[1], STDERR_FILENO);
            close(stderr_fds[1]);
        }
        
        if (working_dir) {
            chdir(working_dir);
        }
        
        execvp(executable, argv);
        _exit(127);
    }
    
    proc->pid = pid;
    
    if (stdin_pipe) {
        close(stdin_fds[0]);
        stdin_pipe->write_fd = stdin_fds[1];
    }
    if (stdout_pipe) {
        close(stdout_fds[1]);
        stdout_pipe->read_fd = stdout_fds[0];
    }
    if (stderr_pipe) {
        close(stderr_fds[1]);
        stderr_pipe->read_fd = stderr_fds[0];
    }
    
    return 0;
}

int agentos_process_wait(agentos_process_t* proc, uint32_t timeout_ms, int* exit_code) {
    int status;
    int options = 0;
    
    if (timeout_ms > 0) {
        sigset_t mask;
        sigset_t old_mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &old_mask);
        
        struct timespec ts;
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        
        siginfo_t info;
        int ret = sigtimedwait(&mask, &info, &ts);
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
        
        if (ret < 0) {
            return -2;
        }
    }
    
    pid_t result = waitpid(proc->pid, &status, options);
    if (result < 0) {
        return -1;
    }
    if (result == 0) {
        return -2;
    }
    
    if (WIFEXITED(status)) {
        if (exit_code) {
            *exit_code = WEXITSTATUS(status);
        }
        return 0;
    } else if (WIFSIGNALED(status)) {
        if (exit_code) {
            *exit_code = -WTERMSIG(status);
        }
        return 0;
    }
    
    return -1;
}

int agentos_process_kill(agentos_process_t* proc) {
    return kill(proc->pid, SIGKILL);
}

void agentos_process_close_pipes(agentos_process_t* proc) {
    (void)proc;
}

#endif

/* ==================== 时间接口 ==================== */

uint64_t agentos_time_ns(void) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

uint64_t agentos_time_ms(void) {
    return agentos_time_ns() / 1000000ULL;
}

/* ==================== 随机数接口 ==================== */

static AGENTOS_THREAD_LOCAL unsigned int g_random_seed = 0;
static AGENTOS_THREAD_LOCAL int g_random_initialized = 0;

void agentos_random_init(void) {
    if (!g_random_initialized) {
        g_random_seed = (unsigned int)agentos_time_ns();
        g_random_initialized = 1;
    }
}

uint32_t agentos_random_uint32(uint32_t min, uint32_t max) {
    if (!g_random_initialized) {
        agentos_random_init();
    }
    
#if defined(AGENTOS_PLATFORM_WINDOWS)
    return min + (uint32_t)((double)rand() / (RAND_MAX + 1.0) * (max - min + 1));
#else
    return min + (uint32_t)((double)rand_r(&g_random_seed) / (RAND_MAX + 1.0) * (max - min + 1));
#endif
}

float agentos_random_float(void) {
    if (!g_random_initialized) {
        agentos_random_init();
    }
    
#if defined(AGENTOS_PLATFORM_WINDOWS)
    return (float)rand() / (float)RAND_MAX;
#else
    return (float)rand_r(&g_random_seed) / (float)RAND_MAX;
#endif
}

int agentos_random_bytes(void* buf, size_t len) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    NTSTATUS status = BCryptGenRandom(
        NULL,
        (PUCHAR)buf,
        (ULONG)len,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    return status == 0 ? 0 : -1;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, (char*)buf + total, len - total);
        if (n <= 0) {
            close(fd);
            return -1;
        }
        total += (size_t)n;
    }
    
    close(fd);
    return 0;
#endif
}

/* ==================== 文件系统接口 ==================== */

int agentos_file_exists(const char* path) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    return _access(path, 0) == 0 ? 1 : 0;
#else
    return access(path, F_OK) == 0 ? 1 : 0;
#endif
}

int agentos_mkdir_p(const char* path) {
    char* tmp = strdup(path);
    if (!tmp) return -1;
    
    size_t len = strlen(tmp);
    if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) {
        tmp[len - 1] = '\0';
    }
    
    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            
#if defined(AGENTOS_PLATFORM_WINDOWS)
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            
            *p = saved;
        }
    }
    
#if defined(AGENTOS_PLATFORM_WINDOWS)
    int ret = _mkdir(tmp);
#else
    int ret = mkdir(tmp, 0755);
#endif
    
    free(tmp);
    return (ret == 0 || errno == EEXIST) ? 0 : -1;
}

int64_t agentos_file_size(const char* path) {
#if defined(AGENTOS_PLATFORM_WINDOWS)
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return -1;
    }
    return st.st_size;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return st.st_size;
#endif
}

/* ==================== 字符串工具 ==================== */

char* agentos_strlcpy(char* dest, size_t dest_size, const char* src) {
    if (dest_size == 0) {
        return dest;
    }
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    
    return dest;
}

char* agentos_strlcat(char* dest, size_t dest_size, const char* src) {
    if (dest_size == 0) {
        return dest;
    }
    
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) {
        return dest;
    }
    
    size_t src_len = strlen(src);
    size_t remaining = dest_size - dest_len - 1;
    size_t copy_len = (src_len < remaining) ? src_len : remaining;
    
    memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';
    
    return dest;
}
