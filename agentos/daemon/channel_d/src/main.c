#include "channel_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static volatile int g_running = 1;
static channel_service_t* g_svc = NULL;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

__attribute__((used))
static int handle_service_request(const char* method,
                                   const char* params_json,
                                   char** response_json,
                                   void* user_data)
{
    channel_service_t* svc = (channel_service_t*)user_data;
    if (!svc || !method || !response_json) return -1;

    if (strcmp(method, "ping") == 0) {
        *response_json = strdup("{\"status\":\"ok\"}");
        return 0;
    }

    if (strcmp(method, "list") == 0) {
        channel_info_t info_list[CHANNEL_MAX_CHANNELS];
        size_t count = 0;
        int rc = channel_service_list(svc, info_list, CHANNEL_MAX_CHANNELS, &count);
        if (rc != 0) {
            *response_json = strdup("{\"error\":\"list failed\"}");
            return -1;
        }

        size_t buf_size = 4096 + count * 1024;
        char* buf = (char*)malloc(buf_size);
        if (!buf) return -1;

        size_t pos = 0;
        pos += snprintf(buf + pos, buf_size - pos, "{\"channels\":[");
        for (size_t i = 0; i < count; i++) {
            if (i > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
            pos += snprintf(buf + pos, buf_size - pos,
                "{\"id\":\"%s\",\"name\":\"%s\",\"type\":%d,\"status\":%d,\"sent\":%zu,\"recv\":%zu}",
                info_list[i].channel_id, info_list[i].name,
                info_list[i].type, info_list[i].status,
                info_list[i].messages_sent, info_list[i].messages_received);
        }
        pos += snprintf(buf + pos, buf_size - pos, "]}");
        *response_json = buf;
        return 0;
    }

    if (strcmp(method, "open") == 0) {
        const char* id_start = strstr(params_json, "\"id\"");
        const char* name_start = strstr(params_json, "\"name\"");
        const char* type_start = strstr(params_json, "\"type\"");

        if (!id_start || !name_start) {
            *response_json = strdup("{\"error\":\"missing id or name\"}");
            return -1;
        }

        char id[128] = {0}, name[256] = {0};
        const char* p = strchr(id_start + 4, '"');
        if (p) { p++; const char* e = strchr(p, '"'); if (e) { size_t l = (size_t)(e-p); if (l>127) l=127; memcpy(id,p,l); }}

        p = strchr(name_start + 6, '"');
        if (p) { p++; const char* e = strchr(p, '"'); if (e) { size_t l = (size_t)(e-p); if (l>255) l=255; memcpy(name,p,l); }}

        channel_type_t type = CHANNEL_TYPE_SOCKET;
        if (type_start) {
            p = strchr(type_start + 6, ':');
            if (p) {
                int t = atoi(p + 1);
                if (t >= 0 && t <= 2) type = (channel_type_t)t;
            }
        }

        int rc = channel_service_open(svc, id, name, type, NULL);
        if (rc != 0) {
            char err[256];
            snprintf(err, sizeof(err), "{\"error\":\"open failed: %d\"}", rc);
            *response_json = strdup(err);
            return -1;
        }

        *response_json = strdup("{\"status\":\"opened\"}");
        return 0;
    }

    if (strcmp(method, "close") == 0) {
        const char* id_start = strstr(params_json, "\"id\"");
        if (!id_start) {
            *response_json = strdup("{\"error\":\"missing id\"}");
            return -1;
        }
        char id[128] = {0};
        const char* p = strchr(id_start + 4, '"');
        if (p) { p++; const char* e = strchr(p, '"'); if (e) { size_t l = (size_t)(e-p); if (l>127) l=127; memcpy(id,p,l); }}

        int rc = channel_service_close(svc, id);
        if (rc != 0) {
            *response_json = strdup("{\"error\":\"close failed\"}");
            return -1;
        }
        *response_json = strdup("{\"status\":\"closed\"}");
        return 0;
    }

    if (strcmp(method, "send") == 0) {
        const char* id_start = strstr(params_json, "\"id\"");
        const char* data_start = strstr(params_json, "\"data\"");
        if (!id_start || !data_start) {
            *response_json = strdup("{\"error\":\"missing id or data\"}");
            return -1;
        }
        char id[128] = {0};
        const char* p = strchr(id_start + 4, '"');
        if (p) { p++; const char* e = strchr(p, '"'); if (e) { size_t l = (size_t)(e-p); if (l>127) l=127; memcpy(id,p,l); }}

        p = strchr(data_start + 6, '"');
        if (p) {
            p++;
            const char* e = strchr(p, '"');
            size_t dlen = e ? (size_t)(e - p) : strlen(p);
            int rc = channel_service_send(svc, id, p, dlen);
            if (rc != 0) {
                char err[256];
                snprintf(err, sizeof(err), "{\"error\":\"send failed: %d\"}", rc);
                *response_json = strdup(err);
                return -1;
            }
        }
        *response_json = strdup("{\"status\":\"sent\"}");
        return 0;
    }

    if (strcmp(method, "health") == 0) {
        bool healthy = channel_service_is_healthy(svc);
        *response_json = strdup(healthy ? "{\"healthy\":true}" : "{\"healthy\":false}");
        return 0;
    }

    *response_json = strdup("{\"error\":\"unknown method\"}");
    return -1;
}

int main(int argc, char* argv[])
{
    const char* socket_dir = NULL;
    uint32_t max_channels = CHANNEL_MAX_CHANNELS;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            i++;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            socket_dir = argv[++i];
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_channels = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: channel_d [-c config] [-s socket_dir] [-n max_channels] [-h]\n");
            return 0;
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    config.max_channels = max_channels;
    if (socket_dir) {
        strncpy(config.socket_dir, socket_dir, sizeof(config.socket_dir) - 1);
    }

    channel_service_t* svc = channel_service_create(&config);
    if (!svc) {
        fprintf(stderr, "Failed to create channel service\n");
        return 1;
    }

    if (channel_service_start(svc) != 0) {
        fprintf(stderr, "Failed to start channel service\n");
        channel_service_destroy(svc);
        return 1;
    }

    fprintf(stdout, "channel_d started (max_channels=%u, socket_dir=%s)\n",
            config.max_channels, config.socket_dir);

    while (g_running) {
        sleep(1);
    }

    fprintf(stdout, "channel_d shutting down\n");
    channel_service_stop(svc);
    channel_service_destroy(svc);
    return 0;
}
