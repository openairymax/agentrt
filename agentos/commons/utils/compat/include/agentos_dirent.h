#ifndef AGENTOS_COMPAT_DIRENT_H
#define AGENTOS_COMPAT_DIRENT_H

#ifdef _WIN32

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

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

static inline DIR* opendir(const char* name) {
    DIR* dir = (DIR*)malloc(sizeof(DIR));
    if (!dir) return NULL;

    char pattern[AGENTOS_MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", name);

    dir->hFind = FindFirstFileA(pattern, &dir->ffd);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

static inline struct dirent* readdir(DIR* dir) {
    if (!dir) return NULL;
    if (!dir->first) {
        if (!FindNextFileA(dir->hFind, &dir->ffd)) return NULL;
    }
    dir->first = 0;
    strncpy(dir->ent.d_name, dir->ffd.cFileName, AGENTOS_MAX_PATH - 1);
    dir->ent.d_name[AGENTOS_MAX_PATH - 1] = '\0';
    return &dir->ent;
}

static inline int closedir(DIR* dir) {
    if (!dir) return -1;
    FindClose(dir->hFind);
    free(dir);
    return 0;
}

#else

#include <dirent.h>

#endif

#endif