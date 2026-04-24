# Cupolas Utils — 安全工具库

`cupolas/src/utils/` 为 Cupolas 各安全子系统提供共享的工具函数集。

## 提供的能力

| 工具 | 说明 |
|------|------|
| **加密工具** | 哈希计算、对称/非对称加密、密钥派生 |
| **编码工具** | Base64、Hex、URL 编解码 |
| **校验工具** | 校验和计算、数据完整性验证 |
| **模式匹配** | 正则表达式匹配、通配符匹配、ACL 模式匹配 |
| **序列化工具** | 安全的数据序列化与反序列化 |

## 使用示例

```c
#include "cupolas/src/utils/crypto.h"
#include "cupolas/src/utils/encoding.h"

// 哈希计算
char hash[65];
crypto_sha256(hash, "data_to_hash", 12);
printf("SHA256: %s", hash);

// 安全随机数生成
uint8_t random_bytes[32];
crypto_random_bytes(random_bytes, 32);

// URL 编码
char encoded[256];
url_encode(encoded, "user input with special chars & = ?");

// 模式匹配
bool matched = pattern_match("/api/v1/*", "/api/v1/users");
printf("Pattern match: %s", matched ? "true" : "false");
```

## 工具列表

```c
// crypto.h
void   crypto_sha256(char* out, const uint8_t* data, size_t len);
void   crypto_random_bytes(uint8_t* out, size_t len);
int    crypto_aes_encrypt(const uint8_t* key, const uint8_t* iv, 
                          const uint8_t* plaintext, uint8_t* ciphertext);
int    crypto_aes_decrypt(const uint8_t* key, const uint8_t* iv,
                          const uint8_t* ciphertext, uint8_t* plaintext);

// encoding.h
void   base64_encode(const uint8_t* in, size_t len, char* out);
int    base64_decode(const char* in, uint8_t* out, size_t* out_len);
void   hex_encode(const uint8_t* in, size_t len, char* out);
int    hex_decode(const char* in, uint8_t* out, size_t* out_len);
void   url_encode(char* out, const char* in);
int    url_decode(char* out, const char* in);

// pattern.h
bool   pattern_match(const char* pattern, const char* str);
bool   pattern_match_acl(const char* acl_pattern, const char* resource);
```

---

*AgentOS Cupolas — Utils*
