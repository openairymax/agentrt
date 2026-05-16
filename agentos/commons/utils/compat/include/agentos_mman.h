#ifndef AGENTOS_COMPAT_MMAN_H
#define AGENTOS_COMPAT_MMAN_H

#ifdef _WIN32

#include <windows.h>
#include <io.h>
#include <fcntl.h>

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED    ((void*)-1)

static inline void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset) {
    (void)addr;
    (void)offset;

    if (fd != -1) {
        HANDLE hFile = (HANDLE)_get_osfhandle(fd);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD flProtect = PAGE_READONLY;
            if (prot & PROT_WRITE) flProtect = PAGE_READWRITE;
            HANDLE hMap = CreateFileMappingA(hFile, NULL, flProtect, 0, (DWORD)len, NULL);
            if (!hMap) return MAP_FAILED;
            DWORD dwAccess = FILE_MAP_READ;
            if (prot & PROT_WRITE) dwAccess = FILE_MAP_ALL_ACCESS;
            void* ret = MapViewOfFile(hMap, dwAccess, 0, 0, len);
            CloseHandle(hMap);
            return ret ? ret : MAP_FAILED;
        }
        return MAP_FAILED;
    }

    if (flags & MAP_ANONYMOUS) {
        DWORD flAlloc = MEM_RESERVE | MEM_COMMIT;
        DWORD flProt = PAGE_READONLY;
        if (prot & PROT_WRITE) flProt = PAGE_READWRITE;
        if (prot & PROT_EXEC) flProt = PAGE_EXECUTE_READWRITE;
        return VirtualAlloc(NULL, len, flAlloc, flProt);
    }

    return MAP_FAILED;
}

static inline int munmap(void* addr, size_t len) {
    (void)len;
    if (!addr || addr == MAP_FAILED) return -1;
    return VirtualFree(addr, 0, MEM_RELEASE) ? 0 : -1;
}

static inline int mprotect(void* addr, size_t len, int prot) {
    DWORD flNew = PAGE_READONLY;
    if (prot & PROT_WRITE) flNew = PAGE_READWRITE;
    if (prot & PROT_EXEC) flNew = (prot & PROT_WRITE) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
    DWORD old;
    return VirtualProtect(addr, len, flNew, &old) ? 0 : -1;
}

static inline int shm_open(const char* name, int oflag, mode_t mode) {
    (void)name;
    (void)oflag;
    (void)mode;
    return -1;
}

static inline int shm_unlink(const char* name) {
    (void)name;
    return -1;
}

#else

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#endif

#endif