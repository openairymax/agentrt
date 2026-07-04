/**
 * @file test_are_ipc.c
 * @brief ARE L2 统一 IPC 消息头单元测试
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * 验证 are_ipc.h / are_ipc.c 实现的：
 * - 结构体大小 = 128 字节（_Static_assert 已在头文件中验证）
 * - 字段偏移与规范一致（_Static_assert 已在头文件中验证）
 * - are_ipc_header_init: 正确填充各字段
 * - are_ipc_checksum_compute: CRC32 计算正确（与 zlib/crc32 兼容）
 * - are_ipc_header_validate: magic/version/reserved/checksum 校验
 * - are_ipc_message_create/destroy: 完整消息生命周期
 * - 辅助函数 are_ipc_msg_type_str / are_ipc_proto_str
 */

#include "are_ipc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 * 自定义测试框架（与项目其他测试一致）
 * ================================================================ */

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_PASS(name)                                            \
    do {                                                           \
        g_tests_passed++;                                          \
        printf("  [PASS] %s\n", name);                             \
    } while (0)

#define TEST_FAIL(name, msg)                                                  \
    do {                                                                      \
        g_tests_failed++;                                                     \
        printf("  [FAIL] %s — %s (line %d)\n", name, msg, __LINE__);          \
    } while (0)

#define TEST_ASSERT(cond, name, msg)                                          \
    do {                                                                      \
        if (!(cond)) {                                                        \
            TEST_FAIL(name, msg);                                             \
            return;                                                           \
        }                                                                     \
    } while (0)

