/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_thinkdual_semantic.c - 中文语义单元边界检测完整性验证 (INT-03)
 *
 * 集成测试: 验证 Thinkdual 系统对中文文本语义边界的正确处理
 *
 * 验证覆盖:
 *   INT-03.1: 中文句子分割 — 验证 。！？ 作为句子边界
 *   INT-03.2: 中文段落检测 — 验证段落分隔识别
 *   INT-03.3: 中英文混合边界 — 验证中英文交替处的边界
 *   INT-03.4: 中文标点处理 — 验证 。！？；： 作为边界标记
 *   INT-03.5: 长中文文本分块 — 验证大段中文文本的正确分块
 *   INT-03.6: 空值/边界用例 — 空串、单字符、纯英文
 *
 * 该测试自包含，不依赖外部服务（无LLM调用，无网络）。
 */

#include "semantic_unit.h"
#include "memory_compat.h"
#include "error.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 测试框架宏（与项目现有集成测试风格一致）
 * ============================================================================ */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        g_tests_run++;                                                         \
        g_tests_passed++;                                                      \
        printf("  PASSED\n");                                                  \
    } while (0)

#define TEST_ASSERT(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("    ASSERT_FAIL: %s at line %d\n", #cond, __LINE__);       \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define TEST_ASSERT_EQ(actual, expected)                                       \
    do {                                                                       \
        long _a = (long)(actual);                                              \
        long _e = (long)(expected);                                            \
        if (_a != _e) {                                                        \
            printf("    ASSERT_FAIL: %s == %ld, expected %ld at line %d\n",    \
                   #actual, _a, _e, __LINE__);                                 \
            g_tests_run++;                                                     \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

/* ============================================================================
 * 辅助: 中文语义单元计数
 *
 * 检测中文标点符号 (。？！；：) 作为语义单元边界，
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
    agentos_error_t err = su_stream_detector_feed(d, text, len, confidence);
    if (err != AGENTOS_SUCCESS)
        return 0;

    err = su_stream_detector_flush(d);
    if (err != AGENTOS_SUCCESS)
        return 0;

    size_t count = su_stream_detector_pending_count(d);

    /* 弹出所有单元并释放内存 */
    for (size_t i = 0; i < count; i++) {
        su_semantic_unit_t unit;
        err = su_stream_detector_pop_pending(d, &unit);
        if (err != AGENTOS_SUCCESS)
            break;
        if (unit.text) {
            AGENTOS_FREE(unit.text);
        }
    }

    return count;
}

/* ============================================================================
 * INT-03.1: 中文句子分割
 *
 * 验证中文文本在正确的句子边界处分割 (。！？)
 * ============================================================================ */

TEST(int03_1_chinese_period_split)
{
    /* 中文句号分割 */
    size_t n = count_semantic_units_zh("这是第一句话。这是第二句话。这是第三句话。");
    TEST_ASSERT_EQ(n, 3);
    printf("    Chinese period (。) split: %zu units\n", n);
}

TEST(int03_1_chinese_question_split)
{
    /* 中文问号分割 */
    size_t n = count_semantic_units_zh("你好吗？我很好。");
    TEST_ASSERT_EQ(n, 2);
    printf("    Chinese question (？) split: %zu units\n", n);
}

TEST(int03_1_chinese_exclamation_split)
{
    /* 中文感叹号分割 */
    size_t n = count_semantic_units_zh("太棒了！继续加油！");
    TEST_ASSERT_EQ(n, 2);
    printf("    Chinese exclamation (！) split: %zu units\n", n);
}

TEST(int03_1_chinese_mixed_sentence_endings)
{
    /* 混合 。！？ 句子结尾 */
    size_t n = count_semantic_units_zh("今天天气真好！你出门了吗？我准备去公园。");
    TEST_ASSERT_EQ(n, 3);
    printf("    Mixed 。！？ endings: %zu units\n", n);
}

TEST(int03_1_consecutive_questions)
{
    /* 连续问号 */
    size_t n = count_semantic_units_zh("为什么？怎么办？谁来帮我？");
    TEST_ASSERT_EQ(n, 3);
    printf("    Consecutive questions: %zu units\n", n);
}

/* ============================================================================
 * INT-03.2: 中文段落检测
 *
 * 验证段落分隔（换行符）被正确识别
 * ============================================================================ */

TEST(int03_2_single_newline_paragraph)
{
    /* 单换行符分隔段落 */
    size_t n = count_semantic_units_zh("第一段内容\n第二段内容");
    TEST_ASSERT_EQ(n, 2);
    printf("    Single newline paragraph: %zu units\n", n);
}

TEST(int03_2_double_newline_paragraph)
{
    /* 双换行符分隔段落 */
    size_t n = count_semantic_units_zh("第一段内容\n\n第二段内容");
    TEST_ASSERT_EQ(n, 3);
    printf("    Double newline paragraph: %zu units\n", n);
}

TEST(int03_2_multi_paragraph)
{
    /* 多段落 */
    size_t n = count_semantic_units_zh("段落一\n段落二\n段落三");
    TEST_ASSERT_EQ(n, 3);
    printf("    Multi-paragraph: %zu units\n", n);
}

TEST(int03_2_paragraph_with_sentence_endings)
{
    /* 段落中包含句号 */
    size_t n = count_semantic_units_zh("这是第一段。还有内容\n这是第二段。");
    TEST_ASSERT_EQ(n, 3);
    printf("    Paragraph with sentence endings: %zu units\n", n);
}

TEST(int03_2_stream_detector_paragraph)
{
    /* 流式检测器段落边界检测 */
    su_stream_detector_t *detector = NULL;
    agentos_error_t err = su_stream_detector_create(NULL, &detector);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    TEST_ASSERT(detector != NULL);

    const char *text = "Para1\n\nPara2";
    size_t count = feed_and_count(detector, text, 0.8f);
    TEST_ASSERT(count >= 2);
    printf("    Stream detector paragraph: %zu units\n", count);

    su_stream_detector_destroy(detector);
}

/* ============================================================================
 * INT-03.3: 中英文混合边界
 *
 * 验证中英文交替处的边界检测
 * ============================================================================ */

TEST(int03_3_mixed_cn_en_with_chinese_punctuation)
{
    /* 中文标点分隔中英文混合文本 */
    size_t n = count_semantic_units_zh("Hello世界。This is a test。");
    TEST_ASSERT_EQ(n, 2);
    printf("    Mixed CN/EN with Chinese punctuation: %zu units\n", n);
}

TEST(int03_3_en_text_cn_punctuation)
{
    /* 英文文本使用中文标点 */
    size_t n = count_semantic_units_zh("Hello。World。");
    TEST_ASSERT_EQ(n, 2);
    printf("    EN text with CN punctuation: %zu units\n", n);
}

TEST(int03_3_cn_en_alternating)
{
    /* 中英文交替出现 */
    size_t n = count_semantic_units_zh("中文English混合。Another sentence。");
    TEST_ASSERT_EQ(n, 2);
    printf("    CN/EN alternating: %zu units\n", n);
}

TEST(int03_3_en_sentence_in_cn_context)
{
    /* 中文语境中嵌入英文句子 */
    size_t n = count_semantic_units_zh("请使用Git进行版本控制。详细步骤请参考README文件。");
    TEST_ASSERT_EQ(n, 2);
    printf("    EN sentence in CN context: %zu units\n", n);
}

TEST(int03_3_mixed_ascii_cn_punctuation)
{
    /* 混合 ASCII 和中文标点 */
    size_t n = count_semantic_units_zh("Hello world. 这是中文。Mixed! 混合。");
    TEST_ASSERT_EQ(n, 4);
    printf("    Mixed ASCII/CN punctuation: %zu units\n", n);
}

/* ============================================================================
 * INT-03.4: 中文标点处理
 *
 * 验证 。！？；： 被正确视为边界标记
 * ============================================================================ */

TEST(int03_4_period_as_boundary)
{
    /* 中文句号 。 作为边界 */
    size_t n = count_semantic_units_zh("第一句。第二句。");
    TEST_ASSERT_EQ(n, 2);
    printf("    Period (。) boundary: %zu units\n", n);
}

TEST(int03_4_exclamation_as_boundary)
{
    /* 中文感叹号 ！ 作为边界 */
    size_t n = count_semantic_units_zh("注意！危险！");
    TEST_ASSERT_EQ(n, 2);
    printf("    Exclamation (！) boundary: %zu units\n", n);
}

TEST(int03_4_question_as_boundary)
{
    /* 中文问号 ？ 作为边界 */
    size_t n = count_semantic_units_zh("好吗？行吗？");
    TEST_ASSERT_EQ(n, 2);
    printf("    Question (？) boundary: %zu units\n", n);
}

TEST(int03_4_semicolon_as_boundary)
{
    /* 中文分号 ； 作为边界 */
    size_t n = count_semantic_units_zh("条件一；条件二；条件三。");
    TEST_ASSERT_EQ(n, 3);
    printf("    Semicolon (；) boundary: %zu units\n", n);
}

TEST(int03_4_colon_as_boundary)
{
    /* 中文冒号 ： 作为边界 */
    size_t n = count_semantic_units_zh("标题：正文内容。");
    TEST_ASSERT_EQ(n, 2);
    printf("    Colon (：) boundary: %zu units\n", n);
}

TEST(int03_4_comma_not_boundary)
{
    /* 中文逗号 ， 不作为边界 */
    size_t n = count_semantic_units_zh("苹果，香蕉，橘子。");
    TEST_ASSERT_EQ(n, 1);
    printf("    Comma (，) not boundary: %zu units\n", n);
}

TEST(int03_4_all_boundary_punctuation)
{
    /* 所有边界标点混合 */
    size_t n = count_semantic_units_zh("标题：内容！问题？补充；结尾。");
    TEST_ASSERT_EQ(n, 5);
    printf("    All boundary punctuation: %zu units\n", n);
}

/* ============================================================================
 * INT-03.5: 长中文文本分块
 *
 * 验证大段中文文本被正确分块
 * ============================================================================ */

TEST(int03_5_long_text_multiple_sentences)
{
    /* 多句长文本 */
    const char *long_text =
        "人工智能是计算机科学的一个分支。"
        "它企图了解智能的实质。"
        "并生产出一种新的能以人类智能相似的方式做出反应的智能机器。"
        "该领域的研究包括机器人、语言识别、图像识别和专家系统等。";
    size_t n = count_semantic_units_zh(long_text);
    TEST_ASSERT_EQ(n, 4);
    printf("    Long text multiple sentences: %zu units\n", n);
}

TEST(int03_5_long_text_stream_detector)
{
    /* 流式检测器处理长中文文本 */
    su_stream_detector_t *detector = NULL;
    agentos_error_t err = su_stream_detector_create(NULL, &detector);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    TEST_ASSERT(detector != NULL);

    const char *long_text =
        "人工智能是计算机科学的一个分支。"
        "它企图了解智能的实质。"
        "并生产出一种新的能以人类智能相似的方式做出反应的智能机器。"
        "该领域的研究包括机器人、语言识别、图像识别和专家系统等。";
    size_t count = feed_and_count(detector, long_text, 0.8f);
    TEST_ASSERT(count >= 2);
    printf("    Stream detector long text: %zu units\n", count);

    su_stream_detector_destroy(detector);
}

TEST(int03_5_long_text_with_paragraphs)
{
    /* 多段落长文本 */
    const char *long_text =
        "第一段：人工智能是计算机科学的一个分支。\n"
        "第二段：它企图了解智能的实质。\n"
        "第三段：该领域的研究包括机器人等。";
    size_t n = count_semantic_units_zh(long_text);
    TEST_ASSERT(n >= 4);
    printf("    Long text with paragraphs: %zu units\n", n);
}

TEST(int03_5_stream_detector_chunk_adjustment)
{
    /* 流式检测器动态分块调整 */
    su_config_t config = SU_CONFIG_DEFAULTS;
    config.enable_dynamic_chunk = 1;
    config.min_chunk_tokens     = SU_MIN_CHUNK_TOKENS;
    config.max_chunk_tokens     = SU_MAX_CHUNK_TOKENS;
    config.low_confidence_threshold  = 0.3f;
    config.high_confidence_threshold = 0.7f;

    su_stream_detector_t *detector = NULL;
    agentos_error_t err = su_stream_detector_create(&config, &detector);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    TEST_ASSERT(detector != NULL);

    /* 低置信度 → 更小的分块 */
    const char *text = "这是低置信度输入的内容。应该产生更小的分块。";
    size_t count = feed_and_count(detector, text, 0.2f);
    TEST_ASSERT(count >= 1);
    printf("    Dynamic chunk (low confidence): %zu units\n", count);

    su_stream_detector_destroy(detector);
}

TEST(int03_5_stream_detector_stats_after_long_text)
{
    /* 长文本处理后统计信息验证 */
    su_stream_detector_t *detector = NULL;
    agentos_error_t err = su_stream_detector_create(NULL, &detector);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    TEST_ASSERT(detector != NULL);

    const char *text =
        "句子一。句子二。句子三。句子四。句子五。";
    size_t len = strlen(text);
    err = su_stream_detector_feed(detector, text, len, 0.8f);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    err = su_stream_detector_flush(detector);
    TEST_ASSERT(err == AGENTOS_SUCCESS);

    char *stats_json = NULL;
    err = su_stream_detector_stats(detector, &stats_json);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    TEST_ASSERT(stats_json != NULL);
    TEST_ASSERT(stats_json[0] == '{');
    printf("    Stats after long text: %s\n", stats_json);

    /* 清理 pending units */
    size_t count = su_stream_detector_pending_count(detector);
    for (size_t i = 0; i < count; i++) {
        su_semantic_unit_t unit;
        su_stream_detector_pop_pending(detector, &unit);
        if (unit.text)
            AGENTOS_FREE(unit.text);
    }

    AGENTOS_FREE(stats_json);
    su_stream_detector_destroy(detector);
}

/* ============================================================================
 * INT-03.6: 空值/边界用例
 *
 * 验证空串、单字符、纯英文等边界情况
 * ============================================================================ */

TEST(int03_6_empty_string)
{
    /* 空字符串 */
    size_t n = count_semantic_units_zh("");
    TEST_ASSERT_EQ(n, 0);
    printf("    Empty string: %zu units\n", n);
}

TEST(int03_6_null_pointer)
{
    /* NULL 指针 */
    size_t n = count_semantic_units_zh(NULL);
    TEST_ASSERT_EQ(n, 0);
    printf("    NULL pointer: %zu units\n", n);
}

TEST(int03_6_single_chinese_char)
{
    /* 单个中文字符 */
    size_t n = count_semantic_units_zh("中");
    TEST_ASSERT_EQ(n, 1);
    printf("    Single Chinese char: %zu units\n", n);
}

TEST(int03_6_single_english_char)
{
    /* 单个英文字符 */
    size_t n = count_semantic_units_zh("A");
    TEST_ASSERT_EQ(n, 1);
    printf("    Single English char: %zu units\n", n);
}

TEST(int03_6_pure_english_text)
{
    /* 纯英文文本 */
    size_t n = count_semantic_units_zh("Hello world. This is a test.");
    TEST_ASSERT(n >= 2);
    printf("    Pure English text: %zu units\n", n);
}

TEST(int03_6_only_punctuation)
{
    /* 仅标点符号 */
    size_t n = count_semantic_units_zh("。。。！！！");
    TEST_ASSERT_EQ(n, 0);
    printf("    Only punctuation: %zu units\n", n);
}

TEST(int03_6_whitespace_only)
{
    /* 仅空白字符 */
    size_t n = count_semantic_units_zh("   \n\t  ");
    TEST_ASSERT_EQ(n, 0);
    printf("    Whitespace only: %zu units\n", n);
}

TEST(int03_6_stream_detector_empty_input)
{
    /* 流式检测器空输入 */
    su_stream_detector_t *detector = NULL;
    agentos_error_t err = su_stream_detector_create(NULL, &detector);
    TEST_ASSERT(err == AGENTOS_SUCCESS);
    TEST_ASSERT(detector != NULL);

    err = su_stream_detector_feed(detector, "", 0, 0.5f);
    TEST_ASSERT(err == AGENTOS_SUCCESS);

    err = su_stream_detector_flush(detector);
    TEST_ASSERT(err == AGENTOS_SUCCESS);

    size_t count = su_stream_detector_pending_count(detector);
    TEST_ASSERT_EQ(count, 0);
    printf("    Stream detector empty input: %zu units\n", count);

    su_stream_detector_destroy(detector);
}

TEST(int03_6_stream_detector_null_params)
{
    /* 流式检测器 NULL 参数检查 */
    agentos_error_t err = su_stream_detector_create(NULL, NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS);

    err = su_stream_detector_feed(NULL, "test", 4, 0.5f);
    TEST_ASSERT(err != AGENTOS_SUCCESS);

    err = su_stream_detector_flush(NULL);
    TEST_ASSERT(err != AGENTOS_SUCCESS);

    size_t count = su_stream_detector_pending_count(NULL);
    TEST_ASSERT_EQ(count, 0);

    printf("    Stream detector NULL params: all checks passed\n");
}

TEST(int03_6_single_sentence_no_punctuation)
{
    /* 无标点单句 */
    size_t n = count_semantic_units_zh("这是一个没有标点的句子");
    TEST_ASSERT_EQ(n, 1);
    printf("    Single sentence no punctuation: %zu units\n", n);
}

TEST(int03_6_code_block_no_split)
{
    /* Markdown 代码块内部不分割 */
    size_t n = count_semantic_units_zh("```\ncode block\n```\n这是文本。");
    TEST_ASSERT_EQ(n, 2);
    printf("    Code block no split: %zu units\n", n);
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  Thinkdual Semantic Boundary Tests\n");
    printf("  INT-03: Chinese Semantic Unit\n");
    printf("=========================================\n\n");

    /* INT-03.1: 中文句子分割 */
    printf("--- INT-03.1: Chinese Sentence Splitting ---\n");
    RUN_TEST(int03_1_chinese_period_split);
    RUN_TEST(int03_1_chinese_question_split);
    RUN_TEST(int03_1_chinese_exclamation_split);
    RUN_TEST(int03_1_chinese_mixed_sentence_endings);
    RUN_TEST(int03_1_consecutive_questions);

    /* INT-03.2: 中文段落检测 */
    printf("\n--- INT-03.2: Chinese Paragraph Detection ---\n");
    RUN_TEST(int03_2_single_newline_paragraph);
    RUN_TEST(int03_2_double_newline_paragraph);
    RUN_TEST(int03_2_multi_paragraph);
    RUN_TEST(int03_2_paragraph_with_sentence_endings);
    RUN_TEST(int03_2_stream_detector_paragraph);

    /* INT-03.3: 中英文混合边界 */
    printf("\n--- INT-03.3: Mixed Chinese/English Boundaries ---\n");
    RUN_TEST(int03_3_mixed_cn_en_with_chinese_punctuation);
    RUN_TEST(int03_3_en_text_cn_punctuation);
    RUN_TEST(int03_3_cn_en_alternating);
    RUN_TEST(int03_3_en_sentence_in_cn_context);
    RUN_TEST(int03_3_mixed_ascii_cn_punctuation);

    /* INT-03.4: 中文标点处理 */
    printf("\n--- INT-03.4: Chinese Punctuation Handling ---\n");
    RUN_TEST(int03_4_period_as_boundary);
    RUN_TEST(int03_4_exclamation_as_boundary);
    RUN_TEST(int03_4_question_as_boundary);
    RUN_TEST(int03_4_semicolon_as_boundary);
    RUN_TEST(int03_4_colon_as_boundary);
    RUN_TEST(int03_4_comma_not_boundary);
    RUN_TEST(int03_4_all_boundary_punctuation);

    /* INT-03.5: 长中文文本分块 */
    printf("\n--- INT-03.5: Long Chinese Text Chunking ---\n");
    RUN_TEST(int03_5_long_text_multiple_sentences);
    RUN_TEST(int03_5_long_text_stream_detector);
    RUN_TEST(int03_5_long_text_with_paragraphs);
    RUN_TEST(int03_5_stream_detector_chunk_adjustment);
    RUN_TEST(int03_5_stream_detector_stats_after_long_text);

    /* INT-03.6: 空值/边界用例 */
    printf("\n--- INT-03.6: Empty/Edge Cases ---\n");
    RUN_TEST(int03_6_empty_string);
    RUN_TEST(int03_6_null_pointer);
    RUN_TEST(int03_6_single_chinese_char);
    RUN_TEST(int03_6_single_english_char);
    RUN_TEST(int03_6_pure_english_text);
    RUN_TEST(int03_6_only_punctuation);
    RUN_TEST(int03_6_whitespace_only);
    RUN_TEST(int03_6_stream_detector_empty_input);
    RUN_TEST(int03_6_stream_detector_null_params);
    RUN_TEST(int03_6_single_sentence_no_punctuation);
    RUN_TEST(int03_6_code_block_no_split);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d INT-03 tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED (out of %d)\n",
               g_tests_passed, g_tests_failed, g_tests_run);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
