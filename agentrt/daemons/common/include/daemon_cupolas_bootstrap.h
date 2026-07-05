/**
 * @file daemon_cupolas_bootstrap.h
 * @brief P3.14 (ACC-DT15): daemon 统一 cupolas 安全穹顶引导
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 为所有 daemon 提供统一的 cupolas 安全穹顶初始化与清理接口，
 * 避免在 12 个 main.c 中重复代码。
 *
 * 调用契约：
 *   - main() 中 agentrt_log_init() 之后、socket/service 创建之前调用 daemon_cupolas_init()
 *   - main() 退出前调用 daemon_cupolas_cleanup()
 *   - init 失败时已自动清理，无需再调 cleanup
 *
 * 安全语义：
 *   - cupolas_init 使用默认配置（NULL config_path），启用 permission_engine +
 *     sanitizer + audit_logger 三大子模块
 *   - 失败时记录 FATAL 日志但**不 abort**（daemon 可降级运行，由各 service 层
 *     fail-closed 逻辑拦截危险操作；此设计避免安全模块初始化失败导致系统不可启动）
 *   - 重复调用 init 是幂等的（cupolas_init 内部有守卫）
 */

#ifndef AGENTRT_DAEMON_CUPOLAS_BOOTSTRAP_H
#define AGENTRT_DAEMON_CUPOLAS_BOOTSTRAP_H

#include "error.h" /* agentrt_error_t */

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief 初始化 cupolas 安全穹顶（统一引导）
     *
     * 在 daemon main() 中 agentrt_log_init() 之后调用。
     * 初始化 permission_engine、sanitizer、audit_logger 三大子模块。
     *
     * @param daemon_name daemon 名称（如 "tool_d"、"llm_d"），用于审计日志标识
     * @return AGENTRT_SUCCESS 成功；错误码失败（FATAL 日志已记录）
     *
     * @ownership daemon_name: BORROW (调用方保留所有权)
     */
    agentrt_error_t daemon_cupolas_init(const char *daemon_name);

    /**
     * @brief 清理 cupolas 安全穹顶
     *
     * 在 daemon main() 退出前调用。刷新审计日志、释放资源。
     * 幂等：重复调用安全。
     */
    void daemon_cupolas_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_DAEMON_CUPOLAS_BOOTSTRAP_H */
