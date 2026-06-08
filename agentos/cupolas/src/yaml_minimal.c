/**
 * @file yaml_minimal.c
 * @brief YAML 1.1 parser - production-grade implementation
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "yaml_minimal.h"

#include "memory_compat.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"


#define INITIAL_NODE_CAPACITY 64
#define MAX_LINE_LEN 8192
#define MAX_DEPTH 64
#define MAX_KEY_LEN 256
#define INITIAL_ANCHORS 64

static void *yaml_safe_realloc(void *ptr, size_t size)
{
    void *tmp = AGENTOS_REALLOC(ptr, size);
    if (!tmp) {
        AGENTOS_FREE(ptr);
    }
    return tmp;
}

struct anchor_entry {
    char *name;
    struct yaml_node *node;
};

struct parse_ctx {
    const char *src;
    size_t len;
    size_t pos;
    int line;
    int line_pos;
    int col;
    yaml_document_t *doc;
    struct anchor_entry *anchors;
    int anchor_count;
    int anchor_capacity;
    char *error_msg;
    char **tag_handles;
    char **tag_prefixes;
    int tag_handle_count;
};

static char peek(struct parse_ctx *ctx);
static char advance(struct parse_ctx *ctx);
static bool at_end(struct parse_ctx *ctx);
static struct yaml_node *parse_value(struct parse_ctx *ctx, int base_indent);
static void register_anchor(struct parse_ctx *ctx, const char *name, struct yaml_node *node);
static struct yaml_node *lookup_anchor(struct parse_ctx *ctx, const char *name);
static char *parse_tag(struct parse_ctx *ctx);
static char *parse_anchor_name(struct parse_ctx *ctx);

static struct yaml_node *alloc_node(yaml_document_t *doc, yaml_node_type_t type)
{
    if (doc->node_count >= doc->node_capacity) {
        size_t new_cap = doc->node_capacity * 2;
        if (new_cap == 0)
            new_cap = INITIAL_NODE_CAPACITY;
        struct yaml_node **new_nodes =
            (struct yaml_node **)AGENTOS_REALLOC(doc->all_nodes, sizeof(struct yaml_node *) * new_cap);
        if (!new_nodes)
            return NULL;
        doc->all_nodes = new_nodes;
        doc->node_capacity = new_cap;
    }
    struct yaml_node *node = (struct yaml_node *)AGENTOS_CALLOC(1, sizeof(struct yaml_node));
    if (!node)
        return NULL;
    node->type = type;
    node->line = 0;
    doc->all_nodes[doc->node_count++] = node;
    return node;
}

static void free_node(struct yaml_node *node)
{
    if (!node)
        return;
    switch (node->type) {
    case YAML_NODE_SCALAR:
        AGENTOS_FREE(node->scalar.value);
        break;
    case YAML_NODE_MAPPING:
        if (node->mapping) {
            for (size_t i = 0; i < yaml_size(node); i++) {
                AGENTOS_FREE(node->mapping[i].key);
            }
            AGENTOS_FREE(node->mapping);
        }
        break;
    case YAML_NODE_SEQUENCE:
        if (node->sequence.items) {
            AGENTOS_FREE(node->sequence.items);
        }
        break;
    default:
        break;
    }
    AGENTOS_FREE(node->anchor_name);
    AGENTOS_FREE(node->tag);
}

static void register_anchor(struct parse_ctx *ctx, const char *name, struct yaml_node *node)
{
    if (!name || !node)
        return;
    if (ctx->anchor_count >= ctx->anchor_capacity) {
        int new_cap = ctx->anchor_capacity * 2;
        if (new_cap == 0)
            new_cap = INITIAL_ANCHORS;
        struct anchor_entry *new_anchors =
            (struct anchor_entry *)AGENTOS_REALLOC(ctx->anchors, sizeof(struct anchor_entry) * new_cap);
        if (!new_anchors)
            return;
        ctx->anchors = new_anchors;
        ctx->anchor_capacity = new_cap;
    }
    if (node->anchor_name)
        AGENTOS_FREE(node->anchor_name);
    node->anchor_name = AGENTOS_STRDUP(name);
    ctx->anchors[ctx->anchor_count].name = AGENTOS_STRDUP(name);
    ctx->anchors[ctx->anchor_count].node = node;
    ctx->anchor_count++;
}

static struct yaml_node *lookup_anchor(struct parse_ctx *ctx, const char *name)
{
    if (!name)
        return NULL;
    for (int i = 0; i < ctx->anchor_count; i++) {
        if (ctx->anchors[i].name && strcmp(ctx->anchors[i].name, name) == 0) {
            return ctx->anchors[i].node;
        }
    }
    return NULL;
}

static struct yaml_node *deep_copy_node(yaml_document_t *doc, struct yaml_node *src)
{
    if (!src)
        return NULL;
    struct yaml_node *copy = alloc_node(doc, src->type);
    if (!copy)
        return NULL;
    copy->line = src->line;
    if (src->anchor_name)
        copy->anchor_name = AGENTOS_STRDUP(src->anchor_name);
    if (src->tag)
        copy->tag = AGENTOS_STRDUP(src->tag);
    switch (src->type) {
    case YAML_NODE_SCALAR:
        if (src->scalar.value) {
            copy->scalar.value = AGENTOS_STRDUP(src->scalar.value);
            copy->scalar.length = src->scalar.length;
        }
        break;
    case YAML_NODE_MAPPING: {
        size_t sz = yaml_size(src);
        copy->mapping =
            (struct yaml_mapping_entry *)AGENTOS_CALLOC(sz + 1, sizeof(struct yaml_mapping_entry));
        for (size_t i = 0; i < sz; i++) {
            copy->mapping[i].key = AGENTOS_STRDUP(src->mapping[i].key);
            copy->mapping[i].value = deep_copy_node(doc, src->mapping[i].value);
        }
        break;
    }
    case YAML_NODE_SEQUENCE: {
        copy->sequence.count = src->sequence.count;
        copy->sequence.items = (struct yaml_sequence_item *)AGENTOS_CALLOC(
            src->sequence.count, sizeof(struct yaml_sequence_item));
        for (size_t i = 0; i < src->sequence.count; i++) {
            copy->sequence.items[i].item = deep_copy_node(doc, src->sequence.items[i].item);
        }
        break;
    }
    default:
        break;
    }
    return copy;
}

static char *parse_tag(struct parse_ctx *ctx)
{
    if (peek(ctx) != '!')
        return NULL;
    advance(ctx);
    if (peek(ctx) == '!')
        advance(ctx);
    size_t cap = 64;
    size_t len = 0;
    char *buf = (char *)AGENTOS_MALLOC(cap);
    if (!buf)
        return NULL;
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ':')
            break;
        advance(ctx);
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char *)yaml_safe_realloc(buf, cap);
            if (!buf)
                return NULL;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return buf;
}

static char *parse_anchor_name(struct parse_ctx *ctx)
{
    size_t cap = 64;
    size_t len = 0;
    char *buf = (char *)AGENTOS_MALLOC(cap);
    if (!buf)
        return NULL;
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ':')
            break;
        advance(ctx);
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char *)yaml_safe_realloc(buf, cap);
            if (!buf)
                return NULL;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return buf;
}

yaml_document_t *yaml_create(void)
{
    yaml_document_t *doc = (yaml_document_t *)AGENTOS_CALLOC(1, sizeof(yaml_document_t));
    return doc;
}

void yaml_destroy(yaml_document_t *doc)
{
    if (!doc)
        return;
    for (size_t i = 0; i < doc->node_count; i++) {
        free_node(doc->all_nodes[i]);
    }
    AGENTOS_FREE(doc->all_nodes);
    AGENTOS_FREE(doc->source);
    AGENTOS_FREE(doc->error_msg);
    AGENTOS_FREE(doc);
}

static void set_error(struct parse_ctx *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    AGENTOS_FREE(ctx->doc->error_msg);
    ctx->doc->error_msg = AGENTOS_STRDUP(buf);
}

static char peek(struct parse_ctx *ctx)
{
    if (ctx->pos >= ctx->len)
        return '\0';
    return ctx->src[ctx->pos];
}

static char advance(struct parse_ctx *ctx)
{
    if (ctx->pos >= ctx->len)
        return '\0';
    char c = ctx->src[ctx->pos++];
    if (c == '\n') {
        ctx->line++;
        ctx->line_pos = 0;
    } else {
        ctx->line_pos++;
    }
    return c;
}

static bool at_end(struct parse_ctx *ctx)
{
    return ctx->pos >= ctx->len;
}

static void skip_ws(struct parse_ctx *ctx)
{
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t')
            advance(ctx);
        else
            break;
    }
}

static void skip_ws_and_nl(struct parse_ctx *ctx)
{
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            advance(ctx);
        else
            break;
    }
}

static int count_indent(struct parse_ctx *ctx)
{
    int indent = 0;
    while (!at_end(ctx)) {
        char c = peek(ctx);
        if (c == ' ') {
            indent++;
            advance(ctx);
        } else if (c == '\t') {
            set_error(ctx, "line %d: YAML forbids tabs for indentation", ctx->line);
            advance(ctx);
            indent += 2;
        } else
            break;
    }
    return indent;
}

static bool is_plain_scalar_char(char c)
{
    switch (c) {
    case ':':
    case '{':
    case '}':
    case '[':
    case ']':
    case ',':
    case '&':
    case '*':
    case '#':
    case '|':
    case '>':
    case '"':
    case '\'':
    case '%':
    case '@':
    case '`':
    case '\n':
    case '\r':
        return false;
    default:
        return !isspace((unsigned char)c);
    }
}

static char *parse_quoted_string(struct parse_ctx *ctx, char quote)
{
    advance(ctx);
    size_t cap = 128;
    size_t len = 0;
    char *buf = (char *)AGENTOS_MALLOC(cap);
    if (!buf)
        return NULL;

    while (!at_end(ctx)) {
        char c = advance(ctx);
        if (c == quote && (len == 0 || buf[len - 1] != '\\'))
            break;
        if (c == '\\' && !at_end(ctx)) {
            char next = advance(ctx);
            switch (next) {
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
            case 'r':
                c = '\r';
                break;
            case '\\':
                c = '\\';
                break;
            case '\'':
                c = '\'';
                break;
            case '"':
                c = '"';
                break;
            default: /* keep both */
                break;
            }
        }
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char *)yaml_safe_realloc(buf, cap);
            if (!buf)
                return NULL;
        }
        buf[len++] = c;
    }
    buf[len] = '\0';
    return buf;
}

