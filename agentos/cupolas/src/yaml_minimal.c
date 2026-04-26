/**
 * @file yaml_minimal.c
 * @brief Minimal YAML 1.1 subset parser - production-grade implementation
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "yaml_minimal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#define INITIAL_NODE_CAPACITY 64
#define MAX_LINE_LEN 8192
#define MAX_DEPTH 64
#define MAX_KEY_LEN 256

struct parse_ctx {
    const char* src;
    size_t len;
    size_t pos;
    int line;
    int line_pos;
    yaml_document_t* doc;
};

static struct yaml_node* alloc_node(yaml_document_t* doc, yaml_node_type_t type) {
    if (doc->node_count >= doc->node_capacity) {
        size_t new_cap = doc->node_capacity * 2;
        if (new_cap == 0) new_cap = INITIAL_NODE_CAPACITY;
        struct yaml_node** new_nodes = (struct yaml_node**)realloc(
            doc->all_nodes, sizeof(struct yaml_node*) * new_cap);
        if (!new_nodes) return NULL;
        doc->all_nodes = new_nodes;
        doc->node_capacity = new_cap;
    }
    struct yaml_node* node = (struct yaml_node*)calloc(1, sizeof(struct yaml_node));
    if (!node) return NULL;
    node->type = type;
    node->line = 0;
    doc->all_nodes[doc->node_count++] = node;
    return node;
}

static void free_node(struct yaml_node* node) {
    if (!node) return;
    switch (node->type) {
        case YAML_NODE_SCALAR:
            free(node->scalar.value);
            break;
        case YAML_NODE_MAPPING:
            if (node->mapping) {
                for (size_t i = 0; i < yaml_size(node); i++) {
                    free(node->mapping[i].key);
                }
                free(node->mapping);
            }
            break;
        case YAML_NODE_SEQUENCE:
            if (node->sequence.items) {
                free(node->sequence.items);
            }
            break;
        default: break;
    }
}

yaml_document_t* yaml_create(void) {
    yaml_document_t* doc = (yaml_document_t*)calloc(1, sizeof(yaml_document_t));
    return doc;
}

void yaml_destroy(yaml_document_t* doc) {
    if (!doc) return;
    for (size_t i = 0; i < doc->node_count; i++) {
        free_node(doc->all_nodes[i]);
    }
    free(doc->all_nodes);
    free(doc->source);
    free(doc->error_msg);
    free(doc);
}

static void set_error(struct parse_ctx* ctx, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    free(ctx->doc->error_msg);
    ctx->doc->error_msg = strdup(buf);
}

static char peek(struct parse_ctx* ctx) {
    if (ctx->pos >= ctx->len) return '\0';
    return ctx->src[ctx->pos];
}

static char advance(struct parse_ctx* ctx) {
    if (ctx->pos >= ctx->len) return '\0';
    char c = ctx->src[ctx->pos++];
    if (c == '\n') { ctx->line++; ctx->line_pos = 0; } else { ctx->line_pos++; }
    return c;
}

static bool at_end(struct parse_ctx* ctx) {
    return ctx->pos >= ctx->len;
}

static void skip_ws(struct parse_ctx* ctx) {
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t') advance(ctx);
        else break;
    }
}

static void skip_ws_and_nl(struct parse_ctx* ctx) {
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') advance(ctx);
        else break;
    }
}

static int count_indent(struct parse_ctx* ctx) {
    int indent = 0;
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ') { indent++; advance(ctx); }
        else if (c == '\t') { indent += 2; advance(ctx); }
        else break;
    }
    return indent;
}

static bool is_plain_scalar_char(char c) {
    switch (c) {
        case ':': case '{': case '}': case '[': case ']':
        case ',': case '&': case '*': case '#': case '|':
        case '>': case '"': case '\'': case '%': case '@':
        case '`': case '\n': case '\r': return false;
        default: return !isspace((unsigned char)c);
    }
}

static char* parse_quoted_string(struct parse_ctx* ctx, char quote) {
    advance(ctx);
    size_t cap = 128;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    while (!at_end(ctx)) {
        char c = advance(ctx);
        if (c == quote && (len == 0 || buf[len-1] != '\\')) break;
        if (c == '\\' && !at_end(ctx)) {
            char next = advance(ctx);
            switch (next) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '\\': c = '\\'; break;
                case '\'': c = '\''; break;
                case '"': c = '"'; break;
                default: /* keep both */ break;
            }
        }
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char*)realloc(buf, cap);
            if (!buf) return NULL;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return buf;
}

