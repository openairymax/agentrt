/**
 * @file domes_network_security.h
 * @brief 网络安全 - TLS 强制、网络过滤
 * @author Spharx
 * @date 2026
 *
 * 设计原则：
 * - 强制加密：所有网络通信必须使用 TLS
 * - 证书验证：严格的证书链验证
 * - 网络过滤：基于策略的网络访问控制
 * - 入侵检测：异常流量检测
 */

#ifndef DOMES_NETWORK_SECURITY_H
#define DOMES_NETWORK_SECURITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief TLS 版本
 */
typedef enum {
    DOMES_TLS_1_0 = 0x0301,
    DOMES_TLS_1_1 = 0x0302,
    DOMES_TLS_1_2 = 0x0303,
    DOMES_TLS_1_3 = 0x0304
} domes_tls_version_t;

/**
 * @brief 证书验证结果
 */
typedef enum {
    DOMES_CERT_OK = 0,
    DOMES_CERT_INVALID = -1,
    DOMES_CERT_EXPIRED = -2,
    DOMES_CERT_REVOKED = -3,
    DOMES_CERT_UNTRUSTED = -4,
    DOMES_CERT_HOST_MISMATCH = -5,
    DOMES_CERT_SELF_SIGNED = -6
} domes_cert_result_t;

/**
 * @brief 网络访问动作
 */
typedef enum {
    DOMES_NET_ALLOW = 0,
    DOMES_NET_DENY = 1,
    DOMES_NET_LOG = 2,
    DOMES_NET_RATE_LIMIT = 3
} domes_net_action_t;

/**
 * @brief 协议类型
 */
typedef enum {
    DOMES_PROTO_TCP = 1,
    DOMES_PROTO_UDP = 2,
    DOMES_PROTO_HTTP = 3,
    DOMES_PROTO_HTTPS = 4,
    DOMES_PROTO_WEBSOCKET = 5,
    DOMES_PROTO_DNS = 6
} domes_protocol_t;

/**
 * @brief TLS 配置
 */
typedef struct {
    domes_tls_version_t min_version;    /**< 最低 TLS 版本 */
    domes_tls_version_t max_version;    /**< 最高 TLS 版本 */
    const char** cipher_suites;          /**< 允许的加密套件 */
    size_t cipher_count;
    const char** curves;                 /**< 允许的椭圆曲线 */
    size_t curve_count;
    bool require_cert_verify;            /**< 要求证书验证 */
    bool allow_self_signed;              /**< 允许自签名证书 */
    bool check_revocation;               /**< 检查吊销状态 */
    bool enable_ocsp_stapling;           /**< 启用 OCSP Stapling */
    bool enable_sni;                     /**< 启用 SNI */
    const char* ca_bundle_path;          /**< CA 证书包路径 */
    const char* client_cert_path;        /**< 客户端证书路径 */
    const char* client_key_path;         /**< 客户端私钥路径 */
} domes_tls_config_t;

/**
 * @brief 网络过滤规则
 */
typedef struct {
    char* rule_id;                       /**< 规则 ID */
    char* description;                   /**< 描述 */
    
    char* src_ip_pattern;                /**< 源 IP 模式 (CIDR) */
    char* dst_ip_pattern;                /**< 目标 IP 模式 (CIDR) */
    uint16_t src_port_start;             /**< 源端口范围起始 */
    uint16_t src_port_end;               /**< 源端口范围结束 */
    uint16_t dst_port_start;             /**< 目标端口范围起始 */
    uint16_t dst_port_end;               /**< 目标端口范围结束 */
    
    domes_protocol_t protocol;           /**< 协议 */
    char* host_pattern;                  /**< 主机模式 (支持通配符) */
    char* url_pattern;                   /**< URL 模式 */
    
    domes_net_action_t action;           /**< 动作 */
    int priority;                        /**< 优先级 */
    bool enabled;                        /**< 是否启用 */
    
    uint32_t rate_limit;                 /**< 速率限制 (请求/秒) */
    uint32_t burst_limit;                /**< 突发限制 */
} domes_net_filter_rule_t;