static char *parse_plain_scalar(struct parse_ctx *ctx, int end_indent)
{
    size_t cap = 256;
    size_t len = 0;
    char *buf = (char *)AGENTOS_MALLOC(cap);
    if (!buf)
        return NULL;

    while (!at_end(ctx)) {
        char c = peek(ctx);

        if (c == ':' && (ctx->pos + 1 < ctx->len &&
                         (ctx->src[ctx->pos + 1] == ' ' || ctx->src[ctx->pos + 1] == '\n')))
            break;
        if (c == '#' && len > 0 && buf[len - 1] == ' ')
            break;
        if (c == '\n' || c == '\r')
            break;
        if (!is_plain_scalar_char(c))
            break;

        advance(ctx);
        if (len + 2 >= cap) {
            cap *= 2;
            buf = (char *)yaml_safe_realloc(buf, cap);
            if (!buf)
                return NULL;
        }
        buf[len++] = c;
    }

    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t'))
        len--;
    buf[len] = '\0';

    if (len == 0) {
        AGENTOS_FREE(buf);
        return AGENTOS_STRDUP("");
    }
    return buf;
}

static struct yaml_node *parse_value(struct parse_ctx *ctx, int base_indent);

static struct yaml_node *parse_sequence(struct parse_ctx *ctx, int base_indent)
{
    struct yaml_node *seq = alloc_node(ctx->doc, YAML_NODE_SEQUENCE);
    if (!seq)
        return NULL;

