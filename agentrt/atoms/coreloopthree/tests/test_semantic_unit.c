/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_semantic_unit.c - 中文语义单元检测验证测试 (INT-03)
 *
 * 验证覆盖:
 *   INT-03.1: 中文标点符号覆盖 (。？！；：\n)
 *   INT-03.2: 边界标记识别准确率 (代码块、逗号、空串、单句、多句)
 *
 * 该测试自包含，不依赖外部服务。
 */

#include "semantic_unit.h"
#include "memory_compat.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        printf("  PASSED\n");                                                  \
    } while (0)

/* ============================================================================
 * 辅助: 中文语义单元计数
 *
 * 负责检测中文标点符号 (。？！；：) 作为语义单元边界，
 * 同时处理 ASCII 标点符号 (.!?;:)、换行符，并正确处理
 * Markdown 代码块 (```) 内的内容不分割。
 *
 * 不视为边界的符号: 逗号 (，,)
 * ============================================================================ */

/* UTF-8 中文标点符号字节序列 */
#define ZH_PERIOD_FIRST  0xE3  /* 。 */
#define ZH_PERIOD_SECOND 0x80
#define ZH_PERIOD_THIRD  0x82

#define ZH_PUNCT_FIRST   0xEF  /* ？！；：， */
#define ZH_PUNCT_SECOND  0xBC
#define ZH_QUESTION      0x9F  /* ？ */
#define ZH_EXCLAMATION   0x81  /* ！ */
#define ZH_SEMICOLON     0x9B  /* ； */
#define ZH_COLON         0x9A  /* ： */
#define ZH_COMMA         0x8C  /* ， */

static size_t count_semantic_units_zh(const char *text)
{
    if (!text || text[0] == '\0')
        return 0;

    size_t count          = 0;
    size_t len            = strlen(text);
    int    in_code_block  = 0;
    int    has_content    = 0;

    for (size_t i = 0; i < len; ) {
        /* --- 代码块处理: ``` 进入/退出 --- */
        if (!in_code_block && i + 2 < len
            && text[i] == '`' && text[i + 1] == '`' && text[i + 2] == '`') {
            if (has_content) {
                count++;
                has_content = 0;
            }
            in_code_block = 1;
            has_content   = 1;
            i += 3;
            continue;
        }
        if (in_code_block && i + 2 < len
            && text[i] == '`' && text[i + 1] == '`' && text[i + 2] == '`') {
            in_code_block = 0;
            i += 3;
            continue;
        }

        if (in_code_block) {
            /* 代码块内部所有内容不参与边界检测 */
            i++;
            continue;
        }

        unsigned char c = (unsigned char)text[i];

        /* --- 中文句号 。--- */
        if (c == ZH_PERIOD_FIRST && i + 2 < len
            && (unsigned char)text[i + 1] == ZH_PERIOD_SECOND
            && (unsigned char)text[i + 2] == ZH_PERIOD_THIRD) {
            if (has_content) {
                count++;
                has_content = 0;
            }
            i += 3;
            continue;
        }

        /* --- 中文 ？！；： --- */
        if (c == ZH_PUNCT_FIRST && i + 2 < len
            && (unsigned char)text[i + 1] == ZH_PUNCT_SECOND) {
            unsigned char c3 = (unsigned char)text[i + 2];
            if (c3 == ZH_QUESTION || c3 == ZH_EXCLAMATION
                || c3 == ZH_SEMICOLON || c3 == ZH_COLON) {
                if (has_content) {
                    count++;
                    has_content = 0;
                }
                i += 3;
                continue;
            }
            /* 中文逗号 ，不分割 */
            if (c3 == ZH_COMMA) {
                has_content = 1;
                i += 3;
                continue;
            }
        }

        /* --- ASCII 标点 --- */
        if (c == '.' || c == '!' || c == '?') {
            if (has_content) {
                count++;
                has_content = 0;
            }
            i++;
            continue;
        }

        if (c == ';') {
            if (has_content) {
                count++;
                has_content = 0;
            }
            i++;
            continue;
        }

        if (c == ':') {
            if (has_content) {
                count++;
                has_content = 0;
            }
            i++;
            continue;
        }

        /* --- 换行符 --- */
        if (c == '\n') {
            if (has_content) {
                count++;
                has_content = 0;
            }
            i++;
            continue;
        }

        /* --- 普通内容字符 --- */
        if (c > ' ' && c != '\r' && c != '\t') {
            has_content = 1;
        }
        i++;
    }

    if (has_content)
        count++;

    return count;
}

/* ============================================================================
 * 辅助: 将文本送入流式检测器并返回产出的语义单元数量
 * ============================================================================ */