static char* parse_plain_scalar(struct parse_ctx* ctx, int end_indent) {
    size_t cap = 256;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    while (!at_end(ctx)) {
        char c = peek(ctx);

        if (c == ':' && (ctx->pos + 1 < ctx->len &&
            (ctx->src[ctx->pos + 1] == ' ' || ctx->src[ctx->pos + 1] == '\n'))) break;
        if (c == '#' && len > 0 && (len == 0 || buf[len-1] == ' ')) break;
        if (c == '\n' || c == '\r') break;
        if (!is_plain_scalar_char(c)) break;

        advance(ctx);
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char*)realloc(buf, cap);
            if (!buf) return NULL;
        }
        buf[len++] = c;
    }

    while (len > 0 && (buf[len-1] == ' ' || buf[len-1] == '\t')) len--;
    buf[len] = '\0';

    if (len == 0) { free(buf); return strdup(""); }
    return buf;
}

static struct yaml_node* parse_value(struct parse_ctx* ctx, int base_indent);

static struct yaml_node* parse_sequence(struct parse_ctx* ctx, int base_indent) {
    struct yaml_node* seq = alloc_node(ctx->doc, YAML_NODE_SEQUENCE);
    if (!seq) return NULL;

    size_t cap = 8;
    seq->sequence.items = (struct yaml_sequence_item*)calloc(cap, sizeof(struct yaml_sequence_item));
    seq->sequence.count = 0;
    if (!seq->sequence.items) return NULL;

    do {
        skip_ws_and_nl(ctx);
        if (at_end(ctx)) break;

        int ind = count_indent(ctx);
        if (ind <= base_indent) break;
        if (ind != base_indent + 2 && ind != base_indent + 4 &&
            ind != base_indent + 1) {
            set_error(ctx, "line %d: inconsistent sequence indentation", ctx->line);
            break;
        }

        if (peek(ctx) != '-') break;
        advance(ctx);
        char next_c = peek(ctx);
        if (next_c == ' ' || next_c == '\t' || next_c == '\n') {
            skip_ws(ctx);
        } else {
            ctx->pos--;
        }

        struct yaml_node* item = parse_value(ctx, ind);
        if (!item) continue;

        if (seq->sequence.count >= cap) {
            cap *= 2;
            seq->sequence.items = (struct yaml_sequence_item*)realloc(
                seq->sequence.items, cap * sizeof(struct yaml_sequence_item));
        }
        seq->sequence.items[seq->sequence.count++].item = item;
    } while (!at_end(ctx));

    return seq;
}

static struct yaml_node* parse_mapping(struct parse_ctx* ctx, int base_indent) {
    struct yaml_node* map = alloc_node(ctx->doc, YAML_NODE_MAPPING);
    if (!map) return NULL;

    size_t cap = 8;
    map->mapping = (struct yaml_mapping_entry*)calloc(cap, sizeof(struct yaml_mapping_entry));
    size_t map_size = 0;
    if (!map->mapping) return NULL;