#define RUN_TEST(func)                                                       \
    do {                                                                     \
        printf("--- %s ---\n", #func);                                       \
        func();                                                              \
    } while (0)

/* ================================================================
 * 测试用例
 * ================================================================ */

/* 已知的 CRC32-IEEE 802.3 测试向量（与 zlib/PNG/crc32 兼容） */
static const struct {
    const char *input;
    uint32_t    expected_crc;
} g_crc32_test_vectors[] = {
    {"",                 0x00000000u},
    {"a",                0xE8B7BE43u},
    {"abc",              0x352441C2u},
    {"message digest",   0x20159D7Fu},
    {"123456789",        0xCBF43926u},  /* CRC32 标准测试向量 */
    {"ARE IPC L2 header test", 0xE034613Bu}, /* 通过 python -c 'import zlib; print(hex(zlib.crc32(b"ARE IPC L2 header test")))' 计算 */
};

/* 辅助：仅计算 CRC32（不依赖 are_ipc.c 内部 API）
 * 使用相同的查表算法，独立验证 are_ipc.c 实现 */
static uint32_t test_crc32(const void *data, size_t len)
{
    static const uint32_t table[256] = {
        0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, 0x076DC419u, 0x706AF48Fu, 0xE963A535u, 0x9E6495A3u,
        0x0EDB8832u, 0x79DCB8A4u, 0xE0D5E91Eu, 0x97D2D988u, 0x09B64C2Bu, 0x7EB17CBDu, 0xE7B82D07u, 0x90BF1D91u,
        0x1DB71064u, 0x6AB020F2u, 0xF3B97148u, 0x84BE41DEu, 0x1ADAD47Du, 0x6DDDE4EBu, 0xF4D4B551u, 0x83D385C7u,
        0x136C9856u, 0x646BA8C0u, 0xFD62F97Au, 0x8A65C9ECu, 0x14015C4Fu, 0x63066CD9u, 0xFA0F3D63u, 0x8D080DF5u,
        0x3B6E20C8u, 0x4C69105Eu, 0xD56041E4u, 0xA2677172u, 0x3C03E4D1u, 0x4B04D447u, 0xD20D85FDu, 0xA50AB56Bu,
        0x35B5A8FAu, 0x42B2986Cu, 0xDBBBC9D6u, 0xACBCF940u, 0x32D86CE3u, 0x45DF5C75u, 0xDCD60DCFu, 0xABD13D59u,
        0x26D930ACu, 0x51DE003Au, 0xC8D75180u, 0xBFD06116u, 0x21B4F4B5u, 0x56B3C423u, 0xCFBA9599u, 0xB8BDA50Fu,
        0x2802B89Eu, 0x5F058808u, 0xC60CD9B2u, 0xB10BE924u, 0x2F6F7C87u, 0x58684C11u, 0xC1611DABu, 0xB6662D3Du,
        0x76DC4190u, 0x01DB7106u, 0x98D220BCu, 0xEFD5102Au, 0x71B18589u, 0x06B6B51Fu, 0x9FBFE4A5u, 0xE8B8D433u,
        0x7807C9A2u, 0x0F00F934u, 0x9609A88Eu, 0xE10E9818u, 0x7F6A0DBBu, 0x086D3D2Du, 0x91646C97u, 0xE6635C01u,
        0x6B6B51F4u, 0x1C6C6162u, 0x856530D8u, 0xF262004Eu, 0x6C0695EDu, 0x1B01A57Bu, 0x8208F4C1u, 0xF50FC457u,
        0x65B0D9C6u, 0x12B7E950u, 0x8BBEB8EAu, 0xFCB9887Cu, 0x62DD1DDFu, 0x15DA2D49u, 0x8CD37CF3u, 0xFBD44C65u,
        0x4DB26158u, 0x3AB551CEu, 0xA3BC0074u, 0xD4BB30E2u, 0x4ADFA541u, 0x3DD895D7u, 0xA4D1C46Du, 0xD3D6F4FBu,
        0x4369E96Au, 0x346ED9FCu, 0xAD678846u, 0xDA60B8D0u, 0x44042D73u, 0x33031DE5u, 0xAA0A4C5Fu, 0xDD0D7CC9u,
        0x5005713Cu, 0x270241AAu, 0xBE0B1010u, 0xC90C2086u, 0x5768B525u, 0x206F85B3u, 0xB966D409u, 0xCE61E49Fu,
        0x5EDEF90Eu, 0x29D9C998u, 0xB0D09822u, 0xC7D7A8B4u, 0x59B33D17u, 0x2EB40D81u, 0xB7BD5C3Bu, 0xC0BA6CADu,
        0xEDB88320u, 0x9ABFB3B6u, 0x03B6E20Cu, 0x74B1D29Au, 0xEAD54739u, 0x9DD277AFu, 0x04DB2615u, 0x73DC1683u,
        0xE3630B12u, 0x94643B84u, 0x0D6D6A3Eu, 0x7A6A5AA8u, 0xE40ECF0Bu, 0x9309FF9Du, 0x0A00AE27u, 0x7D079EB1u,
        0xF00F9344u, 0x8708A3D2u, 0x1E01F268u, 0x6906C2FEu, 0xF762575Du, 0x806567CBu, 0x196C3671u, 0x6E6B06E7u,
        0xFED41B76u, 0x89D32BE0u, 0x10DA7A5Au, 0x67DD4ACCu, 0xF9B9DF6Fu, 0x8EBEEFF9u, 0x17B7BE43u, 0x60B08ED5u,
        0xD6D6A3E8u, 0xA1D1937Eu, 0x38D8C2C4u, 0x4FDFF252u, 0xD1BB67F1u, 0xA6BC5767u, 0x3FB506DDu, 0x48B2364Bu,
        0xD80D2BDAu, 0xAF0A1B4Cu, 0x36034AF6u, 0x41047A60u, 0xDF60EFC3u, 0xA867DF55u, 0x316E8EEFu, 0x4669BE79u,
        0xCB61B38Cu, 0xBC66831Au, 0x256FD2A0u, 0x5268E236u, 0xCC0C7795u, 0xBB0B4703u, 0x220216B9u, 0x5505262Fu,
        0xC5BA3BBEu, 0xB2BD0B28u, 0x2BB45A92u, 0x5CB36A04u, 0xC2D7FFA7u, 0xB5D0CF31u, 0x2CD99E8Bu, 0x5BDEAE1Du,
        0x9B64C2B0u, 0xEC63F226u, 0x756AA39Cu, 0x026D930Au, 0x9C0906A9u, 0xEB0E363Fu, 0x72076785u, 0x05005713u,
        0x95BF4A82u, 0xE2B87A14u, 0x7BB12BAEu, 0x0CB61B38u, 0x92D28E9Bu, 0xE5D5BE0Du, 0x7CDCEFB7u, 0x0BDBDF21u,
        0x86D3D2D4u, 0xF1D4E242u, 0x68DDB3F8u, 0x1FDA836Eu, 0x81BE16CDu, 0xF6B9265Bu, 0x6FB077E1u, 0x18B74777u,
        0x88085AE6u, 0xFF0F6A70u, 0x66063BCAu, 0x11010B5Cu, 0x8F659EFFu, 0xF862AE69u, 0x616BFFD3u, 0x166CCF45u,
        0xA00AE278u, 0xD70DD2EEu, 0x4E048354u, 0x3903B3C2u, 0xA7672661u, 0xD06016F7u, 0x4969474Du, 0x3E6E77DBu,
        0xAED16A4Au, 0xD9D65ADCu, 0x40DF0B66u, 0x37D83BF0u, 0xA9BCAE53u, 0xDEBB9EC5u, 0x47B2CF7Fu, 0x30B5FFE9u,
        0xBDBDF21Cu, 0xCABAC28Au, 0x53B39330u, 0x24B4A3A6u, 0xBAD03605u, 0xCDD70693u, 0x54DE5729u, 0x23D967BFu,
        0xB3667A2Eu, 0xC4614AB8u, 0x5D681B02u, 0x2A6F2B94u, 0xB40BBE37u, 0xC30C8EA1u, 0x5A05DF1Bu, 0x2D02EF8Du,
    };
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ table[(crc ^ p[i]) & 0xFFu];
    }
    return crc ^ 0xFFFFFFFFu;
}