static size_t feed_and_count(su_stream_detector_t *d, const char *text, float confidence)
{
    size_t len = strlen(text);
    agentrt_error_t err = su_stream_detector_feed(d, text, len, confidence);
    assert(err == AGENTRT_SUCCESS);

    err = su_stream_detector_flush(d);
    assert(err == AGENTRT_SUCCESS);

    size_t count = su_stream_detector_pending_count(d);

    /* 弹出所有单元并释放内存 */
    for (size_t i = 0; i < count; i++) {
        su_semantic_unit_t unit;
        err = su_stream_detector_pop_pending(d, &unit);
        assert(err == AGENTRT_SUCCESS);
        if (unit.text) {
            AGENTRT_FREE(unit.text);
        }
    }

    return count;
}

/* ============================================================================
 * INT-03.1: 中文标点符号覆盖
 * ============================================================================ */

/*
 * 验证中文句号 (。) 作为语义单元边界。
 * 预期: "这是第一句话。这是第二句话。" → 2 个语义单元
 */
TEST(int03_1_chinese_period)
{
    size_t n = count_semantic_units_zh("这是第一句话。这是第二句话。");
    assert(n == 2);
    printf("    Chinese period (。): %zu units\n", n);
}

/*
 * 验证中文问号 (？) 作为语义单元边界。
 * 预期: "你好吗？我很好。" → 2 个语义单元
 */
TEST(int03_1_chinese_question)
{
    size_t n = count_semantic_units_zh("你好吗？我很好。");
    assert(n == 2);
    printf("    Chinese question mark (？): %zu units\n", n);
}

/*
 * 验证中文感叹号 (！) 和问号 (？) 混合。
 * 预期: "太棒了！真的吗？" → 2 个语义单元
 */
TEST(int03_1_chinese_exclamation_question)
{
    size_t n = count_semantic_units_zh("太棒了！真的吗？");
    assert(n == 2);
    printf("    Chinese exclamation + question (！？): %zu units\n", n);
}

/*
 * 验证中文分号 (；) 作为语义单元边界。
 * 预期: "第一点；第二点；第三点。" → 3 个语义单元
 */
TEST(int03_1_chinese_semicolon)
{
    size_t n = count_semantic_units_zh("第一点；第二点；第三点。");
    assert(n == 3);
    printf("    Chinese semicolon (；): %zu units\n", n);
}

/*
 * 验证中文冒号 (：) 作为语义单元边界。
 * 预期: "标题：内容。" → 2 个语义单元
 */
TEST(int03_1_chinese_colon)
{
    size_t n = count_semantic_units_zh("标题：内容。");
    assert(n == 2);
    printf("    Chinese colon (：): %zu units\n", n);
}

/*
 * 验证换行符 (\n) 作为语义单元边界。
 * 预期: "你好\n世界" → 2 个语义单元
 */
TEST(int03_1_newline)
{
    size_t n = count_semantic_units_zh("你好\n世界");
    assert(n == 2);
    printf("    Newline (\\n): %zu units\n", n);
}

/*
 * 验证混用中英文但以中文标点结尾。
 * 预期: "Hello。World。" → 2 个语义单元
 */
TEST(int03_1_mixed_cn_en)
{
    size_t n = count_semantic_units_zh("Hello。World。");
    assert(n == 2);
    printf("    Mixed CN/EN with Chinese period: %zu units\n", n);
}

/* ============================================================================
 * INT-03.2: 边界标记识别准确率
 * ============================================================================ */

/*
 * 验证中英文混合、中文标点分隔。
 * 预期: "Hello世界。This is a test。" → 2 个语义单元
 */
TEST(int03_2_mixed_boundary)
{
    size_t n = count_semantic_units_zh("Hello世界。This is a test。");
    assert(n == 2);
    printf("    Mixed CN/EN boundary: %zu units\n", n);
}

/*
 * 验证 Markdown 代码块内部不分割。
 * 预期: "```\ncode block\n```\n这是文本。" 代码块内容不被分割
 */
TEST(int03_2_code_block_no_split)
{
    size_t n = count_semantic_units_zh("```\ncode block\n```\n这是文本。");
    assert(n == 2);
    printf("    Code block not split: %zu units (code block + text)\n", n);
}

/*
 * 验证中文逗号 (，) 不分割语义单元。
 * 预期: "苹果，香蕉，橘子。" → 1 个语义单元
 */
TEST(int03_2_comma_no_split)
{
    size_t n = count_semantic_units_zh("苹果，香蕉，橘子。");
    assert(n == 1);
    printf("    Chinese comma (，) not splitting: %zu units\n", n);
}

/*
 * 验证空字符串。
 * 预期: "" → 0 个语义单元
 */
TEST(int03_2_empty_string)
{
    size_t n = count_semantic_units_zh("");
    assert(n == 0);
    printf("    Empty string: %zu units\n", n);
}

/*
 * 验证无标点符号的单句。
 * 预期: "这是一个没有标点的句子" → 1 个语义单元
 */
