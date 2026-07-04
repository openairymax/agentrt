/**
 * @file are_ipc.c
 * @brief ARE L2 统一 IPC 消息头实现（CRC32 + 初始化 + 校验）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @details
 * 实现 are_ipc.h 中声明的 API：
 * - are_ipc_header_init: 初始化消息头为有效的 L2 REQUEST/RESPONSE/NOTIFY/ERROR
 * - are_ipc_checksum_compute: 计算 CRC32-IEEE 802.3（覆盖 header[0:116] + payload）
 * - are_ipc_header_validate: 校验 magic/version/reserved/payload_len/checksum
 * - are_ipc_message_create/destroy: 完整消息生命周期
 * - are_ipc_msg_type_str/are_ipc_proto_str: 辅助字符串转换
 *
 * CRC32 实现使用 IEEE 802.3 多项式 0xEDB88320（反射形式），
 * 与 zlib/PNG/crc32 兼容。256 项查表作为编译时 const 数组，
 * 零运行时初始化、无线程安全问题。
 *
 * @see Docs/Capital_Specifications/are_standards/L2_service_protocol.md §2
 */

#include "are_ipc.h"

#include "agentos_time.h"
#include "memory_compat.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ================================================================
 * CRC32-IEEE 802.3 查表实现（编译时 const，零运行时初始化）
 * ================================================================ */

/* 反射多项式：x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^11 +
 *            x^10 + x^8 + x^7 + x^5 + x^4 + x^2 + x + 1
 * 即 0xEDB88320（zlib/PNG/crc32 兼容）
 *
 * 表为编译时常量，避免任何运行时初始化与线程安全问题。
 * 生成脚本：见 are_ipc.c 末尾注释。 */
