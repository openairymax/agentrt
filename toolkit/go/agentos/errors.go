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
// 对于 2xx 成功状态码返回 nil
func HTTPStatusToError(statusCode int, message string) *AgentOSError {
	// 2xx 状态码表示成功，不应转换为错误
	if statusCode >= 200 && statusCode < 300 {
		return nil
	}

	switch {
	case statusCode == http.StatusBadRequest:
		return NewError(CodeInvalidParameter, message, nil)
	case statusCode == http.StatusUnauthorized:
		return NewError(CodeUnauthorized, message, nil)
	case statusCode == http.StatusForbidden:
		return NewError(CodeForbidden, message, nil)
	case statusCode == http.StatusNotFound:
		return NewError(CodeNotFound, message, nil)
	case statusCode == http.StatusRequestTimeout:
		return NewError(CodeTimeout, message, nil)
	case statusCode == http.StatusConflict:
		return NewError(CodeConflict, message, nil)
	case statusCode == http.StatusTooManyRequests:
		return NewError(CodeRateLimited, message, nil)
	case statusCode == http.StatusUnprocessableEntity:
		return NewError(CodeValidationError, message, nil)
	case statusCode >= 500:
		return NewError(CodeServerError, message, nil)
	default:
		return NewError(CodeUnknown, message, nil)
	}
}

// ============================================================
// 错误码常量
// 基于 ErrorCodeReference.md 规范，采用十六进制分类体系：
//   0x0xxx 通用错误 (General)
//   0x1xxx 核心循环错误 (CoreLoop)
//   0x2xxx 认知层错误 (Cognition)
//   0x3xxx 执行层错误 (Execution)
//   0x4xxx 记忆层错误 (Memory)
//   0x5xxx 系统调用错误 (Syscall)
//   0x6xxx 安全域错误 (Security)
//   0x7xxx 动态模块错误 (gateway)
// ============================================================

const (
	CodeSuccess          ErrorCode = "0x0000"
	CodeUnknown          ErrorCode = "0x0001"
	CodeInvalidParameter ErrorCode = "0x0002"
	CodeMissingParameter ErrorCode = "0x0003"
	CodeTimeout          ErrorCode = "0x0004"
	CodeNotFound         ErrorCode = "0x0005"
	CodeAlreadyExists    ErrorCode = "0x0006"
	CodeConflict         ErrorCode = "0x0007"
	CodeInvalidConfig    ErrorCode = "0x0008"
	CodeInvalidEndpoint  ErrorCode = "0x0009"
	CodeNetworkError     ErrorCode = "0x000A"
	CodeConnectionRefused ErrorCode = "0x000B"
	CodeServerError      ErrorCode = "0x000C"
	CodeUnauthorized     ErrorCode = "0x000D"
	CodeForbidden        ErrorCode = "0x000E"
	CodeRateLimited      ErrorCode = "0x000F"
	CodeInvalidResponse  ErrorCode = "0x0010"
	CodeParseError       ErrorCode = "0x0011"
	CodeValidationError  ErrorCode = "0x0012"
	CodeNotSupported     ErrorCode = "0x0013"
	CodeInternal         ErrorCode = "0x0014"
	CodeBusy             ErrorCode = "0x0015"

	CodeLoopCreateFailed ErrorCode = "0x1001"
	CodeLoopStartFailed  ErrorCode = "0x1002"
	CodeLoopStopFailed   ErrorCode = "0x1003"

	CodeCognitionFailed    ErrorCode = "0x2001"
	CodeDAGBuildFailed     ErrorCode = "0x2002"
	CodeAgentDispatchFailed ErrorCode = "0x2003"
	CodeIntentParseFailed  ErrorCode = "0x2004"

	CodeTaskFailed    ErrorCode = "0x3001"
	CodeTaskCancelled ErrorCode = "0x3002"
	CodeTaskTimeout   ErrorCode = "0x3003"

	CodeMemoryNotFound ErrorCode = "0x4001"
	CodeMemoryEvolveFailed ErrorCode = "0x4002"
	CodeMemorySearchFailed ErrorCode = "0x4003"

	CodeSessionNotFound ErrorCode = "0x4004"
	CodeSessionExpired  ErrorCode = "0x4005"

	CodeSkillNotFound  ErrorCode = "0x4006"
	CodeSkillExecution ErrorCode = "0x4007"

	CodeTelemetryError ErrorCode = "0x5001"
	CodeSyscallError  ErrorCode = "0x5002"

	CodePermissionDenied ErrorCode = "0x6001"
	CodeCorruptedData    ErrorCode = "0x6002"
)

