#ifndef AGENTRT_COMPAT_DIRENT_H
#define AGENTRT_COMPAT_DIRENT_H

#ifdef _WIN32

#include "memory_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define AGENTRT_MAX_PATH 260

struct dirent {
    char d_name[AGENTRT_MAX_PATH];
};

typedef struct {
    HANDLE hFind;
    WIN32_FIND_DATAA ffd;
    struct dirent ent;
    int first;
} DIR;

static inline DIR *opendir(const char *name)
{
    DIR *dir = (DIR *)AGENTRT_MALLOC(sizeof(DIR));
    if (!dir)
        return NULL;

    char pattern[AGENTRT_MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", name);

    dir->hFind = FindFirstFileA(pattern, &dir->ffd);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        AGENTRT_FREE(dir);
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
    AGENTRT_STRNCPY_TERM(dir->ent.d_name, dir->ffd.cFileName, AGENTRT_MAX_PATH);
    dir->ent.d_name[AGENTRT_MAX_PATH - 1] = '\0';
    return &dir->ent;
}

static inline int closedir(DIR *dir)
{
    if (!dir)
        return -1;
    FindClose(dir->hFind);
    AGENTRT_FREE(dir);
    return 0;
}

#else

#include <dirent.h>

#endif

#endif