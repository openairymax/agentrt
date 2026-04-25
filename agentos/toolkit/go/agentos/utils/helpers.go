// AgentOS Go SDK - 公共辅助函数模块
// Version: 3.0.0
// Last updated: 2026-03-22
//
// 提供类型安全的 map 数据提取、API 响应解析和 URL 构建等通用工具函数。
// 对应 Python SDK: utils.py + utils/__init__.py

package utils

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"net/url"
	"strings"
	"time"

	"github.com/spharx/agentos/toolkit/go/agentos/types"
)

// ============================================================
// Map 类型安全提取函数
// ============================================================

// GetString 从 map 中安全提取字符串值
func GetString(m map[string]interface{}, key string) string {
	if v, ok := m[key]; ok {
		if s, ok := v.(string); ok {
			return s
		}
	}
	return ""
}

// GetInt64 从 map 中安全提取 int64 值
func GetInt64(m map[string]interface{}, key string) int64 {
	if v, ok := m[key]; ok {
		switch n := v.(type) {
		case int:
			return int64(n)
		case int64:
			return n
		case float64:
			return int64(n)
		case json.Number:
			if i, err := n.Int64(); err == nil {
				return i
			}
		}
	}
	return 0
}

// GetFloat64 从 map 中安全提取 float64 值
func GetFloat64(m map[string]interface{}, key string) float64 {
	if v, ok := m[key]; ok {
		switch n := v.(type) {
		case float64:
			return n
		case int:
			return float64(n)
		case int64:
			return float64(n)
		case json.Number:
			if f, err := n.Float64(); err == nil {
				return f
			}
		}
	}
	return 0
}

// GetFloat 从 map 中安全提取 float64 值，支持默认值
func GetFloat(m map[string]interface{}, key string, defaultValue float64) float64 {
	if v, ok := m[key]; ok {
		switch n := v.(type) {
		case float64:
			return n
		case int:
			return float64(n)
		case int64:
			return float64(n)
		case json.Number:
			if f, err := n.Float64(); err == nil {
				return f
			}
		}
	}
	return defaultValue
}

// GetTime 从 map 中安全提取并解析时间，支持默认值
func GetTime(m map[string]interface{}, key string, defaultValue time.Time) time.Time {
	if v, ok := m[key]; ok {
		switch t := v.(type) {
		case string:
			if parsed, err := time.Parse(time.RFC3339, t); err == nil {
				return parsed
			}
		case float64:
			secs := int64(t)
			return time.Unix(secs, 0)
		case int64:
			return time.Unix(t, 0)
		}
	}
	return defaultValue
}

// GetBoolWithDefault 从 map 中安全提取 bool 值，支持默认值
func GetBoolWithDefault(m map[string]interface{}, key string, defaultValue bool) bool {
	if v, ok := m[key]; ok {
		if b, ok := v.(bool); ok {
			return b
		}
	}
	return defaultValue
}

// GetBool 从 map 中安全提取 bool 值
func GetBool(m map[string]interface{}, key string) bool {
	if v, ok := m[key]; ok {
		if b, ok := v.(bool); ok {
			return b
		}
	}
	return false
}

// GetMap 从 map 中安全提取嵌套 map
func GetMap(m map[string]interface{}, key string) map[string]interface{} {
	if v, ok := m[key]; ok {
		if nested, ok := v.(map[string]interface{}); ok {
			return nested
		}
	}
	return nil
}

// GetStringMap 从 map 中安全提取 string→string map
func GetStringMap(m map[string]interface{}, key string) map[string]string {
	result := make(map[string]string)
	if v, ok := m[key]; ok {
		if nested, ok := v.(map[string]interface{}); ok {
			for k, val := range nested {
				if s, ok := val.(string); ok {
					result[k] = s
				}
			}
		}
	}
	return result
}

// GetInterfaceSlice 从 map 中安全提取 []interface{} 切片
func GetInterfaceSlice(m map[string]interface{}, key string) []interface{} {
	if v, ok := m[key]; ok {
		if slice, ok := v.([]interface{}); ok {
			return slice
		}
	}
	return nil
}

// ============================================================
// API 响应解析函数
// ============================================================

// ExtractDataMap 从 APIResponse 中提取 Data 字段为 map
func ExtractDataMap(resp *types.APIResponse) (map[string]interface{}, bool) {
	if resp == nil || !resp.Success || resp.Data == nil {
		return nil, false
	}
	data, ok := resp.Data.(map[string]interface{})
	return data, ok
}