/**
 * @brief HTTP 安全配置
 */
typedef struct {
    bool enforce_https;                  /**< 强制 HTTPS */
    bool hsts_enabled;                   /**< 启用 HSTS */
    uint32_t hsts_max_age;               /**< HSTS 最大年龄 */
    bool hsts_include_subdomains;        /**< HSTS 包含子域名 */
    
    const char** allowed_methods;        /**< 允许的 HTTP 方法 */
    size_t method_count;
    
    const char** required_headers;       /**< 必需的请求头 */
    size_t header_count;
    
    const char** forbidden_headers;      /**< 禁止的请求头 */
    size_t forbidden_count;
    
    uint32_t max_header_size;            /**< 最大请求头大小 */
    uint32_t max_body_size;              /**< 最大请求体大小 */
    uint32_t max_url_length;             /**< 最大 URL 长度 */
    
    uint32_t request_timeout_ms;         /**< 请求超时 */
    uint32_t connect_timeout_ms;         /**< 连接超时 */
} domes_http_security_config_t;

/**
 * @brief DNS 安全配置
 */
typedef struct {
    bool enable_dnssec;                  /**< 启用 DNSSEC */
    bool enable_dns_over_https;          /**< 启用 DoH */
    bool enable_dns_over_tls;            /**< 启用 DoT */
    const char* doh_server;              /**< DoH 服务器 */
    const char* dot_server;              /**< DoT 服务器 */
    const char** allowed_domains;        /**< 允许的域名 */
    size_t domain_count;
    const char** blocked_domains;        /**< 阻止的域名 */
    size_t blocked_count;
} domes_dns_security_config_t;

/**
 * @brief 网络安全完整配置
 */
typedef struct {
    domes_tls_config_t tls;
    domes_http_security_config_t http;
    domes_dns_security_config_t dns;
    
    bool enable_firewall;                /**< 启用防火墙 */
    bool enable_ids;                     /**< 启用入侵检测 */
    bool enable_logging;                 /**< 启用日志 */
    bool enable_audit;                   /**< 启用审计 */
} domes_net_security_config_t;

/**
 * @brief 网络连接信息
 */
typedef struct {
    char* local_ip;
    uint16_t local_port;
    char* remote_ip;
    uint16_t remote_port;
    domes_protocol_t protocol;
    char* hostname;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t start_time;
    uint64_t last_activity;
    bool is_encrypted;
    domes_tls_version_t tls_version;
    char* cipher_suite;
} domes_connection_info_t;

/**
 * @brief 网络统计
 */
typedef struct {
    uint64_t total_connections;
    uint64_t active_connections;
    uint64_t blocked_connections;
    uint64_t tls_connections;
    uint64_t plaintext_blocked;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t dns_queries;
    uint64_t dns_blocked;
    uint64_t http_requests;
    uint64_t https_requests;
} domes_net_stats_t;

/* ============================================================================
 * 生命周期管理
 * ============================================================================ */

/**
 * @brief 初始化网络安全模块
 * @param manager 配置参数 (NULL 使用默认配置)
 * @return 0 成功，非0 失败
 */
int domes_net_security_init(const domes_net_security_config_t* manager);

/**
 * @brief 清理网络安全模块
 */
void domes_net_security_cleanup(void);

/**
 * @brief 获取网络安全状态
 * @param manager 配置输出
 * @return 0 成功，非0 失败
 */
int domes_net_security_get_config(domes_net_security_config_t* manager);

/* ============================================================================
 * TLS 安全
 * ============================================================================ */

/**
 * @brief 配置 TLS
 * @param manager TLS 配置
 * @return 0 成功，非0 失败
 */
int domes_tls_configure(const domes_tls_config_t* manager);

/**
 * @brief 验证证书
 * @param cert_path 证书路径
 * @param hostname 主机名 (可选)
 * @param result 验证结果输出
 * @return 0 成功，非0 失败
 */
int domes_tls_verify_cert(const char* cert_path,
                           const char* hostname,
                           domes_cert_result_t* result);