/* 前向声明：增量 CRC32 辅助函数（供 test_checksum_compute_consistency 使用）
 *
 * test_crc32_update_impl 与 test_crc32_update_inplace 的实际定义位于
 * test_checksum_compute_consistency 之后，需在此提前声明以满足 C11
 * "先声明后使用"要求。两个函数均使用与 test_crc32 / are_ipc.c 相同的
 * 查表算法，独立验证 are_ipc_checksum_compute 实现的正确性。 */
static uint32_t test_crc32_update_impl(uint32_t init, const void *data, size_t len);
static uint32_t test_crc32_update_inplace(uint32_t init, const void *data, size_t len);

/* 测试 1: 结构体大小与偏移（_Static_assert 已在头文件中强制验证，
 *        此测试运行时再次断言以防头文件被篡改） */
static void test_struct_layout(void)
{
    TEST_ASSERT(sizeof(are_ipc_message_header_t) == ARE_IPC_HEADER_SIZE,
                "sizeof(header) == 128", "header size mismatch");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, magic) == 0, "magic offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, version) == 4, "version offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, msg_type) == 6, "msg_type offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, protocol) == 8, "protocol offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, msg_id) == 12, "msg_id offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, trace_id) == 20, "trace_id offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, correlation_id) == 28, "correlation_id offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, source) == 36, "source offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, target) == 68, "target offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, payload_len) == 100, "payload_len offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, flags) == 104, "flags offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, timestamp_ns) == 108, "timestamp_ns offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, checksum) == 116, "checksum offset", "wrong");
    TEST_ASSERT(offsetof(are_ipc_message_header_t, reserved) == 120, "reserved offset", "wrong");
    TEST_PASS("struct layout (sizeof=128, all offsets match spec)");
}

/* 测试 2: are_ipc_header_init 填充字段正确 */
static void test_header_init(void)
{
    are_ipc_message_header_t h;
    agentos_error_t err = are_ipc_header_init(&h, ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                              0x123456789ABCDEF0ull, 0xCAFEBABEull,
                                              "gateway_d", "sched_d");
    TEST_ASSERT(err == AGENTOS_SUCCESS, "header_init returns SUCCESS", "init failed");

    TEST_ASSERT(h.magic == ARE_IPC_MAGIC, "magic == ARE_IPC_MAGIC", "wrong magic");
    TEST_ASSERT(h.version == ARE_IPC_VERSION, "version == 1", "wrong version");
    TEST_ASSERT(h.msg_type == (uint16_t)ARE_MSG_REQUEST, "msg_type == REQUEST", "wrong");
    TEST_ASSERT(h.protocol == (uint32_t)ARE_PROTO_JSON_RPC, "protocol == JSON_RPC", "wrong");
    TEST_ASSERT(h.msg_id == 0x123456789ABCDEF0ull, "msg_id preserved", "wrong");
    TEST_ASSERT(h.trace_id == 0xCAFEBABEull, "trace_id preserved", "wrong");
    TEST_ASSERT(h.correlation_id == h.msg_id, "correlation_id defaults to msg_id", "wrong");
    TEST_ASSERT(strcmp(h.source, "gateway_d") == 0, "source = gateway_d", "wrong");
    TEST_ASSERT(strcmp(h.target, "sched_d") == 0, "target = sched_d", "wrong");
    TEST_ASSERT(h.payload_len == 0, "payload_len = 0 (default)", "wrong");
    TEST_ASSERT(h.flags == 0, "flags = 0 (default)", "wrong");
    TEST_ASSERT(h.timestamp_ns > 0, "timestamp_ns > 0", "should be CLOCK_REALTIME");
    TEST_ASSERT(h.checksum == 0, "checksum = 0 (caller must compute)", "wrong");
    TEST_ASSERT(h.reserved == 0, "reserved = 0", "wrong");

    TEST_PASS("header_init fills all fields correctly");
}

/* 测试 3: are_ipc_header_init NULL 处理 */
static void test_header_init_null(void)
{
    are_ipc_message_header_t h;
    /* NULL source → "unknown" */
    agentos_error_t err = are_ipc_header_init(&h, ARE_MSG_NOTIFY, ARE_PROTO_MCP,
                                              1, 0, NULL, NULL);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "init with NULL source/target succeeds", "failed");
    TEST_ASSERT(strcmp(h.source, "unknown") == 0, "NULL source → 'unknown'", "wrong");
    TEST_ASSERT(strcmp(h.target, "*") == 0, "NULL target → '*'", "wrong");

    /* NULL header → AGENTOS_ERR_INVALID_PARAM */
    err = are_ipc_header_init(NULL, ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC, 1, 0, "a", "b");
    TEST_ASSERT(err == AGENTOS_ERR_INVALID_PARAM, "NULL header → EINVAL", "wrong error");

    /* 非法 msg_type → AGENTOS_ERR_INVALID_PARAM */
    err = are_ipc_header_init(&h, (are_msg_type_t)99, ARE_PROTO_JSON_RPC, 1, 0, "a", "b");
    TEST_ASSERT(err == AGENTOS_ERR_INVALID_PARAM, "invalid msg_type → EINVAL", "wrong error");

    /* 非法 protocol → AGENTOS_ERR_INVALID_PARAM */
    err = are_ipc_header_init(&h, ARE_MSG_REQUEST, (are_proto_t)99, 1, 0, "a", "b");
    TEST_ASSERT(err == AGENTOS_ERR_INVALID_PARAM, "invalid protocol → EINVAL", "wrong error");

    TEST_PASS("header_init NULL/invalid param handling");
}

