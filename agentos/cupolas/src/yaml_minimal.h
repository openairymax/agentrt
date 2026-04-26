/**
 * @file yaml_minimal.h
 * @brief Minimal YAML 1.1 subset parser for AgentOS configuration files
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * Supported YAML features:
 * - Mappings (key-value pairs, indentation-based nesting)
 * - Sequences (arrays with '-' prefix)
 * - Scalar types: string, integer, float, boolean, null
 * - Quoted strings (single and double)
 * - Comments (# to end of line)
 * - Multi-line values (literal block '|', folded '>')
 *
 * NOT supported (intentionally):
 * - Anchors (&) / Aliases (*)
 * - Tags (!!)
 * - Document markers (--- / ...)
 * - Complex keys
 */

#ifndef CUPOLAS_YAML_MINIMAL_H
#define CUPOLAS_YAML_MINIMAL_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum yaml_node_type {
    YAML_NODE_NONE = 0,
    YAML_NODE_SCALAR,
    YAML_NODE_MAPPING,
    YAML_NODE_SEQUENCE
} yaml_node_type_t;

struct yaml_scalar {
    char* value;
    size_t length;
};

struct yaml_mapping_entry {
    char* key;
    struct yaml_node* value;
};

struct yaml_sequence_item {
    struct yaml_node* item;
};

struct yaml_node {
    yaml_node_type_t type;
    union {
        struct yaml_scalar scalar;
        struct yaml_mapping_entry* mapping;
        struct { struct yaml_sequence_item* items; size_t count; } sequence;
    };
    int line;
};

typedef struct yaml_document {
    struct yaml_node* root;
    struct yaml_node** all_nodes;
    size_t node_count;
    size_t node_capacity;
    char* source;
    size_t source_len;
    char* error_msg;
} yaml_document_t;

yaml_document_t* yaml_create(void);
void yaml_destroy(yaml_document_t* doc);

int yaml_parse_string(yaml_document_t* doc, const char* input, size_t len);
int yaml_parse_file(yaml_document_t* doc, const char* filepath);

const char* yaml_get_error(const yaml_document_t* doc);

struct yaml_node* yaml_root(const yaml_document_t* doc);
struct yaml_node* yaml_get(struct yaml_node* node, const char* key);
struct yaml_node* yaml_get_index(struct yaml_node* node, size_t index);
size_t yaml_size(struct yaml_node* node);

const char* yaml_as_string(struct yaml_node* node, const char* default_val);
long long yaml_as_int64(struct yaml_node* node, long long default_val);
double yaml_as_double(struct yaml_node* node, double default_val);
bool yaml_as_bool(struct yaml_node* node, bool default_val);

bool yaml_has_key(struct yaml_node* node, const char* key);

void yaml_dump(struct yaml_node* node, char* buf, size_t bufsize, int indent);
char* yaml_serialize(yaml_document_t* doc);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_YAML_MINIMAL_H */