// BuildURL 拼接基础路径和查询参数，返回完整 URL
func BuildURL(basePath string, queryParams map[string]string) string {
	if len(queryParams) == 0 {
		return basePath
	}
	if strings.Contains(basePath, "?") {
		return basePath + "&" + buildQueryString(queryParams)
	}
	return basePath + "?" + buildQueryString(queryParams)
}

// buildQueryString 将参数 map 编码为查询字符串
func buildQueryString(params map[string]string) string {
	if len(params) == 0 {
		return ""
	}
	values := url.Values{}
	for k, v := range params {
		values.Add(k, v)
	}
	return values.Encode()
}

// ParseTimeFromMap 从 map 中安全提取并解析时间
func ParseTimeFromMap(m map[string]interface{}, key string) time.Time {
	if v, ok := m[key]; ok {
		switch t := v.(type) {
		case string:
			if parsed, err := time.Parse(time.RFC3339, t); err == nil {
				return parsed
			}
		case float64:
			secs := int64(t)
			return time.Unix(secs, 0)
		case int64:
			return time.Unix(t, 0)
		}
	}
	return time.Time{}
}

// ExtractInt64Stats 从 map 中提取所有 int64 类型的统计值
func ExtractInt64Stats(data map[string]interface{}) map[string]int64 {
	stats := make(map[string]int64)
	for k, v := range data {
		switch n := v.(type) {
		case int:
			stats[k] = int64(n)
		case int64:
			stats[k] = n
		case float64:
			stats[k] = int64(n)
		}
	}
	return stats
}

// ParseResponseData 将 APIResponse.Data 解析到目标结构体
func ParseResponseData(resp *types.APIResponse, target interface{}) error {
	if resp == nil || !resp.Success {
		return fmt.Errorf("无效响应")
	}
	data, err := json.Marshal(resp.Data)
	if err != nil {
		return fmt.Errorf("序列化响应数据失败: %w", err)
	}
	return json.Unmarshal(data, target)
}

// AppendPagination 向查询参数追加分页信息
func AppendPagination(params map[string]string, page, pageSize int) map[string]string {
	if params == nil {
		params = make(map[string]string)
	}
	if page > 0 {
		params["page"] = fmt.Sprintf("%d", page)
	}
	if pageSize > 0 {
		params["page_size"] = fmt.Sprintf("%d", pageSize)
	}
	return params
}

// ============================================================
// ID/时间戳生成
// 对应 Python SDK: utils.py (generate_id, generate_timestamp)
// ============================================================

// GenerateID 生成唯一的 AgentOS ID（时间戳+密码学安全随机数）
func GenerateID() string {
	b := make([]byte, 4)
	_, _ = rand.Read(b)
	return fmt.Sprintf("aos_%d_%s", time.Now().UnixNano(), hex.EncodeToString(b))
}

// GenerateTimestamp 生成当前 Unix 时间戳（秒）
func GenerateTimestamp() int64 {
	return time.Now().Unix()
}

// ============================================================
// 验证和清理
// 对应 Python SDK: utils.py (validate_json, sanitize_string)
// ============================================================

// ValidateJSON 验证字符串是否为合法 JSON
func ValidateJSON(s string) bool {
	var v interface{}
	return json.Unmarshal([]byte(s), &v) == nil
}

// SanitizeString 清理字符串中的危险字符
func SanitizeString(s string) string {
	s = strings.TrimSpace(s)
	s = strings.ReplaceAll(s, "\x00", "")
	s = strings.ReplaceAll(s, "\r\n", "\n")
	return s
}

// ============================================================
// 响应验证和提取函数
// ============================================================

// ValidateAndExtractData 验证并提取响应数据，如果数据无效则返回错误
func ValidateAndExtractData(resp *types.APIResponse, errorMsg string) (map[string]interface{}, error) {
	data, ok := ExtractDataMap(resp)
	if !ok {
		return nil, fmt.Errorf("%s", errorMsg)
	}
	return data, nil
}

// ============================================================
// 参数校验函数
// ============================================================

// ValidateRequiredString 验证字符串参数不为空
func ValidateRequiredString(value string, paramName string) error {
	if value == "" {
		return fmt.Errorf("%s不能为空", paramName)
	}
	return nil
}

// ValidatePositiveNumber 验证数字参数为正数
func ValidatePositiveNumber(value int64, paramName string) error {
	if value <= 0 {
		return fmt.Errorf("%s必须为正数", paramName)
	}
	return nil
}

// ValidateNonEmptySlice 验证切片参数不为空
func ValidateNonEmptySlice[T any](value []T, paramName string) error {
	if len(value) == 0 {
		return fmt.Errorf("%s不能为空", paramName)
	}
	return nil
}