/* 测试 4: source/target 截断（> 31 字节） */
static void test_header_init_truncation(void)
{
    are_ipc_message_header_t h;
    const char *long_source = "this_is_a_very_long_daemon_name_exceeding_31_chars_xxxxxx";
    agentos_error_t err = are_ipc_header_init(&h, ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                              1, 0, long_source, "sched_d");
    TEST_ASSERT(err == AGENTOS_SUCCESS, "init with long source succeeds", "failed");
    TEST_ASSERT(h.source[ARE_IPC_MAX_SOURCE_LEN - 1] == '\0', "source NULL-terminated", "not terminated");
    TEST_ASSERT(strlen(h.source) == ARE_IPC_MAX_SOURCE_LEN - 1, "source truncated to 31", "wrong length");
    TEST_ASSERT(strncmp(h.source, long_source, ARE_IPC_MAX_SOURCE_LEN - 1) == 0,
                "source prefix matches", "wrong content");
    TEST_PASS("header_init truncates long source/target");
}

/* 测试 5: CRC32 实现与标准测试向量兼容 */
static void test_crc32_vectors(void)
{
    for (size_t i = 0; i < sizeof(g_crc32_test_vectors) / sizeof(g_crc32_test_vectors[0]); i++) {
        const char *input = g_crc32_test_vectors[i].input;
        size_t len = strlen(input);
        uint32_t expected = g_crc32_test_vectors[i].expected_crc;
        uint32_t actual = test_crc32(input, len);
        /* 验证测试向量本身（参考实现） */
        if (actual != expected) {
            char msg[128];
            snprintf(msg, sizeof(msg), "vector[%zu] '%s': expected=0x%08X actual=0x%08X",
                     i, input, expected, actual);
            TEST_FAIL("crc32 reference impl", msg);
            return;
        }
    }
    TEST_PASS("crc32 reference impl matches standard test vectors");
}

/* 测试 6: are_ipc_checksum_compute 与独立实现一致 */
static void test_checksum_compute_consistency(void)
{
    are_ipc_message_header_t h;
    agentos_error_t err = are_ipc_header_init(&h, ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                              42, 0xDEAD, "gateway_d", "sched_d");
    TEST_ASSERT(err == AGENTOS_SUCCESS, "init succeeds", "failed");

    const char *payload = "{\"jsonrpc\":\"2.0\",\"method\":\"sched.register_agent\"}";
    size_t payload_len = strlen(payload);

    /* are_ipc.c 实现 */
    uint32_t api_crc = are_ipc_checksum_compute(&h, payload, payload_len);

    /* 独立验证：手动构造相同输入（header[0:120) + payload，checksum 视为 0） */
    are_ipc_message_header_t tmp;
    __builtin_memcpy(&tmp, &h, sizeof(tmp));
    tmp.checksum = 0u;
    uint32_t ref_crc = 0xFFFFFFFFu;
    /* header[0:120) = magic ~ checksum 字段（checksum 以 0 参与），共 120 字节 */
    ref_crc = test_crc32_update_inplace(ref_crc, &tmp, offsetof(are_ipc_message_header_t, checksum) + 4u);
    ref_crc = test_crc32_update_inplace(ref_crc, payload, payload_len);
    ref_crc ^= 0xFFFFFFFFu;

    if (api_crc != ref_crc) {
        char msg[128];
        snprintf(msg, sizeof(msg), "api=0x%08X ref=0x%08X", api_crc, ref_crc);
        TEST_FAIL("checksum consistency", msg);
        return;
    }
    TEST_PASS("are_ipc_checksum_compute matches independent impl");
}

/* 辅助：test_crc32 的增量版本（与 test_crc32 共享查表） */
static uint32_t test_crc32_update_inplace(uint32_t init, const void *data, size_t len)
{
    return test_crc32_update_impl(init, data, len);
}

/* 测试 7: are_ipc_header_validate 通过合法消息 */
static void test_validate_valid(void)
{
    are_ipc_message_t *msg = NULL;
    const char *payload = "{\"jsonrpc\":\"2.0\",\"method\":\"llm.complete\",\"id\":1}";
    size_t payload_len = strlen(payload);

    agentos_error_t err = are_ipc_message_create(ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                                 100, 0x7FACE123ull, "llm_d", "sched_d",
                                                 payload, payload_len, &msg);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "message_create succeeds", "failed");
    TEST_ASSERT(msg != NULL, "msg != NULL", "null");

    err = are_ipc_header_validate(&msg->header, msg->payload, msg->header.payload_len);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "validate returns SUCCESS", "validation failed");

    are_ipc_message_destroy(msg);
    TEST_PASS("header_validate accepts valid message");
}