/**
 * @brief 验证证书链
 * @param cert_chain 证书链 (PEM 格式)
 * @param chain_len 证书链长度
 * @param result 验证结果输出
 * @return 0 成功，非0 失败
 */
int domes_tls_verify_cert_chain(const char* cert_chain,
                                  size_t chain_len,
                                  domes_cert_result_t* result);

/**
 * @brief 检查 TLS 连接
 * @param hostname 主机名
 * @param port 端口
 * @param result 验证结果输出
 * @return 0 成功，非0 失败
 */
int domes_tls_check_connection(const char* hostname,
                                 uint16_t port,
                                 domes_cert_result_t* result);

/**
 * @brief 获取支持的加密套件
 * @param suites 加密套件列表输出
 * @param count 数量输出
 * @return 0 成功，非0 失败
 */
int domes_tls_get_cipher_suites(char*** suites, size_t* count);

/**
 * @brief 检查加密套件是否安全
 * @param suite 加密套件名称
 * @return 1 安全，0 不安全
 */
int domes_tls_is_cipher_secure(const char* suite);

/* ============================================================================
 * 网络过滤
 * ============================================================================ */

/**
 * @brief 添加过滤规则
 * @param rule 过滤规则
 * @return 0 成功，非0 失败
 */
int domes_net_add_rule(const domes_net_filter_rule_t* rule);

/**
 * @brief 删除过滤规则
 * @param rule_id 规则 ID
 * @return 0 成功，非0 失败
 */
int domes_net_remove_rule(const char* rule_id);

/**
 * @brief 更新过滤规则
 * @param rule_id 规则 ID
 * @param rule 新规则
 * @return 0 成功，非0 失败
 */
int domes_net_update_rule(const char* rule_id, const domes_net_filter_rule_t* rule);

/**
 * @brief 获取过滤规则
 * @param rule_id 规则 ID
 * @param rule 规则输出
 * @return 0 成功，非0 失败
 */
int domes_net_get_rule(const char* rule_id, domes_net_filter_rule_t* rule);

/**
 * @brief 列出所有规则
 * @param rules 规则数组输出
 * @param count 数量输出
 * @return 0 成功，非0 失败
 */
int domes_net_list_rules(domes_net_filter_rule_t** rules, size_t* count);

/**
 * @brief 检查网络访问权限
 * @param host 目标主机
 * @param port 端口
 * @param protocol 协议
 * @param direction 方向 (inbound/outbound)
 * @return 1 允许，0 拒绝
 */
int domes_net_check_access(const char* host,
                            uint16_t port,
                            domes_protocol_t protocol,
                            const char* direction);

/**
 * @brief 检查 URL 访问权限
 * @param url URL
 * @param method HTTP 方法
 * @return 1 允许，0 拒绝
 */
int domes_net_check_url(const char* url, const char* method);

/* ============================================================================
 * HTTP 安全
 * ============================================================================ */

/**
 * @brief 配置 HTTP 安全
 * @param manager HTTP 安全配置
 * @return 0 成功，非0 失败
 */
int domes_http_configure(const domes_http_security_config_t* manager);

/**
 * @brief 验证 HTTP 请求
 * @param method HTTP 方法
 * @param url URL
 * @param headers 请求头数组
 * @param header_count 请求头数量
 * @param body_size 请求体大小
 * @return 0 有效，非0 无效
 */
int domes_http_validate_request(const char* method,
                                  const char* url,
                                  const char** headers,
                                  size_t header_count,
                                  size_t body_size);

/**
 * @brief 添加安全响应头
 * @param headers 响应头数组
 * @param header_count 请求头数量
 * @param max_headers 最大响应头数量
 * @return 0 成功，非0 失败
 */
int domes_http_add_security_headers(const char** headers,
                                     size_t header_count,
                                     size_t max_headers);

/**
 * @brief 检查 URL 是否安全
 * @param url URL
 * @return 1 安全，0 不安全
 */
int domes_http_is_url_safe(const char* url);

/* ============================================================================
 * DNS 安全
 * ============================================================================ */

