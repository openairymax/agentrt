#ifndef AGENTOS_COMPAT_DIRENT_H
#define AGENTOS_COMPAT_DIRENT_H

#ifdef _WIN32

#include "memory_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define AGENTOS_MAX_PATH 260

struct dirent {
    char d_name[AGENTOS_MAX_PATH];
};

typedef struct {
    HANDLE hFind;
    WIN32_FIND_DATAA ffd;
    struct dirent ent;
    int first;
} DIR;

static inline DIR *opendir(const char *name)
{
    DIR *dir = (DIR *)AGENTOS_MALLOC(sizeof(DIR));
    if (!dir)
        return NULL;

    char pattern[AGENTOS_MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", name);

    dir->hFind = FindFirstFileA(pattern, &dir->ffd);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        AGENTOS_FREE(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

static inline struct dirent *readdir(DIR *dir)
{
    if (!dir)
        return NULL;
    if (!dir->first) {
        if (!FindNextFileA(dir->hFind, &dir->ffd))
            return NULL;
    }
    dir->first = 0;
    AGENTOS_STRNCPY_TERM(dir->ent.d_name, dir->ffd.cFileName, AGENTOS_MAX_PATH);
    dir->ent.d_name[AGENTOS_MAX_PATH - 1] = '\0';
    return &dir->ent;
}

static inline int closedir(DIR *dir)
{
    if (!dir)
        return -1;
    FindClose(dir->hFind);
    AGENTOS_FREE(dir);
    return 0;
}

#else

#include <dirent.h>

#endif

#endif