/* 测试 8: are_ipc_header_validate 拒绝错误的 magic */
static void test_validate_bad_magic(void)
{
    are_ipc_message_t *msg = NULL;
    agentos_error_t err = are_ipc_message_create(ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                                 1, 0, "a", "b", "x", 1, &msg);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create succeeds", "failed");

    /* 篡改 magic */
    msg->header.magic = 0xDEADBEEFu;
    err = are_ipc_header_validate(&msg->header, msg->payload, msg->header.payload_len);
    TEST_ASSERT(err == AGENTOS_ERR_PROTOCOL, "bad magic → ERR_PROTOCOL", "wrong error");

    are_ipc_message_destroy(msg);
    TEST_PASS("header_validate rejects bad magic");
}

/* 测试 9: are_ipc_header_validate 拒绝错误的 version */
static void test_validate_bad_version(void)
{
    are_ipc_message_t *msg = NULL;
    agentos_error_t err = are_ipc_message_create(ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                                 1, 0, "a", "b", "x", 1, &msg);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create succeeds", "failed");

    msg->header.version = 999u;
    err = are_ipc_header_validate(&msg->header, msg->payload, msg->header.payload_len);
    TEST_ASSERT(err == AGENTOS_ERR_PROTOCOL, "bad version → ERR_PROTOCOL", "wrong error");

    are_ipc_message_destroy(msg);
    TEST_PASS("header_validate rejects bad version");
}

/* 测试 10: are_ipc_header_validate 拒绝非零 reserved */
static void test_validate_bad_reserved(void)
{
    are_ipc_message_t *msg = NULL;
    agentos_error_t err = are_ipc_message_create(ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                                 1, 0, "a", "b", "x", 1, &msg);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create succeeds", "failed");

    msg->header.reserved = 1u;
    /* 重新计算 checksum 使其通过 checksum 校验（验证 reserved 独立校验） */
    msg->header.checksum = are_ipc_checksum_compute(&msg->header, msg->payload, msg->header.payload_len);
    err = are_ipc_header_validate(&msg->header, msg->payload, msg->header.payload_len);
    TEST_ASSERT(err == AGENTOS_ERR_PROTOCOL, "non-zero reserved → ERR_PROTOCOL", "wrong error");

    are_ipc_message_destroy(msg);
    TEST_PASS("header_validate rejects non-zero reserved");
}

/* 测试 10.5: are_ipc_header_validate 拒绝非零 _abi_pad */
static void test_validate_bad_abi_pad(void)
{
    are_ipc_message_t *msg = NULL;
    agentos_error_t err = are_ipc_message_create(ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                                 1, 0, "a", "b", "x", 1, &msg);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create succeeds", "failed");

    /* 篡改 _abi_pad（ABI 填充字段，必须为 0） */
    msg->header._abi_pad = 1u;
    /* 重新计算 checksum 使其通过 checksum 校验（验证 _abi_pad 独立校验） */
    msg->header.checksum = are_ipc_checksum_compute(&msg->header, msg->payload, msg->header.payload_len);
    err = are_ipc_header_validate(&msg->header, msg->payload, msg->header.payload_len);
    TEST_ASSERT(err == AGENTOS_ERR_PROTOCOL, "non-zero _abi_pad → ERR_PROTOCOL", "wrong error");

    are_ipc_message_destroy(msg);
    TEST_PASS("header_validate rejects non-zero _abi_pad");
}

/* 测试 11: are_ipc_header_validate 拒绝错误的 checksum */
static void test_validate_bad_checksum(void)
{
    are_ipc_message_t *msg = NULL;
    agentos_error_t err = are_ipc_message_create(ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                                 1, 0, "a", "b", "x", 1, &msg);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create succeeds", "failed");

    msg->header.checksum ^= 0xFFFFu;  /* 翻转部分位 */
    err = are_ipc_header_validate(&msg->header, msg->payload, msg->header.payload_len);
    TEST_ASSERT(err == AGENTOS_ERR_CHECKSUM, "bad checksum → ERR_CHECKSUM", "wrong error");

    are_ipc_message_destroy(msg);
    TEST_PASS("header_validate rejects bad checksum");
}