    size_t cap = 8;
    seq->sequence.items =
        (struct yaml_sequence_item *)AGENTOS_CALLOC(cap, sizeof(struct yaml_sequence_item));
    seq->sequence.count = 0;
    if (!seq->sequence.items)
        return NULL;

    do {
        skip_ws_and_nl(ctx);
        if (at_end(ctx))
            break;

        int ind = count_indent(ctx);
        if (ind <= base_indent)
            break;
        if (ind != base_indent + 2 && ind != base_indent + 4 && ind != base_indent + 1) {
            set_error(ctx, "line %d: inconsistent sequence indentation", ctx->line);
            break;
        }

        if (peek(ctx) != '-')
            break;
        advance(ctx);
        char next_c = peek(ctx);
        if (next_c == ' ' || next_c == '\t' || next_c == '\n') {
            skip_ws(ctx);
        } else {
            ctx->pos--;
        }

        struct yaml_node *item = parse_value(ctx, ind);
        if (!item)
            continue;

        if (seq->sequence.count >= cap) {
            cap *= 2;
            seq->sequence.items = (struct yaml_sequence_item *)yaml_safe_realloc(
                seq->sequence.items, cap * sizeof(struct yaml_sequence_item));
            if (!seq->sequence.items)
                return NULL;
        }
        seq->sequence.items[seq->sequence.count++].item = item;
    } while (!at_end(ctx));

    return seq;
}

static void merge_mapping_into(yaml_document_t *doc, struct yaml_node *target,
                               struct yaml_node *source)
{
    if (!doc || !target || !source || target->type != YAML_NODE_MAPPING ||
        source->type != YAML_NODE_MAPPING)
        return;

    size_t src_sz = yaml_size(source);
    size_t tgt_sz = yaml_size(target);
    size_t cap = tgt_sz + 1;
    while (cap < tgt_sz + src_sz)
        cap *= 2;

    target->mapping = (struct yaml_mapping_entry *)yaml_safe_realloc(
        target->mapping, cap * sizeof(struct yaml_mapping_entry));
    if (!target->mapping)
        return;

    for (size_t i = 0; i < src_sz; i++) {
        bool found = false;
        size_t tgt_current = yaml_size(target);
        for (size_t j = 0; j < tgt_current; j++) {
            if (target->mapping[j].key && source->mapping[i].key &&
                strcmp(target->mapping[j].key, source->mapping[i].key) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (tgt_current >= cap) {
                cap *= 2;
                target->mapping = (struct yaml_mapping_entry *)yaml_safe_realloc(
                    target->mapping, cap * sizeof(struct yaml_mapping_entry));
                if (!target->mapping)
                    return;
            }
            target->mapping[tgt_current].key = AGENTOS_STRDUP(source->mapping[i].key);
            target->mapping[tgt_current].value = deep_copy_node(doc, source->mapping[i].value);
        }
    }
}

static struct yaml_node *parse_mapping(struct parse_ctx *ctx, int base_indent)
{
    struct yaml_node *map = alloc_node(ctx->doc, YAML_NODE_MAPPING);
    if (!map)
        return NULL;

    size_t cap = 8;
    map->mapping =
        (struct yaml_mapping_entry *)AGENTOS_CALLOC(cap, sizeof(struct yaml_mapping_entry));
    size_t map_size = 0;
    if (!map->mapping)
        return NULL;