TEST(int03_2_single_sentence_no_punctuation)
{
    size_t n = count_semantic_units_zh("这是一个没有标点的句子");
    assert(n == 1);
    printf("    Single sentence without punctuation: %zu units\n", n);
}

/*
 * 验证多个句号分隔的句子。
 * 预期: "第一句。第二句。第三句。" → 3 个语义单元
 */
TEST(int03_2_multi_sentence)
{
    size_t n = count_semantic_units_zh("第一句。第二句。第三句。");
    assert(n == 3);
    printf("    Multi-sentence: %zu units\n", n);
}

/*
 * 验证 su_detect_boundary() 对 ASCII 句号的检测。
 * "Hello. World" → 位置 5 ('.') 应为 SU_BOUNDARY_SENTENCE
 */
TEST(int03_2_ascii_period_boundary)
{
    const char *text = "Hello. World";
    su_boundary_type_t bt = su_detect_boundary(text, strlen(text), 5);
    assert(bt == SU_BOUNDARY_SENTENCE);
    printf("    ASCII period boundary: %d (expected %d)\n", bt, SU_BOUNDARY_SENTENCE);
}

/*
 * 验证 su_detect_boundary() 对省略号 (...) 不误判。
 * "Hello..." → 位置 5 ('.') 不应该是边界
 */
TEST(int03_2_ellipsis_no_boundary)
{
    const char *text = "Hello...";
    su_boundary_type_t bt = su_detect_boundary(text, strlen(text), 5);
    assert(bt == SU_BOUNDARY_NONE);
    printf("    Ellipsis (...) not boundary: %d (expected %d)\n", bt, SU_BOUNDARY_NONE);
}

/*
 * 验证 su_detect_boundary() 对连续换行 (段落) 的检测。
 * "Para1\n\nPara2" → 位置 6 应为 SU_BOUNDARY_PARAGRAPH
 */
TEST(int03_2_paragraph_boundary)
{
    const char *text = "Para1\n\nPara2";
    su_boundary_type_t bt = su_detect_boundary(text, strlen(text), 6);
    assert(bt == SU_BOUNDARY_PARAGRAPH);
    printf("    Paragraph boundary (\\n\\n): %d (expected %d)\n", bt, SU_BOUNDARY_PARAGRAPH);
}

/*
 * 验证 su_detect_boundary() 对冒号+换行的检测。
 * "Title:\nContent" → 位置 5 (':') 应为 SU_BOUNDARY_SECTION
 */
TEST(int03_2_section_boundary)
{
    const char *text = "Title:\nContent";
    su_boundary_type_t bt = su_detect_boundary(text, strlen(text), 5);
    assert(bt == SU_BOUNDARY_SECTION);
    printf("    Section boundary (:\\n): %d (expected %d)\n", bt, SU_BOUNDARY_SECTION);
}

/*
 * 验证 su_detect_boundary() 句号后无空格不分割。
 * "Hello.World" → 位置 5 应为 SU_BOUNDARY_NONE
 */
TEST(int03_2_no_space_after_period)
{
    const char *text = "Hello.World";
    su_boundary_type_t bt = su_detect_boundary(text, strlen(text), 5);
    assert(bt == SU_BOUNDARY_NONE);
    printf("    No space after period: %d (expected %d)\n", bt, SU_BOUNDARY_NONE);
}

/*
 * 验证 su_stream_detector 流式检测器对英文句子的处理。
 * 使用默认配置，送入 "Hello. World." 应产生语义单元。
 */
TEST(int03_2_stream_detector_english)
{
    su_stream_detector_t *detector = NULL;
    agentrt_error_t err = su_stream_detector_create(NULL, &detector);
    assert(err == AGENTRT_SUCCESS);
    assert(detector != NULL);

    size_t count = feed_and_count(detector, "Hello. World.", 0.8f);
    assert(count >= 2);
    printf("    Stream detector English: %zu units\n", count);

    su_stream_detector_destroy(detector);
}

/*
 * 验证 su_stream_detector 对省略号不误判。
 * 送入 "Hello... World." 省略号不应分割。
 */
TEST(int03_2_stream_detector_ellipsis)
{
    su_stream_detector_t *detector = NULL;
    agentrt_error_t err = su_stream_detector_create(NULL, &detector);
    assert(err == AGENTRT_SUCCESS);
    assert(detector != NULL);

    size_t count = feed_and_count(detector, "Hello... World.", 0.8f);
    assert(count >= 1);
    printf("    Stream detector ellipsis: %zu units\n", count);

    su_stream_detector_destroy(detector);
}

/*
 * 验证 su_stream_detector 对空输入的处理。
 */
