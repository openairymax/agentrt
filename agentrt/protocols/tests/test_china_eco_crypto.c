/**
 * @file test_china_eco_crypto.c
 * @brief ChinaEco SM3/SM4密码算法单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * SM3 测试向量来源: GB/T 32905-2016 附录A
 * SM4 测试向量来源: GB/T 32907-2016 附录A
 */
// @owner: team-B

#include "china_eco_adapter.h"
#include "logging_compat.h"
#include "memory_compat.h"     

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_sm3_empty(void)
{
    uint8_t digest[CHINA_ECO_SM3_DIGEST_SIZE];
    int ret __attribute__((unused)) = china_eco_sm3_hash(NULL, 0, digest);
    assert(ret == 0);

    printf("  empty: ");
    for (int i = 0; i < 8; i++)
        printf("%02x", digest[i]);
    printf("...\n");
    return 0;
}

static int test_sm3_abc(void)
{
    const char *input = "abc";
    uint8_t digest[CHINA_ECO_SM3_DIGEST_SIZE];

    int ret __attribute__((unused)) = china_eco_sm3_hash(input, strlen(input), digest);
    assert(ret == 0);

    const uint8_t expected[CHINA_ECO_SM3_DIGEST_SIZE]
        __attribute__((unused)) = {0x66, 0xC7, 0xF0, 0xF4, 0x62, 0xEE, 0xED, 0xD9, 0xD1, 0xF2, 0xD4,
                                   0x6B, 0xDC, 0x10, 0xE4, 0xE2, 0x41, 0x67, 0xC4, 0x87, 0x5C, 0xF2,
                                   0xF7, 0xA2, 0x29, 0x7D, 0xA0, 0x2B, 0x8F, 0x4B, 0xA8, 0xE0};

    assert(memcmp(digest, expected, CHINA_ECO_SM3_DIGEST_SIZE) == 0);
    return 0;
}

static int test_sm3_abcd_x16(void)
{
    const char input[] = "abcdabcdabcdabcdabcdabcdabcdabcd"
                         "abcdabcdabcdabcdabcdabcdabcdabcd";
    uint8_t digest[CHINA_ECO_SM3_DIGEST_SIZE];

    int ret __attribute__((unused)) = china_eco_sm3_hash(input, strlen(input), digest);
    assert(ret == 0);

    const uint8_t expected[CHINA_ECO_SM3_DIGEST_SIZE]
        __attribute__((unused)) = {0xDE, 0xBE, 0x9F, 0xF9, 0x22, 0x75, 0xB8, 0xA1, 0x38, 0x60, 0x48,
                                   0x89, 0xC1, 0x8E, 0x5A, 0x4D, 0x6F, 0xDB, 0x70, 0xE5, 0x38, 0x7E,
                                   0x57, 0x65, 0x29, 0x3D, 0xCB, 0xA3, 0x9C, 0x0C, 0x57, 0x32};

    assert(memcmp(digest, expected, CHINA_ECO_SM3_DIGEST_SIZE) == 0);
    return 0;
}

static int test_sm3_null_params(void)
{
    uint8_t digest[CHINA_ECO_SM3_DIGEST_SIZE];
    int ret __attribute__((unused)) = china_eco_sm3_hash(NULL, 10, digest);
    assert(ret != 0);

    ret = china_eco_sm3_hash("data", 4, NULL);
    assert(ret != 0);

    return 0;
}

static int test_sm4_encrypt_decrypt(void)
{
    china_eco_sm4_context_t ctx = {0};
    for (int i = 0; i < CHINA_ECO_SM4_KEY_SIZE; i++)
        ctx.key[i] = (uint8_t)i;
    for (int i = 0; i < CHINA_ECO_SM4_IV_SIZE; i++)
        ctx.iv[i] = 0x42;
    ctx.initialized = true;

    const char *plaintext = "SM4 CBC test message";
    size_t pt_len = strlen(plaintext);

    uint8_t ciphertext[1024] = {0};
    size_t ct_len = sizeof(ciphertext);

    int ret __attribute__((unused)) =
        china_eco_sm4_encrypt(&ctx, plaintext, pt_len, ciphertext, &ct_len);
    assert(ret == 0);
    assert(ct_len > 0);
    assert(ct_len % CHINA_ECO_SM4_BLOCK_SIZE == 0);
    assert(memcmp(ciphertext, plaintext, pt_len) != 0);

    uint8_t decrypted[1024] = {0};
    size_t dec_len = sizeof(decrypted);

    ret = china_eco_sm4_decrypt(&ctx, ciphertext, ct_len, decrypted, &dec_len);
    assert(ret == 0);

    assert(dec_len >= pt_len);
    assert(memcmp(decrypted, plaintext, pt_len) == 0);

    return 0;
}

static int test_sm4_not_initialized(void)
{
    china_eco_sm4_context_t ctx = {0};
    uint8_t buf[64] = {0};
    size_t len = sizeof(buf);

    int ret __attribute__((unused)) = china_eco_sm4_encrypt(&ctx, "test", 4, buf, &len);
    assert(ret != 0);

    return 0;
}

static int test_sm4_bad_alignment(void)
{
    china_eco_sm4_context_t ctx = {0};
    AGENTRT_MEMSET(ctx.key, 0x01, CHINA_ECO_SM4_KEY_SIZE);
    AGENTRT_MEMSET(ctx.iv, 0x02, CHINA_ECO_SM4_IV_SIZE);
    ctx.initialized = true;

    uint8_t data[128] = {0};
    size_t len = 128;

    int ret __attribute__((unused)) = china_eco_sm4_decrypt(&ctx, "bad", 3, data, &len);
    assert(ret != 0);

    return 0;
}

int main(void)
{
    int failures = 0;

#define RUN_TEST(name)              \
    printf("[TEST] %s... ", #name); \
    if (test_##name() != 0) {       \
        printf("FAILED\n");         \
        failures++;                 \
    } else                          \
        printf("PASSED\n")

    printf("=== SM3 Tests (GB/T 32905-2016) ===\n");
    RUN_TEST(sm3_empty);
    RUN_TEST(sm3_abc);
    RUN_TEST(sm3_abcd_x16);
    RUN_TEST(sm3_null_params);

    printf("\n=== SM4 Tests (GB/T 32907-2016) ===\n");
    RUN_TEST(sm4_encrypt_decrypt);
    RUN_TEST(sm4_not_initialized);
    RUN_TEST(sm4_bad_alignment);

    printf("\nChinaEco crypto tests: %d failures\n", failures);
    return failures;
}
