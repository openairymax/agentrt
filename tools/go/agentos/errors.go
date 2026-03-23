// AgentOS Go SDK - 统一错误体系
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 定义 SDK 的完整错误类型层级、错误码枚举、哨兵错误和 HTTP 状态码映射。
// 所有异常继承自 AgentOSError，支持 errors.Is/As 链式追踪。
// 对应 Python SDK: exceptions.py

package agentos

import (
	"errors"
	"fmt"
	"net/http"
)

// ErrorCode 表示 AgentOS SDK 的错误码类型
type ErrorCode string

// AgentOSError 是 SDK 所有错误的统一基类
type AgentOSError struct {
	Code    ErrorCode
	Message string
	Cause   error
}

// Error 实现 error 接口
func (e *AgentOSError) Error() string {
	if e.Cause != nil {
		return fmt.Sprintf("[%s] %s: %v", e.Code, e.Message, e.Cause)
	}
	return fmt.Sprintf("[%s] %s", e.Code, e.Message)
}

// Unwrap 支持错误链追踪
func (e *AgentOSError) Unwrap() error {
	return e.Cause
}

// Is 实现错误匹配，支持 errors.Is 语义
func (e *AgentOSError) Is(target error) bool {
	t, ok := target.(*AgentOSError)
	if !ok {
		return false
	}
	return e.Code == t.Code
}

// NewError 创建指定错误码的新错误
func NewError(code ErrorCode, message string, cause error) *AgentOSError {
	return &AgentOSError{Code: code, Message: message, Cause: cause}
}

// NewErrorf 格式化创建指定错误码的新错误
func NewErrorf(code ErrorCode, format string, args ...interface{}) *AgentOSError {
	return &AgentOSError{Code: code, Message: fmt.Sprintf(format, args...)}
}

// WrapError 包装已有错误并附加 SDK 错误码
func WrapError(code ErrorCode, message string, cause error) *AgentOSError {
	return &AgentOSError{Code: code, Message: message, Cause: cause}
}

// IsErrorCode 判断错误是否匹配指定错误码
func IsErrorCode(err error, code ErrorCode) bool {
	var agentErr *AgentOSError
	if errors.As(err, &agentErr) {
		return agentErr.Code == code
	}
	return false
}

// IsNetworkError 判断是否为网络相关错误
func IsNetworkError(err error) bool {
	var agentErr *AgentOSError
	if errors.As(err, &agentErr) {
		return agentErr.Code == CodeNetworkError ||
			agentErr.Code == CodeTimeout ||
			agentErr.Code == CodeConnectionRefused
	}
	return false
}

// IsServerError 判断是否为服务端错误
func IsServerError(err error) bool {
	var agentErr *AgentOSError
	if errors.As(err, &agentErr) {
		return agentErr.Code == CodeServerError ||
			agentErr.Code == CodeRateLimited ||
			agentErr.Code == CodeTaskFailed ||
			agentErr.Code == CodeSkillExecution
	}
	return false
}

// HTTPStatusToError 将 HTTP 状态码映射为 SDK 错误
func HTTPStatusToError(statusCode int, message string) *AgentOSError {
	switch {
	case statusCode == http.StatusBadRequest:
		return NewError(CodeInvalidParameter, message, nil)
	case statusCode == http.StatusUnauthorized:
		return NewError(CodeUnauthorized, message, nil)
	case statusCode == http.StatusForbidden:
		return NewError(CodeForbidden, message, nil)
	case statusCode == http.StatusNotFound:
		return NewError(CodeNotFound, message, nil)
	case statusCode == http.StatusTooManyRequests:
		return NewError(CodeRateLimited, message, nil)
	case statusCode >= 500:
		return NewError(CodeServerError, message, nil)
	default:
		return NewError(CodeServerError, message, nil)
	}
}

// ============================================================
// 错误码常量 (Code 前缀)
// ============================================================