static const uint32_t g_crc32_table[256] = {
    0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, 0x076DC419u, 0x706AF48Fu, 0xE963A535u, 0x9E6495A3u, 0x0EDB8832u, 0x79DCB8A4u, 0xE0D5E91Eu, 0x97D2D988u, 0x09B64C2Bu, 0x7EB17CBDu, 0xE7B82D07u, 0x90BF1D91u,
    0x1DB71064u, 0x6AB020F2u, 0xF3B97148u, 0x84BE41DEu, 0x1ADAD47Du, 0x6DDDE4EBu, 0xF4D4B551u, 0x83D385C7u, 0x136C9856u, 0x646BA8C0u, 0xFD62F97Au, 0x8A65C9ECu, 0x14015C4Fu, 0x63066CD9u, 0xFA0F3D63u, 0x8D080DF5u,
    0x3B6E20C8u, 0x4C69105Eu, 0xD56041E4u, 0xA2677172u, 0x3C03E4D1u, 0x4B04D447u, 0xD20D85FDu, 0xA50AB56Bu, 0x35B5A8FAu, 0x42B2986Cu, 0xDBBBC9D6u, 0xACBCF940u, 0x32D86CE3u, 0x45DF5C75u, 0xDCD60DCFu, 0xABD13D59u,
    0x26D930ACu, 0x51DE003Au, 0xC8D75180u, 0xBFD06116u, 0x21B4F4B5u, 0x56B3C423u, 0xCFBA9599u, 0xB8BDA50Fu, 0x2802B89Eu, 0x5F058808u, 0xC60CD9B2u, 0xB10BE924u, 0x2F6F7C87u, 0x58684C11u, 0xC1611DABu, 0xB6662D3Du,
    0x76DC4190u, 0x01DB7106u, 0x98D220BCu, 0xEFD5102Au, 0x71B18589u, 0x06B6B51Fu, 0x9FBFE4A5u, 0xE8B8D433u, 0x7807C9A2u, 0x0F00F934u, 0x9609A88Eu, 0xE10E9818u, 0x7F6A0DBBu, 0x086D3D2Du, 0x91646C97u, 0xE6635C01u,
    0x6B6B51F4u, 0x1C6C6162u, 0x856530D8u, 0xF262004Eu, 0x6C0695EDu, 0x1B01A57Bu, 0x8208F4C1u, 0xF50FC457u, 0x65B0D9C6u, 0x12B7E950u, 0x8BBEB8EAu, 0xFCB9887Cu, 0x62DD1DDFu, 0x15DA2D49u, 0x8CD37CF3u, 0xFBD44C65u,
    0x4DB26158u, 0x3AB551CEu, 0xA3BC0074u, 0xD4BB30E2u, 0x4ADFA541u, 0x3DD895D7u, 0xA4D1C46Du, 0xD3D6F4FBu, 0x4369E96Au, 0x346ED9FCu, 0xAD678846u, 0xDA60B8D0u, 0x44042D73u, 0x33031DE5u, 0xAA0A4C5Fu, 0xDD0D7CC9u,
    0x5005713Cu, 0x270241AAu, 0xBE0B1010u, 0xC90C2086u, 0x5768B525u, 0x206F85B3u, 0xB966D409u, 0xCE61E49Fu, 0x5EDEF90Eu, 0x29D9C998u, 0xB0D09822u, 0xC7D7A8B4u, 0x59B33D17u, 0x2EB40D81u, 0xB7BD5C3Bu, 0xC0BA6CADu,
    0xEDB88320u, 0x9ABFB3B6u, 0x03B6E20Cu, 0x74B1D29Au, 0xEAD54739u, 0x9DD277AFu, 0x04DB2615u, 0x73DC1683u, 0xE3630B12u, 0x94643B84u, 0x0D6D6A3Eu, 0x7A6A5AA8u, 0xE40ECF0Bu, 0x9309FF9Du, 0x0A00AE27u, 0x7D079EB1u,
    0xF00F9344u, 0x8708A3D2u, 0x1E01F268u, 0x6906C2FEu, 0xF762575Du, 0x806567CBu, 0x196C3671u, 0x6E6B06E7u, 0xFED41B76u, 0x89D32BE0u, 0x10DA7A5Au, 0x67DD4ACCu, 0xF9B9DF6Fu, 0x8EBEEFF9u, 0x17B7BE43u, 0x60B08ED5u,
    0xD6D6A3E8u, 0xA1D1937Eu, 0x38D8C2C4u, 0x4FDFF252u, 0xD1BB67F1u, 0xA6BC5767u, 0x3FB506DDu, 0x48B2364Bu, 0xD80D2BDAu, 0xAF0A1B4Cu, 0x36034AF6u, 0x41047A60u, 0xDF60EFC3u, 0xA867DF55u, 0x316E8EEFu, 0x4669BE79u,
    0xCB61B38Cu, 0xBC66831Au, 0x256FD2A0u, 0x5268E236u, 0xCC0C7795u, 0xBB0B4703u, 0x220216B9u, 0x5505262Fu, 0xC5BA3BBEu, 0xB2BD0B28u, 0x2BB45A92u, 0x5CB36A04u, 0xC2D7FFA7u, 0xB5D0CF31u, 0x2CD99E8Bu, 0x5BDEAE1Du,
    0x9B64C2B0u, 0xEC63F226u, 0x756AA39Cu, 0x026D930Au, 0x9C0906A9u, 0xEB0E363Fu, 0x72076785u, 0x05005713u, 0x95BF4A82u, 0xE2B87A14u, 0x7BB12BAEu, 0x0CB61B38u, 0x92D28E9Bu, 0xE5D5BE0Du, 0x7CDCEFB7u, 0x0BDBDF21u,
    0x86D3D2D4u, 0xF1D4E242u, 0x68DDB3F8u, 0x1FDA836Eu, 0x81BE16CDu, 0xF6B9265Bu, 0x6FB077E1u, 0x18B74777u, 0x88085AE6u, 0xFF0F6A70u, 0x66063BCAu, 0x11010B5Cu, 0x8F659EFFu, 0xF862AE69u, 0x616BFFD3u, 0x166CCF45u,
    0xA00AE278u, 0xD70DD2EEu, 0x4E048354u, 0x3903B3C2u, 0xA7672661u, 0xD06016F7u, 0x4969474Du, 0x3E6E77DBu, 0xAED16A4Au, 0xD9D65ADCu, 0x40DF0B66u, 0x37D83BF0u, 0xA9BCAE53u, 0xDEBB9EC5u, 0x47B2CF7Fu, 0x30B5FFE9u,
    0xBDBDF21Cu, 0xCABAC28Au, 0x53B39330u, 0x24B4A3A6u, 0xBAD03605u, 0xCDD70693u, 0x54DE5729u, 0x23D967BFu, 0xB3667A2Eu, 0xC4614AB8u, 0x5D681B02u, 0x2A6F2B94u, 0xB40BBE37u, 0xC30C8EA1u, 0x5A05DF1Bu, 0x2D02EF8Du,
};