// ============================================================
// 哨兵错误 (Err 前缀, 支持 errors.Is)
// ============================================================

var (
	ErrSuccess          = NewError(CodeSuccess, "操作成功", nil)
	ErrUnknown          = NewError(CodeUnknown, "未知错误", nil)
	ErrInvalidParameter = NewError(CodeInvalidParameter, "参数无效", nil)
	ErrMissingParameter = NewError(CodeMissingParameter, "缺少必要参数", nil)
	ErrTimeout          = NewError(CodeTimeout, "操作超时", nil)
	ErrNotFound         = NewError(CodeNotFound, "资源未找到", nil)
	ErrAlreadyExists    = NewError(CodeAlreadyExists, "资源已存在", nil)
	ErrConflict         = NewError(CodeConflict, "资源冲突", nil)
	ErrInvalidConfig    = NewError(CodeInvalidConfig, "配置无效", nil)
	ErrInvalidEndpoint  = NewError(CodeInvalidEndpoint, "端点地址无效", nil)
	ErrNetworkError     = NewError(CodeNetworkError, "网络错误", nil)
	ErrConnectionRefused = NewError(CodeConnectionRefused, "连接被拒绝", nil)
	ErrServerError      = NewError(CodeServerError, "服务端错误", nil)
	ErrUnauthorized     = NewError(CodeUnauthorized, "未授权", nil)
	ErrForbidden        = NewError(CodeForbidden, "访问被禁止", nil)
	ErrRateLimited      = NewError(CodeRateLimited, "请求频率超限", nil)
	ErrInvalidResponse  = NewError(CodeInvalidResponse, "响应格式异常", nil)
	ErrParseError       = NewError(CodeParseError, "数据解析失败", nil)
	ErrValidationError  = NewError(CodeValidationError, "数据验证失败", nil)
	ErrNotSupported     = NewError(CodeNotSupported, "操作不支持", nil)
	ErrInternal         = NewError(CodeInternal, "内部错误", nil)
	ErrBusy             = NewError(CodeBusy, "系统繁忙", nil)

	ErrLoopCreateFailed = NewError(CodeLoopCreateFailed, "核心循环创建失败", nil)
	ErrLoopStartFailed  = NewError(CodeLoopStartFailed, "核心循环启动失败", nil)
	ErrLoopStopFailed   = NewError(CodeLoopStopFailed, "核心循环停止失败", nil)

	ErrCognitionFailed    = NewError(CodeCognitionFailed, "认知处理失败", nil)
	ErrDAGBuildFailed     = NewError(CodeDAGBuildFailed, "DAG 构建失败", nil)
	ErrAgentDispatchFailed = NewError(CodeAgentDispatchFailed, "Agent 调度失败", nil)
	ErrIntentParseFailed  = NewError(CodeIntentParseFailed, "意图解析失败", nil)

	ErrTaskFailed    = NewError(CodeTaskFailed, "任务执行失败", nil)
	ErrTaskCancelled = NewError(CodeTaskCancelled, "任务已取消", nil)
	ErrTaskTimeout   = NewError(CodeTaskTimeout, "任务超时", nil)

	ErrMemoryNotFound    = NewError(CodeMemoryNotFound, "记忆未找到", nil)
	ErrMemoryEvolveFailed = NewError(CodeMemoryEvolveFailed, "记忆演化失败", nil)
	ErrMemorySearchFailed = NewError(CodeMemorySearchFailed, "记忆搜索失败", nil)

	ErrSessionNotFound = NewError(CodeSessionNotFound, "会话未找到", nil)
	ErrSessionExpired  = NewError(CodeSessionExpired, "会话已过期", nil)

	ErrSkillNotFound  = NewError(CodeSkillNotFound, "技能未找到", nil)
	ErrSkillExecution = NewError(CodeSkillExecution, "技能执行失败", nil)

	ErrTelemetryError   = NewError(CodeTelemetryError, "遥测错误", nil)
	ErrSyscallError     = NewError(CodeSyscallError, "系统调用错误", nil)
	ErrPermissionDenied = NewError(CodePermissionDenied, "权限不足", nil)
	ErrCorruptedData    = NewError(CodeCorruptedData, "数据损坏", nil)
)