    while (!at_end(ctx)) {
        skip_ws_and_nl(ctx);
        if (at_end(ctx))
            break;

        int ind = count_indent(ctx);
        if (ind <= base_indent)
            break;

        if (map_size > 0 && ind < base_indent + 2)
            break;

        char *key = parse_plain_scalar(ctx, ind);
        if (!key || key[0] == '\0') {
            AGENTOS_FREE(key);
            break;
        }

        skip_ws(ctx);
        if (peek(ctx) != ':') {
            set_error(ctx, "line %d: expected ':' after key '%s'", ctx->line, key);
            AGENTOS_FREE(key);
            break;
        }
        advance(ctx);

        skip_ws_and_nl(ctx);
        if (at_end(ctx) || peek(ctx) == '\n' || peek(ctx) == '\r') {
            int next_ind = count_indent(ctx);
            if (next_ind > ind) {
                struct yaml_node *val = parse_value(ctx, ind);
                if (val) {
                    if (strcmp(key, "<<") == 0) {
                        if (val->type == YAML_NODE_SEQUENCE) {
                            for (size_t si = 0; si < val->sequence.count; si++) {
                                merge_mapping_into(ctx->doc, map, val->sequence.items[si].item);
                            }
                        } else {
                            merge_mapping_into(ctx->doc, map, val);
                        }
                        AGENTOS_FREE(key);
                        map_size = yaml_size(map);
                        continue;
                    }
                    if (map_size >= cap) {
                        cap *= 2;
                        map->mapping = (struct yaml_mapping_entry *)yaml_safe_realloc(
                            map->mapping, cap * sizeof(struct yaml_mapping_entry));
                        if (!map->mapping)
                            return NULL;
                    }
                    map->mapping[map_size].key = key;
                    map->mapping[map_size].value = val;
                    map_size++;
                    continue;
                }
            }
            if (strcmp(key, "<<") == 0) {
                AGENTOS_FREE(key);
                continue;
            }
            struct yaml_node *null_node = alloc_node(ctx->doc, YAML_NODE_SCALAR);
            if (null_node)
                null_node->scalar.value = AGENTOS_STRDUP("");
            if (map_size >= cap) {
                cap *= 2;
                map->mapping = (struct yaml_mapping_entry *)yaml_safe_realloc(
                    map->mapping, cap * sizeof(struct yaml_mapping_entry));
                if (!map->mapping)
                    return NULL;
            }
            map->mapping[map_size].key = key;
            map->mapping[map_size].value = null_node;
            map_size++;
            continue;
        }

        struct yaml_node *val = parse_value(ctx, ind);
        if (!val)
            val = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (val && val->type == YAML_NODE_NONE)
            val->type = YAML_NODE_SCALAR;
        if (val && val->type == YAML_NODE_SCALAR && !val->scalar.value)
            val->scalar.value = AGENTOS_STRDUP("");

        if (strcmp(key, "<<") == 0) {
            if (val->type == YAML_NODE_SEQUENCE) {
                for (size_t si = 0; si < val->sequence.count; si++) {
                    merge_mapping_into(ctx->doc, map, val->sequence.items[si].item);
                }
            } else {
                merge_mapping_into(ctx->doc, map, val);
            }
            AGENTOS_FREE(key);
            free_node(val);
            map_size = yaml_size(map);
            continue;
        }

        if (map_size >= cap) {
            cap *= 2;
            map->mapping = (struct yaml_mapping_entry *)yaml_safe_realloc(
                map->mapping, cap * sizeof(struct yaml_mapping_entry));
            if (!map->mapping)
                return NULL;
        }
        map->mapping[map_size].key = key;
        map->mapping[map_size].value = val;
        map_size++;
    }

    return map;
}

static struct yaml_node *parse_value(struct parse_ctx *ctx, int base_indent)
{
    if (at_end(ctx))
        return alloc_node(ctx->doc, YAML_NODE_NONE);

    char *tag = NULL;
    char *anchor_name = NULL;

    if (peek(ctx) == '!') {
        tag = parse_tag(ctx);
        skip_ws(ctx);
    }

    if (peek(ctx) == '&') {
        advance(ctx);
        anchor_name = parse_anchor_name(ctx);
        skip_ws(ctx);
    }

    if (peek(ctx) == '*') {
        advance(ctx);
        char *alias_name = parse_anchor_name(ctx);
        struct yaml_node *aliased = lookup_anchor(ctx, alias_name);
        if (!aliased) {
            set_error(ctx, "line %d: unknown alias '%s'", ctx->line, alias_name);
            AGENTOS_FREE(alias_name);
            AGENTOS_FREE(tag);
            AGENTOS_FREE(anchor_name);
            return NULL;
        }
        struct yaml_node *copy = deep_copy_node(ctx->doc, aliased);
        if (copy) {
            if (tag) {
                AGENTOS_FREE(copy->tag);
                copy->tag = tag;
                tag = NULL;
            }
            if (anchor_name) {
                register_anchor(ctx, anchor_name, copy);
            }
        }
        AGENTOS_FREE(alias_name);
        AGENTOS_FREE(anchor_name);
        AGENTOS_FREE(tag);
        return copy;
    }