/* 测试 12: are_ipc_message_create/destroy 完整生命周期 */
static void test_message_lifecycle(void)
{
    are_ipc_message_t *msg = NULL;
    const char *payload = "{\"test\":\"data\"}";
    size_t payload_len = strlen(payload);

    agentos_error_t err = are_ipc_message_create(ARE_MSG_RESPONSE, ARE_PROTO_A2A,
                                                 42, 0xBEEF, "sched_d", "gateway_d",
                                                 payload, payload_len, &msg);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create succeeds", "failed");
    TEST_ASSERT(msg != NULL, "msg != NULL", "null");
    TEST_ASSERT(msg->payload != NULL, "payload != NULL", "null");
    TEST_ASSERT(msg->payload_size == payload_len, "payload_size == len", "wrong");
    TEST_ASSERT(msg->header.payload_len == payload_len, "header.payload_len == len", "wrong");
    TEST_ASSERT(msg->header.magic == ARE_IPC_MAGIC, "magic set", "wrong");
    TEST_ASSERT(msg->header.checksum != 0, "checksum computed", "zero");
    TEST_ASSERT(memcmp(msg->payload, payload, payload_len) == 0, "payload copied", "mismatch");

    /* 立即验证创建的消息 */
    err = are_ipc_header_validate(&msg->header, msg->payload, msg->header.payload_len);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "created message validates", "validation failed");

    are_ipc_message_destroy(msg);

    /* 测试零长度 payload */
    err = are_ipc_message_create(ARE_MSG_NOTIFY, ARE_PROTO_MCP, 1, 0, "a", "b",
                                 NULL, 0, &msg);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create with NULL payload succeeds", "failed");
    TEST_ASSERT(msg->payload == NULL, "payload NULL for zero len", "non-null");
    TEST_ASSERT(msg->payload_size == 0, "payload_size == 0", "wrong");
    are_ipc_message_destroy(msg);

    /* 测试超限 payload */
    err = are_ipc_message_create(ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC, 1, 0, "a", "b",
                                 NULL, ARE_IPC_MAX_PAYLOAD + 1, &msg);
    TEST_ASSERT(err == AGENTOS_ERR_INVALID_PARAM, "oversize payload → EINVAL", "wrong");
    TEST_ASSERT(msg == NULL, "msg remains NULL on error", "non-null");

    /* 测试 payload_len > 0 但 payload=NULL */
    err = are_ipc_message_create(ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC, 1, 0, "a", "b",
                                 NULL, 10, &msg);
    TEST_ASSERT(err == AGENTOS_ERR_INVALID_PARAM, "NULL payload with len>0 → EINVAL", "wrong");

    /* destroy(NULL) 安全 */
    are_ipc_message_destroy(NULL);

    TEST_PASS("message_create/destroy lifecycle");
}

/* 测试 13: 辅助函数 are_ipc_msg_type_str / are_ipc_proto_str */
static void test_helper_strings(void)
{
    TEST_ASSERT(strcmp(are_ipc_msg_type_str(ARE_MSG_REQUEST), "REQUEST") == 0,
                "msg_type_str(REQUEST)", "wrong");
    TEST_ASSERT(strcmp(are_ipc_msg_type_str(ARE_MSG_RESPONSE), "RESPONSE") == 0,
                "msg_type_str(RESPONSE)", "wrong");
    TEST_ASSERT(strcmp(are_ipc_msg_type_str(ARE_MSG_NOTIFY), "NOTIFY") == 0,
                "msg_type_str(NOTIFY)", "wrong");
    TEST_ASSERT(strcmp(are_ipc_msg_type_str(ARE_MSG_ERROR), "ERROR") == 0,
                "msg_type_str(ERROR)", "wrong");
    TEST_ASSERT(strcmp(are_ipc_msg_type_str((are_msg_type_t)99), "UNKNOWN") == 0,
                "msg_type_str(99) = UNKNOWN", "wrong");

    TEST_ASSERT(strcmp(are_ipc_proto_str(ARE_PROTO_JSON_RPC), "JSON-RPC") == 0,
                "proto_str(JSON_RPC)", "wrong");
    TEST_ASSERT(strcmp(are_ipc_proto_str(ARE_PROTO_MCP), "MCP") == 0,
                "proto_str(MCP)", "wrong");
    TEST_ASSERT(strcmp(are_ipc_proto_str(ARE_PROTO_A2A), "A2A") == 0,
                "proto_str(A2A)", "wrong");
    TEST_ASSERT(strcmp(are_ipc_proto_str(ARE_PROTO_OPENAI), "OpenAI") == 0,
                "proto_str(OPENAI)", "wrong");
    TEST_ASSERT(strcmp(are_ipc_proto_str(ARE_PROTO_CUSTOM), "CUSTOM") == 0,
                "proto_str(CUSTOM)", "wrong");
    TEST_ASSERT(strcmp(are_ipc_proto_str((are_proto_t)99), "UNKNOWN") == 0,
                "proto_str(99) = UNKNOWN", "wrong");

    TEST_PASS("helper string functions");
}