    while (!at_end(ctx)) {
        skip_ws_and_nl(ctx);
        if (at_end(ctx)) break;

        int ind = count_indent(ctx);
        if (ind <= base_indent) break;

        if (map_size > 0 && ind < base_indent + 2) break;

        char* key = parse_plain_scalar(ctx, ind);
        if (!key || key[0] == '\0') { free(key); break; }

        skip_ws(ctx);
        if (peek(ctx) != ':') {
            set_error(ctx, "line %d: expected ':' after key '%s'", ctx->line, key);
            free(key);
            break;
        }
        advance(ctx);

        skip_ws_and_nl(ctx);
        if (at_end(ctx) || peek(ctx) == '\n' || peek(ctx) == '\r') {
            int next_ind = count_indent(ctx);
            if (next_ind > ind) {
                struct yaml_node* val = parse_value(ctx, ind);
                if (val) {
                    if (map_size >= cap) {
                        cap *= 2;
                        map->mapping = (struct yaml_mapping_entry*)realloc(
                            map->mapping, cap * sizeof(struct yaml_mapping_entry));
                    }
                    map->mapping[map_size].key = key;
                    map->mapping[map_size].value = val;
                    map_size++;
                    continue;
                }
            }
            struct yaml_node* null_node = alloc_node(ctx->doc, YAML_NODE_SCALAR);
            if (null_node) null_node->scalar.value = strdup("");
            if (map_size >= cap) {
                cap *= 2;
                map->mapping = (struct yaml_mapping_entry*)realloc(
                    map->mapping, cap * sizeof(struct yaml_mapping_entry));
            }
            map->mapping[map_size].key = key;
            map->mapping[map_size].value = null_node;
            map_size++;
            continue;
        }

        struct yaml_node* val = parse_value(ctx, ind);
        if (!val) val = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (val && val->type == YAML_NODE_NONE) val->type = YAML_NODE_SCALAR;
        if (val && val->type == YAML_NODE_SCALAR && !val->scalar.value)
            val->scalar.value = strdup("");

        if (map_size >= cap) {
            cap *= 2;
            map->mapping = (struct yaml_mapping_entry*)realloc(
                map->mapping, cap * sizeof(struct yaml_mapping_entry));
        }
        map->mapping[map_size].key = key;
        map->mapping[map_size].value = val;
        map_size++;
    }

    return map;
}

static struct yaml_node* parse_value(struct parse_ctx* ctx, int base_indent) {
    if (at_end(ctx)) return alloc_node(ctx->doc, YAML_NODE_NONE);

    char c = peek(ctx);
    if (c == '"') {
        char* s = parse_quoted_string(ctx, '"');
        struct yaml_node* n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (n) { n->scalar.value = s; n->scalar.length = s ? strlen(s) : 0; }
        return n;
    }
    if (c == '\'') {
        char* s = parse_quoted_string(ctx, '\'');
        struct yaml_node* n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (n) { n->scalar.value = s; n->scalar.length = s ? strlen(s) : 0; }
        return n;
    }
    if (c == '|') {
        advance(ctx);
        skip_ws_and_nl(ctx);
        size_t cap = 1024;
        size_t len = 0;
        char* buf = (char*)malloc(cap);
        if (!buf) return NULL;
        int literal_indent = count_indent(ctx);
        while (!at_end(ctx)) {
            int line_ind = count_indent(ctx);
            if (line_ind < literal_indent) break;
            if (len > 0) { buf[len++] = '\n'; }
            while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r') {
                if (len + 2 >= cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
                buf[len++] = advance(ctx);
            }
            if (peek(ctx) == '\r') advance(ctx);
            if (peek(ctx) == '\n') advance(ctx);
        }
        buf[len] = '\0';
        struct yaml_node* n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (n) { n->scalar.value = buf; n->scalar.length = len; }
        return n;
    }
    if (c == '-') {
        char ahead = '\0';
        if (ctx->pos + 1 < ctx->len) ahead = ctx->src[ctx->pos + 1];
        if (ahead == ' ' || ahead == '\t' || ahead == '\n' || ahead == '\r')
            return parse_sequence(ctx, base_indent);
    }
    if (c == '[') {
        advance(ctx);
        struct yaml_node* seq = alloc_node(ctx->doc, YAML_NODE_SEQUENCE);
        size_t cap = 4;
        seq->sequence.items = (struct yaml_sequence_item*)calloc(cap, sizeof(struct yaml_sequence_item));
        seq->sequence.count = 0;
        skip_ws(ctx);
        while (!at_end(ctx) && peek(ctx) != ']') {
            if (seq->sequence.count > 0) {
                if (peek(ctx) == ',') advance(ctx);
                skip_ws(ctx);
            }
            struct yaml_node* item = parse_value(ctx, base_indent + 100);
            if (item) {
                if (seq->sequence.count >= cap) {
                    cap *= 2;
                    seq->sequence.items = (struct yaml_sequence_item*)realloc(
                        seq->sequence.items, cap * sizeof(struct yaml_sequence_item));
                }
                seq->sequence.items[seq->sequence.count++].item = item;
            }
            skip_ws(ctx);
        }
        if (peek(ctx) == ']') advance(ctx);
        return seq;
    }
    if (c == '{') {
        advance(ctx);
        struct yaml_node* map = alloc_node(ctx->doc, YAML_NODE_MAPPING);
        size_t cap = 4;
        map->mapping = (struct yaml_mapping_entry*)calloc(cap, sizeof(struct yaml_mapping_entry));
        size_t msz = 0;
        skip_ws(ctx);
        while (!at_end(ctx) && peek(ctx) != '}') {
            if (msz > 0) { if (peek(ctx) == ',') advance(ctx); skip_ws(ctx); }
            char* k = parse_plain_scalar(ctx, base_indent + 100);
            if (!k) break;
            skip_ws(ctx);
            if (peek(ctx) == ':') advance(ctx);
            skip_ws(ctx);
            struct yaml_node* v = parse_value(ctx, base_indent + 100);
            if (!v) v = alloc_node(ctx->doc, YAML_NODE_SCALAR);
            if (v && v->type == YAML_NODE_NONE) v->type = YAML_NODE_SCALAR;
            if (v && v->type == YAML_NODE_SCALAR && !v->scalar.value) v->scalar.value = strdup("");
            if (msz >= cap) { cap *= 2; map->mapping = (struct yaml_mapping_entry*)realloc(map->mapping, cap * sizeof(struct yaml_mapping_entry)); }
            map->mapping[msz].key = k;
            map->mapping[msz].value = v;
            msz++;
            skip_ws(ctx);
        }
        if (peek(ctx) == '}') advance(ctx);
        return map;
    }

