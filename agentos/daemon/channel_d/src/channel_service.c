#include "channel_service.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

typedef struct {
    channel_info_t info;
    int socket_fd;
    void* shm_ptr;
    size_t shm_size;
    char shm_name[128];
    int shm_fd;
    channel_message_cb_t callback;
    void* callback_user_data;
    uint8_t* recv_buffer;
    size_t recv_buffer_size;
    size_t recv_buffer_used;
} channel_entry_t;

struct channel_service {
    channel_config_t config;
    channel_entry_t channels[CHANNEL_MAX_CHANNELS];
    size_t channel_count;
    bool running;
    bool healthy;
    uint64_t total_messages_sent;
    uint64_t total_messages_received;
};

static uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static channel_entry_t* find_channel(channel_service_t* svc, const char* channel_id)
{
    for (size_t i = 0; i < svc->channel_count; i++) {
        if (strcmp(svc->channels[i].info.channel_id, channel_id) == 0) {
            return &svc->channels[i];
        }
    }
    return NULL;
}

static int create_socket_channel(channel_entry_t* entry, const char* endpoint)
{
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, endpoint, sizeof(addr.sun_path) - 1);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    unlink(endpoint);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 128) < 0) {
        close(fd);
        unlink(endpoint);
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    entry->socket_fd = fd;
    return 0;
}

static int create_shm_channel(channel_entry_t* entry, const char* endpoint)
{
    (void)endpoint;
    snprintf(entry->shm_name, sizeof(entry->shm_name), "%s%s",
             "/agentos_ch_", entry->info.channel_id);

    size_t shm_size = entry->info.buffer_size > 0 ? entry->info.buffer_size : 65536;

    int fd = shm_open(entry->shm_name, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return -1;

    if (ftruncate(fd, (off_t)shm_size) < 0) {
        close(fd);
        shm_unlink(entry->shm_name);
        return -1;
    }

    void* ptr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        close(fd);
        shm_unlink(entry->shm_name);
        return -1;
    }

    memset(ptr, 0, shm_size);

    entry->shm_fd = fd;
    entry->shm_ptr = ptr;
    entry->shm_size = shm_size;
    return 0;
}

static void destroy_channel(channel_entry_t* entry)
{
    if (entry->socket_fd >= 0) {
        close(entry->socket_fd);
        if (entry->info.type == CHANNEL_TYPE_SOCKET && entry->info.endpoint[0]) {
            unlink(entry->info.endpoint);
        }
        entry->socket_fd = -1;
    }

    if (entry->shm_ptr && entry->shm_ptr != MAP_FAILED) {
        munmap(entry->shm_ptr, entry->shm_size);
        entry->shm_ptr = NULL;
    }
    if (entry->shm_fd >= 0) {
        close(entry->shm_fd);
        entry->shm_fd = -1;
    }
    if (entry->shm_name[0]) {
        shm_unlink(entry->shm_name);
        entry->shm_name[0] = '\0';
    }

    if (entry->recv_buffer) {
        free(entry->recv_buffer);
        entry->recv_buffer = NULL;
    }
    entry->recv_buffer_size = 0;
    entry->recv_buffer_used = 0;
}

channel_service_t* channel_service_create(const channel_config_t* config)
{
    channel_service_t* svc = (channel_service_t*)calloc(1, sizeof(channel_service_t));
    if (!svc) return NULL;

    if (config) {
        svc->config = *config;
    } else {
        channel_config_t defaults = CHANNEL_CONFIG_DEFAULTS;
        svc->config = defaults;
    }

    for (size_t i = 0; i < CHANNEL_MAX_CHANNELS; i++) {
        svc->channels[i].socket_fd = -1;
        svc->channels[i].shm_fd = -1;
    }

    svc->healthy = true;
    return svc;
}

void channel_service_destroy(channel_service_t* svc)
{
    if (!svc) return;

    if (svc->running) {
        channel_service_stop(svc);
    }

    for (size_t i = 0; i < svc->channel_count; i++) {
        destroy_channel(&svc->channels[i]);
    }

    free(svc);
}

int channel_service_start(channel_service_t* svc)
{
    if (!svc) return -1;
    if (svc->running) return 0;

    if (svc->config.socket_dir[0]) {
        mkdir(svc->config.socket_dir, 0755);
    }

    svc->running = true;
    svc->healthy = true;
    return 0;
}

int channel_service_stop(channel_service_t* svc)
{
    if (!svc || !svc->running) return -1;

    for (size_t i = 0; i < svc->channel_count; i++) {
        destroy_channel(&svc->channels[i]);
    }
    svc->channel_count = 0;
    svc->running = false;
    return 0;
}