/**
 * @brief 配置 DNS 安全
 * @param manager DNS 安全配置
 * @return 0 成功，非0 失败
 */
int domes_dns_configure(const domes_dns_security_config_t* manager);

/**
 * @brief 安全 DNS 解析
 * @param hostname 主机名
 * @param ip_out IP 输出缓冲区
 * @param ip_len 缓冲区长度
 * @return 0 成功，非0 失败
 */
int domes_dns_resolve(const char* hostname, char* ip_out, size_t ip_len);

/**
 * @brief 检查域名是否允许
 * @param domain 域名
 * @return 1 允许，0 拒绝
 */
int domes_dns_is_domain_allowed(const char* domain);

/**
 * @brief 验证 DNSSEC
 * @param domain 域名
 * @return 1 有效，0 无效
 */
int domes_dns_verify_dnssec(const char* domain);

/* ============================================================================
 * 连接管理
 * ============================================================================ */

/**
 * @brief 获取活动连接列表
 * @param connections 连接信息数组输出
 * @param count 数量输出
 * @return 0 成功，非0 失败
 */
int domes_net_get_connections(domes_connection_info_t** connections, size_t* count);

/**
 * @brief 关闭连接
 * @param local_ip 本地 IP
 * @param local_port 本地端口
 * @param remote_ip 远程 IP
 * @param remote_port 远程端口
 * @return 0 成功，非0 失败
 */
int domes_net_close_connection(const char* local_ip, uint16_t local_port,
                                const char* remote_ip, uint16_t remote_port);

/**
 * @brief 获取网络统计
 * @param stats 统计输出
 * @return 0 成功，非0 失败
 */
int domes_net_get_stats(domes_net_stats_t* stats);

/**
 * @brief 重置网络统计
 */
void domes_net_reset_stats(void);

/* ============================================================================
 * 入侵检测
 * ============================================================================ */

/**
 * @brief 启用入侵检测
 * @param enabled 是否启用
 * @return 0 成功，非0 失败
 */
int domes_net_ids_enable(bool enabled);

/**
 * @brief 检测异常流量
 * @param connection 连接信息
 * @return 1 异常，0 正常
 */
int domes_net_detect_anomaly(const domes_connection_info_t* connection);

/**
 * @brief 设置入侵检测回调
 * @param callback 回调函数
 * @return 0 成功，非0 失败
 */
int domes_net_ids_set_callback(void (*callback)(const char* alert_type,
                                                 const char* details,
                                                 const domes_connection_info_t* conn));

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/**
 * @brief 获取 TLS 版本名称
 * @param version TLS 版本
 * @return 版本名称字符串
 */
const char* domes_tls_version_string(domes_tls_version_t version);

/**
 * @brief 获取协议名称
 * @param protocol 协议类型
 * @return 协议名称字符串
 */
const char* domes_protocol_string(domes_protocol_t protocol);

/**
 * @brief 获取证书验证结果描述
 * @param result 验证结果
 * @return 结果描述字符串
 */
const char* domes_cert_result_string(domes_cert_result_t result);

/**
 * @brief 解析 URL
 * @param url URL
 * @param scheme 协议输出
 * @param host 主机输出
 * @param port 端口输出
 * @param path 路径输出
 * @return 0 成功，非0 失败
 */
int domes_net_parse_url(const char* url,
                         char* scheme, char* host,
                         uint16_t* port, char* path);

/**
 * @brief 检查 IP 是否在 CIDR 范围内
 * @param ip IP 地址
 * @param cidr CIDR 表示法
 * @return 1 在范围内，0 不在
 */
int domes_net_ip_in_cidr(const char* ip, const char* cidr);

/**
 * @brief 验证 IP 地址格式
 * @param ip IP 地址
 * @return 1 有效，0 无效
 */
int domes_net_validate_ip(const char* ip);

/**
 * @brief 验证端口号
 * @param port 端口号
 * @return 1 有效，0 无效
 */
int domes_net_validate_port(uint16_t port);

#ifdef __cplusplus
}
#endif

#endif /* DOMES_NETWORK_SECURITY_H */