/**
 * @brief CRC32-IEEE 802.3 增量计算
 *
 * @param init 初始值（首次调用用 0xFFFFFFFFu，最终结果异或 0xFFFFFFFFu）
 * @param data [in] 数据缓冲区
 * @param len 字节数
 * @return uint32_t 中间 CRC 值
 *
 * @note 使用编译时 const 查表，线程安全（纯函数 + 只读全局）
 */
static uint32_t are_crc32_update(uint32_t init, const void *data, size_t len)
{
    uint32_t crc = init;
    const uint8_t *p = (const uint8_t *)data;

    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ g_crc32_table[(crc ^ p[i]) & 0xFFu];
    }
    return crc;
}

/* ================================================================
 * 公共 API 实现
 * ================================================================ */

agentos_error_t are_ipc_header_init(are_ipc_message_header_t *header,
                                    are_msg_type_t msg_type,
                                    are_proto_t protocol,
                                    uint64_t msg_id,
                                    uint64_t trace_id,
                                    const char *source,
                                    const char *target)
{
    if (!header) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 校验枚举值在合法范围内 */
    if ((int)msg_type < 0 || (int)msg_type > (int)ARE_MSG_ERROR) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if ((int)protocol < 0 || (int)protocol > (int)ARE_PROTO_CUSTOM) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 全部清零（包含 reserved 字段必须为 0、checksum 字段初始为 0） */
    __builtin_memset(header, 0, sizeof(*header));

    header->magic    = ARE_IPC_MAGIC;
    header->version  = (uint16_t)ARE_IPC_VERSION;
    header->msg_type = (uint16_t)msg_type;
    header->protocol = (uint32_t)protocol;
    header->msg_id   = msg_id;
    header->trace_id = trace_id;
    /* correlation_id 默认 = msg_id（调用方可在初始化后覆盖） */
    header->correlation_id = msg_id;

    /* source：NULL → "unknown"，截断至 31 字节 + 终结符 */
    const char *src = source ? source : "unknown";
    /* 使用 __builtin_strncpy 绕开 strncpy 毒化（与项目内存安全规范一致） */
    __builtin_strncpy(header->source, src, ARE_IPC_MAX_SOURCE_LEN - 1u);
    header->source[ARE_IPC_MAX_SOURCE_LEN - 1u] = '\0';

    /* target：NULL → "*"（广播） */
    const char *tgt = target ? target : "*";
    __builtin_strncpy(header->target, tgt, ARE_IPC_MAX_TARGET_LEN - 1u);
    header->target[ARE_IPC_MAX_TARGET_LEN - 1u] = '\0';

    /* timestamp_ns：CLOCK_REALTIME 纳秒（ARE L2 §2.2 要求） */
    header->timestamp_ns = agentos_time_realtime_ns();

    /* payload_len / flags / checksum / reserved 已由 memset 清零 */

    return AGENTOS_SUCCESS;
}

uint32_t are_ipc_checksum_compute(const are_ipc_message_header_t *header,
                                  const void *payload,
                                  size_t payload_len)
{
    if (!header) {
        return 0u;
    }

    /* 构造临时缓冲区：复制 header，将 checksum 字段视为 0 参与计算 */
    are_ipc_message_header_t tmp;
    __builtin_memcpy(&tmp, header, sizeof(tmp));
    tmp.checksum = 0u;

    /* CRC32 计算范围：header[0:120)（即 magic ~ checksum 字段，共 120 字节，
     * checksum 字段以 0 参与计算）+ payload。
     *
     * 不覆盖 reserved（offset 120-123）和 _abi_pad（offset 124-127）：
     * 这两个字段必须为 0，由 are_ipc_header_validate 独立检查。
     * 设计理由：reserved/_abi_pad 是 ABI 约束而非数据完整性约束，
     * 独立检查可避免 CRC 重算时对它们的依赖，且与规范 §2.1 一致。 */
    uint32_t crc = 0xFFFFFFFFu;
    crc = are_crc32_update(crc, &tmp, offsetof(are_ipc_message_header_t, checksum) + 4u);

    /* 如果有 payload，继续增量计算 */
    if (payload && payload_len > 0) {
        crc = are_crc32_update(crc, payload, payload_len);
    }

    return crc ^ 0xFFFFFFFFu;
}

