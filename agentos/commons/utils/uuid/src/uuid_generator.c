/**
 * @file uuid_generator.c
 * @brief UUID v4 生成器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "uuid_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <rpc.h>
#pragma comment(lib, "rpcrt4.lib")
#elif defined(__linux__)
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <uuid/uuid.h>
#endif

/* Unified base library compatibility layer */
#include "../../memory/include/memory_compat.h"
#include "../../string/include/string_compat.h"

static int g_uuid_initialized = 0;
static uint64_t g_uuid_counter = 0;

agentos_uuid_error_t agentos_uuid_init(void) {
    if (g_uuid_initialized) {
        return AGENTOS_UUID_SUCCESS;
    }

#if defined(_WIN32) || defined(_WIN64)
    RPC_STATUS status = UuidCreateSequential(NULL);
    if (status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY) {
        return AGENTOS_UUID_EUNAVAIL;
    }
#elif defined(__linux__) || defined(__APPLE__)
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return AGENTOS_UUID_EUNAVAIL;
    }
    close(fd);
#endif

    g_uuid_counter = (uint64_t)time(NULL) ^ 0x123456789ABCDEF0ULL;
    g_uuid_initialized = 1;
    return AGENTOS_UUID_SUCCESS;
}

void agentos_uuid_cleanup(void) {
    g_uuid_initialized = 0;
    g_uuid_counter = 0;
}

agentos_uuid_error_t agentos_uuid_v4(char* out_buf, size_t buf_len) {
    if (!out_buf || buf_len < AGENTOS_UUID_STR_LEN) {
        return AGENTOS_UUID_EINVALID;
    }

    if (!g_uuid_initialized) {
        agentos_uuid_error_t err = agentos_uuid_init();
        if (err != AGENTOS_UUID_SUCCESS) {
            return err;
        }
    }

    uint8_t uuid[16];

#if defined(_WIN32) || defined(_WIN64)
    UUID uuid_win;
    if (UuidCreate(&uuid_win) != RPC_S_OK) {
        return AGENTOS_UUID_EUNAVAIL;
    }
    memcpy(uuid, &uuid_win, 16);

#elif defined(__linux__)
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return AGENTOS_UUID_EUNAVAIL;
    }
    ssize_t n = read(fd, uuid, 16);
    close(fd);
    if (n != 16) {
        return AGENTOS_UUID_EUNAVAIL;
    }

#elif defined(__APPLE__)
    uuid_t uuid_mac;
    uuid_generate(uuid_mac);
    memcpy(uuid, uuid_mac, 16);

#else
    srand((unsigned int)(time(NULL) ^ (uintptr_t)&uuid));
    for (int i = 0; i < 16; i++) {
        uuid[i] = (unsigned char)(rand() & 0xFF);
    }
#endif

    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    snprintf(out_buf, buf_len,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5],
        uuid[6], uuid[7],
        uuid[8], uuid[9],
        uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

    g_uuid_counter++;
    return AGENTOS_UUID_SUCCESS;
}

agentos_uuid_error_t agentos_uuid_with_prefix(const char* prefix, char* out_buf, size_t buf_len) {
    if (!prefix || !out_buf || buf_len < AGENTOS_UUID_PREFIXED_STR_LEN) {
        return AGENTOS_UUID_EINVALID;
    }

    size_t prefix_len = strlen(prefix);
    if (prefix_len >= buf_len) {
        return AGENTOS_UUID_EINVALID;
    }

    char uuid_str[AGENTOS_UUID_STR_LEN];
    agentos_uuid_error_t err = agentos_uuid_v4(uuid_str, sizeof(uuid_str));
    if (err != AGENTOS_UUID_SUCCESS) {
        return err;
    }

    snprintf(out_buf, buf_len, "%s%s", prefix, uuid_str);
    return AGENTOS_UUID_SUCCESS;
}

int agentos_uuid_is_valid(const char* uuid) {
    if (!uuid) {
        return 0;
    }

    size_t len = strlen(uuid);
    if (len != 36 && len != 38) {
        return 0;
    }

    const char* p = uuid;
    if (p[0] == '{') {
        p++;
    }

    for (int i = 0; i < 36; i++) {
        char c = p[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') {
                return 0;
            }
        } else {
            if (!((c >= '0' && c <= '9') ||
                  (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F'))) {
                return 0;
            }
        }
    }

    if (p[36] == '}') {
        return 1;
    }

    return (len == 36);
}

agentos_uuid_error_t agentos_uuid_bin_to_str(const uint8_t* uuid_bin, char* out_buf, size_t buf_len) {
    if (!uuid_bin || !out_buf || buf_len < AGENTOS_UUID_STR_LEN) {
        return AGENTOS_UUID_EINVALID;
    }

    snprintf(out_buf, buf_len,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid_bin[0], uuid_bin[1], uuid_bin[2], uuid_bin[3],
        uuid_bin[4], uuid_bin[5],
        uuid_bin[6], uuid_bin[7],
        uuid_bin[8], uuid_bin[9],
        uuid_bin[10], uuid_bin[11], uuid_bin[12], uuid_bin[13], uuid_bin[14], uuid_bin[15]);

    return AGENTOS_UUID_SUCCESS;
}

agentos_uuid_error_t agentos_uuid_str_to_bin(const char* uuid_str, uint8_t* out_bin) {
    if (!uuid_str || !out_bin) {
        return AGENTOS_UUID_EINVALID;
    }

    if (!agentos_uuid_is_valid(uuid_str)) {
        return AGENTOS_UUID_EINVALID;
    }

    const char* p = uuid_str;
    if (p[0] == '{') {
        p++;
    }

    int idx = 0;
    for (int i = 0; i < 36; i++) {
        if (p[i] == '-') {
            continue;
        }

        char byte_str[3] = {p[i], p[i + 1], 0};
        out_bin[idx++] = (uint8_t)strtol(byte_str, NULL, 16);
        i++;
    }

    return AGENTOS_UUID_SUCCESS;
}