    char c = peek(ctx);
    if (c == '"') {
        char *s = parse_quoted_string(ctx, '"');
        struct yaml_node *n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (n) {
            n->scalar.value = s;
            n->scalar.length = s ? strlen(s) : 0;
            if (anchor_name)
                register_anchor(ctx, anchor_name, n);
            if (tag) {
                n->tag = tag;
                tag = NULL;
            }
        }
        AGENTOS_FREE(anchor_name);
        AGENTOS_FREE(tag);
        return n;
    }
    if (c == '\'') {
        char *s = parse_quoted_string(ctx, '\'');
        struct yaml_node *n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (n) {
            n->scalar.value = s;
            n->scalar.length = s ? strlen(s) : 0;
            if (anchor_name)
                register_anchor(ctx, anchor_name, n);
            if (tag) {
                n->tag = tag;
                tag = NULL;
            }
        }
        AGENTOS_FREE(anchor_name);
        AGENTOS_FREE(tag);
        return n;
    }
    if (c == '|' || c == '>') {
        bool is_folded = (c == '>');
        advance(ctx);

        enum { CHOMP_CLIP, CHOMP_STRIP, CHOMP_KEEP } chomp = CHOMP_CLIP;
        int block_indent = -1;

        while (!at_end(ctx)) {
            char mod = peek(ctx);
            if (mod == '-') {
                chomp = CHOMP_STRIP;
                advance(ctx);
            } else if (mod == '+') {
                chomp = CHOMP_KEEP;
                advance(ctx);
            } else if (mod >= '1' && mod <= '9') {
                block_indent = mod - '0';
                advance(ctx);
            } else
                break;
        }

        skip_ws(ctx);
        if (!at_end(ctx) && peek(ctx) == '#') {
            while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r')
                advance(ctx);
        }
        if (!at_end(ctx) && peek(ctx) == '\r')
            advance(ctx);
        if (!at_end(ctx) && peek(ctx) == '\n')
            advance(ctx);

        size_t cap = 1024;
        size_t len = 0;
        char *buf = (char *)AGENTOS_MALLOC(cap);
        if (!buf)
            return NULL;

        int first_line_indent = -1;
        bool has_content = false;

        while (!at_end(ctx)) {
            int line_ind = 0;
            while (!at_end(ctx) && peek(ctx) == ' ') {
                line_ind++;
                advance(ctx);
            }

            if (at_end(ctx))
                break;

            char line_char = peek(ctx);
            if (line_char == '\n' || line_char == '\r') {
                if (peek(ctx) == '\r')
                    advance(ctx);
                if (peek(ctx) == '\n')
                    advance(ctx);
                if (chomp == CHOMP_KEEP) {
                    if (len + 2 >= cap) {
                        cap *= 2;
                        buf = (char *)yaml_safe_realloc(buf, cap);
                        if (!buf)
                            return NULL;
                    }
                    buf[len++] = '\n';
                } else {
                    if (len + 2 >= cap) {
                        cap *= 2;
                        buf = (char *)yaml_safe_realloc(buf, cap);
                        if (!buf)
                            return NULL;
                    }
                    buf[len++] = '\n';
                }
                continue;
            }

            if (line_char == ' ' || line_char == '\t') {
                continue;
            }

            if (block_indent < 0) {
                if (first_line_indent < 0) {
                    first_line_indent = line_ind;
                    block_indent = line_ind;
                }
            }

            if (line_ind < block_indent && line_char != '\0')
                break;

            if (has_content) {
                if (is_folded) {
                    if (len > 0 && buf[len - 1] == '\n') {
                        buf[len - 1] = '\n';
                    } else {
                        if (len + 2 >= cap) {
                            cap *= 2;
                            buf = (char *)yaml_safe_realloc(buf, cap);
                            if (!buf)
                                return NULL;
                        }
                        buf[len++] = ' ';
                    }
                } else {
                    if (len + 2 >= cap) {
                        cap *= 2;
                        buf = (char *)yaml_safe_realloc(buf, cap);
                        if (!buf)
                            return NULL;
                    }
                    buf[len++] = '\n';
                }
            }

            while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r') {
                if (len + 2 >= cap) {
                    cap *= 2;
                    buf = (char *)yaml_safe_realloc(buf, cap);
                    if (!buf)
                        return NULL;
                }
                buf[len++] = advance(ctx);
            }
            has_content = true;

            if (!at_end(ctx) && peek(ctx) == '\r')
                advance(ctx);
            if (!at_end(ctx) && peek(ctx) == '\n')
                advance(ctx);
        }

        switch (chomp) {
        case CHOMP_STRIP:
            while (len > 0 && buf[len - 1] == '\n')
                len--;
            break;
        case CHOMP_KEEP:
            break;
        case CHOMP_CLIP:
        default:
            if (len > 0 && buf[len - 1] != '\n') {
                if (len + 2 >= cap) {
                    cap *= 2;
                    buf = (char *)yaml_safe_realloc(buf, cap);
                    if (!buf)
                        return NULL;
                }
                buf[len++] = '\n';
            }
            break;
        }

        buf[len] = '\0';
        struct yaml_node *n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
        if (n) {
            n->scalar.value = buf;
            n->scalar.length = len;
            if (anchor_name)
                register_anchor(ctx, anchor_name, n);
            if (tag) {
                n->tag = tag;
                tag = NULL;
            }
        }
        AGENTOS_FREE(anchor_name);
        AGENTOS_FREE(tag);
        return n;
    }
    if (c == '-') {
        char ahead = '\0';
        if (ctx->pos + 1 < ctx->len)
            ahead = ctx->src[ctx->pos + 1];
        if (ahead == ' ' || ahead == '\t' || ahead == '\n' || ahead == '\r') {
            struct yaml_node *seq = parse_sequence(ctx, base_indent);
            if (seq && anchor_name)
                register_anchor(ctx, anchor_name, seq);
            if (seq && tag) {
                AGENTOS_FREE(seq->tag);
                seq->tag = tag;
                tag = NULL;
            }
            AGENTOS_FREE(anchor_name);
            AGENTOS_FREE(tag);
            return seq;
        }
    }
    if (c == '[') {
        advance(ctx);
        struct yaml_node *seq = alloc_node(ctx->doc, YAML_NODE_SEQUENCE);
        size_t cap = 4;
        seq->sequence.items =
            (struct yaml_sequence_item *)AGENTOS_CALLOC(cap, sizeof(struct yaml_sequence_item));
        seq->sequence.count = 0;
        skip_ws(ctx);
        while (!at_end(ctx) && peek(ctx) != ']') {
            if (seq->sequence.count > 0) {
                if (peek(ctx) == ',')
                    advance(ctx);
                skip_ws(ctx);
            }
            struct yaml_node *item = parse_value(ctx, base_indent + 100);
            if (item) {
                if (seq->sequence.count >= cap) {
                    cap *= 2;
                    seq->sequence.items = (struct yaml_sequence_item *)yaml_safe_realloc(
                        seq->sequence.items, cap * sizeof(struct yaml_sequence_item));
                    if (!seq->sequence.items)
                        return NULL;
                }
                seq->sequence.items[seq->sequence.count++].item = item;
            }
            skip_ws(ctx);
        }
        if (peek(ctx) == ']')
            advance(ctx);
        if (anchor_name)
            register_anchor(ctx, anchor_name, seq);
        if (tag) {
            AGENTOS_FREE(seq->tag);
            seq->tag = tag;
            tag = NULL;
        }
        AGENTOS_FREE(anchor_name);
        AGENTOS_FREE(tag);
        return seq;
    }
    if (c == '{') {
        advance(ctx);
        struct yaml_node *map = alloc_node(ctx->doc, YAML_NODE_MAPPING);
        size_t cap = 4;
        map->mapping =
            (struct yaml_mapping_entry *)AGENTOS_CALLOC(cap, sizeof(struct yaml_mapping_entry));
        size_t msz = 0;
        skip_ws(ctx);
        while (!at_end(ctx) && peek(ctx) != '}') {
            if (msz > 0) {
                if (peek(ctx) == ',')
                    advance(ctx);
                skip_ws(ctx);
            }
            char *k = parse_plain_scalar(ctx, base_indent + 100);
            if (!k)
                break;
            skip_ws(ctx);
            if (peek(ctx) == ':')
                advance(ctx);
            skip_ws(ctx);
            struct yaml_node *v = parse_value(ctx, base_indent + 100);
            if (!v)
                v = alloc_node(ctx->doc, YAML_NODE_SCALAR);
            if (v && v->type == YAML_NODE_NONE)
                v->type = YAML_NODE_SCALAR;
            if (v && v->type == YAML_NODE_SCALAR && !v->scalar.value)
                v->scalar.value = AGENTOS_STRDUP("");
            if (msz >= cap) {
                cap *= 2;
                map->mapping = (struct yaml_mapping_entry *)yaml_safe_realloc(
                    map->mapping, cap * sizeof(struct yaml_mapping_entry));
                if (!map->mapping)
                    return NULL;
            }
            map->mapping[msz].key = k;
            map->mapping[msz].value = v;
            msz++;
            skip_ws(ctx);
        }
        if (peek(ctx) == '}')
            advance(ctx);
        if (anchor_name)
            register_anchor(ctx, anchor_name, map);
        if (tag) {
            AGENTOS_FREE(map->tag);
            map->tag = tag;
            tag = NULL;
        }
        AGENTOS_FREE(anchor_name);
        AGENTOS_FREE(tag);
        return map;
    }

