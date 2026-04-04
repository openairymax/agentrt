/**
 * @file test_layer3_structure.c
 * @brief L3结构层单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 测试L3层的图结构、序列结构、关系处理等功能
 * 遵循 ARCHITECTURAL_PRINCIPLES.md 的 E-8 可测试性原则
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include "layer3_structure.h"

#define MAX_NODES 100
#define MAX_EDGES 500

int test_graph_creation(void) {
    printf("  测试图结构创建...\n");

    agentos_graph_t* graph = NULL;
    int err = agentos_graph_create(MAX_NODES, MAX_EDGES, &graph);
    if (err != 0 || graph == NULL) {
        printf("    创建图结构失败\n");
        return 1;
    }

    agentos_graph_destroy(graph);
    printf("    图结构创建测试通过\n");
    return 0;
}

int test_graph_node_operations(void) {
    printf("  测试图节点操作...\n");

    agentos_graph_t* graph = NULL;
    agentos_graph_create(MAX_NODES, MAX_EDGES, &graph);

    agentos_node_id_t node_id;
    int err = agentos_graph_add_node(graph, "test_node", &node_id);
    if (err != 0) {
        printf("    添加节点失败\n");
        agentos_graph_destroy(graph);
        return 1;
    }

    const char* node_name = NULL;
    err = agentos_graph_get_node_name(graph, node_id, &node_name);
    if (err != 0 || node_name == NULL) {
        printf("    获取节点名称失败\n");
        agentos_graph_destroy(graph);
        return 1;
    }

    agentos_graph_destroy(graph);
    printf("    图节点操作测试通过\n");
    return 0;
}

int test_graph_edge_operations(void) {
    printf("  测试图边操作...\n");

    agentos_graph_t* graph = NULL;
    agentos_graph_create(MAX_NODES, MAX_EDGES, &graph);

    agentos_node_id_t node1, node2;
    agentos_graph_add_node(graph, "node1", &node1);
    agentos_graph_add_node(graph, "node2", &node2);

    agentos_edge_id_t edge_id;
    int err = agentos_graph_add_edge(graph, node1, node2, "relates_to", &edge_id);
    if (err != 0) {
        printf("    添加边失败\n");
        agentos_graph_destroy(graph);
        return 1;
    }

    agentos_graph_destroy(graph);
    printf("    图边操作测试通过\n");
    return 0;
}

int test_graph_traversal(void) {
    printf("  测试图遍历...\n");

    agentos_graph_t* graph = NULL;
    agentos_graph_create(MAX_NODES, MAX_EDGES, &graph);

    agentos_node_id_t nodes[5];
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "node_%d", i);
        agentos_graph_add_node(graph, name, &nodes[i]);
    }

    for (int i = 0; i < 4; i++) {
        agentos_graph_add_edge(graph, nodes[i], nodes[i + 1], "next", NULL);
    }

    agentos_node_id_t neighbors[10];
    size_t neighbor_count = 0;
    int err = agentos_graph_get_neighbors(graph, nodes[0], neighbors, 10, &neighbor_count);

    if (err != 0) {
        printf("    获取邻居节点失败\n");
        agentos_graph_destroy(graph);
        return 1;
    }

    agentos_graph_destroy(graph);
    printf("    图遍历测试通过 (找到 %zu 个邻居)\n", neighbor_count);
    return 0;
}

int test_sequence_creation(void) {
    printf("  测试序列结构创建...\n");

    agentos_sequence_t* seq = NULL;
    int err = agentos_sequence_create(&seq);
    if (err != 0 || seq == NULL) {
        printf("    创建序列结构失败\n");
        return 1;
    }

    agentos_sequence_destroy(seq);
    printf("    序列结构创建测试通过\n");
    return 0;
}

int test_sequence_operations(void) {
    printf("  测试序列操作...\n");

    agentos_sequence_t* seq = NULL;
    agentos_sequence_create(&seq);

    for (int i = 0; i < 10; i++) {
        char item[32];
        snprintf(item, sizeof(item), "item_%d", i);
        agentos_sequence_push_back(seq, item);
    }

    size_t length = 0;
    agentos_sequence_length(seq, &length);
    if (length != 10) {
        printf("    序列长度错误: 期望10，实际%zu\n", length);
        agentos_sequence_destroy(seq);
        return 1;
    }

    agentos_sequence_destroy(seq);
    printf("    序列操作测试通过\n");
    return 0;
}

int test_sequence_access(void) {
    printf("  测试序列访问...\n");

    agentos_sequence_t* seq = NULL;
    agentos_sequence_create(&seq);

    const char* items[] = {"a", "b", "c", "d", "e"};
    for (int i = 0; i < 5; i++) {
        agentos_sequence_push_back(seq, items[i]);
    }

    const char* item = NULL;
    int err = agentos_sequence_get(seq, 2, &item);
    if (err != 0 || item == NULL || strcmp(item, "c") != 0) {
        printf("    序列元素访问失败\n");
        agentos_sequence_destroy(seq);
        return 1;
    }

    agentos_sequence_destroy(seq);
    printf("    序列访问测试通过\n");
    return 0;
}

int test_relation_creation(void) {
    printf("  测试关系结构创建...\n");

    agentos_relation_t* rel = NULL;
    int err = agentos_relation_create("depends_on", &rel);
    if (err != 0 || rel == NULL) {
        printf("    创建关系结构失败\n");
        return 1;
    }

    agentos_relation_destroy(rel);
    printf("    关系结构创建测试通过\n");
    return 0;
}

int test_relation_binding(void) {
    printf("  测试关系绑定...\n");

    agentos_binder_t* binder = NULL;
    int err = agentos_binder_create(&binder);
    if (err != 0 || binder == NULL) {
        printf("    创建绑定器失败\n");
        return 1;
    }

    err = agentos_binder_bind(binder, "entity1", "entity2", "related_to");
    if (err != 0) {
        printf("    绑定实体失败\n");
        agentos_binder_destroy(binder);
        return 1;
    }

    agentos_binder_destroy(binder);
    printf("    关系绑定测试通过\n");
    return 0;
}

int test_relation_unbinding(void) {
    printf("  测试关系解绑...\n");

    agentos_binder_t* binder = NULL;
    agentos_binder_create(&binder);

    agentos_binder_bind(binder, "entity1", "entity2", "related_to");

    int err = agentos_binder_unbind(binder, "entity1", "entity2", "related_to");
    if (err != 0) {
        printf("    解绑实体失败\n");
        agentos_binder_destroy(binder);
        return 1;
    }

    agentos_binder_destroy(binder);
    printf("    关系解绑测试通过\n");
    return 0;
}

int test_binder_query(void) {
    printf("  测试绑定器查询...\n");

    agentos_binder_t* binder = NULL;
    agentos_binder_create(&binder);

    agentos_binder_bind(binder, "A", "B", "depends_on");
    agentos_binder_bind(binder, "A", "C", "depends_on");
    agentos_binder_bind(binder, "B", "C", "depends_on");

    const char* targets[10];
    size_t count = 0;
    int err = agentos_binder_query(binder, "A", "depends_on", targets, 10, &count);

    if (err != 0) {
        printf("    查询绑定关系失败\n");
        agentos_binder_destroy(binder);
        return 1;
    }

    if (count != 2) {
        printf("    查询结果数量错误: 期望2，实际%zu\n", count);
        agentos_binder_destroy(binder);
        return 1;
    }

    agentos_binder_destroy(binder);
    printf("    绑定器查询测试通过 (找到 %zu 个关系)\n", count);
    return 0;
}

int main(void) {
    printf("开始运行 memoryrovol L3 结构层单元测试...\n");

    int failures = 0;

    failures |= test_graph_creation();
    failures |= test_graph_node_operations();
    failures |= test_graph_edge_operations();
    failures |= test_graph_traversal();
    failures |= test_sequence_creation();
    failures |= test_sequence_operations();
    failures |= test_sequence_access();
    failures |= test_relation_creation();
    failures |= test_relation_binding();
    failures |= test_relation_unbinding();
    failures |= test_binder_query();

    if (failures == 0) {
        printf("\n所有L3结构层测试通过！\n");
        return 0;
    } else {
        printf("\n%d 个L3结构层测试失败\n", failures);
        return 1;
    }
}