agentos_error_t are_ipc_header_validate(const are_ipc_message_header_t *header,
                                        const void *payload,
                                        size_t payload_len)
{
    if (!header) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 1. magic 校验 */
    if (header->magic != ARE_IPC_MAGIC) {
        return AGENTOS_ERR_PROTOCOL;
    }

    /* 2. version 校验 */
    if (header->version != (uint16_t)ARE_IPC_VERSION) {
        return AGENTOS_ERR_PROTOCOL;
    }

    /* 3. reserved 必须为 0 */
    if (header->reserved != 0u) {
        return AGENTOS_ERR_PROTOCOL;
    }

    /* 3.5 _abi_pad 必须为 0（ABI 填充字段，非零表示发送方违规） */
    if (header->_abi_pad != 0u) {
        return AGENTOS_ERR_PROTOCOL;
    }

    /* 4. payload_len 不超限 */
    if (header->payload_len > ARE_IPC_MAX_PAYLOAD) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 5. 调用方传入的 payload_len 必须与 header.payload_len 一致 */
    if (payload_len != header->payload_len) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 6. checksum 校验 */
    uint32_t expected = are_ipc_checksum_compute(header, payload, payload_len);
    if (header->checksum != expected) {
        return AGENTOS_ERR_CHECKSUM;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t are_ipc_message_create(are_msg_type_t msg_type,
                                       are_proto_t protocol,
                                       uint64_t msg_id,
                                       uint64_t trace_id,
                                       const char *source,
                                       const char *target,
                                       const void *payload,
                                       size_t payload_len,
                                       are_ipc_message_t **out_msg)
{
    if (!out_msg) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    *out_msg = NULL;

    if (payload_len > ARE_IPC_MAX_PAYLOAD) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (payload_len > 0 && !payload) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 分配消息结构体 */
    are_ipc_message_t *msg =
        (are_ipc_message_t *)AGENTOS_CALLOC(1, sizeof(are_ipc_message_t));
    if (!msg) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    /* 初始化 header */
    agentos_error_t err = are_ipc_header_init(&msg->header, msg_type, protocol, msg_id,
                                              trace_id, source, target);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(msg);
        return err;
    }
    msg->header.payload_len = (uint32_t)payload_len;

    /* 分配并拷贝 payload */
    if (payload_len > 0) {
        msg->payload = AGENTOS_MALLOC(payload_len);
        if (!msg->payload) {
            AGENTOS_FREE(msg);
            return AGENTOS_ERR_OUT_OF_MEMORY;
        }
        __builtin_memcpy(msg->payload, payload, payload_len);
        msg->payload_size = payload_len;
    } else {
        msg->payload = NULL;
        msg->payload_size = 0;
    }

    /* 计算 checksum 并写入 header */
    msg->header.checksum = are_ipc_checksum_compute(&msg->header, msg->payload, payload_len);

    *out_msg = msg;
    return AGENTOS_SUCCESS;
}

void are_ipc_message_destroy(are_ipc_message_t *msg)
{
    if (!msg) {
        return;
    }
    if (msg->payload) {
        AGENTOS_FREE(msg->payload);
        msg->payload = NULL;
    }
    AGENTOS_FREE(msg);
}

const char *are_ipc_msg_type_str(are_msg_type_t msg_type)
{
    switch (msg_type) {
    case ARE_MSG_REQUEST:  return "REQUEST";
    case ARE_MSG_RESPONSE: return "RESPONSE";
    case ARE_MSG_NOTIFY:   return "NOTIFY";
    case ARE_MSG_ERROR:    return "ERROR";
    default:               return "UNKNOWN";
    }
}

const char *are_ipc_proto_str(are_proto_t protocol)
{
    switch (protocol) {
    case ARE_PROTO_JSON_RPC: return "JSON-RPC";
    case ARE_PROTO_MCP:      return "MCP";
    case ARE_PROTO_A2A:      return "A2A";
    case ARE_PROTO_OPENAI:   return "OpenAI";
    case ARE_PROTO_CUSTOM:   return "CUSTOM";
    default:                 return "UNKNOWN";
    }
}
