// AgentOS Go SDK - HTTP状态码全覆盖测试
// Version: 3.0.0
// Last updated: 2026-04-05
//
// 测试所有HTTP状态码到错误码的映射

package client

import (
	"context"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/spharx/agentos/toolkit/go/agentos"
)

func TestHTTPStatusToError_FullCoverage(t *testing.T) {
	testCases := []struct {
		name       string
		statusCode int
		wantCode   agentos.ErrorCode
	}{
		// 2xx 成功状态码
		{
			name:       "HTTP_200_OK",
			statusCode: http.StatusOK,
			wantCode:   "",
		},
		{
			name:       "HTTP_201_Created",
			statusCode: http.StatusCreated,
			wantCode:   "",
		},
		{
			name:       "HTTP_204_NoContent",
			statusCode: http.StatusNoContent,
			wantCode:   "",
		},

		// 4xx 客户端错误
		{
			name:       "HTTP_400_BadRequest",
			statusCode: http.StatusBadRequest,
			wantCode:   agentos.CodeInvalidParameter,
		},
		{
			name:       "HTTP_401_Unauthorized",
			statusCode: http.StatusUnauthorized,
			wantCode:   agentos.CodeUnauthorized,
		},
		{
			name:       "HTTP_403_Forbidden",
			statusCode: http.StatusForbidden,
			wantCode:   agentos.CodeForbidden,
		},
		{
			name:       "HTTP_404_NotFound",
			statusCode: http.StatusNotFound,
			wantCode:   agentos.CodeNotFound,
		},
		{
			name:       "HTTP_405_MethodNotAllowed",
			statusCode: http.StatusMethodNotAllowed,
			wantCode:   agentos.CodeNotSupported,
		},
		{
			name:       "HTTP_408_RequestTimeout",
			statusCode: http.StatusRequestTimeout,
			wantCode:   agentos.CodeTimeout,
		},
		{
			name:       "HTTP_409_Conflict",
			statusCode: http.StatusConflict,
			wantCode:   agentos.CodeConflict,
		},
		{
			name:       "HTTP_410_Gone",
			statusCode: http.StatusGone,
			wantCode:   agentos.CodeNotFound,
		},
		{
			name:       "HTTP_411_LengthRequired",
			statusCode: http.StatusLengthRequired,
			wantCode:   agentos.CodeInvalidParameter,
		},
		{
			name:       "HTTP_412_PreconditionFailed",
			statusCode: http.StatusPreconditionFailed,
			wantCode:   agentos.CodeValidationError,
		},
		{
			name:       "HTTP_413_PayloadTooLarge",
			statusCode: http.StatusRequestEntityTooLarge,
			wantCode:   agentos.CodeInvalidParameter,
		},
		{
			name:       "HTTP_414_URITooLong",
			statusCode: http.StatusRequestURITooLong,
			wantCode:   agentos.CodeInvalidParameter,
		},
		{
			name:       "HTTP_415_UnsupportedMediaType",
			statusCode: http.StatusUnsupportedMediaType,
			wantCode:   agentos.CodeNotSupported,
		},
		{
			name:       "HTTP_422_UnprocessableEntity",
			statusCode: http.StatusUnprocessableEntity,
			wantCode:   agentos.CodeValidationError,
		},
		{
			name:       "HTTP_423_Locked",
			statusCode: http.StatusLocked,
			wantCode:   agentos.CodeConflict,
		},
		{
			name:       "HTTP_424_FailedDependency",
			statusCode: http.StatusFailedDependency,
			wantCode:   agentos.CodeServerError,
		},
		{
			name:       "HTTP_429_TooManyRequests",
			statusCode: http.StatusTooManyRequests,
			wantCode:   agentos.CodeRateLimited,
		},

		// 5xx 服务端错误
		{
			name:       "HTTP_500_InternalServerError",
			statusCode: http.StatusInternalServerError,
			wantCode:   agentos.CodeServerError,
		},
		{
			name:       "HTTP_501_NotImplemented",
			statusCode: http.StatusNotImplemented,
			wantCode:   agentos.CodeNotSupported,
		},
		{
			name:       "HTTP_502_BadGateway",
			statusCode: http.StatusBadGateway,
			wantCode:   agentos.CodeServerError,
		},
		{
			name:       "HTTP_503_ServiceUnavailable",
			statusCode: http.StatusServiceUnavailable,
			wantCode:   agentos.CodeServerError,
		},
		{
			name:       "HTTP_504_GatewayTimeout",
			statusCode: http.StatusGatewayTimeout,
			wantCode:   agentos.CodeTimeout,
		},
		{
			name:       "HTTP_505_HTTPVersionNotSupported",
			statusCode: http.StatusHTTPVersionNotSupported,
			wantCode:   agentos.CodeNotSupported,
		},
		{
			name:       "HTTP_507_InsufficientStorage",
			statusCode: 507,
			wantCode:   agentos.CodeServerError,
		},
		{
			name:       "HTTP_508_LoopDetected",
			statusCode: 508,
			wantCode:   agentos.CodeServerError,
		},
		{
			name:       "HTTP_511_NetworkAuthenticationRequired",
			statusCode: 511,
			wantCode:   agentos.CodeUnauthorized,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				w.WriteHeader(tc.statusCode)
			}))
			defer ts.Close()

			c, err := NewClient(agentos.WithEndpoint(ts.URL))
			if err != nil {
				t.Fatal(err)
			}
			defer c.Close()

			_, err = c.Get(context.Background(), "/test")

			if tc.wantCode == "" {
				if err != nil {
					t.Errorf("%s: 不期望错误但返回: %v", tc.name, err)
				}
			} else {
				if err == nil {
					t.Errorf("%s: 期望错误码 %s 但未返回错误", tc.name, tc.wantCode)
				} else if !agentos.IsErrorCode(err, tc.wantCode) {
					t.Errorf("%s: 期望错误码 %s, got %v", tc.name, tc.wantCode, err)
				}
			}
		})
	}
}

