// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file unistd.h
 * @brief Windows compatibility shim for POSIX <unistd.h>
 *
 * Provides Windows equivalents for common POSIX unistd functions/types
 * used across the AgentOS commons module.
 */

#ifndef AGENTOS_COMPAT_UNISTD_H
#define AGENTOS_COMPAT_UNISTD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <process.h>
#include <direct.h>
#include <fcntl.h>

/* Standard file descriptors */
#ifndef STDIN_FILENO
#define STDIN_FILENO  0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO  1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO  2
#endif

/* File access modes */
#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 1
#endif

/* Process ID type and getter */
#ifndef pid_t
#define pid_t int
#endif
#define getpid()    ((pid_t)GetCurrentProcessId())
#define getuid()    ((uid_t)0)
#define geteuid()   ((uid_t)0)
#define getppid()   ((pid_t)0)

/* Signed size type */
#ifndef ssize_t
#define ssize_t SSIZE_T
#endif

/* Sleep functions */
#define usleep(us)  Sleep(((us) + 999) / 1000)
#define sleep(s)    Sleep((s) * 1000)

/* Path and working directory */
#define chdir(d)    _chdir(d)
#define getcwd(b,s) _getcwd(b,s)

/* File operations */
#define read(f,b,c)   _read(f,b,c)
#define write(f,b,c)  _write(f,b,c)
#define close(f)      _close(f)
#define access(p,m)   _access(p,m)
#define unlink(p)     _unlink(p)
#define rmdir(p)      _rmdir(p)
#define lseek(f,o,w)  _lseek(f,o,w)
#define isatty(f)     _isatty(f)

/* Process execution */
#define execv(p,a)    _execv(p,(const char* const*)a)
#define execvp(f,a)   _execvp(f,(const char* const*)a)
#define execl(p,a,...) _execl(p,a,__VA_ARGS__)

/* Pipe */
#define pipe(fds)     _pipe(fds, 4096, _O_BINARY)

/* Environment */
#ifndef environ
#define environ _environ
#endif

/* Configurable system variables (sysconf stubs) */
#define _SC_PAGESIZE            1
#define _SC_NPROCESSORS_ONLN    2
#define _SC_OPEN_MAX            3
#define _SC_CLK_TCK             4

long sysconf(int name);

/* Hostname (declared by winsock2.h on Windows) */
#define HOST_NAME_MAX 256

/* Cryptographically-secure random (fallback to BCryptGenRandom via platform.c) */
#include <stdlib.h>
long agentos_platform_getentropy(void* buf, size_t len);
#ifndef getentropy
#define getentropy(buf, len) agentos_platform_getentropy(buf, len)
#endif

/* Read/write link (not available on Windows) */
#define readlink(p,b,s)  (-1)
#define symlink(o,n)     (-1)

/* Truncate */
#define ftruncate(f,l)   _chsize(f,l)
#define truncate(p,l)    (-1)

/* Fork / exec stubs (not available on Windows) */
#define fork()           (-1)
#define setsid()         (-1)

/* File sync */
#define fsync(f)         _commit(f)
#define fdatasync(f)     _commit(f)

/* Terminal */
#define ttyname(f)       (NULL)
#define ttyname_r(f,b,s) (-1)

/* User info types */
#ifndef uid_t
#define uid_t int
#endif
#ifndef gid_t
#define gid_t int
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMPAT_UNISTD_H */