    size_t saved_pos = ctx->pos;
    char *scalar = parse_plain_scalar(ctx, base_indent);
    if (!scalar) {
        AGENTOS_FREE(tag);
        AGENTOS_FREE(anchor_name);
        return alloc_node(ctx->doc, YAML_NODE_NONE);
    }

    skip_ws(ctx);
    if (peek(ctx) == ':') {
        AGENTOS_FREE(scalar);
        ctx->pos = saved_pos;
        struct yaml_node *m = parse_mapping(ctx, base_indent);
        if (m && anchor_name)
            register_anchor(ctx, anchor_name, m);
        if (m && tag) {
            AGENTOS_FREE(m->tag);
            m->tag = tag;
            tag = NULL;
        }
        AGENTOS_FREE(anchor_name);
        AGENTOS_FREE(tag);
        return m;
    }

    struct yaml_node *n = alloc_node(ctx->doc, YAML_NODE_SCALAR);
    if (n) {
        n->scalar.value = scalar;
        n->scalar.length = strlen(scalar);
        if (anchor_name) {
            register_anchor(ctx, anchor_name, n);
        }
        if (tag) {
            n->tag = tag;
            tag = NULL;
        }
    } else {
        AGENTOS_FREE(scalar);
    }
    AGENTOS_FREE(anchor_name);
    AGENTOS_FREE(tag);
    return n;
}

static void skip_yaml_directives(struct parse_ctx *ctx)
{
    while (!at_end(ctx)) {
        if (peek(ctx) == '%') {
            while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r') {
                advance(ctx);
            }
            if (!at_end(ctx) && peek(ctx) == '\r')
                advance(ctx);
            if (!at_end(ctx) && peek(ctx) == '\n')
                advance(ctx);
        } else if (peek(ctx) == ' ' || peek(ctx) == '\t' || peek(ctx) == '\n' ||
                   peek(ctx) == '\r') {
            advance(ctx);
        } else {
            break;
        }
    }
}

static bool is_document_marker(struct parse_ctx *ctx, const char *marker)
{
    size_t marker_len = strlen(marker);
    if (ctx->pos + marker_len > ctx->len)
        return false;
    if (memcmp(ctx->src + ctx->pos, marker, marker_len) != 0)
        return false;
    if (ctx->pos + marker_len < ctx->len) {
        char after = ctx->src[ctx->pos + marker_len];
        if (after != ' ' && after != '\t' && after != '\n' && after != '\r' && after != '\0')
            return false;
    }
    return true;
}

static void skip_document_marker(struct parse_ctx *ctx, const char *marker)
{
    size_t marker_len = strlen(marker);
    ctx->pos += marker_len;
    while (!at_end(ctx) && peek(ctx) != '\n' && peek(ctx) != '\r')
        advance(ctx);
    if (!at_end(ctx) && peek(ctx) == '\r')
        advance(ctx);
    if (!at_end(ctx) && peek(ctx) == '\n')
        advance(ctx);
}

static void cleanup_parse_ctx(struct parse_ctx *ctx)
{
    if (ctx->anchors) {
        for (int i = 0; i < ctx->anchor_count; i++) {
            AGENTOS_FREE(ctx->anchors[i].name);
        }
        AGENTOS_FREE(ctx->anchors);
    }
    if (ctx->tag_handles) {
        for (int i = 0; i < ctx->tag_handle_count; i++) {
            AGENTOS_FREE(ctx->tag_handles[i]);
        }
        AGENTOS_FREE(ctx->tag_handles);
    }
    if (ctx->tag_prefixes) {
        for (int i = 0; i < ctx->tag_handle_count; i++) {
            AGENTOS_FREE(ctx->tag_prefixes[i]);
        }
        AGENTOS_FREE(ctx->tag_prefixes);
    }
}