int channel_service_open(channel_service_t* svc,
                          const char* channel_id,
                          const char* name,
                          channel_type_t type,
                          const char* endpoint)
{
    if (!svc || !channel_id || !name) return -1;
    if (svc->channel_count >= svc->config.max_channels) return -2;
    if (find_channel(svc, channel_id)) return -3;

    channel_entry_t* entry = &svc->channels[svc->channel_count];
    memset(entry, 0, sizeof(channel_entry_t));
    entry->socket_fd = -1;
    entry->shm_fd = -1;

    strncpy(entry->info.channel_id, channel_id, sizeof(entry->info.channel_id) - 1);
    strncpy(entry->info.name, name, sizeof(entry->info.name) - 1);
    entry->info.type = type;
    entry->info.status = CHANNEL_STATUS_OPEN;
    entry->info.buffer_size = svc->config.default_buffer_size;

    if (endpoint) {
        strncpy(entry->info.endpoint, endpoint, sizeof(entry->info.endpoint) - 1);
    } else {
        if (type == CHANNEL_TYPE_SOCKET) {
            snprintf(entry->info.endpoint, sizeof(entry->info.endpoint),
                     "%s/%s.sock", svc->config.socket_dir, channel_id);
        } else if (type == CHANNEL_TYPE_SHM) {
            snprintf(entry->info.endpoint, sizeof(entry->info.endpoint),
                     "%s%s", svc->config.shm_prefix, channel_id);
        }
    }

    entry->info.created_at = get_time_ms();
    entry->info.last_activity = entry->info.created_at;

    int rc = 0;
    switch (type) {
        case CHANNEL_TYPE_SOCKET:
            rc = create_socket_channel(entry, entry->info.endpoint);
            break;
        case CHANNEL_TYPE_SHM:
            rc = create_shm_channel(entry, entry->info.endpoint);
            break;
        case CHANNEL_TYPE_PIPE:
            if (entry->info.endpoint[0]) {
                rc = mkfifo(entry->info.endpoint, 0666);
                if (rc < 0 && errno != EEXIST) rc = -1; else rc = 0;
            }
            break;
        default:
            rc = -1;
            break;
    }

    if (rc < 0) {
        return -4;
    }

    entry->recv_buffer_size = entry->info.buffer_size;
    entry->recv_buffer = (uint8_t*)calloc(1, entry->recv_buffer_size);
    if (!entry->recv_buffer) {
        destroy_channel(entry);
        return -5;
    }

    svc->channel_count++;
    return 0;
}

int channel_service_close(channel_service_t* svc, const char* channel_id)
{
    if (!svc || !channel_id) return -1;

    for (size_t i = 0; i < svc->channel_count; i++) {
        if (strcmp(svc->channels[i].info.channel_id, channel_id) == 0) {
            destroy_channel(&svc->channels[i]);
            if (i < svc->channel_count - 1) {
                svc->channels[i] = svc->channels[svc->channel_count - 1];
                memset(&svc->channels[svc->channel_count - 1], 0, sizeof(channel_entry_t));
                svc->channels[svc->channel_count - 1].socket_fd = -1;
                svc->channels[svc->channel_count - 1].shm_fd = -1;
            }
            svc->channel_count--;
            return 0;
        }
    }
    return -2;
}

