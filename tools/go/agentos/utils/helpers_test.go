// AgentOS Go SDK - 工具函数模块单元测试
// Version: 3.0.0

package utils

import (
	"testing"
	"time"

	"github.com/spharx/agentos/tools/go/agentos/types"
)

func TestGetString(t *testing.T) {
	m := map[string]interface{}{"key": "value"}
	if GetString(m, "key") != "value" {
		t.Error("GetString 应返回 value")
	}
	if GetString(m, "missing") != "" {
		t.Error("缺少的 key 应返回空字符串")
	}
	if GetString(m, "wrong_type") != "" {
		t.Error("类型不匹配应返回空字符串")
	}
}

func TestGetInt64(t *testing.T) {
	m := map[string]interface{}{
		"int":   42,
		"int64": int64(99),
		"float": float64(3.7),
	}
	if GetInt64(m, "int") != 42 {
		t.Error("GetInt64(int) 失败")
	}
	if GetInt64(m, "int64") != 99 {
		t.Error("GetInt64(int64) 失败")
	}
	if GetInt64(m, "float") != 3 {
		t.Error("GetInt64(float) 截断失败")
	}
	if GetInt64(m, "missing") != 0 {
		t.Error("缺少的 key 应返回 0")
	}
}

func TestGetFloat64(t *testing.T) {
	m := map[string]interface{}{"val": 3.14}
	if GetFloat64(m, "val") != 3.14 {
		t.Error("GetFloat64 失败")
	}
}

func TestGetBool(t *testing.T) {
	m := map[string]interface{}{"flag": true}
	if !GetBool(m, "flag") {
		t.Error("GetBool 应返回 true")
	}
	if GetBool(m, "missing") {
		t.Error("缺少的 key 应返回 false")
	}
}

func TestGetMap(t *testing.T) {
	m := map[string]interface{}{
		"nested": map[string]interface{}{"inner": "val"},
	}
	nested := GetMap(m, "nested")
	if nested["inner"] != "val" {
		t.Error("GetMap 应返回嵌套 map")
	}
	if GetMap(m, "missing") != nil {
		t.Error("缺少的 key 应返回 nil")
	}
}

func TestGetStringMap(t *testing.T) {
	m := map[string]interface{}{
		"data": map[string]interface{}{"a": "1", "b": "2"},
	}
	result := GetStringMap(m, "data")
	if result["a"] != "1" || result["b"] != "2" {
		t.Error("GetStringMap 失败")
	}
}

func TestGetInterfaceSlice(t *testing.T) {
	m := map[string]interface{}{
		"items": []interface{}{"a", "b"},
	}
	items := GetInterfaceSlice(m, "items")
	if len(items) != 2 {
		t.Error("GetInterfaceSlice 失败")
	}
	if GetInterfaceSlice(m, "missing") != nil {
		t.Error("缺少的 key 应返回 nil")
	}
}

func TestExtractDataMap_Nil(t *testing.T) {
	_, ok := ExtractDataMap(nil)
	if ok {
		t.Error("nil 响应应返回 false")
	}
}

func TestExtractDataMap_Failed(t *testing.T) {
	resp := &types.APIResponse{Success: false}
	_, ok := ExtractDataMap(resp)
	if ok {
		t.Error("失败响应应返回 false")
	}
}

func TestExtractDataMap_Valid(t *testing.T) {
	resp := &types.APIResponse{
		Success: true,
		Data:    map[string]interface{}{"key": "val"},
	}
	data, ok := ExtractDataMap(resp)
	if !ok || data["key"] != "val" {
		t.Error("有效响应应返回正确数据")
	}
}

func TestBuildURL(t *testing.T) {
	if BuildURL("/path", nil) != "/path" {
		t.Error("无参数应返回原路径")
	}
	url := BuildURL("/path", map[string]string{"a": "1", "b": "2"})
	if url != "/path?a=1&b=2" && url != "/path?b=2&a=1" {
		t.Errorf("BuildURL = %q", url)
	}
}

func TestBuildURL_ExistingQuery(t *testing.T) {
	url := BuildURL("/path?x=1", map[string]string{"a": "2"})
	if url != "/path?x=1&a=2" && url != "/path?x=1&2=a" {
		t.Errorf("BuildURL with existing query = %q", url)
	}
}

func TestParseTimeFromMap(t *testing.T) {
	m := map[string]interface{}{
		"valid": "2026-03-22T00:00:00Z",
		"wrong": "not-a-date",
	}
	parsed := ParseTimeFromMap(m, "valid")
	if parsed.IsZero() {
		t.Error("合法时间应解析成功")
	}
	zero := ParseTimeFromMap(m, "wrong")
	if !zero.IsZero() {
		t.Error("非法时间应返回零值")
	}
}

func TestExtractInt64Stats(t *testing.T) {
	m := map[string]interface{}{
		"a": 1,
		"b": int64(2),
		"c": float64(3.7),
		"d": "string",
	}
	stats := ExtractInt64Stats(m)
	if stats["a"] != 1 || stats["b"] != 2 || stats["c"] != 3 {
		t.Errorf("ExtractInt64Stats = %v", stats)
	}
	if _, ok := stats["d"]; ok {
		t.Error("字符串值不应被提取")
	}
}

func TestParseResponseData_Invalid(t *testing.T) {
	err := ParseResponseData(nil, &map[string]interface{}{})
	if err == nil {
		t.Error("nil 响应应返回错误")
	}
}

func TestAppendPagination(t *testing.T) {
	params := AppendPagination(nil, 2, 10)
	if params["page"] != "2" || params["page_size"] != "10" {
		t.Errorf("AppendPagination = %v", params)
	}
	params = AppendPagination(nil, 0, 0)
	if len(params) != 0 {
		t.Errorf("零值不应追加参数, got %v", params)
	}
}

func TestGenerateID(t *testing.T) {
	id1 := GenerateID()
	if id1 == "" {
		t.Error("ID 不应为空")
	}
	if len(id1) < 5 {
		t.Errorf("ID 应有合理长度, got %d", len(id1))
	}
	time.Sleep(time.Nanosecond)
	id2 := GenerateID()
	if id2 == "" {
		t.Error("ID 不应为空")
	}
}

func TestGenerateTimestamp(t *testing.T) {
	ts := GenerateTimestamp()
	if ts <= 0 {
		t.Error("时间戳应大于零")
	}
	now := time.Now().Unix()
	if now-ts > 2 {
		t.Error("时间戳应接近当前时间")
	}
}

func TestValidateJSON(t *testing.T) {
	if !ValidateJSON(`{"a":1}`) {
		t.Error("合法 JSON 应返回 true")
	}
	if ValidateJSON(`not json`) {
		t.Error("非法 JSON 应返回 false")
	}
}

func TestSanitizeString(t *testing.T) {
	if SanitizeString("  hello  ") != "hello" {
		t.Error("应去除前后空格")
	}
	if SanitizeString("a\x00b") != "ab" {
		t.Error("应移除 null 字符")
	}
	if SanitizeString("a\r\nb") != "a\nb" {
		t.Error("应替换 CRLF 为 LF")
	}
}