int yaml_parse_string(yaml_document_t *doc, const char *input, size_t len)
{
    if (!doc || !input)
        return AGENTOS_EINVAL;
    __builtin_memset(doc, 0, sizeof(*doc));

    size_t start_pos = 0;
    if (len >= 3 && (unsigned char)input[0] == 0xEF && (unsigned char)input[1] == 0xBB &&
        (unsigned char)input[2] == 0xBF) {
        start_pos = 3;
    }

    size_t effective_len = (len > 0) ? len : strlen(input);
    if (effective_len == 0) {
        doc->root = alloc_node(doc, YAML_NODE_SCALAR);
        if (doc->root)
            doc->root->scalar.value = AGENTOS_STRDUP("");
        return 0;
    }

    doc->source = (char *)AGENTOS_MALLOC(effective_len + 1);
    if (!doc->source)
        return AGENTOS_EINVAL;
    __builtin_memcpy(doc->source, input, effective_len);
    doc->source[effective_len] = '\0';
    doc->source_len = effective_len;

    struct parse_ctx ctx;
    __builtin_memset(&ctx, 0, sizeof(ctx));
    ctx.src = doc->source;
    ctx.len = effective_len;
    ctx.pos = start_pos;
    ctx.doc = doc;
    ctx.line = 1;
    ctx.anchor_count = 0;
    ctx.anchor_capacity = 0;
    ctx.anchors = NULL;
    ctx.tag_handles = NULL;
    ctx.tag_prefixes = NULL;
    ctx.tag_handle_count = 0;

    skip_yaml_directives(&ctx);

    if (is_document_marker(&ctx, "---")) {
        skip_document_marker(&ctx, "---");
    }

    doc->root = parse_value(&ctx, -1);

    cleanup_parse_ctx(&ctx);
    return doc->root ? 0 : -1;
}

int yaml_parse_multi(yaml_document_t *doc, const char *input, size_t len)
{
    if (!doc || !input)
        return AGENTOS_EINVAL;

    size_t start_pos = 0;
    if (len >= 3 && (unsigned char)input[0] == 0xEF && (unsigned char)input[1] == 0xBB &&
        (unsigned char)input[2] == 0xBF) {
        start_pos = 3;
    }

    size_t effective_len = (len > 0) ? len : strlen(input);

    char *source_copy = (char *)AGENTOS_MALLOC(effective_len + 1);
    if (!source_copy)
        return AGENTOS_EINVAL;
    __builtin_memcpy(source_copy, input, effective_len);
    source_copy[effective_len] = '\0';

    struct parse_ctx ctx;
    __builtin_memset(&ctx, 0, sizeof(ctx));
    ctx.src = source_copy;
    ctx.len = effective_len;
    ctx.pos = start_pos;
    ctx.doc = doc;
    ctx.line = 1;
    ctx.anchor_count = 0;
    ctx.anchor_capacity = 0;
    ctx.anchors = NULL;

    skip_yaml_directives(&ctx);
    if (is_document_marker(&ctx, "---")) {
        skip_document_marker(&ctx, "---");
    }

    doc->source = source_copy;
    doc->source_len = effective_len;
    doc->document_index = 0;
    doc->root = parse_value(&ctx, -1);

    yaml_document_t *current = doc;

    while (!at_end(&ctx)) {
        skip_ws_and_nl(&ctx);

        if (is_document_marker(&ctx, "...")) {
            skip_document_marker(&ctx, "...");
            skip_ws_and_nl(&ctx);
        }

        if (at_end(&ctx))
            break;

        skip_yaml_directives(&ctx);

        if (is_document_marker(&ctx, "---")) {
            skip_document_marker(&ctx, "---");
        } else {
            break;
        }

        yaml_document_t *next_doc = yaml_create();
        if (!next_doc)
            break;

        next_doc->source = source_copy;
        next_doc->source_len = effective_len;
        next_doc->document_index = current->document_index + 1;

        ctx.doc = next_doc;
        ctx.anchor_count = 0;

        next_doc->root = parse_value(&ctx, -1);
        current->next = next_doc;
        current = next_doc;
    }

    cleanup_parse_ctx(&ctx);
    return doc->root ? 0 : -1;
}

void yaml_destroy_chain(yaml_document_t *doc)
{
    if (!doc)
        return;
    yaml_document_t *current = doc;
    while (current) {
        yaml_document_t *next = current->next;
        current->next = NULL;
        current->source = NULL;
        yaml_destroy(current);
        current = next;
    }
}

int yaml_parse_file(yaml_document_t *doc, const char *filepath)
{
    if (!doc || !filepath)
        return AGENTOS_EINVAL;
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        doc->error_msg = (char *)AGENTOS_MALLOC(256);
        snprintf(doc->error_msg, 256, "Cannot open file: %s", filepath);
        return AGENTOS_EINVAL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 10 * 1024 * 1024) {
        fclose(f);
        doc->error_msg = AGENTOS_STRDUP("File too large");
        return AGENTOS_EINVAL;
    }
    if (sz == 0) {
        fclose(f);
        doc->root = alloc_node(doc, YAML_NODE_SCALAR);
        if (doc->root)
            doc->root->scalar.value = AGENTOS_STRDUP("");
        return 0;
    }
    char *buf = (char *)AGENTOS_MALLOC(sz + 1);
    if (!buf) {
        fclose(f);
        return AGENTOS_EINVAL;
    }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    if (rd != (size_t)sz) {
        AGENTOS_FREE(buf);
        return AGENTOS_EINVAL;
    }
    int ret = yaml_parse_string(doc, buf, rd);
    AGENTOS_FREE(buf);
    return ret;
}