/* 测试 14: trace_id 跨进程贯穿（echo 语义验证） */
static void test_trace_id_echo(void)
{
    const uint64_t trace_id = 0xABCDEF0123456789ull;

    /* 客户端发起 REQUEST */
    are_ipc_message_t *req = NULL;
    agentos_error_t err = are_ipc_message_create(ARE_MSG_REQUEST, ARE_PROTO_JSON_RPC,
                                                 42, trace_id, "gateway_d", "sched_d",
                                                 "{\"method\":\"sched.register_agent\"}", 30, &req);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create REQUEST", "failed");
    TEST_ASSERT(req->header.trace_id == trace_id, "REQUEST trace_id preserved", "wrong");

    /* 服务端返回 RESPONSE，必须 echo trace_id */
    are_ipc_message_t *resp = NULL;
    err = are_ipc_message_create(ARE_MSG_RESPONSE, ARE_PROTO_JSON_RPC,
                                 req->header.msg_id, req->header.trace_id,
                                 "sched_d", "gateway_d",
                                 "{\"result\":\"ok\"}", 15, &resp);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "create RESPONSE", "failed");
    TEST_ASSERT(resp->header.trace_id == req->header.trace_id, "RESPONSE echo trace_id", "wrong");
    TEST_ASSERT(resp->header.msg_id == req->header.msg_id, "RESPONSE echo msg_id", "wrong");
    TEST_ASSERT(strcmp(resp->header.source, "sched_d") == 0, "RESPONSE source = sched_d", "wrong");
    TEST_ASSERT(strcmp(resp->header.target, "gateway_d") == 0, "RESPONSE target = gateway_d", "wrong");

    are_ipc_message_destroy(req);
    are_ipc_message_destroy(resp);
    TEST_PASS("trace_id echo semantics for REQUEST/RESPONSE");
}

/* ================================================================
 * 辅助函数实现
 *
 * test_crc32_update_inplace 已在 test_checksum_compute_consistency 之前
 * 通过前向声明可见，此处仅保留 test_crc32_update_impl 的定义。
 * ================================================================ */

static uint32_t test_crc32_update_impl(uint32_t init, const void *data, size_t len)
{
    /* 复用 test_crc32 的查表（但 test_crc32 每次都做 0xFFFFFFFF XOR，不能直接复用）
     * 此处独立实现增量版本 */
    static const uint32_t table[256] = {
        0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, 0x076DC419u, 0x706AF48Fu, 0xE963A535u, 0x9E6495A3u,
        0x0EDB8832u, 0x79DCB8A4u, 0xE0D5E91Eu, 0x97D2D988u, 0x09B64C2Bu, 0x7EB17CBDu, 0xE7B82D07u, 0x90BF1D91u,
        0x1DB71064u, 0x6AB020F2u, 0xF3B97148u, 0x84BE41DEu, 0x1ADAD47Du, 0x6DDDE4EBu, 0xF4D4B551u, 0x83D385C7u,
        0x136C9856u, 0x646BA8C0u, 0xFD62F97Au, 0x8A65C9ECu, 0x14015C4Fu, 0x63066CD9u, 0xFA0F3D63u, 0x8D080DF5u,
        0x3B6E20C8u, 0x4C69105Eu, 0xD56041E4u, 0xA2677172u, 0x3C03E4D1u, 0x4B04D447u, 0xD20D85FDu, 0xA50AB56Bu,
        0x35B5A8FAu, 0x42B2986Cu, 0xDBBBC9D6u, 0xACBCF940u, 0x32D86CE3u, 0x45DF5C75u, 0xDCD60DCFu, 0xABD13D59u,
        0x26D930ACu, 0x51DE003Au, 0xC8D75180u, 0xBFD06116u, 0x21B4F4B5u, 0x56B3C423u, 0xCFBA9599u, 0xB8BDA50Fu,
        0x2802B89Eu, 0x5F058808u, 0xC60CD9B2u, 0xB10BE924u, 0x2F6F7C87u, 0x58684C11u, 0xC1611DABu, 0xB6662D3Du,
        0x76DC4190u, 0x01DB7106u, 0x98D220BCu, 0xEFD5102Au, 0x71B18589u, 0x06B6B51Fu, 0x9FBFE4A5u, 0xE8B8D433u,
        0x7807C9A2u, 0x0F00F934u, 0x9609A88Eu, 0xE10E9818u, 0x7F6A0DBBu, 0x086D3D2Du, 0x91646C97u, 0xE6635C01u,
        0x6B6B51F4u, 0x1C6C6162u, 0x856530D8u, 0xF262004Eu, 0x6C0695EDu, 0x1B01A57Bu, 0x8208F4C1u, 0xF50FC457u,
        0x65B0D9C6u, 0x12B7E950u, 0x8BBEB8EAu, 0xFCB9887Cu, 0x62DD1DDFu, 0x15DA2D49u, 0x8CD37CF3u, 0xFBD44C65u,
        0x4DB26158u, 0x3AB551CEu, 0xA3BC0074u, 0xD4BB30E2u, 0x4ADFA541u, 0x3DD895D7u, 0xA4D1C46Du, 0xD3D6F4FBu,
        0x4369E96Au, 0x346ED9FCu, 0xAD678846u, 0xDA60B8D0u, 0x44042D73u, 0x33031DE5u, 0xAA0A4C5Fu, 0xDD0D7CC9u,
        0x5005713Cu, 0x270241AAu, 0xBE0B1010u, 0xC90C2086u, 0x5768B525u, 0x206F85B3u, 0xB966D409u, 0xCE61E49Fu,
        0x5EDEF90Eu, 0x29D9C998u, 0xB0D09822u, 0xC7D7A8B4u, 0x59B33D17u, 0x2EB40D81u, 0xB7BD5C3Bu, 0xC0BA6CADu,
        0xEDB88320u, 0x9ABFB3B6u, 0x03B6E20Cu, 0x74B1D29Au, 0xEAD54739u, 0x9DD277AFu, 0x04DB2615u, 0x73DC1683u,
        0xE3630B12u, 0x94643B84u, 0x0D6D6A3Eu, 0x7A6A5AA8u, 0xE40ECF0Bu, 0x9309FF9Du, 0x0A00AE27u, 0x7D079EB1u,
        0xF00F9344u, 0x8708A3D2u, 0x1E01F268u, 0x6906C2FEu, 0xF762575Du, 0x806567CBu, 0x196C3671u, 0x6E6B06E7u,
        0xFED41B76u, 0x89D32BE0u, 0x10DA7A5Au, 0x67DD4ACCu, 0xF9B9DF6Fu, 0x8EBEEFF9u, 0x17B7BE43u, 0x60B08ED5u,
        0xD6D6A3E8u, 0xA1D1937Eu, 0x38D8C2C4u, 0x4FDFF252u, 0xD1BB67F1u, 0xA6BC5767u, 0x3FB506DDu, 0x48B2364Bu,
        0xD80D2BDAu, 0xAF0A1B4Cu, 0x36034AF6u, 0x41047A60u, 0xDF60EFC3u, 0xA867DF55u, 0x316E8EEFu, 0x4669BE79u,
        0xCB61B38Cu, 0xBC66831Au, 0x256FD2A0u, 0x5268E236u, 0xCC0C7795u, 0xBB0B4703u, 0x220216B9u, 0x5505262Fu,
        0xC5BA3BBEu, 0xB2BD0B28u, 0x2BB45A92u, 0x5CB36A04u, 0xC2D7FFA7u, 0xB5D0CF31u, 0x2CD99E8Bu, 0x5BDEAE1Du,
        0x9B64C2B0u, 0xEC63F226u, 0x756AA39Cu, 0x026D930Au, 0x9C0906A9u, 0xEB0E363Fu, 0x72076785u, 0x05005713u,
        0x95BF4A82u, 0xE2B87A14u, 0x7BB12BAEu, 0x0CB61B38u, 0x92D28E9Bu, 0xE5D5BE0Du, 0x7CDCEFB7u, 0x0BDBDF21u,
        0x86D3D2D4u, 0xF1D4E242u, 0x68DDB3F8u, 0x1FDA836Eu, 0x81BE16CDu, 0xF6B9265Bu, 0x6FB077E1u, 0x18B74777u,
        0x88085AE6u, 0xFF0F6A70u, 0x66063BCAu, 0x11010B5Cu, 0x8F659EFFu, 0xF862AE69u, 0x616BFFD3u, 0x166CCF45u,
        0xA00AE278u, 0xD70DD2EEu, 0x4E048354u, 0x3903B3C2u, 0xA7672661u, 0xD06016F7u, 0x4969474Du, 0x3E6E77DBu,
        0xAED16A4Au, 0xD9D65ADCu, 0x40DF0B66u, 0x37D83BF0u, 0xA9BCAE53u, 0xDEBB9EC5u, 0x47B2CF7Fu, 0x30B5FFE9u,
        0xBDBDF21Cu, 0xCABAC28Au, 0x53B39330u, 0x24B4A3A6u, 0xBAD03605u, 0xCDD70693u, 0x54DE5729u, 0x23D967BFu,
        0xB3667A2Eu, 0xC4614AB8u, 0x5D681B02u, 0x2A6F2B94u, 0xB40BBE37u, 0xC30C8EA1u, 0x5A05DF1Bu, 0x2D02EF8Du,
    };
    uint32_t crc = init;
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ table[(crc ^ p[i]) & 0xFFu];
    }
    return crc;
}