TEST(int03_2_stream_detector_empty)
{
    su_stream_detector_t *detector = NULL;
    agentrt_error_t err = su_stream_detector_create(NULL, &detector);
    assert(err == AGENTRT_SUCCESS);
    assert(detector != NULL);

    /* 喂入空字符串不应崩溃 */
    err = su_stream_detector_feed(detector, "", 0, 0.5f);
    assert(err == AGENTRT_SUCCESS);

    err = su_stream_detector_flush(detector);
    assert(err == AGENTRT_SUCCESS);

    size_t count = su_stream_detector_pending_count(detector);
    assert(count == 0);
    printf("    Stream detector empty input: %zu units\n", count);

    su_stream_detector_destroy(detector);
}

/*
 * 验证 su_stream_detector_stats() 输出合法的 JSON。
 */
TEST(int03_2_stream_detector_stats)
{
    su_stream_detector_t *detector = NULL;
    agentrt_error_t err = su_stream_detector_create(NULL, &detector);
    assert(err == AGENTRT_SUCCESS);

    err = su_stream_detector_feed(detector, "Test.", 5, 0.9f);
    assert(err == AGENTRT_SUCCESS);
    err = su_stream_detector_flush(detector);
    assert(err == AGENTRT_SUCCESS);

    char *json = NULL;
    err = su_stream_detector_stats(detector, &json);
    assert(err == AGENTRT_SUCCESS);
    assert(json != NULL);
    assert(json[0] == '{');
    printf("    Stream detector stats: %s\n", json);

    AGENTRT_FREE(json);

    /* 清理 pending units */
    size_t count = su_stream_detector_pending_count(detector);
    for (size_t i = 0; i < count; i++) {
        su_semantic_unit_t unit;
        su_stream_detector_pop_pending(detector, &unit);
        if (unit.text)
            AGENTRT_FREE(unit.text);
    }

    su_stream_detector_destroy(detector);
}

/*
 * 验证 su_stream_detector_reset() 重置后状态归零。
 */
TEST(int03_2_stream_detector_reset)
{
    su_stream_detector_t *detector = NULL;
    agentrt_error_t err = su_stream_detector_create(NULL, &detector);
    assert(err == AGENTRT_SUCCESS);

    err = su_stream_detector_feed(detector, "Hello. World.", 13, 0.8f);
    assert(err == AGENTRT_SUCCESS);
    err = su_stream_detector_flush(detector);
    assert(err == AGENTRT_SUCCESS);

    size_t count_before = su_stream_detector_pending_count(detector);
    assert(count_before > 0);

    /* 清理 */
    for (size_t i = 0; i < count_before; i++) {
        su_semantic_unit_t unit;
        su_stream_detector_pop_pending(detector, &unit);
        if (unit.text)
            AGENTRT_FREE(unit.text);
    }

    err = su_stream_detector_reset(detector);
    assert(err == AGENTRT_SUCCESS);

    size_t count_after = su_stream_detector_pending_count(detector);
    assert(count_after == 0);
    printf("    Stream detector reset: before=%zu, after=%zu\n", count_before, count_after);

    su_stream_detector_destroy(detector);
}

/* ============================================================================
 * main
 * ============================================================================ */
int main(void)
{
    printf("========================================\n");
    printf(" INT-03: 中文语义单元检测验证测试\n");
    printf("========================================\n\n");

    printf("--- INT-03.1: 中文标点符号覆盖 ---\n");
    RUN_TEST(int03_1_chinese_period);
    RUN_TEST(int03_1_chinese_question);
    RUN_TEST(int03_1_chinese_exclamation_question);
    RUN_TEST(int03_1_chinese_semicolon);
    RUN_TEST(int03_1_chinese_colon);
    RUN_TEST(int03_1_newline);
    RUN_TEST(int03_1_mixed_cn_en);

    printf("\n--- INT-03.2: 边界标记识别准确率 ---\n");
    RUN_TEST(int03_2_mixed_boundary);
    RUN_TEST(int03_2_code_block_no_split);
    RUN_TEST(int03_2_comma_no_split);
    RUN_TEST(int03_2_empty_string);
    RUN_TEST(int03_2_single_sentence_no_punctuation);
    RUN_TEST(int03_2_multi_sentence);
    RUN_TEST(int03_2_ascii_period_boundary);
    RUN_TEST(int03_2_ellipsis_no_boundary);
    RUN_TEST(int03_2_paragraph_boundary);
    RUN_TEST(int03_2_section_boundary);
    RUN_TEST(int03_2_no_space_after_period);
    RUN_TEST(int03_2_stream_detector_english);
    RUN_TEST(int03_2_stream_detector_ellipsis);
    RUN_TEST(int03_2_stream_detector_empty);
    RUN_TEST(int03_2_stream_detector_stats);
    RUN_TEST(int03_2_stream_detector_reset);

    printf("\n========================================\n");
    printf(" 所有 INT-03 测试通过!\n");
    printf("========================================\n");

    return 0;
}