int channel_service_send(channel_service_t* svc,
                           const char* channel_id,
                           const void* data,
                           size_t data_len)
{
    if (!svc || !channel_id || !data || data_len == 0) return -1;

    channel_entry_t* entry = find_channel(svc, channel_id);
    if (!entry || entry->info.status != CHANNEL_STATUS_OPEN) return -2;

    entry->info.last_activity = get_time_ms();

    switch (entry->info.type) {
        case CHANNEL_TYPE_SOCKET: {
            if (entry->socket_fd < 0) return -3;
            struct sockaddr_un client_addr;
            memset(&client_addr, 0, sizeof(client_addr));
            client_addr.sun_family = AF_UNIX;
            strncpy(client_addr.sun_path, entry->info.endpoint, sizeof(client_addr.sun_path) - 1);
            client_addr.sun_path[sizeof(client_addr.sun_path) - 1] = '\0';
            int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (client_fd < 0) return -3;
            if (connect(client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
                close(client_fd);
                return -3;
            }
            uint32_t net_len = htonl((uint32_t)data_len);
            ssize_t w1 = write(client_fd, &net_len, sizeof(net_len));
            ssize_t w2 = write(client_fd, data, data_len);
            close(client_fd);
            if (w1 < 0 || w2 < 0) return -3;
            break;
        }
        case CHANNEL_TYPE_SHM: {
            if (!entry->shm_ptr) return -3;
            size_t header_size = sizeof(uint32_t) * 2;
            if (data_len + header_size > entry->shm_size) return -4;
            volatile uint32_t* msg_len = (volatile uint32_t*)entry->shm_ptr;
            volatile uint32_t* msg_flag = (volatile uint32_t*)((char*)entry->shm_ptr + sizeof(uint32_t));
            *msg_len = (uint32_t)data_len;
            memcpy((char*)entry->shm_ptr + header_size, data, data_len);
            __sync_synchronize();
            *msg_flag = 1;
            break;
        }
        case CHANNEL_TYPE_PIPE: {
            if (entry->info.endpoint[0]) {
                int fd = open(entry->info.endpoint, O_WRONLY | O_NONBLOCK);
                if (fd < 0) return -3;
                uint32_t net_len = htonl((uint32_t)data_len);
                if (write(fd, &net_len, sizeof(net_len)) < 0) { close(fd); return -3; }
                if (write(fd, data, data_len) < 0) { close(fd); return -3; }
                close(fd);
            }
            break;
        }
        default:
            return -3;
    }

    entry->info.messages_sent++;
    svc->total_messages_sent++;

    if (entry->callback) {
        entry->callback(channel_id, data, data_len, entry->callback_user_data);
    }

    return 0;
}

int channel_service_receive(channel_service_t* svc,
                              const char* channel_id,
                              void** out_data,
                              size_t* out_len)
{
    if (!svc || !channel_id || !out_data || !out_len) return -1;

    channel_entry_t* entry = find_channel(svc, channel_id);
    if (!entry || entry->info.status != CHANNEL_STATUS_OPEN) return -2;

    entry->info.last_activity = get_time_ms();

    switch (entry->info.type) {
        case CHANNEL_TYPE_SOCKET: {
            if (entry->socket_fd < 0) return -3;
            int client_fd = accept(entry->socket_fd, NULL, NULL);
            if (client_fd < 0) return -3;
            uint32_t net_len = 0;
            ssize_t r1 = read(client_fd, &net_len, sizeof(net_len));
            if (r1 <= 0) { close(client_fd); return -3; }
            uint32_t msg_len = ntohl(net_len);
            if (msg_len == 0 || msg_len > svc->config.default_buffer_size) {
                close(client_fd);
                return -4;
            }
            void* buf = malloc(msg_len);
            if (!buf) { close(client_fd); return -5; }
            ssize_t r2 = read(client_fd, buf, msg_len);
            close(client_fd);
            if (r2 <= 0) { free(buf); return -3; }
            *out_data = buf;
            *out_len = (size_t)r2;
            break;
        }
        case CHANNEL_TYPE_SHM: {
            if (!entry->shm_ptr) return -3;
            volatile uint32_t* msg_len = (volatile uint32_t*)entry->shm_ptr;
            volatile uint32_t* msg_flag = (volatile uint32_t*)((char*)entry->shm_ptr + sizeof(uint32_t));
            __sync_synchronize();
            if (*msg_flag != 1) return 0;
            uint32_t len = *msg_len;
            if (len == 0 || len > entry->shm_size - sizeof(uint32_t) * 2) return -4;
            void* buf = malloc(len);
            if (!buf) return -5;
            memcpy(buf, (char*)entry->shm_ptr + sizeof(uint32_t) * 2, len);
            __sync_synchronize();
            *msg_flag = 0;
            *out_data = buf;
            *out_len = len;
            break;
        }
        case CHANNEL_TYPE_PIPE: {
            if (entry->info.endpoint[0]) {
                int fd = open(entry->info.endpoint, O_RDONLY | O_NONBLOCK);
                if (fd < 0) return 0;
                uint32_t net_len = 0;
                ssize_t r1 = read(fd, &net_len, sizeof(net_len));
                if (r1 <= 0) { close(fd); return 0; }
                uint32_t msg_len = ntohl(net_len);
                if (msg_len == 0) { close(fd); return 0; }
                void* buf = malloc(msg_len);
                if (!buf) { close(fd); return -5; }
                ssize_t r2 = read(fd, buf, msg_len);
                close(fd);
                if (r2 <= 0) { free(buf); return 0; }
                *out_data = buf;
                *out_len = (size_t)r2;
            } else {
                return 0;
            }
            break;
        }
        default:
            return -3;
    }

    entry->info.messages_received++;
    svc->total_messages_received++;
    return 0;
}

int channel_service_list(channel_service_t* svc,
                           channel_info_t* out_list,
                           size_t list_capacity,
                           size_t* out_count)
{
    if (!svc || !out_list || !out_count) return -1;

    size_t count = svc->channel_count;
    if (count > list_capacity) count = list_capacity;

    for (size_t i = 0; i < count; i++) {
        out_list[i] = svc->channels[i].info;
    }

    *out_count = count;
    return 0;
}

int channel_service_get_info(channel_service_t* svc,
                               const char* channel_id,
                               channel_info_t* out_info)
{
    if (!svc || !channel_id || !out_info) return -1;

    channel_entry_t* entry = find_channel(svc, channel_id);
    if (!entry) return -2;

    *out_info = entry->info;
    return 0;
}

int channel_service_set_callback(channel_service_t* svc,
                                   const char* channel_id,
                                   channel_message_cb_t callback,
                                   void* user_data)
{
    if (!svc || !channel_id) return -1;

    channel_entry_t* entry = find_channel(svc, channel_id);
    if (!entry) return -2;

    entry->callback = callback;
    entry->callback_user_data = user_data;
    return 0;
}

bool channel_service_is_healthy(channel_service_t* svc)
{
    if (!svc) return false;
    return svc->healthy;
}