const (
	CodeInvalidConfig     ErrorCode = "INVALID_CONFIG"
	CodeInvalidEndpoint   ErrorCode = "INVALID_ENDPOINT"
	CodeInvalidParameter  ErrorCode = "INVALID_PARAMETER"
	CodeMissingParameter  ErrorCode = "MISSING_PARAMETER"
	CodeNetworkError      ErrorCode = "NETWORK_ERROR"
	CodeTimeout           ErrorCode = "TIMEOUT"
	CodeConnectionRefused ErrorCode = "CONNECTION_REFUSED"
	CodeServerError       ErrorCode = "SERVER_ERROR"
	CodeNotFound          ErrorCode = "NOT_FOUND"
	CodeUnauthorized      ErrorCode = "UNAUTHORIZED"
	CodeForbidden         ErrorCode = "FORBIDDEN"
	CodeRateLimited       ErrorCode = "RATE_LIMITED"
	CodeInvalidResponse   ErrorCode = "INVALID_RESPONSE"
	CodeParseError        ErrorCode = "PARSE_ERROR"
	CodeTaskFailed        ErrorCode = "TASK_FAILED"
	CodeTaskCancelled     ErrorCode = "TASK_CANCELLED"
	CodeTaskTimeout       ErrorCode = "TASK_TIMEOUT"
	CodeSessionNotFound   ErrorCode = "SESSION_NOT_FOUND"
	CodeSessionExpired    ErrorCode = "SESSION_EXPIRED"
	CodeMemoryNotFound    ErrorCode = "MEMORY_NOT_FOUND"
	CodeSkillNotFound     ErrorCode = "SKILL_NOT_FOUND"
	CodeSkillExecution    ErrorCode = "SKILL_EXECUTION"
	CodeTelemetryError    ErrorCode = "TELEMETRY_ERROR"
	CodeValidationError   ErrorCode = "VALIDATION_ERROR"
)

// ============================================================
// 哨兵错误 (Err 前缀, 支持 errors.Is)
// ============================================================

var (
	ErrInvalidConfig     = NewError(CodeInvalidConfig, "配置无效", nil)
	ErrInvalidEndpoint   = NewError(CodeInvalidEndpoint, "端点地址无效", nil)
	ErrInvalidParameter  = NewError(CodeInvalidParameter, "参数无效", nil)
	ErrMissingParameter  = NewError(CodeMissingParameter, "缺少必要参数", nil)
	ErrNetworkError      = NewError(CodeNetworkError, "网络错误", nil)
	ErrTimeout           = NewError(CodeTimeout, "操作超时", nil)
	ErrConnectionRefused = NewError(CodeConnectionRefused, "连接被拒绝", nil)
	ErrServerError       = NewError(CodeServerError, "服务端错误", nil)
	ErrNotFound          = NewError(CodeNotFound, "资源未找到", nil)
	ErrUnauthorized      = NewError(CodeUnauthorized, "未授权", nil)
	ErrForbidden         = NewError(CodeForbidden, "访问被禁止", nil)
	ErrRateLimited       = NewError(CodeRateLimited, "请求频率超限", nil)
	ErrInvalidResponse   = NewError(CodeInvalidResponse, "响应格式异常", nil)
	ErrParseError        = NewError(CodeParseError, "数据解析失败", nil)
	ErrTaskFailed        = NewError(CodeTaskFailed, "任务执行失败", nil)
	ErrTaskCancelled     = NewError(CodeTaskCancelled, "任务已取消", nil)
	ErrTaskTimeout       = NewError(CodeTaskTimeout, "任务超时", nil)
	ErrSessionNotFound   = NewError(CodeSessionNotFound, "会话未找到", nil)
	ErrSessionExpired    = NewError(CodeSessionExpired, "会话已过期", nil)
	ErrMemoryNotFound    = NewError(CodeMemoryNotFound, "记忆未找到", nil)
	ErrSkillNotFound     = NewError(CodeSkillNotFound, "技能未找到", nil)
	ErrSkillExecution    = NewError(CodeSkillExecution, "技能执行失败", nil)
	ErrTelemetryError    = NewError(CodeTelemetryError, "遥测错误", nil)
	ErrValidationError   = NewError(CodeValidationError, "数据验证失败", nil)
)