const char *yaml_get_error(const yaml_document_t *doc)
{
    return doc ? doc->error_msg : NULL;
}

struct yaml_node *yaml_root(const yaml_document_t *doc)
{
    return doc ? doc->root : NULL;
}

size_t yaml_size(struct yaml_node *node)
{
    if (!node)
        return 0;
    switch (node->type) {
    case YAML_NODE_MAPPING: {
        size_t c = 0;
        for (size_t i = 0;; i++) {
            if (!node->mapping[i].key)
                break;
            c++;
        }
        return c;
    }
    case YAML_NODE_SEQUENCE:
        return node->sequence.count;
    default:
        return 0;
    }
}

bool yaml_has_key(struct yaml_node *node, const char *key)
{
    if (!node || node->type != YAML_NODE_MAPPING || !key)
        return false;
    size_t sz = yaml_size(node);
    for (size_t i = 0; i < sz; i++) {
        if (node->mapping[i].key && strcmp(node->mapping[i].key, key) == 0)
            return true;
    }
    return false;
}

struct yaml_node *yaml_get(struct yaml_node *node, const char *key)
{
    if (!node || node->type != YAML_NODE_MAPPING || !key)
        return NULL;
    size_t sz = yaml_size(node);
    for (size_t i = 0; i < sz; i++) {
        if (node->mapping[i].key && strcmp(node->mapping[i].key, key) == 0)
            return node->mapping[i].value;
    }
    return NULL;
}

struct yaml_node *yaml_get_index(struct yaml_node *node, size_t index)
{
    if (!node || node->type != YAML_NODE_SEQUENCE)
        return NULL;
    if (index >= node->sequence.count)
        return NULL;
    return node->sequence.items[index].item;
}

const char *yaml_as_string(struct yaml_node *node, const char *default_val)
{
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    return node->scalar.value;
}

long long yaml_as_int64(struct yaml_node *node, long long default_val)
{
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    char *endp = NULL;
    long long v = strtoll(node->scalar.value, &endp, 0);
    return (endp && *endp == '\0') ? v : default_val;
}

double yaml_as_double(struct yaml_node *node, double default_val)
{
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    char *endp = NULL;
    double v = strtod(node->scalar.value, &endp);
    return (endp && *endp == '\0') ? v : default_val;
}

bool yaml_as_bool(struct yaml_node *node, bool default_val)
{
    if (!node || node->type != YAML_NODE_SCALAR || !node->scalar.value)
        return default_val;
    const char *s = node->scalar.value;
    if (strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 || strcmp(s, "on") == 0 ||
        strcmp(s, "1") == 0)
        return true;
    if (strcmp(s, "false") == 0 || strcmp(s, "no") == 0 || strcmp(s, "off") == 0 ||
        strcmp(s, "0") == 0 || strcmp(s, "null") == 0 || strcmp(s, "~") == 0)
        return false;
    return default_val;
}

void yaml_dump(struct yaml_node *node, char *buf, size_t bufsize, int indent)
{
    if (!node || !buf || bufsize == 0)
        return;
    size_t off = strlen(buf);
#define APPEND(...)                                                                 \
    do {                                                                            \
        off += snprintf(buf + off, bufsize > off ? bufsize - off : 0, __VA_ARGS__); \
    } while (0)

    for (int i = 0; i < indent; i++)
        APPEND("  ");

    switch (node->type) {
    case YAML_NODE_NONE:
        APPEND("~");
        break;
    case YAML_NODE_SCALAR: {
        const char *v = node->scalar.value ? node->scalar.value : "";
        bool needs_quote = (*v == '\0') || strchr(v, ':') || strchr(v, '#') || strchr(v, '[') ||
                           strchr(v, '{') || strchr(v, ',') || strchr(v, '"') || strchr(v, '\'');
        if (needs_quote) {
            APPEND("\"%s\"", v);
        } else {
            APPEND("%s", v);
        }
        break;
    }
    case YAML_NODE_MAPPING: {
        size_t sz = yaml_size(node);
        if (sz == 0) {
            APPEND("{}");
            break;
        }
        APPEND("{\n");
        for (size_t i = 0; i < sz; i++) {
            for (int j = 0; j < indent + 1; j++)
                APPEND("  ");
            APPEND("%s: ", node->mapping[i].key);
            yaml_dump(node->mapping[i].value, buf, bufsize, indent + 1);
            if (i < sz - 1)
                APPEND(",");
            APPEND("\n");
        }
        for (int i = 0; i < indent; i++)
            APPEND("  ");
        APPEND("}");
        break;
    }
    case YAML_NODE_SEQUENCE: {
        size_t cnt = node->sequence.count;
        if (cnt == 0) {
            APPEND("[]");
            break;
        }
        APPEND("[\n");
        for (size_t i = 0; i < cnt; i++) {
            yaml_dump(node->sequence.items[i].item, buf, bufsize, indent + 1);
            if (i < cnt - 1)
                APPEND(",");
            APPEND("\n");
        }
        for (int i = 0; i < indent; i++)
            APPEND("  ");
        APPEND("]");
        break;
    }
    }
#undef APPEND
}

char *yaml_serialize(yaml_document_t *doc)
{
    if (!doc || !doc->root)
        return NULL;

    size_t bufsize = 4096;
    char *buf = (char *)AGENTOS_MALLOC(bufsize);
    if (!buf)
        return NULL;

    buf[0] = '\0';
    yaml_dump(doc->root, buf, bufsize, 0);

    size_t len = strlen(buf);
    char *result = (char *)AGENTOS_MALLOC(len + 1);
    if (result) {
        __builtin_memcpy(result, buf, len + 1);
    }
    AGENTOS_FREE(buf);
    return result;
}