    size_t saved_pos = ctx->pos;
    char* scalar = parse_plain_scalar(ctx, base_indent);
    if (!scalar) return alloc_node(ctx->doc, YAML_NODE_NONE);

    skip_ws(ctx);
    if (peek(ctx) == ':') {
        free(scalar);
        ctx->pos = saved_pos;
        return parse_mapping(ctx, base_indent);
    }

    struct yaml_node* n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
    if (n) {
        n->scalar.value = scalar;
        n->scalar.length = strlen(scalar);
    }
    return n;
}

int yaml_parse_string(yaml_document_t* doc, const char* input, size_t len) {
    if (!doc || !input || len == 0) return -1;
    memset(doc, 0, sizeof(*doc));

    doc->source = (char*)malloc(len + 1);
    if (!doc->source) return -1;
    memcpy(doc->source, input, len);
    doc->source[len] = '\0';
    doc->source_len = len;

    struct parse_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.src = doc->source;
    ctx.len = len;
    ctx.doc = doc;
    ctx.line = 1;

    doc->root = parse_value(&ctx, -1);
    return doc->root ? 0 : -1;
}

int yaml_parse_file(yaml_document_t* doc, const char* filepath) {
    if (!doc || !filepath) return -1;
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        doc->error_msg = (char*)malloc(256);
        snprintf(doc->error_msg, 256, "Cannot open file: %s", filepath);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 10 * 1024 * 1024) {
        fclose(f);
        doc->error_msg = strdup("File empty or too large");
        return -1;
    }
    char* buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    int ret = yaml_parse_string(doc, buf, rd);
    free(buf);
    return ret;
}

const char* yaml_get_error(const yaml_document_t* doc) {
    return doc ? doc->error_msg : NULL;
}

struct yaml_node* yaml_root(const yaml_document_t* doc) {
    return doc ? doc->root : NULL;
}

size_t yaml_size(struct yaml_node* node) {
    if (!node) return 0;
    switch (node->type) {
        case YAML_NODE_MAPPING: {
            size_t c = 0;
            for (size_t i = 0; ; i++) {
                if (!node->mapping[i].key) break;
                c++;
            }
            return c;
        }
        case YAML_NODE_SEQUENCE: return node->sequence.count;
        default: return 0;
    }
}

bool yaml_has_key(struct yaml_node* node, const char* key) {
    if (!node || node->type != YAML_NODE_MAPPING || !key) return false;
    size_t sz = yaml_size(node);
    for (size_t i = 0; i < sz; i++) {
        if (node->mapping[i].key && strcmp(node->mapping[i].key, key) == 0) return true;
    }
    return false;
}

