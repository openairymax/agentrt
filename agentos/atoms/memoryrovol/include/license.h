/**
 * @file license.h
 * @brief MemoryRovol License 离线验证接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供 RSA-4096 签名验证、4级分层授权、离线许可证管理。
 * 预编译闭源库内部使用，通过 __attribute__((constructor)) 自动初始化。
 */

#ifndef AGENTOS_LICENSE_H
#define AGENTOS_LICENSE_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AGENTOS_LICENSE_TRIAL = 0,
    AGENTOS_LICENSE_PRO = 1,
    AGENTOS_LICENSE_ENTERPRISE = 2,
    AGENTOS_LICENSE_ENTERPRISE_PLUS = 3,
    AGENTOS_LICENSE_INVALID = 255
} agentos_license_level_t;

typedef struct {
    int valid;
    agentos_license_level_t level;
    char license_id[64];
    char customer[128];
    time_t expires_at;
    time_t checked_at;
    uint32_t feature_flags;
} agentos_license_status_t;

int agentos_license_init(const char* license_path);
void agentos_license_shutdown(void);
const agentos_license_status_t* agentos_license_get_status(void);
int agentos_license_check_feature(uint32_t feature_flag);
int agentos_license_reload(const char* license_path);

#define AGENTOS_FEATURE_L1_RAW         (1u << 0)
#define AGENTOS_FEATURE_L2_FEATURE      (1u << 1)
#define AGENTOS_FEATURE_L3_STRUCTURE    (1u << 2)
#define AGENTOS_FEATURE_L4_PATTERN      (1u << 3)
#define AGENTOS_FEATURE_FORGETTING      (1u << 4)
#define AGENTOS_FEATURE_ATTRACTOR       (1u << 5)
#define AGENTOS_FEATURE_PERSISTENCE     (1u << 6)
#define AGENTOS_FEATURE_ASYNC           (1u << 7)
#define AGENTOS_FEATURE_FAISS           (1u << 8)

#define AGENTOS_ELICENSE                (-16)

#define AGENTOS_TRIAL_MAX_RECORDS       100
#define AGENTOS_TRIAL_DELAY_SEC         3
#define AGENTOS_TRIAL_DURATION_DAYS     14

#ifdef MEMORYROVOL_OSS
#define MR_LICENSE_CHECK(feature) (0)
#define MR_LICENSE_GUARD(feature, ret_var) do { ret_var = AGENTOS_ELICENSE; return AGENTOS_ELICENSE; } while(0)
#define MR_LICENSE_GUARD_VOID(feature) do { return; } while(0)
#else
#define MR_LICENSE_CHECK(feature) agentos_license_check_feature(feature)
#define MR_LICENSE_GUARD(feature, ret_var) do { \
    if (!agentos_license_check_feature(feature)) { \
        ret_var = AGENTOS_ELICENSE; \
        return AGENTOS_ELICENSE; \
    } \
} while(0)
#define MR_LICENSE_GUARD_VOID(feature) do { \
    if (!agentos_license_check_feature(feature)) { \
        return; \
    } \
} while(0)
#endif

int agentos_license_is_trial(void);
int agentos_license_trial_check_write(void);
uint32_t agentos_license_trial_record_count(void);
int agentos_license_trial_expired(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LICENSE_H */
