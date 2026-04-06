// AgentOS Rust SDK - 错误码测试
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 测试错误码映射、错误链和错误处理

use agentos::*;

#[test]
fn test_error_code_mapping() {
    let test_cases = vec![
        (CODE_INVALID_PARAMETER, "0x0002"),
        (CODE_MISSING_PARAMETER, "0x0003"),
        (CODE_TIMEOUT, "0x0004"),
        (CODE_NOT_FOUND, "0x0005"),
        (CODE_ALREADY_EXISTS, "0x0006"),
        (CODE_CONFLICT, "0x0007"),
        (CODE_INVALID_CONFIG, "0x0008"),
        (CODE_INVALID_ENDPOINT, "0x0009"),
        (CODE_NETWORK_ERROR, "0x000A"),
        (CODE_CONNECTION_REFUSED, "0x000B"),
        (CODE_SERVER_ERROR, "0x000C"),
        (CODE_UNAUTHORIZED, "0x000D"),
        (CODE_FORBIDDEN, "0x000E"),
        (CODE_RATE_LIMITED, "0x000F"),
        (CODE_INVALID_RESPONSE, "0x0010"),
        (CODE_PARSE_ERROR, "0x0011"),
        (CODE_VALIDATION_ERROR, "0x0012"),
        (CODE_NOT_SUPPORTED, "0x0013"),
        (CODE_INTERNAL, "0x0014"),
        (CODE_BUSY, "0x0015"),
    ];

    for (code, expected_str) in test_cases {
        assert_eq!(code, expected_str, "错误码 {} 不匹配", code);
    }
}

#[test]
fn test_error_creation() {
    let err = AgentOSError::new(CODE_INVALID_PARAMETER, "参数无效");
    assert_eq!(err.code(), CODE_INVALID_PARAMETER);
    assert_eq!(err.message(), "参数无效");
    assert!(err.source().is_none());
}

#[test]
fn test_error_with_source() {
    let root_err = std::io::Error::new(std::io::ErrorKind::ConnectionRefused, "连接被拒绝");
    let sdk_err = AgentOSError::with_source(CODE_NETWORK_ERROR, "无法连接服务器", root_err);

    assert_eq!(sdk_err.code(), CODE_NETWORK_ERROR);
    assert_eq!(sdk_err.message(), "无法连接服务器");
    assert!(sdk_err.source().is_some());
}

#[test]
fn test_error_chain() {
    let root_err = std::io::Error::new(std::io::ErrorKind::TimedOut, "操作超时");
    let mid_err = AgentOSError::with_source(CODE_NETWORK_ERROR, "网络请求失败", root_err);
    let top_err = AgentOSError::with_source(CODE_TIMEOUT, "任务执行超时", mid_err);

    let mut chain: Vec<&str> = vec![];
    let mut current: Option<&dyn std::error::Error> = Some(&top_err);
    while let Some(err) = current {
        chain.push(err.to_string().as_str());
        current = err.source();
    }

    assert!(chain.len() >= 3, "错误链应至少包含3层");
}

#[test]
fn test_error_display() {
    let err = AgentOSError::new(CODE_INVALID_PARAMETER, "参数不能为空");
    let display = format!("{}", err);

    assert!(display.contains("0x0002"), "错误显示应包含错误码");
    assert!(display.contains("参数不能为空"), "错误显示应包含错误消息");
}

#[test]
fn test_error_debug() {
    let err = AgentOSError::new(CODE_TIMEOUT, "请求超时");
    let debug = format!("{:?}", err);

    assert!(debug.contains("AgentOSError"), "Debug 输出应包含类型名");
    assert!(debug.contains("code"), "Debug 输出应包含 code 字段");
    assert!(debug.contains("message"), "Debug 输出应包含 message 字段");
}

#[test]
fn test_http_status_to_error_code() {
    let test_cases = vec![
        (400, CODE_INVALID_PARAMETER),
        (401, CODE_UNAUTHORIZED),
        (403, CODE_FORBIDDEN),
        (404, CODE_NOT_FOUND),
        (405, CODE_NOT_SUPPORTED),
        (408, CODE_TIMEOUT),
        (409, CODE_CONFLICT),
        (422, CODE_VALIDATION_ERROR),
        (429, CODE_RATE_LIMITED),
        (500, CODE_SERVER_ERROR),
        (502, CODE_SERVER_ERROR),
        (503, CODE_SERVER_ERROR),
        (504, CODE_TIMEOUT),
    ];

    for (status, expected_code) in test_cases {
        let result = http_status_to_error_code(status);
        assert_eq!(result, expected_code, "HTTP {} 应映射到 {}", status, expected_code);
    }
}

#[test]
fn test_error_from_io_error() {
    let io_err = std::io::Error::new(std::io::ErrorKind::ConnectionRefused, "连接被拒绝");
    let sdk_err: AgentOSError = io_err.into();

    assert_eq!(sdk_err.code(), CODE_CONNECTION_REFUSED);
}

#[test]
fn test_error_from_reqwest_error() {
    // 模拟 reqwest 错误转换
    // 在实际测试中需要 mock reqwest 错误
}

#[test]
fn test_result_type_alias() {
    fn returns_result() -> Result<String, AgentOSError> {
        Ok("success".to_string())
    }

    fn returns_error() -> Result<String, AgentOSError> {
        Err(AgentOSError::new(CODE_INVALID_PARAMETER, "测试错误"))
    }

    assert!(returns_result().is_ok());
    assert!(returns_error().is_err());
}

#[test]
fn test_error_is_network_error() {
    let err = AgentOSError::new(CODE_NETWORK_ERROR, "网络错误");
    assert!(err.is_network_error());

    let err2 = AgentOSError::new(CODE_INVALID_PARAMETER, "参数错误");
    assert!(!err2.is_network_error());
}

#[test]
fn test_error_is_timeout() {
    let err = AgentOSError::new(CODE_TIMEOUT, "超时");
    assert!(err.is_timeout());

    let err2 = AgentOSError::new(CODE_NETWORK_ERROR, "网络错误");
    assert!(!err2.is_timeout());
}

#[test]
fn test_error_is_retryable() {
    let retryable_codes = vec![
        CODE_NETWORK_ERROR,
        CODE_TIMEOUT,
        CODE_SERVER_ERROR,
        CODE_BUSY,
        CODE_RATE_LIMITED,
    ];

    for code in retryable_codes {
        let err = AgentOSError::new(code, "可重试错误");
        assert!(err.is_retryable(), "错误码 {} 应该可重试", code);
    }

    let non_retryable_codes = vec![
        CODE_INVALID_PARAMETER,
        CODE_UNAUTHORIZED,
        CODE_FORBIDDEN,
        CODE_NOT_FOUND,
    ];

    for code in non_retryable_codes {
        let err = AgentOSError::new(code, "不可重试错误");
        assert!(!err.is_retryable(), "错误码 {} 不应可重试", code);
    }
}

#[test]
fn test_error_serialization() {
    let err = AgentOSError::new(CODE_INVALID_PARAMETER, "参数无效");
    let json = serde_json::to_string(&err).expect("序列化失败");

    assert!(json.contains("0x0002"), "JSON 应包含错误码");
    assert!(json.contains("参数无效"), "JSON 应包含错误消息");

    let deserialized: AgentOSError = serde_json::from_str(&json).expect("反序列化失败");
    assert_eq!(deserialized.code(), err.code());
    assert_eq!(deserialized.message(), err.message());
}
