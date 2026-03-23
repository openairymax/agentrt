// AgentOS Go SDK - 错误处理模块单元测试
// Version: 3.0.0

package agentos

import (
	"errors"
	"net/http"
	"testing"
)

func TestAgentOSError_Error(t *testing.T) {
	err := NewError(CodeInvalidConfig, "测试错误", nil)
	msg := err.Error()
	if msg != "[INVALID_CONFIG] 测试错误" {
		t.Errorf("错误信息 = %q, want [INVALID_CONFIG] 测试错误", msg)
	}
}

func TestAgentOSError_ErrorWithCause(t *testing.T) {
	cause := errors.New("原始错误")
	err := NewError(CodeNetworkError, "包装错误", cause)
	msg := err.Error()
	if msg != "[NETWORK_ERROR] 包装错误: 原始错误" {
		t.Errorf("错误信息 = %q", msg)
	}
}

func TestAgentOSError_Unwrap(t *testing.T) {
	cause := errors.New("原始错误")
	err := NewError(CodeNetworkError, "包装错误", cause)
	unwrapped := err.Unwrap()
	if unwrapped != cause {
		t.Error("Unwrap 应返回原始错误")
	}
}

func TestAgentOSError_UnwrapNil(t *testing.T) {
	err := NewError(CodeInvalidConfig, "无原因错误", nil)
	if err.Unwrap() != nil {
		t.Error("nil 原因的 Unwrap 应返回 nil")
	}
}

func TestAgentOSError_Is(t *testing.T) {
	err1 := NewError(CodeInvalidConfig, "错误1", nil)
	err2 := NewError(CodeInvalidConfig, "错误2", nil)
	err3 := NewError(CodeNetworkError, "错误3", nil)

	if !err1.Is(err2) {
		t.Error("相同错误码应匹配")
	}
	if err1.Is(err3) {
		t.Error("不同错误码不应匹配")
	}
	if err1.Is(errors.New("other")) {
		t.Error("非 AgentOSError 不应匹配")
	}
}

func TestNewErrorf(t *testing.T) {
	err := NewErrorf(CodeTaskTimeout, "任务 %s 超时", "t1")
	if err.Message != "任务 t1 超时" {
		t.Errorf("格式化消息 = %q", err.Message)
	}
	if err.Cause != nil {
		t.Error("NewErrorf 不应有 cause")
	}
}

func TestWrapError(t *testing.T) {
	cause := errors.New("io error")
	err := WrapError(CodeNetworkError, "网络异常", cause)
	if err.Cause != cause {
		t.Error("WrapError 应保留 cause")
	}
	if err.Code != CodeNetworkError {
		t.Error("WrapError 应设置正确错误码")
	}
}

func TestIsErrorCode(t *testing.T) {
	err := NewError(CodeNotFound, "未找到", nil)
	if !IsErrorCode(err, CodeNotFound) {
		t.Error("应匹配 NOT_FOUND 错误码")
	}
	if IsErrorCode(err, CodeNetworkError) {
		t.Error("不应匹配 NETWORK_ERROR 错误码")
	}
	if IsErrorCode(errors.New("other"), CodeNotFound) {
		t.Error("非 AgentOSError 不应匹配")
	}
}

func TestIsNetworkError(t *testing.T) {
	tests := []struct {
		code    ErrorCode
		isNet   bool
	}{
		{CodeNetworkError, true},
		{CodeTimeout, true},
		{CodeConnectionRefused, true},
		{CodeNotFound, false},
		{CodeServerError, false},
	}
	for _, tt := range tests {
		err := NewError(tt.code, "test", nil)
		if IsNetworkError(err) != tt.isNet {
			t.Errorf("IsNetworkError(%s) = %v, want %v", tt.code, IsNetworkError(err), tt.isNet)
		}
	}
}

func TestIsServerError(t *testing.T) {
	tests := []struct {
		code    ErrorCode
		isSrv   bool
	}{
		{CodeServerError, true},
		{CodeRateLimited, true},
		{CodeTaskFailed, true},
		{CodeSkillExecution, true},
		{CodeNotFound, false},
		{CodeNetworkError, false},
	}
	for _, tt := range tests {
		err := NewError(tt.code, "test", nil)
		if IsServerError(err) != tt.isSrv {
			t.Errorf("IsServerError(%s) = %v, want %v", tt.code, IsServerError(err), tt.isSrv)
		}
	}
}

func TestHTTPStatusToError(t *testing.T) {
	tests := []struct {
		status   int
		wantCode ErrorCode
	}{
		{http.StatusBadRequest, CodeInvalidParameter},
		{http.StatusUnauthorized, CodeUnauthorized},
		{http.StatusForbidden, CodeForbidden},
		{http.StatusNotFound, CodeNotFound},
		{http.StatusTooManyRequests, CodeRateLimited},
		{http.StatusInternalServerError, CodeServerError},
		{http.StatusBadGateway, CodeServerError},
		{http.StatusConflict, CodeServerError},
	}
	for _, tt := range tests {
		err := HTTPStatusToError(tt.status, "msg")
		if err.Code != tt.wantCode {
			t.Errorf("HTTPStatusToError(%d) = %s, want %s", tt.status, err.Code, tt.wantCode)
		}
	}
}

func TestSentinelErrors(t *testing.T) {
	if !errors.Is(ErrInvalidConfig, ErrInvalidConfig) {
		t.Error("哨兵错误应支持 errors.Is 自身匹配")
	}
	if errors.Is(ErrInvalidConfig, ErrNotFound) {
		t.Error("不同哨兵错误不应匹配")
	}
	if !IsErrorCode(ErrTimeout, CodeTimeout) {
		t.Error("哨兵错误应返回对应错误码")
	}
}