struct yaml_node* yaml_get(struct yaml_node* node, const char* key) {
    if (!node || node->type != YAML_NODE_MAPPING || !key) return NULL;
    size_t sz = yaml_size(node);
    for (size_t i = 0; i < sz; i++) {
        if (node->mapping[i].key && strcmp(node->mapping[i].key, key) == 0)
            return node->mapping[i].value;
    }
    return NULL;
}

struct yaml_node* yaml_get_index(struct yaml_node* node, size_t index) {
    if (!node || node->type != YAML_NODE_SEQUENCE) return NULL;
    if (index >= node->sequence.count) return NULL;
    return node->sequence.items[index].item;
}

const char* yaml_as_string(struct yaml_node* node, const char* default_val) {
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    return node->scalar.value;
}

long long yaml_as_int64(struct yaml_node* node, long long default_val) {
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    char* endp = NULL;
    long long v = strtoll(node->scalar.value, &endp, 0);
    return (endp && *endp == '\0') ? v : default_val;
}

double yaml_as_double(struct yaml_node* node, double default_val) {
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    char* endp = NULL;
    double v = strtod(node->scalar.value, &endp);
    return (endp && *endp == '\0') ? v : default_val;
}

bool yaml_as_bool(struct yaml_node* node, bool default_val) {
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    const char* s = node->scalar.value;
    if (strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 || strcmp(s, "on") == 0 ||
        strcmp(s, "1") == 0) return true;
    if (strcmp(s, "false") == 0 || strcmp(s, "no") == 0 || strcmp(s, "off") == 0 ||
        strcmp(s, "0") == 0 || strcmp(s, "null") == 0 || strcmp(s, "~") == 0) return false;
    return default_val;
}

void yaml_dump(struct yaml_node* node, char* buf, size_t bufsize, int indent) {
    if (!node || !buf || bufsize == 0) return;
    size_t off = strlen(buf);
    #define APPEND(...) do { \
        off += snprintf(buf + off, bufsize > off ? bufsize - off : 0, __VA_ARGS__); \
    } while(0)

    for (int i = 0; i < indent; i++) APPEND("  ");

    switch (node->type) {
        case YAML_NODE_NONE:
            APPEND("~");
            break;
        case YAML_NODE_SCALAR: {
            const char* v = node->scalar.value ? node->scalar.value : "";
            bool needs_quote = (*v == '\0') || strchr(v, ':') || strchr(v, '#') ||
                             strchr(v, '[') || strchr(v, '{') || strchr(v, ',') ||
                             strchr(v, '"') || strchr(v, '\'');
            if (needs_quote) {
                APPEND("\"%s\"", v);
            } else {
                APPEND("%s", v);
            }
            break;
        }
        case YAML_NODE_MAPPING: {
            size_t sz = yaml_size(node);
            if (sz == 0) { APPEND("{}"); break; }
            APPEND("{\n");
            for (size_t i = 0; i < sz; i++) {
                for (int j = 0; j < indent + 1; j++) APPEND("  ");
                APPEND("%s: ", node->mapping[i].key);
                yaml_dump(node->mapping[i].value, buf, bufsize, indent + 1);
                if (i < sz - 1) APPEND(",");
                APPEND("\n");
            }
            for (int i = 0; i < indent; i++) APPEND("  ");
            APPEND("}");
            break;
        }
        case YAML_NODE_SEQUENCE: {
            size_t cnt = node->sequence.count;
            if (cnt == 0) { APPEND("[]"); break; }
            APPEND("[\n");
            for (size_t i = 0; i < cnt; i++) {
                yaml_dump(node->sequence.items[i].item, buf, bufsize, indent + 1);
                if (i < cnt - 1) APPEND(",");
                APPEND("\n");
            }
            for (int i = 0; i < indent; i++) APPEND("  ");
            APPEND("]");
            break;
        }
    }
    #undef APPEND
}

char* yaml_serialize(yaml_document_t* doc) {
    if (!doc || !doc->root) return NULL;

    size_t bufsize = 4096;
    char* buf = (char*)malloc(bufsize);
    if (!buf) return NULL;

    buf[0] = '\0';
    yaml_dump(doc->root, buf, bufsize, 0);

    size_t len = strlen(buf);
    char* result = (char*)malloc(len + 1);
    if (result) {
        memcpy(result, buf, len + 1);
    }
    free(buf);
    return result;
}