func TestHTTPStatusToError_EdgeCases(t *testing.T) {
	testCases := []struct {
		name       string
		statusCode int
		wantCode   agentos.ErrorCode
	}{
		{
			name:       "未知状态码_99",
			statusCode: 99,
			wantCode:   agentos.CodeUnknown,
		},
		{
			name:       "未知状态码_600",
			statusCode: 600,
			wantCode:   agentos.CodeUnknown,
		},
		{
			name:       "未知状态码_999",
			statusCode: 999,
			wantCode:   agentos.CodeUnknown,
		},
	}

	for _, tc := range testCases {
		t.Run(tc.name, func(t *testing.T) {
			ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				w.WriteHeader(tc.statusCode)
			}))
			defer ts.Close()

			c, err := NewClient(agentos.WithEndpoint(ts.URL))
			if err != nil {
				t.Fatal(err)
			}
			defer c.Close()

			_, err = c.Get(context.Background(), "/test")

			if err == nil {
				t.Errorf("%s: 期望错误但未返回", tc.name)
			} else if !agentos.IsErrorCode(err, tc.wantCode) {
				t.Errorf("%s: 期望错误码 %s, got %v", tc.name, tc.wantCode, err)
			}
		})
	}
}

func TestHTTPStatusToError_WithRetry(t *testing.T) {
	var callCount int

	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		if callCount < 3 {
			w.WriteHeader(http.StatusServiceUnavailable)
			return
		}
		w.WriteHeader(http.StatusOK)
		w.Write([]byte(`{"success":true}`))
	}))
	defer ts.Close()

	c, err := NewClient(
		agentos.WithEndpoint(ts.URL),
		agentos.WithMaxRetries(3),
	)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	_, err = c.Get(context.Background(), "/test")
	if err != nil {
		t.Errorf("重试后应成功: %v", err)
	}

	if callCount != 3 {
		t.Errorf("期望调用3次，实际: %d", callCount)
	}
}

func TestHTTPStatusToError_NoRetryOnClientError(t *testing.T) {
	var callCount int

	ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		w.WriteHeader(http.StatusBadRequest)
	}))
	defer ts.Close()

	c, err := NewClient(
		agentos.WithEndpoint(ts.URL),
		agentos.WithMaxRetries(3),
	)
	if err != nil {
		t.Fatal(err)
	}
	defer c.Close()

	_, err = c.Get(context.Background(), "/test")
	if err == nil {
		t.Error("期望返回错误")
	}

	if callCount != 1 {
		t.Errorf("客户端错误不应重试，期望调用1次，实际: %d", callCount)
	}
}

func TestHTTPStatusToError_All5xxShouldRetry(t *testing.T) {
	retryableStatuses := []int{
		http.StatusInternalServerError,
		http.StatusBadGateway,
		http.StatusServiceUnavailable,
		http.StatusGatewayTimeout,
	}

	for _, status := range retryableStatuses {
		t.Run(http.StatusText(status), func(t *testing.T) {
			var callCount int

			ts := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				callCount++
				if callCount < 2 {
					w.WriteHeader(status)
					return
				}
				w.WriteHeader(http.StatusOK)
				w.Write([]byte(`{"success":true}`))
			}))
			defer ts.Close()

			c, err := NewClient(
				agentos.WithEndpoint(ts.URL),
				agentos.WithMaxRetries(3),
			)
			if err != nil {
				t.Fatal(err)
			}
			defer c.Close()

			_, err = c.Get(context.Background(), "/test")
			if err != nil {
				t.Errorf("状态码 %d 重试后应成功: %v", status, err)
			}

			if callCount < 2 {
				t.Errorf("状态码 %d 应触发重试，实际调用次数: %d", status, callCount)
			}
		})
	}
}
