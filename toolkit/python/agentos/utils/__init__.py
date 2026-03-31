# AgentOS Python SDK - Utilities Module Entry
# Version: 3.0.0
# Last updated: 2026-03-24

"""
Utilities module providing helper functions for AgentOS SDK.

This module exports all utility functions for type-safe data extraction,
API response parsing, URL building, and ID generation.

Corresponds to Go SDK: utils/helpers.go
"""

from .helpers import (
    # Map 类型安全提取函数
    get_string,
    get_int,
    get_float,
    get_bool,
    get_dict,
    get_list,
    # API 响应解析函数
    extract_data_map,
    build_url,
    parse_time_from_map,
    extract_int_stats,
    # ID/时间戳生成
    generate_id,
    generate_timestamp,
    # 验证和清理
    validate_json,
    sanitize_string,
    append_pagination,
    # 响应验证和提取函数
    validate_and_extract_data,
    # 参数校验函数
    validate_required_string,
    validate_positive_number,
    validate_non_empty_list,
)

__all__ = [
    # Map 类型安全提取函数
    "get_string",
    "get_int",
    "get_float",
    "get_bool",
    "get_dict",
    "get_list",
    # API 响应解析函数
    "extract_data_map",
    "build_url",
    "parse_time_from_map",
    "extract_int_stats",
    # ID/时间戳生成
    "generate_id",
    "generate_timestamp",
    # 验证和清理
    "validate_json",
    "sanitize_string",
    "append_pagination",
    # 响应验证和提取函数
    "validate_and_extract_data",
    # 参数校验函数
    "validate_required_string",
    "validate_positive_number",
    "validate_non_empty_list",
]