/* ================================================================
 * main
 * ================================================================ */

int main(void)
{
    printf("========================================================\n");
    printf("  ARE L2 统一 IPC 消息头单元测试\n");
    printf("  sizeof(are_ipc_message_header_t) = %zu\n",
           sizeof(are_ipc_message_header_t));
    printf("========================================================\n\n");

    RUN_TEST(test_struct_layout);
    RUN_TEST(test_header_init);
    RUN_TEST(test_header_init_null);
    RUN_TEST(test_header_init_truncation);
    RUN_TEST(test_crc32_vectors);
    RUN_TEST(test_checksum_compute_consistency);
    RUN_TEST(test_validate_valid);
    RUN_TEST(test_validate_bad_magic);
    RUN_TEST(test_validate_bad_version);
    RUN_TEST(test_validate_bad_reserved);
    RUN_TEST(test_validate_bad_abi_pad);
    RUN_TEST(test_validate_bad_checksum);
    RUN_TEST(test_message_lifecycle);
    RUN_TEST(test_helper_strings);
    RUN_TEST(test_trace_id_echo);

    printf("\n========================================================\n");
    printf("  ARE IPC 测试结果: %d/%d 通过, %d 失败\n",
           g_tests_passed, g_tests_passed + g_tests_failed, g_tests_failed);
    printf("========================================================\n");

    return g_tests_failed == 0 ? 0 : 1;
}
