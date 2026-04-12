/**
 * @file curl_stub.h
 * @brief libcurl stub头文件（用于编译时无libcurl的情况）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 当系统未安装libcurl开发库时，使用此stub文件进行编译。
 * 所有CURL函数调用将返回错误码或空操作。
 */

#ifndef AGENTOS_CURL_STUB_H
#define AGENTOS_CURL_STUB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>  /* for NULL */

/* CURL 类型定义 */
typedef void CURL;
typedef struct curl_slist {
    char* data;
    struct curl_slist* next;
} curl_slist_t;

/* CURL 返回码 */
#define CURLE_OK                     0   /* 成功 */
#define CURLE_UNSUPPORTED_PROTOCOL   1  /* 不支持的协议 */
#define CURLE_FAILED_INIT            2  /* 初始化失败 */
#define CURLE_URL_MALFORMAT          3  /* URL格式错误 */
#define CURLE_NOT_BUILT_IN           4  /* 功能未编译进库 */
#define CURLE_COULDNT_RESOLVE_PROXY  5  /* 无法解析代理主机 */
#define CURLE_COULDNT_RESOLVE_HOST   6  /* 无法解析主机 */
#define CURLE_COULDNT_CONNECT        7  /* 无法连接到服务器 */
#define CURLE_FTP_WEIRD_SERVER_REPLY 8  /* FTP服务器返回意外响应 */
#define CURLE_REMOTE_ACCESS_DENIED   9  /* 访问被拒绝 */
#define CURLE_FTP_ACCEPT_FAILED      10 /* FTP接受失败 */
#define CURLE_FTP_WEIRD_PASS_REPLY   11 /* FTP密码响应异常 */
#define CURLE_FTP_ACCEPT_TIMEOUT     12 /* FTP接受超时 */
#define CURLE_FTP_WEIRD_PASV_REPLY   13 /* FTP PASV响应异常 */
#define CURLE_FTP_WEIRD_227_FORMAT   14 /* FTP 227格式错误 */
#define CURLE_FTP_CANT_GET_HOST      15 /* 无法获取FTP主机 */
#define CURLE_HTTP2                  16 /* HTTP/2层问题 */
#define CURLE_SSL_CONNECT_ERROR      17 /* SSL连接错误 */
#define CURLE_BAD_DOWNLOAD_RESUME    18 /* 无法恢复下载 */
#define CURLE_FILE_COULDNT_READ_FILE 19 /* 无法读取文件 */
#define CURLE_LDAP_INVALID_URL       20 /* LDAP URL无效 */
#define CURLE_TFTP_ILLEGAL           21 /* TFTP操作非法 */
#define CURLE_TFTP_NOSUCHSERVER      22 /* TFTP服务器不存在 */
#define CURLE_PARTIAL_FILE           23 /* 文件传输不完整 */
#define CURLE_NO_DATA_AVAILABLE      24 /* 无可用数据 */
#define CURLE_QUOTE_ERROR            25 /* 引号命令失败 */
#define CURLE_WRITE_ERROR            26 /* 写入本地数据失败 */
#define CURLE_UPLOAD_FAILED          27 /* 上传失败 */
#define CURLE_READ_ERROR             28 /* 读取本地数据失败 */
#define CURLE_OUT_OF_MEMORY          29 /* 内存不足 */
#define CURLE_OPERATION_TIMEDOUT     30 /* 操作超时 */

/* CURL 选项常量 */
typedef int CURLoption;  /* CURL选项类型 */

#define CURLOPT_URL              10002L
#define CURLOPT_WRITEFUNCTION     20011L
#define CURLOPT_WRITEDATA         10001L
#define CURLOPT_HTTPHEADER        10023L
#define CURLOPT_POST              47L
#define CURLOPT_POSTFIELDS        10015L
#define CURLOPT_USERAGENT         10018L
#define CURLOPT_FOLLOWLOCATION    52L
#define CURLOPT_MAXREDIRS         68L
#define CURLOPT_FAILONERROR       45L
#define CURLOPT_TIMEOUT           13L
#define CURLOPT_CONNECTTIMEOUT    78L
#define CURLOPT_SSL_VERIFYPEER     64L
#define CURLOPT_SSL_VERIFYHOST     81L
#define CURLOPT_TIMEOUT_MS        155L  /* 超时（毫秒）*/

/* CURL 信息常量 */
typedef int CURLINFO;
#define CURLINFO_RESPONSE_CODE   2097154L /* HTTP响应码 */

/* CURL 返回码类型 */
typedef int CURLcode;

static inline const char* curl_easy_strerror(CURLcode errornum) {
    (void)errornum;
    return "CURL not available (stub implementation)";
}

static inline CURLcode curl_easy_getinfo(CURL* curl, CURLINFO info, ...) {
    (void)curl; (void)info;
    return CURLE_FAILED_INIT;
}

static inline void curl_easy_reset(CURL* curl) {
    (void)curl;
}

/* CURL 函数声明（stub实现） */
static inline CURL* curl_easy_init(void) {
    return NULL;
}

static inline void curl_easy_cleanup(CURL* curl) {
    (void)curl;
}

static inline int curl_easy_setopt(CURL* curl, CURLoption option, ...) {
    (void)curl;
    (void)option;
    return CURLE_FAILED_INIT;
}

static inline int curl_easy_perform(CURL* curl) {
    (void)curl;
    return CURLE_FAILED_INIT;
}

static inline curl_slist_t* curl_slist_append(curl_slist_t* list, const char* str) {
    (void)list;
    (void)str;
    return NULL;
}

static inline void curl_slist_free_all(curl_slist_t* list) {
    (void)list;
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CURL_STUB_H */