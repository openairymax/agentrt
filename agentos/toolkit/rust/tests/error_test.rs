// AgentOS Rust SDK - 错误码测试
// Version: 3.0.0
// Last updated: 2026-04-27

use agentos_rs::*;

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
fn test_error_creation_with_code() {
    let err = AgentOSError::with_code(CODE_INVALID_PARAMETER, "参数无效");
    assert_eq!(err.code(), CODE_INVALID_PARAMETER);
}

#[test]
fn test_error_display() {
    let err = AgentOSError::with_code(CODE_INVALID_PARAMETER, "参数不能为空");
    let display = format!("{}", err);

    assert!(display.contains("0x0002"), "错误显示应包含错误码");
    assert!(display.contains("参数不能为空"), "错误显示应包含错误消息");
}

#[test]
fn test_error_debug() {
    let err = AgentOSError::with_code(CODE_TIMEOUT, "请求超时");
    let debug = format!("{:?}", err);

    assert!(!debug.is_empty(), "Debug 输出不应为空");
}

#[test]
fn test_http_status_to_error_code() {
    let test_cases = vec![
        (400, CODE_INVALID_PARAMETER),
        (401, CODE_UNAUTHORIZED),
        (403, CODE_FORBIDDEN),
        (404, CODE_NOT_FOUND),
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
        let result = http_status_to_code(status);
        assert_eq!(result, expected_code, "HTTP {} 应映射到 {}", status, expected_code);
    }
}

#[test]
fn test_error_from_io_error() {
    let io_err = std::io::Error::new(std::io::ErrorKind::ConnectionRefused, "连接被拒绝");
    let sdk_err: AgentOSError = io_err.into();

    assert!(sdk_err.code() == CODE_CONNECTION_REFUSED || sdk_err.code() == CODE_INTERNAL,
        "IO ConnectionRefused 应映射到网络错误或内部错误，实际: {}", sdk_err.code());
}

#[test]
fn test_result_type_alias() {
    fn returns_result() -> Result<String, AgentOSError> {
        Ok("success".to_string())
    }

    fn returns_error() -> Result<String, AgentOSError> {
        Err(AgentOSError::with_code(CODE_INVALID_PARAMETER, "测试错误"))
    }

    assert!(returns_result().is_ok());
    assert!(returns_error().is_err());
}

#[test]
fn test_error_is_network_error() {
    let err = AgentOSError::network("网络错误");
    assert!(err.is_network_error());

    let err2 = AgentOSError::with_code(CODE_INVALID_PARAMETER, "参数错误");
    assert!(!err2.is_network_error());
}

#[test]
fn test_error_is_timeout() {
    let err = AgentOSError::timeout("超时");
    assert!(err.is_network_error());

    let err2 = AgentOSError::network("网络错误");
    assert!(err2.is_network_error());
}

#[test]
fn test_error_variants() {
    let network_err = AgentOSError::network("网络错误");
    assert_eq!(network_err.code(), CODE_NETWORK_ERROR);
    assert!(network_err.is_network_error());

    let http_err = AgentOSError::http("服务器错误");
    assert_eq!(http_err.code(), CODE_SERVER_ERROR);
    assert!(http_err.is_server_error());

    let timeout_err = AgentOSError::timeout("超时");
    assert_eq!(timeout_err.code(), CODE_TIMEOUT);
    assert!(timeout_err.is_network_error());

    let conn_refused = AgentOSError::connection_refused("连接被拒绝");
    assert_eq!(conn_refused.code(), CODE_CONNECTION_REFUSED);
    assert!(conn_refused.is_network_error());

    let not_found = AgentOSError::not_found("未找到");
    assert_eq!(not_found.code(), CODE_NOT_FOUND);

    let unauthorized = AgentOSError::unauthorized("未授权");
    assert_eq!(unauthorized.code(), CODE_UNAUTHORIZED);

    let forbidden = AgentOSError::forbidden("禁止访问");
    assert_eq!(forbidden.code(), CODE_FORBIDDEN);

    let invalid_param = AgentOSError::invalid_parameter("参数无效");
    assert_eq!(invalid_param.code(), CODE_INVALID_PARAMETER);

    let missing_param = AgentOSError::missing_parameter("缺少参数");
    assert_eq!(missing_param.code(), CODE_MISSING_PARAMETER);

    let parse_err = AgentOSError::parse_error("解析错误");
    assert_eq!(parse_err.code(), CODE_PARSE_ERROR);

    let internal_err = AgentOSError::internal("内部错误");
    assert_eq!(internal_err.code(), CODE_INTERNAL);
}

#[test]
fn test_error_from_serde_json_error() {
    let json_err = serde_json::from_str::<serde_json::Value>("{invalid}");
    assert!(json_err.is_err());

    let sdk_err: AgentOSError = json_err.unwrap_err().into();
    assert_eq!(sdk_err.code(), CODE_PARSE_ERROR);
}
